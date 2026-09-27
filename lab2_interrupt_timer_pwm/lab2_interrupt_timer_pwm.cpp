#include <array>
#include <cstddef>
#include <limits>
#include <cinttypes>
#include <cstdint>
#include <cstdio>

#include "../hal/gpio.hpp"
#include "../hal/timer.hpp"
#include "../hal/timer_util.hpp"

// Test double for lab2::steady_clock. Does not touch real hardware - it just
// increments a counter every time uptime() is called, as if a timer tick had
// elapsed. Useful for exercising delay logic before your real Timer_A-backed
// steady_clock exists.

enum class timer_peripheral : std::uintptr_t
{
  timer_g6 = 0x4086'8000UL,
  timer_g8 = 0x4009'0000UL,
  timer_g12 = 0x4087'0000UL,
};

enum class timer_channel : std::uint8_t
{
  channel_0 = 0,
  channel_1,
};

struct pwm_pin
{
  timer_peripheral peripheral;
  timer_channel channel;
  std::uintptr_t pinmux_address;
  std::uint32_t mux_index;
};

constexpr pwm_pin red_pin{
  timer_peripheral::timer_g6,
  timer_channel::channel_0,
  0x4042'80E4UL,
  5u
};

constexpr pwm_pin green_pin{
  timer_peripheral::timer_g6,
  timer_channel::channel_1,
  0x4042'80E8UL,
  5u
};

constexpr pwm_pin blue_pin{
  timer_peripheral::timer_g8,
  timer_channel::channel_1,
  0x4042'80C8UL,
  3u
};

enum class timer_reg : std::uintptr_t
{
  pwren   = 0x0800,
  clkdiv  = 0x1000,
  clksel  = 0x1008,

  ccpd    = 0x1100,
  cclkctl = 0x1108,
  cps     = 0x110C,

  ctr     = 0x1800,
  ctrctl  = 0x1804,
  load    = 0x1808,

  cc1     = 0x1814,
  ccctl1  = 0x1834,
  octl1   = 0x1854,
  ccact1  = 0x1874,

  cc0     = 0x1810,
  ccctl0  = 0x1830,
  octl0   = 0x1850,
  ccact0  = 0x1870,
};

constexpr double custom_cosine(double x)
{
  double cos{ 1 }, pow{ x };

  for (auto fac{ 1ull }, n{ 1ull }; n != 19; fac *= ++n, pow *= x) {
    if ((n & 1) == 0) {
      cos += (n & 2 ? -pow : pow) / fac;
    }
  }

  return cos;
}

template<std::size_t CycleSteps>
constexpr std::array<std::uint16_t, CycleSteps> generate_cosine_table()
{
  std::array<std::uint16_t, CycleSteps> samples{};

  constexpr auto max =
    std::numeric_limits<std::uint16_t>::max();

  constexpr double pi = 3.14159265358979323846;
  constexpr double phase_step =
    (2.0 * pi) / CycleSteps;

  for (std::size_t x = 0; x < CycleSteps; x++) {
    auto const y =
      (custom_cosine(phase_step * x) + 1.0) / 2.0;

    samples[x] =
      static_cast<std::uint16_t>(y * max);
  }

  return samples;
}

constexpr auto uint16_cosine2 =
  generate_cosine_table<628>();

class mspm0_steady_clock : public lab2::steady_clock
{
public:
  mspm0_steady_clock()
  {
    auto const base =
      static_cast<std::uintptr_t>(timer_peripheral::timer_g12);

    auto power_enable_register =
      reinterpret_cast<volatile std::uint32_t*>(
        base + static_cast<std::uintptr_t>(timer_reg::pwren));

    auto clock_divider_register =
      reinterpret_cast<volatile std::uint32_t*>(
        base + static_cast<std::uintptr_t>(timer_reg::clkdiv));

    auto clock_select_register =
      reinterpret_cast<volatile std::uint32_t*>(
        base + static_cast<std::uintptr_t>(timer_reg::clksel));

    auto counter_clock_register =
      reinterpret_cast<volatile std::uint32_t*>(
        base + static_cast<std::uintptr_t>(timer_reg::cclkctl));

    auto load_register =
      reinterpret_cast<volatile std::uint32_t*>(
        base + static_cast<std::uintptr_t>(timer_reg::load));

    auto counter_control_register =
      reinterpret_cast<volatile std::uint32_t*>(
        base + static_cast<std::uintptr_t>(timer_reg::ctrctl));

    // Enable power to TIMG12
    *power_enable_register = (0x26u << 24) | 1u;

    // Select BUSCLK
    *clock_select_register = (1u << 3);

    // Divide 32 MHz BUSCLK by 8 -> 4 MHz timer clock
    *clock_divider_register = 7u;

    // Enable the timer clock
    *counter_clock_register = 1u;

    // Let the 32-bit counter use its full range
    *load_register = 0xFFFF'FFFFu;

    // Start at zero, count up, repeat, and enable
    *counter_control_register =
      (2u << 28) |
      (2u << 4) |
      (1u << 1) |
      1u;
  }

private:
  std::uint32_t driver_frequency() override
  {
    return 4'000'000;
  }

  std::uint64_t driver_uptime() override
  {
    auto const base =
      static_cast<std::uintptr_t>(timer_peripheral::timer_g12);

    auto counter_register =
      reinterpret_cast<volatile std::uint32_t*>(
        base + static_cast<std::uintptr_t>(timer_reg::ctr));

    return *counter_register;
  }
};

class mspm0_pwm : public lab2::pwm
{
public:
  mspm0_pwm(pwm_pin p_pin, std::uint32_t p_frequency)
    : m_base(static_cast<std::uintptr_t>(p_pin.peripheral)),
      m_channel(static_cast<std::uint32_t>(p_pin.channel))
  {
    constexpr std::uint32_t timer_frequency = 4'000'000;

    auto power_enable_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base + static_cast<std::uintptr_t>(timer_reg::pwren));

    auto clock_divider_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base + static_cast<std::uintptr_t>(timer_reg::clkdiv));

    auto clock_select_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base + static_cast<std::uintptr_t>(timer_reg::clksel));

    auto prescaler_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base + static_cast<std::uintptr_t>(timer_reg::cps));

    auto counter_clock_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base + static_cast<std::uintptr_t>(timer_reg::cclkctl));

    auto load_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base + static_cast<std::uintptr_t>(timer_reg::load));

    auto counter_control_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base + static_cast<std::uintptr_t>(timer_reg::ctrctl));

    auto ccp_direction_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base + static_cast<std::uintptr_t>(timer_reg::ccpd));

    auto cc_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base +
        static_cast<std::uintptr_t>(timer_reg::cc0) +
        (m_channel * 4u));

    auto ccctl_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base +
        static_cast<std::uintptr_t>(timer_reg::ccctl0) +
        (m_channel * 4u));

    auto octl_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base +
        static_cast<std::uintptr_t>(timer_reg::octl0) +
        (m_channel * 4u));

    auto ccact_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base +
        static_cast<std::uintptr_t>(timer_reg::ccact0) +
        (m_channel * 4u));

    auto pinmux_register =
      reinterpret_cast<volatile std::uint32_t*>(
        p_pin.pinmux_address);

    // Enable timer power
    *power_enable_register = (0x26u << 24) | 1u;

    // Select BUSCLK
    *clock_select_register = (1u << 3);

    // 32 MHz / 8 = 4 MHz
    *clock_divider_register = 7u;

    // No additional prescaling
    *prescaler_register = 0u;

    // Enable timer clock
    *counter_clock_register = 1u;

    // Connect timer peripheral to the selected LED pin
    *pinmux_register = (1u << 7) | p_pin.mux_index;

    // Set this capture/compare channel as an output
    *ccp_direction_register |= (1u << m_channel);

    // Compare mode
    *ccctl_register = 0u;

    // Use timer output
    *octl_register = 0u;

    // HIGH at zero, LOW at compare
    *ccact_register =
      (2u << 9) |
      1u;

    // Calculate PWM period from requested frequency
    auto const period_ticks =
      timer_frequency / p_frequency;

    *load_register = period_ticks - 1u;

    // Start at 50%
    *cc_register = *load_register / 2u;

    // Start at zero, count up, repeat, enable
    *counter_control_register =
      (2u << 28) |
      (2u << 4) |
      (1u << 1) |
      1u;

    m_frequency = timer_frequency / period_ticks;
  }

private:
  std::uint32_t driver_frequency() override
  {
    return m_frequency;
  }

  void driver_duty_cycle(std::uint16_t p_duty_cycle) override
  {
    auto load_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base + static_cast<std::uintptr_t>(timer_reg::load));

    auto cc_register =
      reinterpret_cast<volatile std::uint32_t*>(
        m_base +
        static_cast<std::uintptr_t>(timer_reg::cc0) +
        (m_channel * 4u));

    auto const load =
      static_cast<std::uint32_t>(*load_register);

    *cc_register =
      (load * static_cast<std::uint32_t>(p_duty_cycle)) / 65535u;
  }

  std::uintptr_t m_base;
  std::uint32_t m_channel;
  std::uint32_t m_frequency = 0;
};

class fake_steady_clock : public lab2::steady_clock
{
public:
  fake_steady_clock() = default;

private:
  std::uint32_t driver_frequency() override
  {
    // Use a small frequency to reduce the amount of ticks needed
    // for the delay function.
    return 1'000'000;
  }

  std::uint64_t driver_uptime() override
  {
    return m_count++;
  }

  std::uint64_t m_count = 0;
};

int main()
{
  using namespace std::chrono_literals;

  std::printf("Hello, World\n");

  // TODO(lab2, step 1): Implement lab2::steady_clock

  // TODO(lab2, step 2): Pass your steady clock to lab2::delay() from
  // hal/timer_util.hpp and test it with printf or blinking an LED - your
  // choice. Put a printf on either side of the delay and confirm the gap
  // between them matches the duration you asked for. If a 1s delay is not
  // taking 1 second, your frequency() is wrong.

  // TODO(lab2, step3): Implement lab2::pwm using what you learned from
  // lab2::steady_clock

  // TODO(lab2, step4): Test against an LED and see if you can control the
  // brightness

  // fake_steady_clock clock;

  mspm0_steady_clock clock;

  mspm0_pwm red_pwm(red_pin, 1'000);
  mspm0_pwm green_pwm(green_pin, 1'000);
  mspm0_pwm blue_pwm(blue_pin, 1'000);

  /*
  while (true) {
    blue_pwm.duty_cycle(8'000);
    lab2::delay(clock, 1s);

    blue_pwm.duty_cycle(32'767);
    lab2::delay(clock, 1s);

    blue_pwm.duty_cycle(50'000);
    lab2::delay(clock, 1s);
  }
  */

  /*blue_pwm.duty_cycle(32'767);

  // blue_pwm.duty_cycle(8'000);
  // blue_pwm.duty_cycle(50'000);
  // blue_pwm.duty_cycle(0);       // off
  // blue_pwm.duty_cycle(65'535);  // full brightness

  while (true) {
    red_pwm.duty_cycle(50'000);
    green_pwm.duty_cycle(0);
    blue_pwm.duty_cycle(0);
    lab2::delay(clock, 1s);

    red_pwm.duty_cycle(0);
    green_pwm.duty_cycle(50'000);
    blue_pwm.duty_cycle(0);
    lab2::delay(clock, 1s);

    red_pwm.duty_cycle(0);
    green_pwm.duty_cycle(0);
    blue_pwm.duty_cycle(50'000);
    lab2::delay(clock, 1s);
  }*/

  constexpr std::size_t phase_shift =
  uint16_cosine2.size() / 3;

  while (true) {
    for (std::size_t i = 0;
        i < uint16_cosine2.size();
        i++) {

      auto const red =
        uint16_cosine2[i];

      auto const green =
        uint16_cosine2[
          (i + phase_shift) % uint16_cosine2.size()
        ];

      auto const blue =
        uint16_cosine2[
          (i + (2 * phase_shift)) % uint16_cosine2.size()
        ];

      red_pwm.duty_cycle(red);
      green_pwm.duty_cycle(green);
      blue_pwm.duty_cycle(blue);

      lab2::delay(clock, 5ms);
    }
  }
}