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
  while (true) {
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
void transmit_task(void*)
{
  while (true) {
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

constexpr device_role this_device = device_role::sensor;
}  // namespace



int main()
{
    std::printf("Lab 5 CRC Test\n");

    // Create the hardware and software CRC drivers.
    crc32_hardware hardware_crc;
    crc32_software software_crc;

    // Standard CRC-32 test data.
    std::array<std::uint8_t, 9> test_data{
        '1', '2', '3', '4', '5',
        '6', '7', '8', '9'
    };

    // Calculate the hardware CRC.
    hardware_crc.reset();
    hardware_crc.update(test_data);

    std::uint32_t hardware_result =
        hardware_crc.checksum();

    // Calculate the software CRC.
    software_crc.reset();
    software_crc.update(test_data);

    std::uint32_t software_result =
        software_crc.checksum();

    // Known correct CRC-32 value.
    constexpr std::uint32_t expected = 0xCBF43926U;

    std::printf("Hardware CRC: 0x%08lX\n",
                static_cast<unsigned long>(hardware_result));

    std::printf("Software CRC: 0x%08lX\n",
                static_cast<unsigned long>(software_result));

    std::printf("Expected CRC: 0x%08lX\n",
                static_cast<unsigned long>(expected));

    if (hardware_result == expected &&
        software_result == expected) {

        std::printf("PASS: CRC results match!\n");

    } else {

        std::printf("FAIL: CRC results do not match!\n");
    }

    while (true) {
    }
}

