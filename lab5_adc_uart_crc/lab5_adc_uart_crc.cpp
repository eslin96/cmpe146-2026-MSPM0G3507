// cspell:ignore VDDA IIDX DDEVICE

#include <array>
#include <cstdint>
#include <cstdio>

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
#include "msp0g_uart.hpp"
#include "msp0g_crc.hpp"

// =============================================================================
// Role 1: ADC
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
// Role 2: UART
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
using serial_driver = msp0g::msp0g_uart;

// =============================================================================
// Role 3: CRC
// =============================================================================

// TODO(lab, role 3): Make this class inherit from and implement the
// `lab6::crc32` interface (`crc.hpp`), written register-level against the
// on-chip CRC accelerator - no DriverLib. Its output must match
// crc32_software below for the same input. Check the accelerator's bit-order
// options and whether it applies the final XOR.
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

// One batch of converted samples. This is the item type of batch_queue.
using temperature_batch = std::array<temperature_sample_t, samples_per_batch>;

// A packet on the wire: the batch's bytes, followed by the 4-byte CRC-32.
constexpr std::size_t packet_size =
  sizeof(temperature_batch) + sizeof(std::uint32_t);

// =============================================================================
// Sensor board: semaphore and queue (role 4 creates these in main())
// =============================================================================

// request_task -> sampler_task: "take a batch now". Carries no data.
// TODO(lab, role 4): Create with xSemaphoreCreateBinaryStatic().
SemaphoreHandle_t sample_request = nullptr;
StaticSemaphore_t sample_request_storage;

// sampler_task -> transmit_task: a copy of one temperature_batch.
// TODO(lab, role 4): Create with xQueueCreateStatic(), depth 1, item size
// sizeof(temperature_batch).
QueueHandle_t batch_queue = nullptr;
StaticQueue_t batch_queue_storage;
std::uint8_t batch_queue_buffer[sizeof(temperature_batch)];

// =============================================================================
// Sensor board: tasks
// =============================================================================

// TODO(lab, role 4): Every few ms (vTaskDelay()), compare the serial driver's
// receive_cursor() with the last position you read. When
// sample_request_command arrives, xSemaphoreGive(sample_request). This task
// only reads from the serial driver.
void request_task(void* p_context)
{
    auto* uart = static_cast<lab6::serial*>(p_context);

    auto buffer = uart->receive_buffer();

    std::size_t read_cursor = 0;

    while (true) {

        std::size_t write_cursor = uart->receive_cursor();

        while (read_cursor != write_cursor) {

            std::uint8_t received_byte = buffer[read_cursor];

            if (received_byte == sample_request_command) {
                xSemaphoreGive(sample_request);
            }

            read_cursor = (read_cursor + 1) % buffer.size();
        }

        constexpr TickType_t delay_ticks = pdMS_TO_TICKS(5);
        vTaskDelay(delay_ticks > 0 ? delay_ticks : 1);
    }
}

// TODO(lab, role 1): Construct an adc_driver. Loop forever:
//   1. xSemaphoreTake(sample_request, portMAX_DELAY)
//   2. Fill a std::array<std::uint16_t, samples_per_batch> with one call to
//      adc_driver::read() (one DMA transfer, one task sleep).
//   3. Convert each raw sample into a temperature_batch. See "Temperature
//      conversion" in the README for the calibration constants.
//   4. xQueueSend(batch_queue, &batch, portMAX_DELAY)
void sampler_task(void*)
{
    temperature_batch batch{};

    while (true) {

        // Wait for a sample request.
        xSemaphoreTake(sample_request, portMAX_DELAY);

        // Generate 64 temporary temperature samples.
        batch.fill(250);

        // Send the batch to the transmitter.
        xQueueSend(batch_queue, &batch, portMAX_DELAY);
    }
}

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


// Steady clock reused from Lab 2.
// TIMG12 runs at 4 MHz with a 32 MHz BUSCLK.
class mspm0_steady_clock : public lab2::steady_clock
{
private:
    static constexpr std::uintptr_t timer_base =
        0x4087'0000U;

    static volatile std::uint32_t& reg(
        std::uintptr_t offset)
    {
        return *reinterpret_cast<
            volatile std::uint32_t*>(timer_base + offset);
    }

public:
    mspm0_steady_clock()
    {
        // Enable power to TIMG12.
        reg(0x0800) = (0x26U << 24) | 1U;

        // Allow the peripheral to power up.
        for (std::uint32_t i = 0; i < 16; ++i) {
            __asm volatile("nop");
        }

        // Stop the timer before configuring it.
        reg(0x1804) = 0U;

        // Select BUSCLK.
        reg(0x1008) = (1U << 3);

        // Divide the clock by 8.
        reg(0x1000) = 7U;

        // No additional prescaling.
        reg(0x110C) = 0U;

        // Enable the timer clock.
        reg(0x1108) = 1U;

        // Use the full 32-bit counting range.
        reg(0x1808) = 0xFFFF'FFFFU;

        // Start at zero, count up, repeat, and enable.
        reg(0x1804) =
            (2U << 28) |
            (2U << 4) |
            (1U << 1) |
            1U;
    }

private:
    std::uint32_t driver_frequency() override
    {
        return 4'000'000U;
    }

    std::uint64_t driver_uptime() override
    {
        return reg(0x1800);
    }
};


class corruption_button : public lab1::input_pin
{
public:
    corruption_button()
    {
        // Enable power to GPIOB.
        GPIOB->GPRCM.PWREN = (0x26U << 24) | 1U;

        for (std::uint32_t i = 0; i < 16; ++i) {
            __asm volatile("nop");
        }

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

        // Configure PB21 as an input.
        GPIOB->DOECLR31_0 = (1U << 21);

        IOMUX->SECCFG.PINCM[IOMUX_PINCM49] =
            1U |
            (1U << 7) |
            (1U << 18) |
            pull;

        return true;
    }

    bool driver_level() override
    {
        return (GPIOB->DIN31_0 & (1U << 21)) != 0U;
    }
};

bool corruption_button_held()
{
    static corruption_button button;

    return !button.level();
}

// Dependencies supplied to Kaden's transmit task.
struct transmit_task_context
{
    QueueHandle_t batch_queue;
    lab6::serial* uart;
    bool (*corruption_button_held)();
};


// Kaden's CRC and transmit task.
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

        // Calculate and benchmark hardware CRC.
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

        // Corrupt one bit after calculating the CRC.
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
// Display board: task
// =============================================================================

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
// Task storage
// =============================================================================

// Resize task_stack_words if necessary. Crashes can be caused by stack
// overflows.
constexpr std::uint32_t task_stack_words = 512;

// Display board: one task.
StaticTask_t display_task_tcb;
StackType_t display_task_stack[task_stack_words];

// TODO(lab, role 4): Add a stack and TCB for each of the three sensor tasks.

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


class test_serial : public lab6::serial
{
private:
    std::array<std::uint8_t, 2> rx_buffer{
        0xA5, 0x00
    };

    std::size_t total_bytes = 0;
    bool packet_correct = true;

    bool driver_configure(settings const&) override
    {
        return true;
    }

    void driver_write(
        std::span<std::uint8_t const> data) override
    {
        std::printf("UART write: %u bytes\n",
            static_cast<unsigned>(data.size()));

        // Expected CRC for our 64 samples.
        std::array<std::uint8_t, 4> expected_crc{
            0x31, 0xE6, 0x1F, 0x2E
        };

        for (auto byte : data) {

            if (total_bytes < 128) {

                std::uint8_t expected =
                    (total_bytes % 2 == 0) ? 250 : 0;

                if (byte != expected) {
                    packet_correct = false;
                }

            } else if (total_bytes < 132) {

                if (byte != expected_crc[total_bytes - 128]) {
                    packet_correct = false;
                }

            } else {
                packet_correct = false;
            }

            ++total_bytes;
        }

        if (total_bytes >= 132) {

            if (packet_correct && total_bytes == 132) {
                std::printf("PASS: Correct 132-byte packet!\n");
            } else {
                std::printf("FAIL: Incorrect packet!\n");
            }
        }
    }

    std::span<std::uint8_t const>
    driver_receive_buffer() override
    {
        return rx_buffer;
    }

    std::size_t driver_receive_cursor() override
    {
        return 1;
    }
};

constexpr device_role this_device = device_role::sensor;
}  // namespace



int main()
{
    std::printf("Hello, World\n");

    if constexpr (this_device == device_role::sensor) {

        // Create the binary semaphore.
        sample_request = xSemaphoreCreateBinaryStatic(
            &sample_request_storage);

        // Create the queue for one temperature batch.
        batch_queue = xQueueCreateStatic(
            1,
            sizeof(temperature_batch),
            batch_queue_buffer,
            &batch_queue_storage);

        // Create one shared UART driver.
        static test_serial uart;

        lab6::serial* sensor_uart = &uart;

        // Create the request task.
        xTaskCreateStatic(
            request_task,
            "request_task",
            task_stack_words,
            sensor_uart,
            tskIDLE_PRIORITY + 1,
            request_task_stack,
            &request_task_tcb);

        // Create the sampler task.
        xTaskCreateStatic(
            sampler_task,
            "sampler_task",
            task_stack_words,
            nullptr,
            tskIDLE_PRIORITY + 1,
            sampler_task_stack,
            &sampler_task_tcb);

        // Provide the transmit task with its dependencies.
        static transmit_task_context transmit_context{
        batch_queue,
        sensor_uart,
        corruption_button_held
        };

        // Create the transmit task.
        xTaskCreateStatic(
            transmit_task,
            "transmit_task",
            task_stack_words,
            &transmit_context,
            tskIDLE_PRIORITY + 1,
            transmit_task_stack,
            &transmit_task_tcb);

    } else {

        // Create the display board task.
        xTaskCreateStatic(
            display_task,
            "display_task",
            task_stack_words,
            nullptr,
            tskIDLE_PRIORITY + 1,
            display_task_stack,
            &display_task_tcb);
    }

    // Start the FreeRTOS scheduler.
    vTaskStartScheduler();

    // Only reached if the scheduler fails to start.
    while (true) {
    }
}
