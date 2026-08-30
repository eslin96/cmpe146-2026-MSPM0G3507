#pragma once

#include <cstdint>

#include "../third_party/span/span.hpp"

namespace lab3 {
/**
 * @brief Serial peripheral interface (SPI) communication protocol hardware
 * abstraction interface
 * @deprecated This class is deprecated and will be removed in libhal
 * version 5.0.0 in place of `hal::spi_channel`. `hal::spi_channel` associates a
 * chip select with a spi channel as if it were a lock which provides access
 * access control to the SPI bus.
 *
 * This interface supports the most common SPI features:
 *
 * 1. Word length locked to 8-bits
 * 2. Byte transfer is always MSB first
 * 3. Chip select is not controlled by this driver
 *
 * # Design Philosophy
 *
 * By restricting the SPI interface requirements, we ensure compatibility with
 * nearly all devices that communicate over SPI. This approach simplifies both
 * the requirements and the code for SPI drivers and implementations.
 *
 * ## 8-bit Transfers
 *
 * The 8-bit word length is the most common SPI word length, compatible with
 * almost any SPI peripheral driver, SPI converter driver, and most devices.
 * Devices that support 16-bit word transfers can split the 16-bit word into two
 * bytes. Devices using word formats that aren't multiples of 8 bits are very
 * rare.
 *
 * ## MSB First Transfers
 *
 * The most common bit order for SPI is MSB first. Devices that use LSB first
 * are rare. Drivers using this SPI interface must handle bit reversal to
 * comply with the MSB first requirement. This decision helps to eliminate
 * rare and potentially unsupportable configurations, optimizing the interface
 * for the majority of use cases.
 *
 * ## Manual Chip Select Control
 *
 * Many SPI peripherals have a dedicated chip select pin that can be controlled
 * manually. Automatic chip select control asserts the chip select for the
 * duration of the transfer and de-asserts it at the end, which can be
 * problematic for drivers needing to perform multiple transfers while keeping
 * the chip select asserted. For instance, if payload parts are in ROM, copying
 * them to a buffer for a full transfer requires more stack space than calling
 * the transfer multiple times with the ROM data stream. Some devices, like SD
 * cards, require the chip select to be held with a sequence of clock cycles
 * until they respond with actual data. This sequence can vary, so manual chip
 * select control allows more precise and memory-efficient operations.
 *
 */
class spi
{
public:
  /**
   * @brief Generic settings for a standard SPI device.
   *
   */
  /// Default filler data placed on the bus in place of actual write data when
  /// the write buffer has been exhausted.
  static constexpr std::uint8_t default_filler = 0xFF;

  /**
   * @brief Mode settings which control when data is sampled and shifted out
   *
   */
  enum class mode : std::uint8_t
  {
    /**
     * @brief spi mode 0
     *
     * - Data is shifted out on: falling SCLK, and when CS activates
     * - Data is sampled on: rising SCLK
     * - CPOL (clock polarity): 0
     * - CPHA (clock phase): 0
     */
    m0,

    /**
     * @brief spi mode 1
     *
     * - Data is shifted out on: rising SCLK
     * - Data is sampled on: falling SCLK
     * - CPOL (clock polarity): 0
     * - CPHA (clock phase): 1
     */
    m1,

    /**
     * @brief spi mode 2
     *
     * - Data is shifted out on: rising SCLK, and when CS activates
     * - Data is sampled on: falling SCLK
     * - CPOL (clock polarity): 1
     * - CPHA (clock phase): 0
     */
    m2,

    /**
     * @brief spi mode 3
     *
     * - Data is shifted out on: falling SCLK
     * - Data is sampled on: rising SCLK
     * - CPOL (clock polarity): 1
     * - CPHA (clock phase): 1
     */
    m3,
  };

  /**
   * @brief Generic settings for a standard SPI device.
   *
   */
  struct settings
  {
    /**
     * @brief Best-effort clock rate to set the spi bus to
     *
     * This field denotes the clock rate that an application or driver would
     * like to set the bus to. This clock rate is "best-effort" meaning that the
     * spi channel will try its best to reach this clock rate but will not
     * guarantee the exact clock rate.
     *
     * The clock rate returned from the API `clock_rate()` will always return a
     * value less than or equal to this value. If the clock rate is above what
     * the spi bus can achieve, then the clock rate will be set to the maximum
     * the spi bus can manage.
     */
    std::uint32_t clock_rate = 100'000;

    /**
     * @brief Bus mode select field
     *
     * Use this to select how the spi data and clock are sampled.
     */
    mode bus_mode = mode::m0;
  };

  /**
   * @brief Configure spi to match the settings supplied
   *
   * @param p_settings - settings to apply to spi
   * @return false - if the settings could not be
   * achieved.
   * @return true - if settings were accepted
   */
  bool configure(settings const& p_settings)
  {
    return driver_configure(p_settings);
  }

  /**
   * @brief Send and receive data between a selected device on the spi bus.
   * This function will block until the entire transfer is finished.
   *
   * @param p_data_out - buffer to write data to the bus. If this is set to
   * null/empty then writing is ignored and the p_filler will be written to
   * the bus. If the length is less than p_data_in, then p_filler will be
   * written to the bus after this buffer has been sent.
   * @param p_data_in - buffer to read the data off of the bus. If this is
   * null/empty, then the transfer will be write only and the incoming data will
   * be ignored. If the length of this buffer is less than p_data_out, once this
   * buffer has been filled, the rest of the received bytes on the bus will be
   * dropped.
   * @param p_filler - filler data placed on the bus in place of actual write
   * data when p_data_out has been exhausted.
   */
  void transfer(std::span<std::uint8_t const> p_data_out,
                std::span<std::uint8_t> p_data_in,
                std::uint8_t p_filler = default_filler)
  {
    return driver_transfer(p_data_out, p_data_in, p_filler);
  }

private:
  virtual bool driver_configure(settings const& p_settings) = 0;
  virtual void driver_transfer(std::span<std::uint8_t const> p_data_out,
                               std::span<std::uint8_t> p_data_in,
                               std::uint8_t p_filler) = 0;
};
}  // namespace lab3
