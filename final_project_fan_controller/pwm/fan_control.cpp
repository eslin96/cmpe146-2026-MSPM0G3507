#include "fan_control.hpp"

#include <cmath>

namespace fan_controller {

// Store the fan driver and the controller settings.
fan_control::fan_control(fan_pwm& p_fan, fan_control_config const& p_config)
  : m_fan(p_fan)
  , m_config(p_config)
  , m_duty_cycle(100U)
  , m_config_valid(false)
{
  // Check the temperature range, RPM range, and duty cycle limits.
  m_config_valid =
    std::isfinite(m_config.minimum_temperature) &&
    std::isfinite(m_config.maximum_temperature) &&
    m_config.minimum_temperature < m_config.maximum_temperature &&
    m_config.minimum_rpm > 0U && m_config.minimum_rpm <= m_config.maximum_rpm &&
    m_config.minimum_duty_cycle > 0U && m_config.minimum_duty_cycle <= 100U &&
    m_config.duty_step > 0U && m_config.duty_step <= 100U;

  // Request full cooling until valid readings are available.
  m_fan.set_duty_cycle(m_duty_cycle);
}

// Convert the temperature reading into a target fan speed.
std::uint32_t fan_control::target_speed(float p_temperature) const
{
  // Use the minimum speed below the lower temperature limit.
  if (p_temperature <= m_config.minimum_temperature) {
    return m_config.minimum_rpm;
  }

  // Use the maximum speed above the upper temperature limit.
  if (p_temperature >= m_config.maximum_temperature) {
    return m_config.maximum_rpm;
  }

  // Calculate the position between the two temperature limits.
  double temperature_range = static_cast<double>(m_config.maximum_temperature) -
                             m_config.minimum_temperature;
  double fraction =
    (static_cast<double>(p_temperature) - m_config.minimum_temperature) /
    temperature_range;

  // Calculate the target RPM using a linear temperature curve.
  std::uint32_t rpm_range = m_config.maximum_rpm - m_config.minimum_rpm;
  return m_config.minimum_rpm +
         static_cast<std::uint32_t>(fraction * rpm_range + 0.5);
}

// Adjust the duty cycle using the difference between target and actual RPM.
fan_control_result fan_control::update(float p_temperature,
                                       std::uint32_t p_actual_rpm,
                                       bool p_temperature_valid,
                                       bool p_rpm_valid)
{
  // Request full cooling if the configuration or readings are invalid.
  if (!m_config_valid || !p_temperature_valid ||
      !std::isfinite(p_temperature) || !p_rpm_valid) {
    m_duty_cycle = 100U;
    m_fan.set_duty_cycle(m_duty_cycle);
    return { m_config.maximum_rpm, m_duty_cycle, false };
  }

  // Determine the requested fan speed from the temperature.
  std::uint32_t target_rpm = target_speed(p_temperature);
  int next_duty = m_duty_cycle;

  // Increase the duty cycle when the fan is too slow.
  if (p_actual_rpm < target_rpm &&
      target_rpm - p_actual_rpm > m_config.rpm_deadband) {
    next_duty += m_config.duty_step;
  }
  // Decrease the duty cycle when the fan is too fast.
  else if (p_actual_rpm > target_rpm &&
           p_actual_rpm - target_rpm > m_config.rpm_deadband) {
    next_duty -= m_config.duty_step;
  }

  // Keep the duty cycle within the configured operating range.
  if (next_duty < m_config.minimum_duty_cycle) {
    next_duty = m_config.minimum_duty_cycle;
  }
  if (next_duty > 100) {
    next_duty = 100;
  }

  // Send the new duty cycle to the portable PWM driver.
  m_duty_cycle = static_cast<std::uint8_t>(next_duty);
  m_fan.set_duty_cycle(m_duty_cycle);

  // Return the target RPM and duty cycle for the application.
  return { target_rpm, m_duty_cycle, true };
}

}  // namespace fan_controller
