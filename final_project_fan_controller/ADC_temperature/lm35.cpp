#include "lm35.hpp"

#include <ti/driverlib/driverlib.h>

#include "ti_msp_dl_config.h"

namespace lm35 {
namespace {

constexpr std::uint32_t adc_sample_time_cycles = 560;

}  // namespace

void initialize_adc()
{
  DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM55);

  DL_ADC12_reset(ADC0);
  DL_ADC12_enablePower(ADC0);
  DL_Common_delayCycles(POWER_STARTUP_DELAY);

  DL_ADC12_ClockConfig clock_config{};
  clock_config.clockSel = DL_ADC12_CLOCK_SYSOSC;
  clock_config.divideRatio = DL_ADC12_CLOCK_DIVIDE_1;
  clock_config.freqRange = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32;
  DL_ADC12_setClockConfig(ADC0, &clock_config);

  DL_ADC12_initSingleSample(ADC0,
                            DL_ADC12_REPEAT_MODE_DISABLED,
                            DL_ADC12_SAMPLING_SOURCE_AUTO,
                            DL_ADC12_TRIG_SRC_SOFTWARE,
                            DL_ADC12_SAMP_CONV_RES_12_BIT,
                            DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);

  DL_ADC12_configConversionMem(ADC0,
                               DL_ADC12_MEM_IDX_0,
                               DL_ADC12_INPUT_CHAN_2,
                               DL_ADC12_REFERENCE_VOLTAGE_VDDA,
                               DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
                               DL_ADC12_AVERAGING_MODE_DISABLED,
                               DL_ADC12_BURN_OUT_SOURCE_DISABLED,
                               DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
                               DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
  DL_ADC12_setSampleTime0(ADC0, adc_sample_time_cycles);
  DL_ADC12_enableConversions(ADC0);
}

std::uint16_t read_adc_code()
{
  DL_ADC12_startConversion(ADC0);
  while (DL_ADC12_isConversionStarted(ADC0)) {
  }

  return DL_ADC12_getMemResult(ADC0, DL_ADC12_MEM_IDX_0);
}

float adc_code_to_celsius(std::uint16_t adc_code, float vdda_volts)
{
  float const voltage = adc_code * vdda_volts / 4095.0f;
  return voltage * 100.0f;
}

}  // namespace lm35