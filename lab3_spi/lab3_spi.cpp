#include <cstdio>

#include "../hal/gpio.hpp"
#include "../hal/spi.hpp"

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
