#pragma once

#include <cstddef>
#include <cstdint>

#include "../third_party/span/span.hpp"

namespace lab6 {

/**
 * @brief Hardware abstraction interface for the serial communication
 * protocol.
 *
 * Use this interface for hardware that implements a serial protocol like
 * UART, RS232, RS485, and others that use a similar communication protocol
 * but may use different voltage schemes.
 *
 * This interface only works with 8-bit serial data frames.
 *
 * Reception is expected to be backed by a circular buffer that the driver
 * fills continuously in the background (e.g. via DMA or an interrupt),
 * without direct application intervention. `receive_buffer()` exposes that
 * buffer directly, and `receive_cursor()` tells the caller how far into it
 * the driver has written so far - the application reads out new bytes by
 * comparing its last-seen cursor position to the current one, accounting for
 * wraparound.
 *
 */
class serial
{
public:
  /// Generic settings for a standard serial device.
  struct settings
  {
    /// Set of available stop bits options
    enum class stop_bits : std::uint8_t
    {
      one = 0,
      two,
    };

    /// Set of parity bit options
    enum class parity : std::uint8_t
    {
      /// Disable parity bit as part of the frame
      none = 0,
      /// Enable parity and set 1 (HIGH) when the number of bits is odd
      odd,
      /// Enable parity and set 1 (HIGH) when the number of bits is even
      even,
      /// Enable parity bit and always return 1 (HIGH) for every frame
      forced1,
      /// Enable parity bit and always return 0 (LOW) for every frame
      forced0,
    };

    /// The operating speed of the baud rate (in units of bits per second)
    std::uint32_t baud_rate = 115200;

    /// Number of stop bits for each frame
    stop_bits stop = stop_bits::one;

    /// Parity bit type for each frame
    parity parity = parity::none;
  };

  /**
   * @brief Configure serial to match the settings supplied
   *
   * Implementing drivers must verify if the settings can be applied to
   * hardware before modifying the hardware. This will ensure that if this
   * operation fails, the state of the serial device has not changed.
   *
   * @param p_settings - settings to apply to serial driver
   * @return true - if the settings were successfully applied
   * @return false - if the settings could not be achieved
   */
  bool configure(settings const& p_settings)
  {
    return driver_configure(p_settings);
  }

  /**
   * @brief Write data to the transmitter line of the serial port
   *
   * @param p_data - data to be transmitted over the serial port
   */
  void write(std::span<std::uint8_t const> p_data)
  {
    driver_write(p_data);
  }

  /**
   * @brief Returns this serial driver's receive buffer
   *
   * Use this along with `receive_cursor()` to determine if new data has
   * arrived. See `receive_cursor()` for more details.
   *
   * @return std::span<std::uint8_t const> - a const span over the receive
   * buffer used by the serial port. Calling `size()` on the span will
   * always return a value of at least 1.
   */
  [[nodiscard]] std::span<std::uint8_t const> receive_buffer()
  {
    return driver_receive_buffer();
  }

  /**
   * @brief Returns the current write position of the circular receive
   * buffer
   *
   * The receive cursor represents the position where the next byte of data
   * will be written into the receive buffer. This position advances as new
   * data arrives. To determine how much new data has arrived, store the
   * previous cursor position and compare it with the current cursor
   * position, accounting for buffer wraparound.
   *
   * The cursor value will ALWAYS follow this equation:
   *
   *          0 <= cursor && cursor < receive_buffer().size()
   *
   * Example:
   *
   *   auto old_head = port.receive_cursor();
   *   // ... wait for new data ...
   *   auto new_head = port.receive_cursor();
   *   // Account for circular wraparound when calculating bytes received
   *   auto buffer_size = port.receive_buffer().size();
   *   auto bytes_received = (new_head + buffer_size - old_head) % buffer_size;
   *
   * @return std::size_t - position of the write cursor for the circular
   * buffer
   */
  [[nodiscard]] std::size_t receive_cursor()
  {
    return driver_receive_cursor();
  }

private:
  virtual bool driver_configure(settings const& p_settings) = 0;
  virtual void driver_write(std::span<std::uint8_t const> p_data) = 0;
  virtual std::span<std::uint8_t const> driver_receive_buffer() = 0;
  virtual std::size_t driver_receive_cursor() = 0;
};
}  // namespace lab6
