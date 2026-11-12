#include "msp0g_crc.hpp"

namespace {

constexpr std::uintptr_t crc_base = 0x4044'0000U;

constexpr std::uintptr_t pwren_offset = 0x0800U;
constexpr std::uintptr_t rstctl_offset = 0x0804U;
constexpr std::uintptr_t clksel_offset = 0x1004U;
constexpr std::uintptr_t crcctrl_offset = 0x1100U;
constexpr std::uintptr_t crcseed_offset = 0x1104U;
constexpr std::uintptr_t crcin_offset = 0x1108U;
constexpr std::uintptr_t crcout_offset = 0x110CU;

constexpr std::uint32_t pwren_key = 0x26U << 24;
constexpr std::uint32_t rstctl_key = 0xB1U << 24;
constexpr std::uint32_t initial_value = 0xFFFF'FFFFU;

volatile std::uint32_t& reg(std::uintptr_t p_offset)
{
  return *reinterpret_cast<volatile std::uint32_t*>(crc_base + p_offset);
}

volatile std::uint8_t& byte_reg(std::uintptr_t p_offset)
{
  return *reinterpret_cast<volatile std::uint8_t*>(crc_base + p_offset);
}

void wait_for_power_startup()
{
  // TI's generated MSPM0 startup code uses POWER_STARTUP_DELAY = 16.
  for (std::uint32_t i = 0; i < 16; ++i) {
    __asm volatile("nop");
  }
}

}  // namespace

namespace lab6 {

crc32_hardware::crc32_hardware()
{
  // Reset, enable power, wait for startup, select MCLK, then configure and
  // seed the CRC peripheral.
  reg(rstctl_offset) = rstctl_key | (1U << 1) | 1U;
  reg(pwren_offset) = pwren_key | 1U;
  wait_for_power_startup();
  reg(clksel_offset) = 1U;
  driver_reset();
}

void crc32_hardware::driver_reset()
{
  // POLYSIZE = 0 selects CRC32-ISO3309. BITREVERSE = 1 selects the reflected
  // input/output bit order used by CRC-32/IEEE.
  reg(crcctrl_offset) = 1U << 1;
  reg(crcseed_offset) = initial_value;
}

void crc32_hardware::driver_update(std::span<std::uint8_t const> p_data)
{
  for (auto const byte : p_data) {
    // Use an 8-bit write so each input element advances the CRC by one byte.
    byte_reg(crcin_offset) = byte;
  }
}

std::uint32_t crc32_hardware::driver_checksum()
{
  // Apply the standard CRC-32 final XOR to the raw peripheral result.
  return reg(crcout_offset) ^ initial_value;
}

}  // namespace lab6
