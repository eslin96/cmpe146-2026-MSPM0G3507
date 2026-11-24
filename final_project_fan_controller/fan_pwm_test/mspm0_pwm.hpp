#pragma once

#include <cstdint>

#include "../../hal/timer.hpp"

namespace fan_controller {

class mspm0_pwm : public lab2::pwm
{
public:
  // Create a PWM driver with the requested frequency.
  explicit mspm0_pwm(std::uint32_t p_frequency_hz);

private:
  // Return the configured PWM frequency.
  std::uint32_t driver_frequency() override;

  // Change the PWM duty cycle.
  void driver_duty_cycle(std::uint16_t p_duty_cycle) override;

  // Store the actual PWM frequency.
  std::uint32_t m_frequency_hz;

  // Timer period value calculated from the requested frequency.
  std::uint32_t m_load_value;
};

}  // namespace fan_controller
