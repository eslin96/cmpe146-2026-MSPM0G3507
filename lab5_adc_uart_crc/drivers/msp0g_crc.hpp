#pragma once

#include "../hal/crc.hpp"

namespace lab6 {

class crc32_hardware final : public crc32
{
public:
  crc32_hardware();

  crc32_hardware(const crc32_hardware&) = delete;
  crc32_hardware& operator=(const crc32_hardware&) = delete;

  crc32_hardware(crc32_hardware&&) = delete;
  crc32_hardware& operator=(crc32_hardware&&) = delete;

private:
  void driver_reset() override;

  void driver_update(std::span<const std::uint8_t> p_data) override;

  std::uint32_t driver_checksum() override;
};

}  // namespace lab6
