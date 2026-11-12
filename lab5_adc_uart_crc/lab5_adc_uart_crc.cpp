// cspell:ignore VDDA IIDX DDEVICE
// Lab 5: ADC + UART + CRC using FreeRTOS
//
// Team roles:
//   Dylan (Role 1): ADC, DMA, temperature conversion, sampler_task().
//   Koby  (Role 2): UART driver and physical UART testing.
//   Kaden (Role 3): CRC driver, benchmark and transmit_task().
//   Eslin (Role 4): Sensor task architecture, request_task(), semaphore,
//                  queue, task storage and main().
//   Sonal (Role 5): Display task, CRC validation and RGB LED response.
//
// Comments marked (Eslin) are my application work for check-off.
// Comments marked (Eslin - integration) describe the support code I added
// to connect and test my teammates' work. Their original roles stay the same.
// The professor's instructions are kept for the team as a reference.

#include <array>
#include <cstdint>
#include <cstdio>

// (Eslin) FreeRTOS gives me the task, semaphore and queue interfaces.
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

// DriverLib is required for Dylan's ADC/DMA driver. UART and CRC use
// direct register access, not DriverLib functions or SysConfig.
#include <ti/driverlib/driverlib.h>

#include "../hal/adc.hpp"
#include "../hal/crc.hpp"
#include "../hal/gpio.hpp"
#include "../hal/serial.hpp"
#include "../hal/timer.hpp"
#include "msp0g_uart.hpp"
#include "msp0g_crc.hpp"

// =============================================================================
// Role 1: ADC (Dylan)
// =============================================================================

// TODO(lab, role 1): Make this class inherit from and implement the
// `lab5::adc16_stream` interface (`adc.hpp`) using DriverLib, wired to the
// internal temperature sensor (ADC0 channel 11). Follow the "1. ADC" guide in
// the README:
//   - Constructor: power up ADC0, configure MEM0 for channel 11 against VDDA,
//     set the sample time, and set up the ADC's DMA trigger and DMA channel.
//   - driver_read(): re-arm the DMA for the given buffer, save the calling
//     task's handle, start the conversion, then sleep with ulTaskNotifyTake()
//     until ADC0_IRQHandler() wakes you. Upscale each 12-bit sample to 16 bits
//     before returning.
class adc_driver
{};

// TODO(lab, role 1): Store the handle of the task waiting in
// adc_driver::driver_read() somewhere this handler can reach. When
// DL_ADC12_getPendingInterrupt(ADC0) returns DL_ADC12_IIDX_DMA_DONE, wake that
// task with vTaskNotifyGiveFromISR() and finish with portYIELD_FROM_ISR().
extern "C" void ADC0_IRQHandler()
{}

// =============================================================================
// Role 2: UART (Koby)
// =============================================================================

// TODO(lab, role 2): Make this class inherit from and implement the
// `lab6::serial` interface (`serial.hpp`), written register-level - no
// DriverLib. driver_configure() computes the baud rate dividers from the
// requested settings. Reception fills a circular buffer from the UART's RX
// interrupt; driver_receive_cursor() returns where the next byte will be
// written. Make the buffer larger than one packet (256 bytes is safe).
//
// Add the matching `extern "C" void UARTn_IRQHandler()` for the UART instance
// you pick (UART0 through UART3).
// (Eslin - integration) Koby's implementation is in msp0g_uart.hpp/.cpp.
// This alias lets the application use his driver through the serial interface.
// Integrated and builds; physical UART TX/RX and pin setup still need testing.
using serial_driver = msp0g::msp0g_uart;

// =============================================================================
// Role 3: CRC (Kaden)
// =============================================================================

// TODO(lab, role 3): Make this class inherit from and implement the
// `lab6::crc32` interface (`crc.hpp`), written register-level against the
// on-chip CRC accelerator - no DriverLib. Its output must match
// crc32_software below for the same input. Check the accelerator's bit-order
// options and whether it applies the final XOR.
// (Eslin - integration) Kaden's implementation is in msp0g_crc.hpp/.cpp.
// The standard CRC test passed on my LaunchPad (see lab5-crc-test).
using crc32_hardware = lab6::crc32_hardware;

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

constexpr std::size_t samples_per_batch = 64;

// TODO(lab, team): Agree on an encoding for a single temperature sample that
// resolves at least 0.5 degC. A fixed-point scheme (e.g. int16_t in units of
// 0.1 degC) or a float both work. Both boards must use the same encoding and
// byte order.
using temperature_sample_t = std::int16_t;

// (Eslin) I use one array of 64 signed 16-bit samples as the queue item.
// That gives me 128 bytes of temperature data in each batch.
using temperature_batch = std::array<temperature_sample_t, samples_per_batch>;

// Shared packet format: 128 sample bytes followed by 4 CRC bytes (132 total).
// The current code sends the sample bytes in the MCU's little-endian order.
// The temperature scale still needs to be agreed on by the whole team.
constexpr std::size_t packet_size =
  sizeof(temperature_batch) + sizeof(std::uint32_t);

// =============================================================================
// Sensor board: semaphore and queue (Eslin)
// =============================================================================

// (Eslin) I use a binary semaphore to tell the sampler when to take a batch.
// It carries a signal, not the temperature readings. I create it in main().
SemaphoreHandle_t sample_request = nullptr;  // Handle used by the tasks.
StaticSemaphore_t sample_request_storage;  // Memory for the semaphore itself.

// (Eslin) I use a queue to pass a complete batch from the sampler to the
// transmitter. FreeRTOS copies the data so the tasks do not share one array.
QueueHandle_t batch_queue = nullptr;  // Handle used to send and receive data.
StaticQueue_t batch_queue_storage;  // Memory for the queue's control data.
// One queue slot: enough space for the entire 64-sample array (128 bytes).
std::uint8_t batch_queue_buffer[sizeof(temperature_batch)];

// =============================================================================
// Sensor board: tasks
// =============================================================================

// (Eslin) Request task: I watch the UART receive buffer for command 0xA5.
// When it arrives, I give the semaphore so Dylan's sampler can take a batch.
// This task only reads the UART; Kaden's transmit task handles writing.
void request_task(void* p_context)
{
    // (Eslin) FreeRTOS passes my UART pointer as void*. I cast it back
    // to lab6::serial* so I can call the serial interface's functions.
    auto* uart = static_cast<lab6::serial*>(p_context);

    // Get a view of the driver's circular receive buffer, not a copy of it.
    auto buffer = uart->receive_buffer();

    // Start reading at index 0. The UART is created before this task starts.
    std::size_t read_cursor = 0;

    while (true) {

        // The UART driver updates this cursor as bytes arrive.
        // It points to where the NEXT byte will be written.
        std::size_t write_cursor = uart->receive_cursor();

        // Different cursors mean there are unread bytes in the buffer.
        while (read_cursor != write_cursor) {

            // Read one byte from the position I have not processed yet.
            std::uint8_t received_byte = buffer[read_cursor];

            // Only 0xA5 is a request. Other bytes are ignored.
            if (received_byte == sample_request_command) {
                // Signal the sampler. If it is blocked on the semaphore,
                // FreeRTOS makes it ready to run. No data is copied here.
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

// Dylan (Role 1): sampler_task() is still pending integration.
// TODO(lab, role 1): Construct an adc_driver. Loop forever:
//   1. xSemaphoreTake(sample_request, portMAX_DELAY)
//   2. Fill a std::array<std::uint16_t, samples_per_batch> with one call to
//      adc_driver::read() (one DMA transfer, one task sleep).
//   3. Convert each raw sample into a temperature_batch. See "Temperature
//      conversion" in the README for the calibration constants.
//   4. xQueueSend(batch_queue, &batch, portMAX_DELAY)
void sampler_task(void*)
{
  while (true) {
  }
}

// Kaden (Role 3): transmitter requirements from the professor (reference).
// Integrated and tested with simulated samples and UART; real link pending.
// TODO(lab, role 3): Construct a crc32_hardware and a lab1::input_pin for
// your corruption-test button (S1 or S2). Loop forever:
//   1. xQueueReceive(batch_queue, &batch, portMAX_DELAY)
//   2. Compute the CRC-32 over the batch's bytes with both crc32_hardware and
//      crc32_software, timing each with a lab2::steady_clock. Print both
//      durations.
//   3. If the button is held, flip one bit in the batch (after the CRC, so
//      the CRC no longer matches).
//   4. Write the batch, then the CRC, with the serial driver. This task only
//      writes to the serial driver.

// (Eslin - integration) I adapted my Lab 2 steady clock for Kaden's benchmark.
// TIMG12 counts at 4 MHz if BUSCLK is 32 MHz (one tick = 0.25 microseconds).
// The constructor sets up the timer once, and uptime() reads its counter.
class mspm0_steady_clock : public lab2::steady_clock
{
private:
    static constexpr std::uintptr_t timer_base =
        0x4087'0000U;

    // Return the register at the base address plus its offset. Volatile
    // keeps the compiler from treating hardware register reads as constant.
    static volatile std::uint32_t& reg(
        std::uintptr_t offset)
    {
        return *reinterpret_cast<
            volatile std::uint32_t*>(timer_base + offset);
    }

public:
    mspm0_steady_clock()
    {
        // PWREN: bits 31:24 hold the unlock key 0x26; bit 0 enables power.
        reg(0x0800) = (0x26U << 24) | 1U;

        // Allow the peripheral to power up.
        for (std::uint32_t i = 0; i < 16; ++i) {
            __asm volatile("nop");
        }

        // Stop the timer before configuring it.
        reg(0x1804) = 0U;

        // Select BUSCLK.
        reg(0x1008) = (1U << 3);

        // CLKDIV uses the divider minus one: 7 means divide BUSCLK by 8.
        reg(0x1000) = 7U;

        // No additional prescaling.
        reg(0x110C) = 0U;

        // Enable the timer clock.
        reg(0x1108) = 1U;

        // Use the full 32-bit counting range.
        reg(0x1808) = 0xFFFF'FFFFU;

        // CTRCTL: start at zero, count up, repeat, and enable.
        reg(0x1804) =
            (2U << 28) |  // Start counter at zero on enable.
            (2U << 4) |   // Count up.
            (1U << 1) |   // Repeat after reaching LOAD.
            1U;           // Enable counter.
    }

private:
    std::uint32_t driver_frequency() override
    {
        return 4'000'000U;
    }

    // The interface returns uint64_t, but TIMG12's raw counter is 32-bit.
    // The benchmark uses unsigned 32-bit subtraction across counter wrap.
    std::uint64_t driver_uptime() override
    {
        return reg(0x1800);
    }
};

// (Eslin - integration) I added the S2 input driver for Kaden's corruption
// demonstration. S2 is PB21 and is active-low: pressed means LOW.
// It implements the professor's lab1::input_pin interface using registers.
class corruption_button : public lab1::input_pin
{
public:
    corruption_button()
    {
        // Unlock GPIOB's power register and enable the peripheral.
        GPIOB->GPRCM.PWREN = (0x26U << 24) | 1U;

        for (std::uint32_t i = 0; i < 16; ++i) {
            __asm volatile("nop");
        }

        // The default input settings select an internal pull-up resistor.
        configure(settings{});
    }

private:
    bool driver_configure(
        settings const& p_settings) override
    {
        std::uint32_t pull = 0U;

        switch (p_settings.resistor) {

            case lab1::pin_resistor::none:
                break;

            case lab1::pin_resistor::pull_up:
                pull = (1U << 17);
                break;

            case lab1::pin_resistor::pull_down:
                pull = (1U << 16);
                break;

            default:
                return false;
        }

        // Clear PB21's output-enable bit so it acts as an input.
        GPIOB->DOECLR31_0 = (1U << 21);

        // PINCM49 is PB21. Select GPIO, connect the pin, enable the input
        // buffer, and apply the requested pull resistor.
        IOMUX->SECCFG.PINCM[IOMUX_PINCM49] =
            1U |
            (1U << 7) |
            (1U << 18) |
            pull;

        return true;
    }

    bool driver_level() override
    {
        // Mask all the other GPIOB pins and return PB21's electrical level.
        return (GPIOB->DIN31_0 & (1U << 21)) != 0U;
    }
};

// (Eslin - integration) Give the transmitter a simple function to check S2.
bool corruption_button_held()
{
    // Static means I configure the button once, on the first call.
    static corruption_button button;

    // Invert the electrical level so true means the button is pressed.
    return !button.level();
}

// (Eslin) I use Kaden's context structure to pass the queue, shared UART
// and button function to his task through one FreeRTOS void* argument.
struct transmit_task_context
{
    QueueHandle_t batch_queue;
    lab6::serial* uart;
    bool (*corruption_button_held)();
};

// Kaden (Role 3): CRC and transmit task, adapted during integration.
void transmit_task(void* p_parameters)
{
    auto* context =
        static_cast<transmit_task_context*>(p_parameters);

    if (context == nullptr ||
        context->batch_queue == nullptr ||
        context->uart == nullptr) {

        std::printf("Invalid transmit task context\n");

        while (true) {
            vTaskDelay(1);
        }
    }

    temperature_batch samples{};

    crc32_hardware hardware_crc;
    crc32_software software_crc;

    mspm0_steady_clock clock;

    while (true) {

        // Wait for Dylan's temperature batch.
        if (xQueueReceive(
                context->batch_queue,
                &samples,
                portMAX_DELAY) != pdPASS) {

            continue;
        }

        // Access the temperature samples as bytes.
        auto const sample_bytes =
            std::span<std::uint8_t const>{
                reinterpret_cast<std::uint8_t const*>(
                    samples.data()),
                sizeof(samples)
            };

        // Calculate and benchmark hardware CRC. Reset happens before timing;
        // the measured interval covers update() and checksum().
        hardware_crc.reset();

        auto const hardware_start =
            static_cast<std::uint32_t>(clock.uptime());

        hardware_crc.update(sample_bytes);

        auto const hardware_checksum =
            hardware_crc.checksum();

        std::uint32_t hardware_elapsed =
            static_cast<std::uint32_t>(clock.uptime())
            - hardware_start;

        // Calculate and benchmark software CRC.
        software_crc.reset();

        auto const software_start =
            static_cast<std::uint32_t>(clock.uptime());

        software_crc.update(sample_bytes);

        auto const software_checksum =
            software_crc.checksum();

        std::uint32_t software_elapsed =
            static_cast<std::uint32_t>(clock.uptime())
            - software_start;

        // Print the measured timer counts.
        std::printf(
            "CRC hardware: %lu ticks, software: %lu ticks\n",
            static_cast<unsigned long>(hardware_elapsed),
            static_cast<unsigned long>(software_elapsed));

        std::printf(
            "Timer frequency: %lu Hz\n",
            static_cast<unsigned long>(clock.frequency()));

        // Compare the hardware and software results.
        if (hardware_checksum != software_checksum) {

            std::printf(
                "CRC mismatch: HW=0x%08lX SW=0x%08lX\n",
                static_cast<unsigned long>(
                    hardware_checksum),
                static_cast<unsigned long>(
                    software_checksum));

        } else {

            std::printf("Hardware and software CRC match\n");
        }

        // Corrupt one bit after calculating the CRC. The checksum stays
        // unchanged, so the display board should detect the changed data.
        if (context->corruption_button_held != nullptr &&
            context->corruption_button_held()) {

            auto* mutable_bytes =
                reinterpret_cast<std::uint8_t*>(
                    samples.data());

            mutable_bytes[0] ^= 0x01U;

            std::printf("CRC corruption test\n");
        }

        // Send the 128 bytes of temperature data.
        context->uart->write(sample_bytes);

        // Convert the CRC to four little-endian bytes.
        std::array<std::uint8_t, 4> crc_bytes{
            static_cast<std::uint8_t>(
                hardware_checksum >> 0),

            static_cast<std::uint8_t>(
                hardware_checksum >> 8),

            static_cast<std::uint8_t>(
                hardware_checksum >> 16),

            static_cast<std::uint8_t>(
                hardware_checksum >> 24)
        };

        // Send the CRC after the temperature data.
        context->uart->write(
            std::span<std::uint8_t const>{
                crc_bytes.data(),
                crc_bytes.size()
            });
    }
}

// =============================================================================
// Display board: task (Sonal, Role 5)
// =============================================================================

// Sonal's display implementation is still pending integration.
// Professor's original Role 4 instructions (display half of the application):
// TODO(lab, role 4): Construct a serial_driver, a crc32_hardware, two
// lab1::input_pin (S1 and S2), and three lab1::output_pin (the RGB LED).
// Loop forever:
//   1. Wait for S1 or S2 to be pressed.
//   2. Save the current receive_cursor(), then write sample_request_command.
//   3. Wait until packet_size bytes have arrived after the saved position. If
//      they don't arrive within 500 ms, print a timeout message and go back
//      to step 1.
//   4. Recompute the CRC over the received batch and compare it to the
//      received CRC. On a mismatch, print a message and set the LED magenta
//      (red + blue).
//   5. Otherwise, average the batch and set the LED: blue for cold, yellow
//      for warm, red for hot. You pick the bands.
void display_task(void*)
{
  while (true) {
  }
}

// =============================================================================
// Task storage (Eslin)
// =============================================================================

// (Eslin) I reserve 512 StackType_t entries per task, not 512 bytes.
// Each task needs its own stack for local variables and function calls.
// Each TCB (task control block) stores the task's scheduling information.
// The storage is static so it remains available for the lifetime of the tasks.
// Resize task_stack_words if necessary. Crashes can be caused by stack
// overflows.
constexpr std::uint32_t task_stack_words = 512;

// Display board: one task (Sonal's task uses this storage).
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

// (Eslin) Give each of the three sensor tasks its own TCB and stack.
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

// (Eslin) Select which application to build before flashing a board.
// Sensor runs three tasks; display runs one. Change to display for Sonal's board.
constexpr device_role this_device = device_role::sensor;
}  // namespace

// (Eslin) main() sets up the application before FreeRTOS starts scheduling.
int main()
{
    std::printf("Hello, World\n");

    // The role is known at compile time, so only the selected setup runs.
    if constexpr (this_device == device_role::sensor) {

        // Create the binary semaphore using the storage I reserved above.
        // It starts empty; the sampler waits until request_task gives it.
        sample_request = xSemaphoreCreateBinaryStatic(
            &sample_request_storage);

        // (Eslin) Create a queue that can hold one complete temperature batch.
        // The storage is already reserved, so FreeRTOS does not allocate it.
        batch_queue = xQueueCreateStatic(
            1,                          // Number of items the queue holds.
            sizeof(temperature_batch),  // Bytes copied for each item.
            batch_queue_buffer,         // Memory for the actual data.
            &batch_queue_storage);      // Memory for queue control information.

        // (Eslin) Create one shared UART object using Koby's driver.
        // Static keeps it alive for both tasks after the scheduler starts.
        // UART1 is selected for now; Koby still needs to verify the pin setup
        // and the physical TX/RX link with the team.
        static serial_driver uart(
            lab6::serial::settings{},   // Default baud rate: 115200, 8N1.
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

        // (Eslin) Create Dylan's sampler task. It uses the shared semaphore
        // and queue directly, so it does not need a context pointer (nullptr).
        xTaskCreateStatic(
            sampler_task,
            "sampler_task",
            task_stack_words,
            nullptr,
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

        // (Eslin) If building the display role, create only Sonal's task.
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

    // (Eslin) Start the scheduler after all objects and tasks are ready.
    // FreeRTOS now decides which ready task runs. This normally never returns.
    vTaskStartScheduler();

    // Stay here if the scheduler fails to start.
    while (true) {
    }
}
