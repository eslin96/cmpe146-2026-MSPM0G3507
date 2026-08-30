#pragma once

#include <cstdint>

#include "../third_party/span/span.hpp"

namespace lab6 {

/**
 * @brief 32-bit CRC (cyclic redundancy check) hardware abstraction
 * interface.
 *
 * Implementations compute a running CRC-32 over data fed to them
 * incrementally via `update()`. This shape lets both a from-scratch
 * software implementation and a hardware-accelerated peripheral driver sit
 * behind the same interface, so an application can swap between them (e.g.
 * to benchmark one against the other) without changing any call sites.
 *
 */
class crc32
{
public:
  /**
   * @brief Reset the running CRC value back to its initial state.
   *
   * Call this before starting a new checksum calculation.
   */
  void reset()
  {
    driver_reset();
  }

  /**
   * @brief Feed bytes into the running CRC calculation.
   *
   * May be called multiple times in a row to fold in additional data; the
   * result is the same as if all the data had been passed in a single call.
   *
   * @param p_data - bytes to fold into the running CRC value
   */
  void update(std::span<std::uint8_t const> p_data)
  {
    driver_update(p_data);
  }

  /**
   * @brief Get the CRC value computed so far.
   *
   * Does not reset the running calculation - further calls to `update()`
   * will continue to build on the current value until `reset()` is called.
   *
   * @return std::uint32_t - the current 32-bit CRC value
   */
  [[nodiscard]] std::uint32_t checksum()
  {
    return driver_checksum();
  }

private:
  virtual void driver_reset() = 0;
  virtual void driver_update(std::span<std::uint8_t const> p_data) = 0;
  virtual std::uint32_t driver_checksum() = 0;
};
}  // namespace lab6
