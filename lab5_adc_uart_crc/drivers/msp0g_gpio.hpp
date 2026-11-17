#pragma once

#include <cstdint>

#include "../hal/gpio.hpp"
#include "msp0g_iomux.hpp"

// Peripheral Base Addresses:
// GPIOA = 0x400A0000, GPIOB = 0x400A2000
// IOMUX = 0x40428000

// GPIO Register Offsets:
// PWREN = 0x800, RSTCTL = 0x804,
// DOUTSET31_0 = 0x1290, DOUTCLR31_0 = 0x12A0,
// DOESET31_0 = 0x12D0, DOECLR31_0 = 0x12E0, DIN31_0 = 0x1380

// Needed IOMUX Register Offsets:
// SW1 = PA18 = PINCM40, offset 0xA0
// SW2 = PB21 = PINCM49, offset 0xC4
// BLUE = PB22 = PINCM50, offset 0xC8
// RED = PB26 = PINCM57, offset 0xE4
// GREEN = PB27 = PINCM58, offset 0xE8

// Needed GPIO Bit Mapping:
// PWREN -> bit 0 = enable, key = 0x26
// RSTCTL -> bit 0 = reset assert, key = 0x1
// note: key enables writing to the enable bits and goes into
// the highest byte of the reg

// Needed IOMUX Bit Mapping:
// PF = bits 0-5, controls which peripheral to select (like GPIO)
// PC = bit 7, allows connection to from mux to peripheral
// PIPD = bit 16, enables pulldown
// PIPU = bit 17, enables pullup
// INENA = bit 18, allows the peripheral to be read by MCU
// HIZ1 = bit 25, high impedance/open-drain

namespace msp0g {

/** @brief Select GPIO port A or B. */
enum class gpio_port : std::uint8_t
{
  a = 0,
  b
};

/**
 * @brief Describe a GPIO pin and its associated IOMUX register.
 *
 * The caller must provide a port, pin number, and PINCM number that refer to
 * the same physical pin. Construction checks the individual ranges and aborts
 * if either number is invalid, but it does not verify the physical mapping.
 */
struct gpio_pin
{
  gpio_port port;      ///< GPIO port containing the pin.
  std::uint8_t pin;    ///< Port bit number: 0-31 for A or 0-27 for B.
  std::uint8_t pincm;  ///< One-based PINCM register number, from 1 to 60.
};

/** @brief Drive and sample a GPIO output through the lab1 interface. */
class msp0g_outpin : public lab1::output_pin
{

public:
  /**
   * @brief Configure the pin as a GPIO output and initialize its port if
   * needed.
   *
   * @param p_gpio_pin Port, bit number, and associated PINCM register.
   * @param p_settings Resistor and open-drain settings; defaults to no resistor
   *                   and open-drain disabled.
   *
   * @note Checks PWREN before initialization, so an already-powered port is not
   *       reset when another GPIO object is constructed. Invalid pin or PINCM
   *       values abort the application. Call level() to set the desired output
   *       state after construction.
   */
  msp0g_outpin(const gpio_pin& p_gpio_pin, const settings& p_settings = {});

private:
  /**
   * @brief Apply output resistor and open-drain settings.
   * @param p_settings Electrical settings for the pin.
   * @return Always true because invalid pins abort during construction and all
   *         output settings are supported.
   */
  bool driver_configure(const settings& p_settings) override;

  /**
   * @brief Set or clear the pin's output latch using the GPIO set/clear
   * registers.
   * @param p_high True requests high; false requests low.
   */
  void driver_level(bool p_high) override;

  /**
   * @brief Sample the pin through the GPIO input register.
   * @return True for a high input; false for a low input.
   * @note Reads the input level rather than the stored output latch.
   */
  bool driver_level() override;

  msp0g_iomux m_pincm;
  std::uint32_t m_pin_mask = 0U;
  gpio_port m_port;
};

/** @brief Sample a GPIO input through the lab1 interface. */
class msp0g_inpin : public lab1::input_pin
{

public:
  /**
   * @brief Configure the pin as a GPIO input and initialize its port if needed.
   *
   * @param p_gpio_pin Port, bit number, and associated PINCM register.
   * @param p_settings Initial resistor setting; defaults to a pull-up.
   *
   * @note Enables the input path and disables GPIO output for this pin. PWREN
   *       is checked before initialization, so an already-powered port is not
   *       reset. Invalid pin or PINCM values abort the application.
   */
  msp0g_inpin(const gpio_pin& p_gpio_pin, const settings& p_settings = {});

private:
  /**
   * @brief Apply the input pin's resistor setting.
   * @param p_settings Pull-up, pull-down, or no resistor.
   * @return Always true because invalid pins abort during construction and all
   *         input resistor settings are supported.
   */
  bool driver_configure(const settings& p_settings) override;

  /**
   * @brief Sample the GPIO input register for this pin.
   * @return True for a high input; false for a low input.
   */
  bool driver_level() override;

  msp0g_iomux m_pincm;
  std::uint32_t m_pin_mask = 0U;
  gpio_port m_port;
};

}  // namespace msp0g
