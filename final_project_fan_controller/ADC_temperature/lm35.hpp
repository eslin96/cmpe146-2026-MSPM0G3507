#pragma once

#include <cstdint>

namespace lm35 {

void initialize_adc();
std::uint16_t read_adc_code();
float adc_code_to_celsius(std::uint16_t adc_code, float vdda_volts);

}  // namespace lm35