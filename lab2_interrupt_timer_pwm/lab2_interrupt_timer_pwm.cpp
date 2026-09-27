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
  timer_g12 = 0x4087'0000UL,
};

enum class timer_reg : std::uintptr_t
{
  pwren   = 0x0800,
  clkdiv  = 0x1000,
  clksel  = 0x1008,
  cclkctl = 0x1108,
  ctr     = 0x1800,
  ctrctl  = 0x1804,
  load    = 0x1808,
};

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
  //fake_steady_clock clock;

  mspm0_steady_clock clock;
  while (true) {

    lab2::delay(clock, 1s);
    std::printf("Sleep 1\n");
    lab2::delay(clock, 1s);
    std::printf("Sleep 2\n");
    lab2::delay(clock, 1s);
    std::printf("Sleep 3\n");
    // TODO(lab2, step 5): Use the steady clock together with your PWM driver to
    // animate the RGB LED as a continuous color wheel, as described in
    // README.md.
  }
}
