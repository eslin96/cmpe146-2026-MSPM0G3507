#include <cstdio>

#include "../hal/gpio.hpp"
#include "../hal/spi.hpp"

class mspm0_spi : public lab3::spi
{

private:
  static constexpr std::uintptr_t spi0_base_add = 0x40468000;

  static constexpr std::uintptr_t power_enable_offset = 0x800;
  static constexpr std::uintptr_t reset_control_offset = 0x804;
  static constexpr std::uintptr_t reset_status_offset = 0x814;
  static constexpr std::uintptr_t clock_divider_offset = 0x1000;
  static constexpr std::uintptr_t clock_select_offset = 0x1004;
  static constexpr std::uintptr_t clock_prescaler_and_divider_offset = 0x1108;

  bool driver_configure(settings const& p_settings) override
  {
    auto *power_enable_register = reinterpret_cast<volatile std::uint32_t*>(
      spi0_base_add + power_enable_offset);

    *power_enable_register = (0x26u << 24) | 1u;

    auto *reset_control_register = reinterpret_cast<volatile std::uint32_t*>(
      spi0_base_add + reset_control_offset);

    *reset_control_register =
      (0xB1u << 24) | (1u << 1) | (1u << 0);

    auto* reset_status_register =
      reinterpret_cast<volatile std::uint32_t*>(
        spi0_base_add + reset_status_offset);

    while ((*reset_status_register & (1u << 16)) == 0u)
    {
    }

    auto* clock_select_register =
      reinterpret_cast<volatile std::uint32_t*>(
        spi0_base_add + clock_select_offset);

    *clock_select_register = (1u << 3);

    auto* clock_divider_register =
      reinterpret_cast<volatile std::uint32_t*>(
        spi0_base_add + clock_divider_offset);

    *clock_divider_register = 0u;

    auto* clock_prescaler_and_divider_register =
      reinterpret_cast<volatile std::uint32_t*>(
        spi0_base_add + clock_prescaler_and_divider_offset);
    return true;
  
  }

  void driver_transfer(
    std::span<std::uint8_t const> p_data_out, 
    std::span<std::uint8_t> p_data_in,
    std::uint8_t p_filler) override
  {

  }
};


/**
 * @brief Generic SPI flash memory driver
 *
 * This driver works with common SPI flash memory with JEDEC ID
 * This driver needs both an SPI peripheral and an output pin for chip select.
 */
class flash_memory
{
public:
  /**
   * @brief Construct a new flash memory object
   *
   * @param p_spi - spi port connected to the spi flash memory
   * @param p_chip_select - pin connected to spi flash memory chip select
   */
  flash_memory(lab3::spi* p_spi, lab1::output_pin* p_chip_select)
    : m_spi(p_spi)
    , m_chip_select(p_chip_select)
  {
  }

  // TODO(lab3, step 2): Add APIs to access various parts of the flash memory as
  // required by the lab.

private:
  lab3::spi* m_spi = nullptr;
  lab1::output_pin* m_chip_select = nullptr;
};

int main()
{
  std::printf("Hello, World\n");

  // TODO(lab3, step 3): Construct an spi driver object and gpio driver object
  // in order to construct a flash memory object. Use the flash memory object to
  // retrieve and print the manufacturer ID, capacity, and the first 32-byte
  // block of the flash memory. Then take user input and write up to 32 bytes to
  // that block.

  while (true) {
    continue;  // loop here forever
  }
}
