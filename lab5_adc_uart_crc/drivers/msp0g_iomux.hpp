#pragma once

#include <cstdint>

#include "../hal/gpio.hpp"

namespace msp0g {

/**
 * @brief Select whether the pin's input path is enabled through INENA.
 */
enum class input_enable
{
  disable = 0,
  enable
};

/** @brief Electrical settings that can be changed after pin selection. */
struct iomux_settings
{

  lab1::pin_resistor resistor =
    lab1::pin_resistor::none;  ///< Pull resistor choice.
  bool open_drain = false;     ///< Enable open-drain operation when true.
};

/** @brief Pin, peripheral function, and input path selected at construction. */
struct iomux_config
{

  uint8_t pincm;   ///< One-based PINCM register number, from 1 to 60.
  uint8_t pf_sel;  ///< Peripheral function selector, from 0 to 0x3F.
  input_enable inena = input_enable::disable;  ///< Input path selection.
};

/** @brief Select a pin's peripheral function and configure its electrical
 * state. */
class msp0g_iomux
{

public:
  /**
   * @brief Select the pin function, connect the pin, and apply initial
   * settings.
   *
   * @param p_iomux_config PINCM number, peripheral selector, and input enable.
   * @param p_iomux_settings Pull resistor and open-drain configuration.
   *
   * @note PINCM must be 1-60 and the selector must be at most 0x3F. An invalid
   *       value aborts the application before writing the PINCM register. The
   *       caller must check that the selected function exists on the pin.
   */
  msp0g_iomux(const iomux_config& p_iomux_config,
              const iomux_settings& p_iomux_settings);

  /**
   * @brief Apply the pin's pull resistor and open-drain settings.
   *
   * @param p_iomux_settings Resistor choice and open-drain enable state.
   * @note Enabling either pull resistor clears the other; selecting none
   *       clears both. Pin function and input enable are unchanged.
   */
  void config(const iomux_settings& p_iomux_settings);

private:
  std::uint8_t m_pincm;
};
}  // namespace msp0g
