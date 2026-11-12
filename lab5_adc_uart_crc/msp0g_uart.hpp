#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../hal/serial.hpp"

namespace msp0g {

/** @brief Select the UART peripheral used by the driver. */
enum class uartx : std::uint8_t
{
  uart0 = 0,
  uart1,
  uart2,
  uart3
};

/**
 * @brief Provide polled transmission and interrupt-driven reception over UART.
 *
 * The driver uses 8-bit data frames and stores received bytes in a circular
 * buffer. Configure the selected UART's TX and RX pins through IOMUX
 * separately.
 */
class msp0g_uart : public lab6::serial
{

public:
  /**
   * @brief Reset, power, configure, and enable the selected UART peripheral.
   *
   * @param p_settings Initial baud rate, stop-bit, and parity settings.
   * @param p_uartx UART peripheral to initialize.
   *
   * @note Successful construction gives this object exclusive use of the
   * selected UART peripheral.
   */
  msp0g_uart(const settings& p_settings, uartx p_uartx);

  /**
   * @brief Drain received bytes from the UART FIFO into the circular buffer.
   *
   * @note This function is intended to be called only by the selected UART's
   * interrupt handler.
   */
  void handle_interrupt();

private:
  static constexpr std::size_t receive_buffer_size = 256;

  std::uintptr_t m_uart_base;
  std::uint8_t m_uart_index;
  std::array<std::uint8_t, receive_buffer_size> m_receive_buffer{};
  volatile std::size_t m_receive_cursor = 0;

  /**
   * @brief Reset and power the selected UART peripheral.
   *
   * @return true if the peripheral was initialized; false if it was already
   * powered.
   */
  bool initialize_uart();

  /** @brief Configure and enable interrupt-driven UART reception. */
  void initialize_receive_interrupt();

  /**
   * @brief Validate and apply new baud-rate and frame-format settings.
   *
   * @param p_settings Requested UART settings.
   * @return true if the settings are realizable and were applied; otherwise
   * false.
   */
  bool driver_configure(const settings& p_settings) override;

  /**
   * @brief Transmit every byte by polling for available TX FIFO space.
   *
   * @param p_data Bytes to transmit in order.
   */
  void driver_write(std::span<const std::uint8_t> p_data) override;

  /**
   * @brief Expose the interrupt-driven circular receive buffer.
   *
   * @return A read-only span covering the complete receive buffer.
   */
  std::span<const std::uint8_t> driver_receive_buffer() override;

  /**
   * @brief Report where the receive interrupt will store the next byte.
   *
   * @return The current circular-buffer write position.
   */
  std::size_t driver_receive_cursor() override;

  /**
   * @brief Apply settings that have already passed validation.
   *
   * @param p_settings Baud-rate and frame-format settings to apply.
   */
  void apply_config(const settings& p_settings);
};

}  // namespace msp0g

