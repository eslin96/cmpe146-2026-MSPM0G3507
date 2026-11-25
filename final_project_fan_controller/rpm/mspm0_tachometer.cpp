#include "mspm0_tachometer.hpp"

#include <ti/driverlib/driverlib.h>

namespace fan_controller {

mspm0_tachometer::mspm0_tachometer(mspm0_tachometer_config p_config)
  : m_config(p_config)
  , m_pulse_count(0U)
  , m_ready(false)
{
}

mspm0_tachometer_config mspm0_tachometer::launchpad_pa14(
  std::uint32_t p_pulses_per_revolution)
{
  return {
    GPIOA,
    DL_GPIO_PIN_14,
    IOMUX_PINCM36,
    DL_GPIO_PIN_14_EDGE_FALL,
    GPIOA_INT_IRQn,
    p_pulses_per_revolution
  };
}

bool mspm0_tachometer::initialize()
{
  if (m_config.port == nullptr || m_config.pin == 0U ||
      (m_config.pin & (m_config.pin - 1U)) != 0U ||
      m_config.pulses_per_revolution == 0U) {
    return false;
  }

  // The PC-fan tachometer is normally an open-collector output. The
  // pull-up makes the released state HIGH and each tach pulse a FALLING edge.
  DL_GPIO_initDigitalInputFeatures(
    m_config.iomux,
    DL_GPIO_INVERSION_DISABLE,
    DL_GPIO_RESISTOR_PULL_UP,
    DL_GPIO_HYSTERESIS_DISABLE,
    DL_GPIO_WAKEUP_DISABLE);

  // GPIO pins 0-15 and 16-31 use separate edge-polarity registers. Preserve
  // the other pins because the same GPIO port may also own a button interrupt.
  std::uint32_t pin_number = 0U;
  std::uint32_t pin_mask = m_config.pin;
  while ((pin_mask & 1U) == 0U) {
    pin_mask >>= 1U;
    ++pin_number;
  }

  std::uint32_t const local_pin_number =
    pin_number < 16U ? pin_number : pin_number - 16U;
  std::uint32_t const polarity_mask =
    0x3U << (2U * local_pin_number);

  if (pin_number < 16U) {
    std::uint32_t polarity =
      DL_GPIO_getLowerPinsPolarity(m_config.port);
    polarity = (polarity & ~polarity_mask) |
               (m_config.edge_polarity & polarity_mask);
    DL_GPIO_setLowerPinsPolarity(m_config.port, polarity);
  } else {
    std::uint32_t polarity =
      DL_GPIO_getUpperPinsPolarity(m_config.port);
    polarity = (polarity & ~polarity_mask) |
               (m_config.edge_polarity & polarity_mask);
    DL_GPIO_setUpperPinsPolarity(m_config.port, polarity);
  }

  m_pulse_count = 0U;
  m_ready = true;

  DL_GPIO_clearInterruptStatus(m_config.port, m_config.pin);
  DL_GPIO_enableInterrupt(m_config.port, m_config.pin);

  NVIC_ClearPendingIRQ(m_config.interrupt_number);
  NVIC_EnableIRQ(m_config.interrupt_number);

  return true;
}

bool mspm0_tachometer::ready() const
{
  return m_ready;
}

bool mspm0_tachometer::handle_gpio_interrupt()
{
  if (!m_ready) {
    return false;
  }

  std::uint32_t const pending =
    DL_GPIO_getEnabledInterruptStatus(m_config.port, m_config.pin);

  if ((pending & m_config.pin) == 0U) {
    return false;
  }

  DL_GPIO_clearInterruptStatus(m_config.port, m_config.pin);
  ++m_pulse_count;
  return true;
}

std::uint32_t mspm0_tachometer::take_pulse_count()
{
  // A pulse can arrive while the task is copying and clearing the counter.
  // Temporarily masking interrupts makes the snapshot and clear one atomic
  // operation from the tachometer ISR's point of view.
  std::uint32_t const saved_interrupt_mask = __get_PRIMASK();
  __disable_irq();

  std::uint32_t const pulse_count = m_pulse_count;
  m_pulse_count = 0U;

  if (saved_interrupt_mask == 0U) {
    __enable_irq();
  }

  return pulse_count;
}

rpm_reading mspm0_tachometer::sample(std::uint32_t p_window_ms)
{
  if (!m_ready) {
    return { 0U, false };
  }

  return calculate_rpm(take_pulse_count(),
                       m_config.pulses_per_revolution,
                       p_window_ms);
}

}  // namespace fan_controller
