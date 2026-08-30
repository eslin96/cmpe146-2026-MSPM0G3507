#pragma once

#include <cstdint>

#include "../third_party/span/span.hpp"

namespace lab4 {

/**
 * @brief Inter-integrated Circuit (I2C) hardware abstraction interface.
 *
 * Also known as Two Wire Interface (TWI) communication protocol. This is a
 * commonly used protocol for communication with sensors and peripheral
 * devices because it only requires two connections: SDA (data signal) and
 * SCL (clock signal). This is possible because the protocol for I2C is
 * addressable.
 *
 */
class i2c
{
public:
  /// Generic settings for a standard I2C device
  struct settings
  {
    /// The serial clock rate in hertz.
    uint32_t clock_rate = 100'000;
  };

  /**
   * @brief Configure i2c to match the settings supplied
   *
   * @param p_settings - settings to apply to i2c driver
   * @return true - if the settings were successfully applied
   * @return false - if the settings could not be achieved
   */
  bool configure(settings const& p_settings)
  {
    return driver_configure(p_settings);
  }

  /**
   * @brief Perform an i2c transaction with another device on the bus. The
   * type of transaction depends on the values of the input parameters. This
   * function blocks until the entire transfer is finished.
   *
   * - For write transactions, pass p_data_in as an empty span and pass a
   *   buffer to p_data_out.
   * - For read transactions, pass p_data_out as an empty span and pass a
   *   buffer to p_data_in.
   * - For write-then-read transactions, pass a buffer for both p_data_out
   *   and p_data_in.
   *
   * @param p_address 7-bit address of the device to communicate with.
   * @param p_data_out data to be written to the addressed device. Pass an
   * empty span to skip writing.
   * @param p_data_in buffer to store data read from the addressed device.
   * Pass an empty span to skip reading.
   * @return true - if the transaction completed successfully
   * @return false - if the device did not acknowledge its address or the bus
   * entered an invalid state (e.g. arbitration loss, bus fault)
   */
  bool transaction(std::uint8_t p_address,
                   std::span<std::uint8_t const> p_data_out,
                   std::span<std::uint8_t> p_data_in)
  {
    return driver_transaction(p_address, p_data_out, p_data_in);
  }

private:
  virtual bool driver_configure(settings const& p_settings) = 0;
  virtual bool driver_transaction(std::uint8_t p_address,
                                  std::span<std::uint8_t const> p_data_out,
                                  std::span<std::uint8_t> p_data_in) = 0;
};
}  // namespace lab4
