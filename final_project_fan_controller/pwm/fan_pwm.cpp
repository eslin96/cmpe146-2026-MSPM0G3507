
#include "fan_pwm.hpp"

namespace fan_controller {

// Store the PWM driver provided to the constructor.
fan_pwm::fan_pwm(lab2::pwm& p_pwm_channel)
  : m_pwm_channel(p_pwm_channel)
{
}

// Set the PWM duty cycle using a percentage.
void fan_pwm::set_duty_cycle(std::uint8_t p_percent)
{
  // Do not allow a duty cycle above 100%.
  if (p_percent > 100U) {
    p_percent = 100U;
  }

  // Convert 0-100% into the 16-bit PWM range.
  std::uint32_t duty_value =
    (static_cast<std::uint32_t>(p_percent) * 65535U + 50U) / 100U;

  // Send the duty cycle to the PWM driver.
  m_pwm_channel.duty_cycle(static_cast<std::uint16_t>(duty_value));
}

// Return the PWM frequency.
std::uint32_t fan_pwm::frequency()
{
  return m_pwm_channel.frequency();
}

}  // namespace fan_controller
