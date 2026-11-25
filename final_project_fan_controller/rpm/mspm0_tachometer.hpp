#pragma once

#include <cstdint>

#include <ti/devices/msp/msp.h>

#include "rpm_measurement.hpp"

namespace fan_controller {

/**
 * @brief Hardware settings for an MSPM0 GPIO tachometer input.
 *
 * The fan tachometer output is normally open-collector/open-drain, so the
 * input is configured with a pull-up and a falling-edge interrupt. The
 * edge-polarity field is kept in the configuration so another pin or edge
 * polarity can be selected without changing the driver.
 */
struct mspm0_tachometer_config
{
  GPIO_Regs* port;
  std::uint32_t pin;
  std::uint32_t iomux;
  std::uint32_t edge_polarity;
  IRQn_Type interrupt_number;
  std::uint32_t pulses_per_revolution;
};

class mspm0_tachometer
{
public:
  explicit mspm0_tachometer(mspm0_tachometer_config p_config);

  /**
   * @brief Return the default LaunchPad tachometer input configuration.
   *
   * PA14 is PINCM36 and is available on the LaunchPad pin-extension header.
   * The default assumes the common two tachometer pulses per revolution.
   */
  [[nodiscard]] static mspm0_tachometer_config launchpad_pa14(
    std::uint32_t p_pulses_per_revolution = 2U);

  /**
   * @brief Configure the input, GPIO edge interrupt, and NVIC line.
   *
   * The application's GPIO interrupt handler must call
   * handle_gpio_interrupt() when the configured GPIO port has a pending
   * interrupt. This class deliberately does not define GROUP1_IRQHandler so
   * it can coexist with the team's button interrupts.
   */
  bool initialize();

  [[nodiscard]] bool ready() const;

  /**
   * @brief Service the configured pin from the shared GPIO ISR.
   * @return true if this driver consumed a tachometer edge.
   */
  bool handle_gpio_interrupt();

  /**
   * @brief Take one measurement window and clear its pulse counter.
   *
   * Call this from task context after the selected measurement window has
   * elapsed. A completed window containing no pulses returns valid RPM 0.
   */
  [[nodiscard]] rpm_reading sample(std::uint32_t p_window_ms);

  /** @brief Atomically take and clear the accumulated pulse count. */
  [[nodiscard]] std::uint32_t take_pulse_count();

private:
  mspm0_tachometer_config m_config;
  volatile std::uint32_t m_pulse_count;
  bool m_ready;
};

}  // namespace fan_controller
