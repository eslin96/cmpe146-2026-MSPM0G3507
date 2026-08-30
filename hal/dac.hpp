#pragma once

#include <cstdint>

namespace lab5 {

/**
 * @brief 16-bit Digital to Analog Converter (DAC) hardware abstraction
 * interface.
 *
 * Use this interface for devices and peripherals that can create arbitrary
 * analog voltages between a defined Vss (negative reference) and Vcc
 * (positive reference) voltage.
 *
 * This interface can represent DACs with precision 16-bits and below. See
 * `lab5::dac16::write()` for more details.
 *
 */
class dac16
{
public:
  /**
   * @brief Set the output voltage of the DAC.
   *
   * The input value `p_percentage` is a 16-bit unsigned number from 0
   * (0x0000) to 65535 (0xFFFF).
   *
   * This value is linearly proportional to the output voltage relative to
   * Vss and Vcc such that if Vss is 0V (gnd) and Vcc is 5V then:
   *
   *   - 0.000V (0.000%) is 65535 * 0.000 = 0
   *   - 1.250V (25.00%) is 65535 * 0.250 = 16383
   *   - 2.225V (44.50%) is 65535 * 0.445 = 29163
   *   - 5.000V (100.0%) is 65535 * 1.000 = 65535
   *
   * For drivers where the underlying hardware has a DAC precision below
   * 16-bits, the implementation should set the DAC output register to the
   * most significant bits of this value. This preserves the most
   * significant information about the intended output level.
   *
   * @param p_percentage - value from 0 (0x0000) to 65535 (0xFFFF)
   * representing the proportion of the output voltage from Vss to Vcc.
   */
  void write(std::uint16_t p_percentage)
  {
    driver_write(p_percentage);
  }

private:
  virtual void driver_write(std::uint16_t p_percentage) = 0;
};
}  // namespace lab5
