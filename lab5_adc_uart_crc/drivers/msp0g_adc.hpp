#pragma once

// cspell:ignore SYSOSC VDDA TSTRIM SLASEX SLAU DMASZ

// DMA-backed driver for the on-chip temperature sensor, and the conversion
// from a raw sample to temperature.

#include <cstddef>
#include <cstdint>

#include "../hal/adc.hpp"

namespace msp0g {

/**
 * @brief One converted temperature reading, in units of 0.1 degC.
 *
 * 250 means 25.0 degC. Both boards must use this encoding and little-endian
 * byte order.
 */
using temperature_sample_t = std::int16_t;

/** @brief How many `temperature_sample_t` counts make up one degree C. */
inline constexpr std::int32_t temperature_units_per_degree = 10;

/**
 * @brief Upscale a 12-bit ADC code to 16 bits by bit duplication.
 *
 * 0x000 maps to 0x0000 and 0xFFF maps to 0xFFFF.
 *
 * @param p_code 12-bit ADC code. Bits above bit 11 are ignored.
 */
constexpr std::uint16_t upscale_12_to_16(std::uint16_t p_code)
{
  std::uint16_t const code = p_code & 0x0FFFU;
  return static_cast<std::uint16_t>((code << 4) | (code >> 8));
}

/**
 * @brief Convert one upscaled sample to 0.1 degC.
 *
 * T = 30 degC + (V_sample - V_trim) / TSc, with TSc = -1.8 mV/degC and
 * V = 3.3 V * code / 0xFFFF. Integer math; the Cortex-M0+ has no FPU.
 *
 * @param p_sample 16-bit-scaled ADC sample.
 * @param p_trim 16-bit-scaled factory code at 30 degC.
 */
constexpr temperature_sample_t to_temperature(std::uint16_t p_sample,
                                              std::uint16_t p_trim)
{
  constexpr std::int64_t supply_microvolts = 3'300'000;
  constexpr std::int64_t sensor_microvolts_per_degree = 1'800;
  constexpr std::int64_t trim_temperature = 30;
  constexpr std::int64_t full_scale = 0xFFFF;

  // The sensor coefficient is negative, so a higher voltage is colder.
  std::int64_t const delta_counts =
    static_cast<std::int64_t>(p_sample) - static_cast<std::int64_t>(p_trim);

  // delta_degrees = -(delta_counts * 3.3 V / full_scale) / 1.8 mV
  // Multiply by the units-per-degree up front so we divide only once.
  std::int64_t const numerator =
    delta_counts * supply_microvolts * temperature_units_per_degree;
  std::int64_t const denominator = full_scale * sensor_microvolts_per_degree;

  // Round to nearest (not toward zero) so the error stays symmetric.
  std::int64_t const half_denominator = denominator / 2;
  std::int64_t const rounded_delta =
    (numerator >= 0 ? numerator + half_denominator
                    : numerator - half_denominator) /
    denominator;

  return static_cast<temperature_sample_t>(
    trim_temperature * temperature_units_per_degree - rounded_delta);
}

/**
 * @brief Factory calibration code at 30 degC, scaled to 16 bits.
 *
 * Read once. It does not change at run time.
 */
std::uint16_t temperature_trim_sample();

/**
 * @brief DMA-backed driver that samples the on-chip temperature sensor.
 *
 * One read() fills the caller's buffer in a single DMA transfer. The calling
 * task sleeps until the DMA-done interrupt. The constructor configures ADC0
 * and DMA channel 0 completely.
 *
 * @note Only one instance, and only one task may call read() at a time.
 */
class msp0g_adc : public lab5::adc16_stream
{
public:
  /** @brief Configure ADC0 channel 11 and DMA channel 0. */
  msp0g_adc();

  msp0g_adc(msp0g_adc const&) = delete;
  msp0g_adc& operator=(msp0g_adc const&) = delete;

  /**
   * @brief Wake the task blocked in read() on DMA done.
   *
   * Call only from ADC0_IRQHandler().
   */
  static void handle_interrupt();

private:
  /**
   * @brief Fill p_buffer with one DMA transfer, then upscale each sample.
   *
   * Blocks until DMA done. A size of 0 or above 65535 is ignored.
   */
  void driver_read(std::span<std::uint16_t> p_buffer) override;
};

}  // namespace msp0g
