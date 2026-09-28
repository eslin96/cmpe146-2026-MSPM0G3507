#include <array>
#include <cstddef>
#include <limits>
#include <cinttypes>
#include <cstdint>
#include <cstdio>

#include "../hal/gpio.hpp"
#include "../hal/timer.hpp"
#include "../hal/timer_util.hpp"

// Base addresses for the timers I am using.
// TIMG6 and TIMG8 are used for PWM.
// TIMG12 is used as my steady clock.
enum class timer_peripheral : std::uintptr_t
{
  timer_g6 = 0x4086'8000UL,
  timer_g8 = 0x4009'0000UL,
  timer_g12 = 0x4087'0000UL,
};

// The capture/compare channel used by each PWM output.
enum class timer_channel : std::uint8_t
{
  channel_0 = 0,
  channel_1,
};

// Stores the information needed to connect a timer PWM
// channel to one of the RGB LED pins.
struct pwm_pin
{
  timer_peripheral peripheral;
  timer_channel channel;
  std::uintptr_t pinmux_address;
  std::uint32_t mux_index;
};

// Red LED uses PB26 -> TIMG6 channel 0.
constexpr pwm_pin red_pin{
  timer_peripheral::timer_g6,
  timer_channel::channel_0,
  0x4042'80E4UL,
  5u
};

// Green LED uses PB27 -> TIMG6 channel 1.
constexpr pwm_pin green_pin{
  timer_peripheral::timer_g6,
  timer_channel::channel_1,
  0x4042'80E8UL,
  5u
};

// Blue LED uses PB22 -> TIMG8 channel 1.
constexpr pwm_pin blue_pin{
  timer_peripheral::timer_g8,
  timer_channel::channel_1,
  0x4042'80C8UL,
  3u
};

// Timer register offsets.
// The final register address is:
// timer base address + register offset.
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

// constexpr version of cosine so I can make the
// color lookup table at compile time.
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

// Creates a cosine lookup table with values from
// 0 to 65535, which matches the PWM duty cycle range.
template<std::size_t CycleSteps>
constexpr std::array<std::uint16_t, CycleSteps> generate_cosine_table()
{
  std::array<std::uint16_t, CycleSteps> samples{};

  constexpr auto max =
    std::numeric_limits<std::uint16_t>::max();

  constexpr double pi = 3.14159265358979323846;

  // Distance between each point in the cosine wave.
  constexpr double phase_step =
    (2.0 * pi) / CycleSteps;

  for (std::size_t x = 0; x < CycleSteps; x++) {

    // Cosine normally goes from -1 to 1.
    // This changes it to a range from 0 to 1.
    auto const y =
      (custom_cosine(phase_step * x) + 1.0) / 2.0;

    // Scale 0 to 1 into the PWM range 0 to 65535.
    samples[x] =
      static_cast<std::uint16_t>(y * max);
  }

  return samples;
}

// 628 samples gives me a smooth cosine wave for the RGB LED.
constexpr auto uint16_cosine2 =
  generate_cosine_table<628>();


// Steady clock using TIMG12.
class mspm0_steady_clock : public lab2::steady_clock
{
public:
  mspm0_steady_clock()
  {
    // Base address of TIMG12.
    auto const base =
      static_cast<std::uintptr_t>(timer_peripheral::timer_g12);

    // Get pointers to the timer registers I need.
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

    // Turn on power to TIMG12.
    // 0x26 is the key and bit 0 enables power.
    *power_enable_register = (0x26u << 24) | 1u;

    // Select BUSCLK as the timer clock source.
    *clock_select_register = (1u << 3);

    // BUSCLK is 32 MHz.
    // Divide by 8 to get a 4 MHz timer clock.
    *clock_divider_register = 7u;

    // Enable the timer clock.
    *counter_clock_register = 1u;

    // TIMG12 is 32-bit, so use the full counting range.
    *load_register = 0xFFFF'FFFFu;

    // Start at 0, count up, repeat, and enable the timer.
    *counter_control_register =
      (2u << 28) |
      (2u << 4) |
      (1u << 1) |
      1u;
  }

private:
  std::uint32_t driver_frequency() override
  {
    // 32 MHz / 8 = 4 MHz.
    return 4'000'000;
  }

  std::uint64_t driver_uptime() override
  {
    auto const base =
      static_cast<std::uintptr_t>(timer_peripheral::timer_g12);

    // Read the current timer count.
    auto counter_register =
      reinterpret_cast<volatile std::uint32_t*>(
        base + static_cast<std::uintptr_t>(timer_reg::ctr));

    return *counter_register;
  }
};


// PWM driver used for each RGB LED channel.
class mspm0_pwm : public lab2::pwm
{
public:
  mspm0_pwm(pwm_pin p_pin, std::uint32_t p_frequency)
    : m_base(static_cast<std::uintptr_t>(p_pin.peripheral)),
      m_channel(static_cast<std::uint32_t>(p_pin.channel))
  {
    // Timer clock after dividing BUSCLK by 8.
    constexpr std::uint32_t timer_frequency = 4'000'000;

    // Get pointers to the timer registers.
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

    // The CC registers are 4 bytes apart.
    // Channel 0 adds 0 bytes and channel 1 adds 4 bytes.
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

    // Turn on power to the timer.
    *power_enable_register = (0x26u << 24) | 1u;

    // Use BUSCLK as the timer clock.
    *clock_select_register = (1u << 3);

    // Divide 32 MHz by 8 to get 4 MHz.
    *clock_divider_register = 7u;

    // Do not divide the clock any more.
    *prescaler_register = 0u;

    // Enable the timer clock.
    *counter_clock_register = 1u;

    // Connect the timer output to the LED pin.
    *pinmux_register = (1u << 7) | p_pin.mux_index;

    // Set this capture/compare channel as an output.
    *ccp_direction_register |= (1u << m_channel);

    // Use compare mode for PWM.
    *ccctl_register = 0u;

    // Use the timer output signal.
    *octl_register = 0u;

    // Output goes HIGH at zero and LOW at the compare value.
    // This creates the PWM pulse.
    *ccact_register =
      (2u << 9) |
      1u;

    // Number of timer ticks needed for one PWM period.
    // Example: 4 MHz / 1 kHz = 4000 ticks.
    auto const period_ticks =
      timer_frequency / p_frequency;

    // LOAD controls the PWM period.
    *load_register = period_ticks - 1u;

    // Start the LED at about 50% duty cycle.
    *cc_register = *load_register / 2u;

    // Start at 0, count up, repeat, and enable.
    *counter_control_register =
      (2u << 28) |
      (2u << 4) |
      (1u << 1) |
      1u;

    // Save the actual PWM frequency.
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

    // Convert the 0 to 65535 duty cycle value into
    // the compare value used by the timer.
    *cc_register =
      (load * static_cast<std::uint32_t>(p_duty_cycle)) / 65535u;
  }

  // Save which timer and channel this PWM object uses.
  std::uintptr_t m_base;
  std::uint32_t m_channel;

  // Stores the PWM frequency.
  std::uint32_t m_frequency = 0;
};


// Fake clock that can be used for testing delay code
// without using the real hardware timer.
class fake_steady_clock : public lab2::steady_clock
{
public:
  fake_steady_clock() = default;

private:
  std::uint32_t driver_frequency() override
  {
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

  // Create the steady clock using TIMG12.
  mspm0_steady_clock clock;

  // Create a PWM output for each RGB LED color.
  // All three are running at 1 kHz.
  mspm0_pwm red_pwm(red_pin, 1'000);
  mspm0_pwm green_pwm(green_pin, 1'000);
  mspm0_pwm blue_pwm(blue_pin, 1'000);


  /*
  // Test used to make sure changing the duty cycle
  // actually changes the brightness of the blue LED.
  while (true) {
    blue_pwm.duty_cycle(8'000);      // dim
    lab2::delay(clock, 1s);

    blue_pwm.duty_cycle(32'767);     // about 50%
    lab2::delay(clock, 1s);

    blue_pwm.duty_cycle(50'000);     // brighter
    lab2::delay(clock, 1s);
  }
  */


  /*
  // Other blue LED duty cycle values I used for testing.
  blue_pwm.duty_cycle(32'767);       // about 50%
  // blue_pwm.duty_cycle(8'000);     // dim
  // blue_pwm.duty_cycle(50'000);    // bright
  // blue_pwm.duty_cycle(0);         // off
  // blue_pwm.duty_cycle(65'535);    // full brightness


  // Test used to make sure red, green, and blue
  // PWM channels were all connected correctly.
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
  }
  */


  // Shift each RGB color by 1/3 of the cosine table.
  // This gives about a 120 degree phase difference.
  constexpr std::size_t phase_shift =
    uint16_cosine2.size() / 3;

  while (true) {

    // Go through the whole cosine table.
    for (std::size_t i = 0;
         i < uint16_cosine2.size();
         i++) {

      // Red starts at the current position.
      auto const red =
        uint16_cosine2[i];

      // Green is shifted by 1/3 of the wave.
      auto const green =
        uint16_cosine2[
          (i + phase_shift) % uint16_cosine2.size()
        ];

      // Blue is shifted by 2/3 of the wave.
      auto const blue =
        uint16_cosine2[
          (i + (2 * phase_shift)) % uint16_cosine2.size()
        ];

      // Change the brightness of each RGB channel.
      red_pwm.duty_cycle(red);
      green_pwm.duty_cycle(green);
      blue_pwm.duty_cycle(blue);

      // Small delay so the color transition is smooth and visible.
      lab2::delay(clock, 5ms);
    }
  }
}