#include <array>
#include <cstdint>
#include <cstdio>

#include "../hal/adc.hpp"
#include "../hal/crc.hpp"
#include "../hal/gpio.hpp"
#include "../hal/i2c.hpp"
#include "../hal/serial.hpp"
#include "../hal/timer.hpp"

// TODO(lab6, step 1): Make this class inherit from and implement the
// `lab6::serial` interface (`serial.hpp`), using DriverLib or register-level
// code. driver_receive_buffer()/driver_receive_cursor() must be backed by a
// circular buffer that a DMA transfer fills continuously in the background.
// See `lab6::serial::receive_cursor()`'s docs for how a caller is expected to
// consume it.
class serial_driver
{};

// TODO(lab6, step 2): Make this class inherit from and implement the
// `lab6::crc32` interface (`crc.hpp`), written register-level against the
// on-chip CRC accelerator peripheral - no DriverLib. This is the
// hardware-accelerated implementation you'll benchmark against the
// `crc32_software` reference supplied below.
class crc32_hardware
{};

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

int main()
{
  std::printf("Hello, World\n");

  // TODO(lab6, step 3): Construct your accelerometer driver (lab4) and
  // adc16_stream driver (lab5), a serial_driver, and a crc32_hardware
  // (crc32_software above needs no setup - it's ready to use as-is).
  // Construct a lab1::input_pin for the start/stop button and a
  // lab2::steady_clock for timing the CRC benchmark.

  while (true) {
    // TODO(lab6, step 4): Wait for the button to signal start. Once
    // started, the two devices take turns: gather the IMU reading OR 64 ADC
    // samples, compute its CRC with both your crc32_hardware and the
    // supplied crc32_software (timing each with the steady_clock and
    // printing both durations - crc32_hardware should win), transmit the
    // data with the CRC appended, then wait to receive the other device's
    // data and validate its CRC and print its results. Repeat until the
    // button signals stop.
  }
}
