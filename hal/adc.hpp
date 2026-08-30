#pragma once

#include <cstdint>

#include "../third_party/span/span.hpp"

namespace lab5 {

/**
 * @brief 16-bit Analog to Digital Converter (ADC) hardware abstraction
 * interface, streamed into a buffer.
 *
 * ADC peripherals only know the proportion of a voltage signal relative to a
 * Vss (negative reference) and a Vcc (positive reference) and thus cannot
 * describe the voltage directly.
 *
 * This interface is meant for ADCs of 16-bits and below. Most common ADCs
 * fall into this category.
 *
 * Unlike a single-sample `read()` API, this interface fills an entire buffer
 * of samples per call. This lets a DMA-backed implementation sleep the
 * calling task for the duration of the whole transfer, instead of waking up
 * once per sample.
 *
 */
class adc16_stream
{
public:
  /**
   * @brief Fill p_buffer with p_buffer.size() consecutive ADC samples
   *
   * Each sample written into p_buffer is guaranteed by the implementing
   * driver to be between 0 and 65535 (0xFFFF). Each value represents the
   * voltage measured by the ADC from Vss (negative reference) to Vcc
   * (positive reference).
   *
   * For example, if Vss is 0V (gnd) and Vcc is 5V then a value of 32767
   * (0x7FFF) (half of 65535) would mean a measured voltage of 2.5V.
   *
   * If the ADC's resolution is below 16-bits, the implementation is required
   * to upscale each sample via bit duplication (shift the sampled bits to
   * the top of the value, then repeat them into the lower bits) so that a
   * sample of 0 always maps to 0 and the maximum sample always maps to
   * 0xFFFF.
   *
   * This call blocks until p_buffer has been completely filled. Drivers
   * backed by DMA should put the calling task to sleep for the duration of
   * the whole transfer and let the DMA transfer-complete interrupt wake it,
   * rather than busy-waiting or waking up once per sample.
   *
   * @param p_buffer - buffer to fill with consecutive ADC samples. The
   * number of samples taken is p_buffer.size().
   */
  void read(std::span<std::uint16_t> p_buffer)
  {
    driver_read(p_buffer);
  }

private:
  virtual void driver_read(std::span<std::uint16_t> p_buffer) = 0;
};
}  // namespace lab5
