
#pragma once

#include <cstdint>

#include "../../hal/timer.hpp"

namespace fan_controller {

class fan_pwm
{
public:
  // Create a fan controller using an existing PWM channel.
  explicit fan_pwm(lab2::pwm& p_pwm_channel);

  // Change the PWM duty cycle (0 to 100 percent).
  void set_duty_cycle(std::uint8_t p_percent);

  // Read the frequency of the PWM channel.
  std::uint32_t frequency();

private:
  // Reference to the PWM driver that controls the hardware.
  lab2::pwm& m_pwm_channel;
};

}  // namespace fan_controller
