#pragma once

#include <cstdint>

#include "fan_pwm.hpp"

namespace fan_controller {

// Settings for the temperature curve and RPM controller.
struct fan_control_config
{
  float minimum_temperature;
  float maximum_temperature;
  std::uint32_t minimum_rpm;
  std::uint32_t maximum_rpm;
  std::uint8_t minimum_duty_cycle;
  std::uint8_t duty_step;
  std::uint32_t rpm_deadband;
};

// Return the current control values to the application.
struct fan_control_result
{
  std::uint32_t target_rpm;
  std::uint8_t duty_cycle;
  bool control_valid;
};

class fan_control
{
public:
  // Create a controller using an existing fan PWM driver.
  fan_control(fan_pwm& p_fan, fan_control_config const& p_config);

  // Update the fan speed using fresh temperature and RPM readings.
  // Call this function at a fixed interval from one control task.
  fan_control_result update(float p_temperature,
                            std::uint32_t p_actual_rpm,
                            bool p_temperature_valid,
                            bool p_rpm_valid);

private:
  // Convert temperature in degrees Celsius into a target RPM.
  std::uint32_t target_speed(float p_temperature) const;

  // Reference to the portable fan PWM driver.
  fan_pwm& m_fan;

  // Store the temperature curve and controller settings.
  fan_control_config m_config;

  // Store the last requested duty cycle as a percentage.
  std::uint8_t m_duty_cycle;

  // Track whether the controller settings are valid.
  bool m_config_valid;
};

}  // namespace fan_controller
