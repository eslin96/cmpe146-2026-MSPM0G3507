#pragma once

#include <cstdint>

#include "../hal/timer.hpp"

namespace msp0g {

/**
 * @brief Select a general-purpose timer supported by the PWM driver.
 *
 * Supports TIMG0, TIMG6, TIMG7, and TIMG8. TIMG12 is reserved for the steady
 * clock and does not have the prescaler used by this PWM implementation.
 */
enum class pwm_timer : std::uint8_t
{
  timg0 = 0,
  timg6,
  timg7,
  timg8
};

/**
 * @brief Select capture/compare channel 0 or 1 for PWM output.
 */
enum class pwm_timer_channel : std::uint8_t
{
  cc0 = 0,
  cc1
};

/** @brief Requested PWM frequency and timer/channel selection. */
struct pwm_config
{

  std::uint32_t freq;  ///< Requested PWM frequency in Hz, from 16 to 1,000,000.
  pwm_timer timer;     ///< Timer providing the PWM period.
  pwm_timer_channel cc;  ///< Capture/compare channel providing the output.
};

/**
 * @brief Accumulate a 64-bit uptime from the 32-bit TIMG12 counter.
 *
 * The counter runs at 4 MHz when BUSCLK is 32 MHz. Uptime is returned in timer
 * ticks rather than seconds and is updated whenever uptime() is called.
 */
class msp0g_steady_clock : public lab2::steady_clock
{

public:
  /**
   * @brief Reset TIMG12 and start its continuous 32-bit up-counter.
   *
   * @note Use only one instance. Constructing another resets the timer used by
   *       existing instances and disrupts their accumulated uptime.
   *       Clock calculations assume a 32 MHz BUSCLK.
   */
  msp0g_steady_clock();

private:
  std::uint64_t m_uptime;
  std::uint32_t m_prev_count;

  /**
   * @brief Report the assumed TIMG12 tick frequency.
   * @return 4,000,000 ticks per second.
   */
  std::uint32_t driver_frequency() override;
  /**
   * @brief Accumulate elapsed ticks using the difference from the previous
   * read.
   * @return Total ticks observed since construction.
   * @note Unsigned subtraction handles counter wraparound. Call at intervals
   *       shorter than one 32-bit counter period to avoid losing elapsed ticks
   *       (about 17.9 minutes at 4 MHz).
   */
  std::uint64_t driver_uptime() override;
};

/**
 * @brief Generate PWM with a shared timer period and a dedicated output
 * channel.
 *
 * Configure the desired timer output pin through IOMUX separately. Frequency
 * calculations assume a 32 MHz timer source divided to a 1 MHz counter clock.
 * Each selected timer must be owned exclusively by this PWM driver, although
 * its two channels may be shared by compatible msp0g_pwm objects.
 */
class msp0g_pwm : public lab2::pwm
{

public:
  /**
   * @brief Initialize or share a PWM timer and reserve its selected channel.
   *
   * @param p_config Requested frequency, timer, and capture/compare channel.
   * @pre The requested frequency is between 16 and 1,000,000 Hz, inclusive.
   * @pre The selected timer is either unused or was configured by another
   *      msp0g_pwm object. No other driver may configure or use that timer.
   *
   * @note Objects sharing a timer must have the same calculated frequency and
   *       use different, unoccupied channels. Construction aborts if the
   *       frequency is out of range, the existing calculated frequency is
   *       different, or the requested channel is already in use. Successful
   *       channel reservations remain for the rest of the program run.
   *
   * Integer division quantizes the period, so the calculated frequency can be
   * higher than requested. Set the desired duty cycle after construction.
   */
  msp0g_pwm(const pwm_config& p_config);

private:
  std::uintptr_t m_timer_base;
  std::uint8_t m_channel;

  /**
   * @brief Report the PWM frequency calculated from the timer's load value.
   * @return Quantized frequency in Hz, assuming the configured 1 MHz timer
   *         clock.
   */
  std::uint32_t driver_frequency() override;

  /**
   * @brief Scale a normalized duty-cycle request into the channel compare
   * value.
   *
   * @param p_duty_cycle Requested duty cycle from 0 (0%) to 65,535 (100%).
   *
   * @note The compare value is quantized to the timer period. Lower PWM
   *       frequencies provide more counter steps and finer duty resolution.
   */
  void driver_duty_cycle(std::uint16_t p_duty_cycle) override;
};

}  // namespace msp0g
