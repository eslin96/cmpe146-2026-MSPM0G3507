// cspell:ignore SYSOSC VDDA IIDX MEMRES SCOMP FRANGE SCLKDIV DMADONE DMASZ

#include <cstddef>
#include <cstdint>

#include "FreeRTOS.h"
#include "task.h"

#include <ti/driverlib/driverlib.h>

#include "msp0g_adc.hpp"

namespace {

// Full DMA channel. Single-transfer mode moves one ADC result per trigger.
constexpr std::uint32_t dma_channel = 0;
constexpr std::uint32_t samples_per_dma_trigger = 1;

// DMASZ is 16 bits.
constexpr std::size_t max_samples_per_read = 0xFFFF;

// SYSOSC is 32 MHz after reset. Sample window is 12.5 us sensor settle plus
// 5 us ADC wake-up. SCOMP0 is 10 bits, so the cycle count must stay <= 0x3FF.
constexpr std::uint32_t adc_clock_hertz = 32'000'000;
constexpr std::uint32_t sample_time_nanoseconds = 17'500;
constexpr std::uint32_t sample_time_cycles =
  (adc_clock_hertz / 1'000 * sample_time_nanoseconds + 999'999) / 1'000'000;

static_assert(sample_time_cycles >= 400,
              "sample time must be at least 12.5 us");
static_assert(sample_time_cycles <= 0x3FF,
              "sample time must fit the 10-bit SCOMP0 register");

constexpr std::uint32_t power_startup_delay_cycles = 16;

// Task blocked in driver_read(). File scope because the IRQ handler is static.
TaskHandle_t volatile waiting_task = nullptr;

}  // namespace

namespace msp0g {

std::uint16_t temperature_trim_sample()
{
  auto const trim_code =
    static_cast<std::uint16_t>(DL_SYSCTL_getTempCalibrationConstant());

  return upscale_12_to_16(trim_code);
}

msp0g_adc::msp0g_adc()
{
  DL_ADC12_reset(ADC0);
  DL_ADC12_enablePower(ADC0);
  DL_Common_delayCycles(power_startup_delay_cycles);

  // FRANGE must match the 32 MHz clock or end-of-conversion is timed wrong.
  DL_ADC12_ClockConfig clock_config{};
  clock_config.clockSel = DL_ADC12_CLOCK_SYSOSC;
  clock_config.divideRatio = DL_ADC12_CLOCK_DIVIDE_1;
  clock_config.freqRange = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32;
  DL_ADC12_setClockConfig(ADC0, &clock_config);

  // Repeat mode: one start collects every sample in the DMA transfer.
  DL_ADC12_initSingleSample(ADC0,
                            DL_ADC12_REPEAT_MODE_ENABLED,
                            DL_ADC12_SAMPLING_SOURCE_AUTO,
                            DL_ADC12_TRIG_SRC_SOFTWARE,
                            DL_ADC12_SAMP_CONV_RES_12_BIT,
                            DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);

  // Channel 11 is the temperature sensor. Trim was taken against 3.3 V.
  DL_ADC12_configConversionMem(ADC0,
                               DL_ADC12_MEM_IDX_0,
                               DL_ADC12_INPUT_CHAN_11,
                               DL_ADC12_REFERENCE_VOLTAGE_VDDA,
                               DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
                               DL_ADC12_AVERAGING_MODE_DISABLED,
                               DL_ADC12_BURN_OUT_SOURCE_DISABLED,
                               DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
                               DL_ADC12_WINDOWS_COMP_MODE_DISABLED);

  DL_ADC12_setSampleTime0(ADC0, sample_time_cycles);

  // One DMA trigger per MEM0 result. DMA-done wakes the task.
  DL_ADC12_setDMASamplesCnt(ADC0, samples_per_dma_trigger);
  DL_ADC12_enableDMATrigger(ADC0, DL_ADC12_DMA_MEM0_RESULT_LOADED);
  DL_ADC12_enableInterrupt(ADC0, DL_ADC12_INTERRUPT_DMA_DONE);
  NVIC_ClearPendingIRQ(ADC0_INT_IRQn);
  NVIC_EnableIRQ(ADC0_INT_IRQn);

  // Source stays on MEM0. Destination increments through the caller's buffer.
  DL_DMA_Config dma_config{};
  dma_config.transferMode = DL_DMA_SINGLE_TRANSFER_MODE;
  dma_config.extendedMode = DL_DMA_NORMAL_MODE;
  dma_config.destIncrement = DL_DMA_ADDR_INCREMENT;
  dma_config.srcIncrement = DL_DMA_ADDR_UNCHANGED;
  dma_config.destWidth = DL_DMA_WIDTH_HALF_WORD;
  dma_config.srcWidth = DL_DMA_WIDTH_HALF_WORD;
  dma_config.trigger = DMA_ADC0_EVT_GEN_BD_TRIG;
  dma_config.triggerType = DL_DMA_TRIGGER_TYPE_EXTERNAL;
  DL_DMA_initChannel(DMA, dma_channel, &dma_config);

  DL_DMA_setSrcAddr(
    DMA, dma_channel, DL_ADC12_getMemResultAddress(ADC0, DL_ADC12_MEM_IDX_0));

  // ENC locks the config registers, so this stays last.
  DL_ADC12_enableConversions(ADC0);
}

void msp0g_adc::driver_read(std::span<std::uint16_t> p_buffer)
{
  if (p_buffer.empty() || p_buffer.size() > max_samples_per_read) {
    return;
  }

  ulTaskNotifyTake(pdTRUE, 0);

  // The channel clears its own enable when a transfer finishes.
  DL_DMA_disableChannel(DMA, dma_channel);
  auto const destination =
    reinterpret_cast<std::uintptr_t>(p_buffer.data());
  DL_DMA_setDestAddr(DMA, dma_channel, static_cast<std::uint32_t>(destination));
  DL_DMA_setTransferSize(
    DMA, dma_channel, static_cast<std::uint16_t>(p_buffer.size()));
  DL_DMA_enableChannel(DMA, dma_channel);

  // Hardware clears ADC DMAEN on DMA done, so re-arm it every read.
  DL_ADC12_enableDMA(ADC0);

  waiting_task = xTaskGetCurrentTaskHandle();

  DL_ADC12_startConversion(ADC0);
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

  // Repeat mode keeps converting until this.
  DL_ADC12_stopConversion(ADC0);
  waiting_task = nullptr;

  for (auto& sample : p_buffer) {
    sample = upscale_12_to_16(sample);
  }
}

void msp0g_adc::handle_interrupt()
{
  // ADC0 shares one vector. Reading the index also clears that cause.
  if (DL_ADC12_getPendingInterrupt(ADC0) != DL_ADC12_IIDX_DMA_DONE) {
    return;
  }

  BaseType_t higher_priority_task_woken = pdFALSE;
  TaskHandle_t const task = waiting_task;
  if (task != nullptr) {
    vTaskNotifyGiveFromISR(task, &higher_priority_task_woken);
  }

  portYIELD_FROM_ISR(higher_priority_task_woken);
}

}  // namespace msp0g
