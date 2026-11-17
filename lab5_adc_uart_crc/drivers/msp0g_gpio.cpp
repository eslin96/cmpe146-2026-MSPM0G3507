#include <cstdint>
#include <cstdlib>

#include "msp0g_gpio.hpp"

namespace {

constexpr std::uintptr_t gpio_a_base = 0x400A'0000;
constexpr std::uintptr_t gpio_b_base = 0x400A'2000;

constexpr std::uintptr_t pwren_offset = 0x0800;
constexpr std::uintptr_t rstctl_offset = 0x804;
constexpr std::uintptr_t doutset_offset = 0x1290;  // doutset31_0
constexpr std::uintptr_t doutclr_offset = 0x12A0;  // doutclr31_0
constexpr std::uintptr_t doeset_offset = 0x12D0;   // doeset31_0
constexpr std::uintptr_t doeclr_offset = 0x12E0;   // doeclr31_0
constexpr std::uintptr_t din_offset = 0x1380;      // din31_0

constexpr std::uint32_t pwren_mask = 0x2600'0001;  // unlock key and enable bit
constexpr std::uint32_t pwren_enable_mask = 0x1;   // readable enable bit only
constexpr std::uint32_t rstctl_mask = 0xB100'0001;

constexpr std::uint8_t pf_gpio = 0x01;

/**
 * @brief Form a volatile pointer to a GPIO register.
 * @param p_port GPIO port containing the register.
 * @param p_offset Register byte offset from the selected port's base address.
 * @return Pointer to the selected 32-bit memory-mapped register.
 */
volatile std::uint32_t* gpio_reg(msp0g::gpio_port p_port,
                                 std::uintptr_t p_offset)
{

  std::uintptr_t base;

  if (p_port == msp0g::gpio_port::a) {

    base = gpio_a_base;

  } else {

    base = gpio_b_base;
  }
  // NOLINTNEXTLINE(performance-no-int-to-ptr): MMIO register access
  return reinterpret_cast<volatile std::uint32_t*>(base + p_offset);
}

/**
 * @brief Check whether a pin number is available on the selected GPIO port.
 * @param p_port GPIO port A or B.
 * @param p_pin Port-relative pin number to check.
 * @return True for PA0-PA31 or PB0-PB27; false otherwise.
 */
bool valid_pin(msp0g::gpio_port p_port, std::uint8_t p_pin)
{

  if (p_port == msp0g::gpio_port::a) {
    return (p_pin <= 31);
  }

  return (p_pin <= 27);
}

/**
 * @brief Power and reset a GPIO port only when it is not already powered.
 *
 * Reading PWREN avoids a software initialization flag and prevents a later
 * GPIO object from resetting configuration already applied to the same port.
 *
 * @param p_port GPIO port to initialize.
 */
void initialize_gpio_port(msp0g::gpio_port p_port)
{

  auto* rstctl = gpio_reg(p_port, rstctl_offset);
  auto* pwren = gpio_reg(p_port, pwren_offset);

  if ((*pwren & pwren_enable_mask) != 0U) {

    return;
  }

  *rstctl = rstctl_mask;
  *pwren = pwren_mask;

  for (volatile int i = 0; i < 16; i++)
    ;  // required delay after setting PWREN
}

}  // namespace

namespace msp0g {

msp0g_outpin::msp0g_outpin(const gpio_pin& p_gpio_pin,
                           const settings& p_settings)
  : m_pincm({ p_gpio_pin.pincm, pf_gpio, input_enable::enable },
            { p_settings.resistor, p_settings.open_drain })
  , m_port(p_gpio_pin.port)
{

  bool valid = valid_pin(m_port, p_gpio_pin.pin);

  if (!valid) {
    std::abort();
  }

  initialize_gpio_port(m_port);

  m_pin_mask = 1U << p_gpio_pin.pin;

  configure(p_settings);

  auto* doeset = gpio_reg(m_port, doeset_offset);

  *doeset = m_pin_mask;
}

bool msp0g_outpin::driver_configure(const settings& p_settings)
{

  m_pincm.config({ p_settings.resistor, p_settings.open_drain });

  return true;  // all settings are supported for a constructed output pin
}

void msp0g_outpin::driver_level(bool p_high)
{

  if (p_high) {

    auto* doutset = gpio_reg(m_port, doutset_offset);

    *doutset = m_pin_mask;

  } else {

    auto* doutclr = gpio_reg(m_port, doutclr_offset);

    *doutclr = m_pin_mask;
  }
}

bool msp0g_outpin::driver_level()
{

  auto* din = gpio_reg(m_port, din_offset);

  return (*din & m_pin_mask);
}

msp0g_inpin::msp0g_inpin(const gpio_pin& p_gpio_pin, const settings& p_settings)
  : m_pincm({ p_gpio_pin.pincm, pf_gpio, input_enable::enable },
            { p_settings.resistor })
  , m_port(p_gpio_pin.port)
{

  bool valid = valid_pin(m_port, p_gpio_pin.pin);

  if (!valid) {
    std::abort();
  }

  initialize_gpio_port(m_port);

  m_pin_mask = 1U << p_gpio_pin.pin;

  configure(p_settings);

  auto* doeclr = gpio_reg(m_port, doeclr_offset);

  *doeclr = m_pin_mask;
}

bool msp0g_inpin::driver_configure(const settings& p_settings)
{

  m_pincm.config({ p_settings.resistor });

  return true;  // all settings are supported for a constructed input pin
}

bool msp0g_inpin::driver_level()
{

  auto* din = gpio_reg(m_port, din_offset);

  return (*din & m_pin_mask);
}

}  // namespace msp0g
