// cspell:ignore VDDA IIDX DDEVICE

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
//#include <span>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"


#include <ti/driverlib/driverlib.h>

#include "../hal/adc.hpp"
#include "../hal/crc.hpp"
#include "../hal/gpio.hpp"
#include "../hal/serial.hpp"
#include "../hal/timer.hpp"
#include "../drivers/msp0g_adc.hpp"
#include "../drivers/msp0g_crc.hpp"
#include "../drivers/msp0g_gpio.hpp"
#include "../drivers/msp0g_iomux.hpp"
#include "../drivers/msp0g_timer.hpp"
#include "../drivers/msp0g_uart.hpp"

// The ADC driver wakes the task that is blocked in msp0g_adc::read() when
// the DMA transfer has completed.  The symbol name is the vector entry used
// by the MSPM0 startup code.
extern "C" void ADC0_IRQHandler()
{
  msp0g::msp0g_adc::handle_interrupt();
}

// =============================================================================
// Role 1: ADC
// =============================================================================

// The ADC driver is msp0g::msp0g_adc (msp0g_adc.hpp / msp0g_adc.cpp).
namespace msp0g {

/** @brief Samples per batch. Must match samples_per_batch in the app file. */
inline constexpr std::size_t sampler_samples_per_batch = 64;

/**
 * @brief Semaphore and queue passed into sampler_task.
 *
 * The instance must have static storage duration.
 */
struct sampler_context
{
  SemaphoreHandle_t sample_request;
  QueueHandle_t batch_queue;
};

/**
 * @brief Wait for a request, sample, convert, and queue the batch.
 *
 * @param p_context Pointer to a static sampler_context. A null pointer parks
 * the task; a FreeRTOS task must not return.
 */
void sampler_task(void* p_context)
{
  auto* context = static_cast<sampler_context*>(p_context);

  if (context == nullptr) {
    while (true) {
      vTaskDelay(portMAX_DELAY);
    }
  }

  msp0g_adc adc;
  const std::uint16_t trim_sample = temperature_trim_sample();
  std::array<std::uint16_t, sampler_samples_per_batch> raw_samples{};
  std::array<temperature_sample_t, sampler_samples_per_batch> batch{};

  while (true) {
    xSemaphoreTake(context->sample_request, portMAX_DELAY);
    adc.read(raw_samples);

    std::int32_t sum = 0;
    for (std::size_t i = 0; i < raw_samples.size(); i++) {
      batch[i] = to_temperature(raw_samples[i], trim_sample);
      sum += batch[i];
    }

    // Remove once the display board shows the temperature.
    const std::int32_t average =
      sum / static_cast<std::int32_t>(raw_samples.size());
    std::printf("batch raw[0]=%u raw[63]=%u  %ld.%ld C\n",
                static_cast<unsigned>(raw_samples[0]),
                static_cast<unsigned>(raw_samples.back()),
                static_cast<long>(average / 10),
                static_cast<long>(average % 10));

    xQueueSend(context->batch_queue, &batch, portMAX_DELAY);
  }
}

}  // namespace msp0g

// =============================================================================
// Role 2: UART
// =============================================================================

// Role 2 lives in msp0g_uart.hpp / msp0g_uart.cpp. The constructor receives
// the serial settings and the UART instance, applies the baud dividers, and
// enables the RX interrupt. driver_configure() remains because lab6::serial
// requires it, and the constructor applies the initial settings itself.

//aliasing
using serial_driver = msp0g::msp0g_uart;

// =============================================================================
// Role 3: CRC
// =============================================================================

// Role 3's hardware driver is lab6::crc32_hardware in msp0g_crc.hpp. The
// supplied software implementation is the benchmark reference.

// Supplied reference implementation - do not modify. This is the standard
// reflected CRC-32 (IEEE 802.3 / zlib) algorithm, computed bit-by-bit in
// software. Your crc32_hardware driver's output must match this for the
// same input, and should be measurably faster.
class crc32_software : public lab6::crc32
{
private:
  void driver_reset() override
  {
    m_crc = initial_value;
  }

  void driver_update(std::span<std::uint8_t const> p_data) override
  {
    for (auto const byte : p_data) {
      m_crc ^= byte;
      for (int bit = 0; bit < 8; bit++) {
        std::uint32_t const mask = -(m_crc & 1U);
        m_crc = (m_crc >> 1) ^ (polynomial & mask);
      }
    }
  }

  std::uint32_t driver_checksum() override
  {
    return m_crc ^ initial_value;
  }

  static constexpr std::uint32_t polynomial = 0xEDB8'8320U;
  static constexpr std::uint32_t initial_value = 0xFFFF'FFFFU;

  std::uint32_t m_crc = initial_value;
};

namespace {
// =============================================================================
// Shared between both boards
// =============================================================================

// The one-byte message the display board sends to ask the sensor board for
// a fresh batch of readings.
constexpr std::uint8_t sample_request_command = 0xA5;

constexpr std::size_t samples_per_batch =
  msp0g::sampler_samples_per_batch;

// Signed fixed-point temperature in tenths of a degree Celsius. Both boards
// use the MSPM0's little-endian byte order.
using temperature_sample_t = msp0g::temperature_sample_t;

// One batch of converted samples. This is the item type of batch_queue.
using temperature_batch = std::array<temperature_sample_t, samples_per_batch>;

// A packet on the wire: the batch's bytes, followed by the 4-byte CRC-32.
constexpr std::size_t packet_size =
  sizeof(temperature_batch) + sizeof(std::uint32_t);

constexpr msp0g::iomux_config uart_tx_config{
  .pincm = 23,
  .pf_sel = 2,
  .inena = msp0g::input_enable::disable,
};

constexpr msp0g::iomux_config uart_rx_config{
  .pincm = 24,
  .pf_sel = 2,
  .inena = msp0g::input_enable::enable,
};

constexpr lab6::serial::settings uart_settings{
  .baud_rate = 115'200,
  .stop = lab6::serial::settings::stop_bits::one,
  .parity = lab6::serial::settings::parity::none,
};

constexpr msp0g::gpio_pin sw1_pin{ msp0g::gpio_port::a, 18, 40 };
constexpr msp0g::gpio_pin sw2_pin{ msp0g::gpio_port::b, 21, 49 };
constexpr msp0g::gpio_pin blue_led_pin{ msp0g::gpio_port::b, 22, 50 };
constexpr msp0g::gpio_pin red_led_pin{ msp0g::gpio_port::b, 26, 57 };
constexpr msp0g::gpio_pin green_led_pin{ msp0g::gpio_port::b, 27, 58 };

// =============================================================================
// Sensor board: semaphore and queue (role 4 creates these in main())
// =============================================================================

// The professor supplies the handle for the binary semaphore.
SemaphoreHandle_t sample_request = nullptr;
// (Eslin) I reserve memory for the semaphore and create it in main().
// I use it to tell Dylan's sampler to take a batch; it carries no sample data.
StaticSemaphore_t sample_request_storage;

// The professor supplies the handle used to send and receive batches.
QueueHandle_t batch_queue = nullptr;
// (Eslin) I reserve memory for the queue control block and one full batch.
// FreeRTOS copies the sampler's data into the queue, so the sampler and
// transmitter do not have to share the same array.
StaticQueue_t batch_queue_storage;
// One queue slot: enough space for the entire 64-sample array (128 bytes).
std::uint8_t batch_queue_buffer[sizeof(temperature_batch)];

// =============================================================================
// Sensor board: tasks
// =============================================================================

// (Eslin) Request task: I watch the UART receive buffer for command 0xA5.
// This task only reads the UART; Kaden's transmit task handles writing.
void request_task(void* p_context)
{
    // (Eslin) FreeRTOS passes my UART pointer as void*. I cast it back
    // to lab6::serial* so I can call the serial interface's functions.
    auto* uart = static_cast<lab6::serial*>(p_context);

    // Get a view of the driver's circular receive buffer, not a copy of it.
    auto buffer = uart->receive_buffer();

    // Begin at the driver's current write position so bytes left over from
    // before this task started are not interpreted as a new request.
    std::size_t read_cursor = uart->receive_cursor();

    while (true) {

        // The UART driver updates this cursor as bytes arrive.
        // It points to where the NEXT byte will be written.
        std::size_t write_cursor = uart->receive_cursor();

        // Different cursors mean there are unread bytes in the buffer.
        // A full lap between polls would be indistinguishable from no new data.
        while (read_cursor != write_cursor) {

            // Read one byte from the position I have not processed yet.
            std::uint8_t received_byte = buffer[read_cursor];

            // Only 0xA5 is a request. Other bytes are ignored.
            if (received_byte == sample_request_command) {
                // Signal the sampler. If it is blocked on the semaphore,
                // FreeRTOS makes it ready to run. No data is copied here.
                // A binary semaphore holds at most one pending request.
                xSemaphoreGive(sample_request);
            }

            // Move to the next byte. Modulo wraps back to index 0 when
            // I reach the end of the circular buffer.
            read_cursor = (read_cursor + 1) % buffer.size();
        }

        // Convert 5 ms to scheduler ticks. Use at least one tick so the
        // task blocks and other tasks can run, even if 5 ms rounds to zero.
        constexpr TickType_t delay_ticks = pdMS_TO_TICKS(5);
        vTaskDelay(delay_ticks > 0 ? delay_ticks : 1);
    }
}

// Integration note (Role 3): callback used by the transmitter to check S2.
bool corruption_button_held()
{
  // Static means the button is configured once, on the first call.
  static msp0g::msp0g_inpin button(
    sw2_pin, { .resistor = lab1::pin_resistor::pull_up });

  // Invert the electrical level so true means the button is pressed.
  return !button.level();
}

// Kaden's context structure (Role 3), used by the sensor setup in main().
// It groups the queue, shared UART and button callback into one task argument.
struct transmit_task_context
{
  QueueHandle_t batch_queue;
  lab6::serial* uart;
  bool (*corruption_button_held)();
};

// Kaden (Role 3): CRC and transmit task, adapted during integration.
void transmit_task(void* p_parameters)
{
  auto* context = static_cast<transmit_task_context*>(p_parameters);

  if (context == nullptr || context->batch_queue == nullptr ||
      context->uart == nullptr) {
    std::printf("Invalid transmit task context\n");

    while (true) {
      vTaskDelay(1);
    }
  }

  temperature_batch samples{};

  lab6::crc32_hardware hardware_crc;
  crc32_software software_crc;

  msp0g::msp0g_steady_clock clock;

  while (true) {
    // Wait for Dylan's temperature batch.
    if (xQueueReceive(context->batch_queue,
                      &samples,
                      portMAX_DELAY) != pdPASS) {
      continue;
    }

    // Access the temperature samples as bytes.
    auto const sample_bytes = std::span<std::uint8_t const>{
      reinterpret_cast<std::uint8_t const*>(samples.data()), sizeof(samples)
    };

    // Calculate and benchmark hardware CRC. Reset happens before timing;
    // the measured interval covers update() and checksum().
    hardware_crc.reset();

    auto const hardware_start =
      static_cast<std::uint32_t>(clock.uptime());

    hardware_crc.update(sample_bytes);

    auto const hardware_checksum = hardware_crc.checksum();

    std::uint32_t hardware_elapsed =
      static_cast<std::uint32_t>(clock.uptime()) - hardware_start;

    // Calculate and benchmark software CRC.
    software_crc.reset();

    auto const software_start =
      static_cast<std::uint32_t>(clock.uptime());

    software_crc.update(sample_bytes);

    auto const software_checksum = software_crc.checksum();

    std::uint32_t software_elapsed =
      static_cast<std::uint32_t>(clock.uptime()) - software_start;

    // Print the measured timer counts.
    std::printf("CRC hardware: %lu ticks, software: %lu ticks\n",
                static_cast<unsigned long>(hardware_elapsed),
                static_cast<unsigned long>(software_elapsed));

    std::printf("Timer frequency: %lu Hz\n",
                static_cast<unsigned long>(clock.frequency()));

    // Compare the hardware and software results.
    if (hardware_checksum != software_checksum) {
      std::printf("CRC mismatch: HW=0x%08lX SW=0x%08lX\n",
                  static_cast<unsigned long>(hardware_checksum),
                  static_cast<unsigned long>(software_checksum));
    } else {
      std::printf("Hardware and software CRC match\n");
    }

    // Corrupt one bit after calculating the CRC. The checksum stays
    // unchanged, so the display board should detect the changed data.
    if (context->corruption_button_held != nullptr &&
        context->corruption_button_held()) {
      auto* mutable_bytes =
        reinterpret_cast<std::uint8_t*>(samples.data());

      mutable_bytes[0] ^= 0x01U;

      std::printf("CRC corruption test\n");
    }

    // Send the 128 bytes of temperature data.
    context->uart->write(sample_bytes);

    // Convert the CRC to four little-endian bytes.
    std::array<std::uint8_t, 4> crc_bytes{
      static_cast<std::uint8_t>(hardware_checksum >> 0),
      static_cast<std::uint8_t>(hardware_checksum >> 8),
      static_cast<std::uint8_t>(hardware_checksum >> 16),
      static_cast<std::uint8_t>(hardware_checksum >> 24)
    };

    // Send the CRC after the temperature data.
    context->uart->write(
      std::span<std::uint8_t const>{ crc_bytes.data(), crc_bytes.size() });
  }
}

// =============================================================================
// Display board: task
// =============================================================================

bool button_pressed(lab1::input_pin& p_s1, lab1::input_pin& p_s2)
{
  return p_s1.level() || !p_s2.level();
}

void set_rgb(lab1::output_pin& p_red,
             lab1::output_pin& p_green,
             lab1::output_pin& p_blue,
             bool p_red_on,
             bool p_green_on,
             bool p_blue_on)
{
  p_red.level(p_red_on);
  p_green.level(p_green_on);
  p_blue.level(p_blue_on);
}

void display_task(void*)
{
  msp0g::msp0g_iomux uart_tx(uart_tx_config, {});
  msp0g::msp0g_iomux uart_rx(uart_rx_config, {});
  serial_driver serial(uart_settings, msp0g::uartx::uart1);
  lab6::crc32_hardware crc;

  msp0g::msp0g_inpin s1(
    sw1_pin, { .resistor = lab1::pin_resistor::pull_down });
  msp0g::msp0g_inpin s2(
    sw2_pin, { .resistor = lab1::pin_resistor::pull_up });
  msp0g::msp0g_outpin red(red_led_pin);
  msp0g::msp0g_outpin green(green_led_pin);
  msp0g::msp0g_outpin blue(blue_led_pin);

  set_rgb(red, green, blue, false, false, false);

  auto const receive_buffer = serial.receive_buffer();
  constexpr TickType_t receive_timeout = pdMS_TO_TICKS(500);
  constexpr TickType_t poll_delay = pdMS_TO_TICKS(1);

  while (true) {
    //button check
    while (!button_pressed(s1, s2)) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    //debounce
    vTaskDelay(pdMS_TO_TICKS(20));
    if (!button_pressed(s1, s2)) {
      continue;
    }

    std::size_t const start_cursor = serial.receive_cursor();
    constexpr std::array<std::uint8_t, 1> request{
      sample_request_command
    };
    serial.write(request);

    TickType_t const start_tick = xTaskGetTickCount();
    bool packet_ready = false;

    while ((xTaskGetTickCount() - start_tick) < receive_timeout) {
      std::size_t const cursor = serial.receive_cursor();
      std::size_t const received =
        (cursor + receive_buffer.size() - start_cursor) %
        receive_buffer.size();

      if (received >= packet_size) {
        packet_ready = true;
        break;
      }

      vTaskDelay(poll_delay);
    }

    if (!packet_ready) {
      std::printf("Timed out waiting for sensor packet\n");
      set_rgb(red, green, blue, false, false, false);
    } else {
      std::array<std::uint8_t, packet_size> packet{};

      for (std::size_t index = 0; index < packet.size(); index++) {
        packet[index] =
          receive_buffer[(start_cursor + index) % receive_buffer.size()];
      }

      temperature_batch samples{};
      auto* sample_bytes =
        reinterpret_cast<std::uint8_t*>(samples.data());

      for (std::size_t index = 0; index < sizeof(samples); index++) {
        sample_bytes[index] = packet[index];
      }

      std::uint32_t received_crc = 0;

      for (std::size_t index = 0; index < sizeof(received_crc); index++) {
        received_crc |=
          static_cast<std::uint32_t>(packet[sizeof(samples) + index]) <<
          (index * 8U);
      }

      crc.reset();
      crc.update(std::span<std::uint8_t const>{ packet.data(),
                                                sizeof(samples) });

      if (crc.checksum() != received_crc) {
        std::printf("CRC mismatch\n");
        set_rgb(red, green, blue, true, false, true);
      } else {
        std::int32_t sum = 0;

        for (auto const sample : samples) {
          sum += sample;
        }

        std::int32_t const average =
          sum / static_cast<std::int32_t>(samples.size());

        // uncomment for debugging/verifying
        // std::printf("Average temperature: %ld.%ld C\n",
        //             static_cast<long>(average / 10),
        //             static_cast<long>(average % 10));

        if (average < 250) {
          set_rgb(red, green, blue, false, false, true);
        } else if (average < 350) {
          set_rgb(red, green, blue, true, true, false);
        } else {
          set_rgb(red, green, blue, true, false, false);
        }
      }
    }

    while (button_pressed(s1, s2)) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

// =============================================================================
// Task storage
// =============================================================================

// Shared stack depth supplied in the professor's starter code.
// Resize task_stack_words if necessary. Crashes can be caused by stack
// overflows.
constexpr std::uint32_t task_stack_words = 512;

// Display board: one task (professor's starter storage for Sonal's task).
StaticTask_t display_task_tcb;
StackType_t display_task_stack[task_stack_words];

// This lab uses two boards playing different roles. Both roles live in this
// one file, and `this_device` below picks which tasks main() creates.
//
// For this lab, flip this by hand and rebuild before flashing each board.
// A more automated setup (e.g. a CI job that builds and flashes both roles
// from a single invocation) would instead read this from a compiler define
// passed on the build system's command line - something like
// `-DDEVICE_ROLE=DEVICE_ROLE_SENSOR` added to the project's build flags -
// rather than a hardcoded constant, so the role never has to be hand-edited
// in source.

// (Eslin) I give each of the three sensor tasks its own TCB and stack.
// Each stack has 512 StackType_t entries, not 512 bytes, for local variables
// and function calls. Each TCB stores scheduling and task information.
// I reserve these at file scope so they last for the lifetime of the tasks.
// Sensor board: request task.
StaticTask_t request_task_tcb;
StackType_t request_task_stack[task_stack_words];

// Sensor board: sampler task.
StaticTask_t sampler_task_tcb;
StackType_t sampler_task_stack[task_stack_words];

// Sensor board: transmit task.
StaticTask_t transmit_task_tcb;
StackType_t transmit_task_stack[task_stack_words];

enum class device_role : std::uint8_t
{
  sensor,
  display,
};

// Shared starter-code role selection. Change to display for Sonal's board.
// Sensor runs three tasks; display runs one.
constexpr device_role this_device = device_role::sensor;
}  // namespace

// Application entry point (professor's starter code with sensor setup added).
int main()
{
    //std::printf("Hello, World\n");

    // The role is known at compile time, so only the selected setup runs.
    if constexpr (this_device == device_role::sensor) {

        // (Eslin) Create the binary semaphore using the storage I reserved above.
        // It starts empty; the sampler waits until request_task gives it.
        sample_request = xSemaphoreCreateBinaryStatic(
            &sample_request_storage);

        // (Eslin) Create a queue that can hold one complete temperature batch.
        // I supply the storage, so FreeRTOS does not dynamically allocate it.
        batch_queue = xQueueCreateStatic(
            1,                          // Number of items the queue holds.
            sizeof(temperature_batch),  // Bytes copied for each item.
            batch_queue_buffer,         // Memory for the actual data.
            &batch_queue_storage);      // Memory for queue control information.

        // Configure the UART1 pins before constructing the UART driver.
        // These objects must outlive the tasks that use the peripheral.
        static msp0g::msp0g_iomux uart_tx(uart_tx_config, {});
        static msp0g::msp0g_iomux uart_rx(uart_rx_config, {});

        // Create one shared UART object. Static keeps it alive for both
        // request_task and transmit_task after the scheduler starts.
        static serial_driver uart(
            uart_settings,
            msp0g::uartx::uart1);

        // Both tasks use the same driver through the professor's interface.
        lab6::serial* sensor_uart = &uart;

        // (Eslin) Create the task that reads incoming UART requests.
        // xTaskCreateStatic uses the stack and TCB I already reserved.
        xTaskCreateStatic(
            request_task,            // Function the scheduler will run.
            "request_task",          // Task name for debugging.
            task_stack_words,        // Stack depth in StackType_t entries.
            sensor_uart,             // Pointer passed to the task as void*.
            tskIDLE_PRIORITY + 1,    // One priority level above idle.
            request_task_stack,      // This task's own stack.
            &request_task_tcb);      // This task's own control block.

        // Pass the sampler the same semaphore and queue used by the request
        // and transmit tasks.  The context has static storage duration, so it
        // remains valid for the entire lifetime of the task.
        static msp0g::sampler_context sampler_context{
            sample_request,
            batch_queue
        };

        xTaskCreateStatic(
            msp0g::sampler_task,
            "sampler_task",
            task_stack_words,
            &sampler_context,
            tskIDLE_PRIORITY + 1,
            sampler_task_stack,
            &sampler_task_tcb);

        // (Eslin) I pass Kaden the queue, the same UART, and the S2 function.
        // Keep the context static so the pointer stays valid while tasks run.
        static transmit_task_context transmit_context{
            batch_queue,
            sensor_uart,
            corruption_button_held
        };

        // (Eslin) Create the transmitter. It receives the address of the
        // context instead of just the UART, so it can also access the queue.
        // All three sensor tasks use the same priority and separate stacks.
        xTaskCreateStatic(
            transmit_task,
            "transmit_task",
            task_stack_words,
            &transmit_context,
            tskIDLE_PRIORITY + 1,
            transmit_task_stack,
            &transmit_task_tcb);

    } else {

        // Professor's starter code: create only the display task (Sonal).
        // The sensor tasks, semaphore and queue are not created in this branch.
        xTaskCreateStatic(
            display_task,
            "display_task",
            task_stack_words,
            nullptr,
            tskIDLE_PRIORITY + 1,
            display_task_stack,
            &display_task_tcb);
    }

    // Professor's starter code: start scheduling after the tasks are ready.
    // FreeRTOS now decides which ready task runs. This normally never returns.
    vTaskStartScheduler();

    // Only reached if vTaskStartScheduler() fails to start.
    // The idle/timer task memory callbacks required by
    // configSUPPORT_STATIC_ALLOCATION are supplied by the freertos_kernel
    // project's dpl/StaticAllocs_freertos.c - don't redefine them here.
    while (true) {
    }
}
