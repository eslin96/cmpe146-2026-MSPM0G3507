#include "rpm_measurement.hpp"

#include <cstdint>
#include <limits>

namespace fan_controller {

rpm_reading calculate_rpm(std::uint32_t p_pulse_count,
                          std::uint32_t p_pulses_per_revolution,
                          std::uint32_t p_window_ms)
{
  if (p_pulses_per_revolution == 0U || p_window_ms == 0U) {
    return { 0U, false };
  }

  // RPM = pulses * 60,000 /
  //       (pulses_per_revolution * measurement_window_ms).
  std::uint64_t const numerator =
    static_cast<std::uint64_t>(p_pulse_count) * 60'000ULL;
  std::uint64_t const denominator =
    static_cast<std::uint64_t>(p_pulses_per_revolution) * p_window_ms;

  // Round to the nearest whole RPM instead of always truncating downward.
  std::uint64_t const calculated_rpm =
    (numerator + denominator / 2U) / denominator;

  if (calculated_rpm > std::numeric_limits<std::uint32_t>::max()) {
    return { std::numeric_limits<std::uint32_t>::max(), true };
  }

  return { static_cast<std::uint32_t>(calculated_rpm), true };
}

}  // namespace fan_controller
