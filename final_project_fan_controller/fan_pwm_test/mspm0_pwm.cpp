#include "mspm0_pwm.hpp"

#include <ti/devices/msp/msp.h>

namespace fan_controller {

// Configure TIMA1 to generate PWM at the requested frequency.
mspm0_pwm::mspm0_pwm(std::uint32_t p_frequency_hz)
  : m_frequency_hz(0U),
    m_load_value(0U)
{
  // Default BUSCLK frequency is 32 MHz.
  constexpr std::uint32_t timer_clock_hz = 32'000'000U;

  // Reject frequencies outside the supported calculation range.
  if (p_frequency_hz == 0U ||
      p_frequency_hz > timer_clock_hz / 2U) {
    return;
  }

  // Calculate the timer period.
  std::uint32_t timer_counts =
    timer_clock_hz / p_frequency_hz;

  // TIMA1 has a 16-bit counter.
  // Reject periods that do not fit in the timer.
  if (timer_counts > 65536U) {
    return;
  }

  m_load_value = timer_counts - 1U;
  m_frequency_hz = timer_clock_hz / timer_counts;

  // Reset TIMA1.
  // 0xB1 is the reset key.
  TIMA1->GPRCM.RSTCTL = (0xB1U << 24) |
                        (1U << 1) |
                        (1U << 0);

  // Enable power to GPIOA without resetting its other pins.
  // 0x26 is the power key and bit 0 enables power.
  GPIOA->GPRCM.PWREN = (0x26U << 24) | (1U << 0);

  // Enable power to TIMA1.
  TIMA1->GPRCM.PWREN = (0x26U << 24) | (1U << 0);

  // Allow peripheral power to stabilize.
  // Wait at least 16 CPU cycles before configuring the timer.
  for (std::uint32_t i = 0U; i < 16U; ++i) {
    __NOP();
  }

  // Select BUSCLK as the timer clock.
  // Bit 3 selects BUSCLK.
  TIMA1->CLKSEL = (1U << 3);

  // Do not divide the timer clock.
  TIMA1->CLKDIV = 0U;

  // Do not apply an additional clock prescaler.
  TIMA1->COMMONREGS.CPS = 0U;

  // Set the timer period.
  TIMA1->COUNTERREGS.LOAD = m_load_value;

  // Configure continuous upward counting.
  // Bit 29: Start counting from zero when enabled.
  // Bit 5: Select upward counting.
  // Bit 1: Repeat continuously.
  TIMA1->COUNTERREGS.CTRCTL = (1U << 29) |
                              (1U << 5) |
                              (1U << 1);

  // Configure channel 0 for compare mode.
  // Bit 17 = 0 selects compare instead of capture.
  TIMA1->COUNTERREGS.CCCTL_01[0] = 0U;

  // Set the initial PWM output to LOW.
  TIMA1->COUNTERREGS.CCACT_01[0] = (1U << 1);

  // Start with zero duty cycle.
  TIMA1->COUNTERREGS.CC_01[0] = 0U;

  // Select the timer signal generator as the output source.
  TIMA1->COUNTERREGS.OCTL_01[0] = 0U;

  
  // Configure channel 0 as an output.
  TIMA1->COMMONREGS.CCPD = (1U << 0);

  // Route TIMA1 CCP0 to PA10.
  // Bits 2 and 0 select peripheral function 5.
  // Bit 7 connects the pin to the peripheral.
  IOMUX->SECCFG.PINCM[IOMUX_PINCM21] =
    (1U << 7) | (1U << 2) | (1U << 0);

  // Enable PA10 output.
  GPIOA->DOESET31_0 = (1U << 10);

  // Enable the timer clock.
  TIMA1->COMMONREGS.CCLKCTL = (1U << 0);

  // Start the timer counter.
  TIMA1->COUNTERREGS.CTRCTL |= (1U << 0);
}


// Return the configured PWM frequency.
std::uint32_t mspm0_pwm::driver_frequency()
{
  return m_frequency_hz;
}

// Update the PWM duty cycle using the class interface.
// The input range is 0 to 65535.
void mspm0_pwm::driver_duty_cycle(std::uint16_t p_duty_cycle)
{
  if (m_frequency_hz == 0U) {
    return;
  }

  // Handle 0% duty cycle.
  if (p_duty_cycle == 0U) {
    // Keep the output LOW at the zero event.
    TIMA1->COUNTERREGS.CCACT_01[0] = (1U << 1);
    TIMA1->COUNTERREGS.CC_01[0] = 0U;
    return;
  }

  // Handle 100% duty cycle.
  if (p_duty_cycle == 65535U) {
    // Keep the output HIGH at the zero event.
    // Disable the compare action so the output stays HIGH.
    TIMA1->COUNTERREGS.CCACT_01[0] = (1U << 0);
    TIMA1->COUNTERREGS.CC_01[0] = 0U;
    return;
  }

  // Convert the requested duty cycle into timer counts.
  std::uint32_t timer_counts = m_load_value + 1U;

  std::uint32_t compare_value =
    (static_cast<std::uint32_t>(p_duty_cycle) *
     timer_counts + 32767U) / 65535U;

  // Keep the compare event inside the timer period.
  if (compare_value == 0U) {
    compare_value = 1U;
  }

  if (compare_value >= timer_counts) {
    compare_value = timer_counts - 1U;
  }

  // Update the compare register.
  TIMA1->COUNTERREGS.CC_01[0] = compare_value;

  // Bit 0: Set output HIGH when the counter reaches zero.
  // Bit 10: Set output LOW when counting up reaches compare.
  TIMA1->COUNTERREGS.CCACT_01[0] =
    (1U << 0) | (1U << 10);
}

}  // namespace fan_controller
