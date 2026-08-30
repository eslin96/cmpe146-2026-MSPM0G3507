#pragma once

#include <cstdint>

namespace lab2 {

/**
 * @brief Hardware abstraction interface for a steady clock mechanism
 *
 * Implementations of this interface must follow the same requirements as a
 * std::chrono::steady_clock, in that the clock is monotonic & steady. An
 * additional requirement is added to ensure that the clock is reliable. Meaning
 * calls to the interface functions do not return errors because this clock
 * should be infallible. To ensure this, this clock should be driven by the
 * platform's peripheral drivers or some other mechanism that is unlikely to go
 * offline while the platform is in a normal operating state.
 *
 * This clock is steady meaning that subsequent calls to get the uptime of this
 * clock cannot decrease as physical time moves forward and the time between
 * ticks of this clock are constant and defined by the clock's frequency.
 *
 * This can be used to get the time since the boot up, or to be more accurate,
 * the time when the steady clock object is created. This clock is most suitable
 * for measuring time intervals.
 *
 * After creation of this clock, the operating frequency shall not change.
 */
class steady_clock
{
public:
  /**
   * @brief Get the operating frequency of the steady clock
   *
   * @return std::uint32_t - operating frequency of the steady clock. Guaranteed
   * to be a positive value by the implementing driver.
   */
  [[nodiscard]] std::uint32_t frequency()
  {
    return driver_frequency();
  }

  /**
   * @brief Get the current value of the steady clock
   *
   * @return std::uint64_t - Number of counts that the steady clock has counted
   * since it started.
   */
  [[nodiscard]] std::uint64_t uptime()
  {
    return driver_uptime();
  }

private:
  virtual std::uint32_t driver_frequency() = 0;
  virtual std::uint64_t driver_uptime() = 0;
};

/**
 * @brief Pulse Width Modulation (PWM) channel hardware abstraction.
 *
 * This driver controls the waveform generation of a square wave and its
 * properties such as frequency and duty cycle.
 *
 * Frequency, meaning how often the waveform cycles from from low to high.
 *
 * Duty cycle, what proportion of the wavelength of the pulse is the voltage
 * HIGH.
 *
 *  ```
 *     ____________________       _
 *    |                    |     |
 *   _|                    |_____|
 *    ^                    ^     ^
 *    |<------ high ------>|<low>|
 *
 *    HIGH Duration = 18 segments
 *    LOW Duration =  5 segments
 *    Duty Cycle = 20 / (20 + 5) = 80%
 *
 *    If each segment is 1us then the wavelength is 25us
 *    Thus frequency is (1 / 25us) = 40kHz
 * ```
 *
 * PWM is used for power control like motor control, lighting, transmitting
 * signals to servos, sending telemetry and much more.
 *
 */
class pwm
{
public:
  /**
   * @brief Returns the frequency of this pwm channel
   *
   * @returns std::uint32_t - frequency in hertz as an unsigned integer
   */
  std::uint32_t frequency()
  {
    return driver_frequency();
  }

  /**
   * @brief Set the pwm waveform duty cycle
   *
   * The input value `p_duty_cycle` is a 16-bit unsigned value from 0 to 65535.
   *
   * The 16-bit value is directly proportional to the duty cycle
   * percentage, such that a value of:
   *
   *     - 0% would be 0 (65535 * 0.00)
   *     - 100% would be 65535 (65535 * 1.00)
   *     - 25% would be 16384 (65535 * 0.25)
   *     - 44.5% would be 29163 (65535 * 0.445)
   *
   * @param p_duty_cycle - a value from 0 to 65535 representing the duty
   * cycle percentage.
   */
  void duty_cycle(std::uint16_t p_duty_cycle)
  {
    driver_duty_cycle(p_duty_cycle);
  }

private:
  virtual std::uint32_t driver_frequency() = 0;
  virtual void driver_duty_cycle(std::uint16_t p_duty_cycle) = 0;
};
}  // namespace lab2
