#include <cstdio>
#include <cstdint>

#include "../hal/gpio.hpp"

namespace {

  constexpr std::uintptr_t io_mux_addr = 0x40428000;
  
  constexpr std::uintptr_t gpio_a_addr = 0x400A0000;
  constexpr std::uintptr_t gpio_b_addr = 0x400A2000;

  constexpr std::uintptr_t power_enable_offset = 0x800;
  constexpr std::uintptr_t doe_offset = 0x12C0;
  constexpr std::uintptr_t dout_offset = 0x1280;
  constexpr std::uintptr_t din_offset = 0x1380;

  std::uint32_t volatile* io_mux_array = 
    reinterpret_cast<std::uint32_t volatile*>(io_mux_addr);

  constexpr std::uint32_t periph_conn_bit = 7;
  constexpr std::uint32_t pull_up_bit = 17;
  constexpr std::uint32_t pull_down_bit = 16;
  constexpr std::uint32_t input_enable_bit = 18;

  constexpr std::uint8_t pin_map[2][32] = {
      /* port A [0] */
      {
          /* pins of port A from 0 to 31 */
          1,  2,  7,  8,  9,  10, 11, 14, 19, 20, 21, 22, 34, 35, 36, 37,
          38, 39, 40, 41, 42, 46, 47, 53, 54, 55, 59, 60, 3,  4,  5,  6,
      },
      /* port B [1] */
      {
          /* pins of port B from 0 to 31 */
          12, 13, 15, 16, 17, 18, 23, 24, 25, 26, 27, 28, 29, 30,
          31, 32, 33, 43, 44, 45, 48, 49, 50, 51, 52, 56, 57, 58,
      },
  };

  std::uint8_t get_pincm_index(std::uint8_t port, std::uint8_t pin) {

    std::uint8_t pincm_index = 0;
    if (port == 'A'){
      pincm_index = pin_map[0][pin];
    } else if (port == 'B') {
      pincm_index = pin_map[1][pin];
    }

    return pincm_index;
  }

class mspm0_output_pin : public lab1::output_pin{

  public:

  mspm0_output_pin(std::uint8_t p_port, std::uint8_t p_pin) : port(p_port), pin(p_pin) {
  }

  private:

  bool driver_configure(lab1::output_pin::settings const& p_settings) override {
    
    std::uint8_t pincm_index = get_pincm_index(port, pin);
    std::uintptr_t gpio_addr =0;

    if (port == 'A') {
    gpio_addr = gpio_a_addr;
    }else if (port == 'B'){
    gpio_addr = gpio_b_addr;
    }
   
    std::uintptr_t power_enable_addr = gpio_addr + power_enable_offset;
    std::uint32_t volatile* power_enable_register = 
      reinterpret_cast<std::uint32_t volatile*>(power_enable_addr);
    
    *power_enable_register = (0x26 << 24) | 1;

    std::uintptr_t doe_addr = gpio_addr + doe_offset;
    std::uint32_t volatile* doe_register =
      reinterpret_cast<std::uint32_t volatile*>(doe_addr);

    //io_mux_array[pincm_index] = 1 << periph_conn_bit | 1;
    io_mux_array[pincm_index] = 1 << input_enable_bit | 1 << periph_conn_bit |1;

    *doe_register |= 1 << pin;

    return true;
  }

  void driver_level(bool p_high) override { 
    
    std::uintptr_t gpio_addr = 0;

    if (port == 'A') {
    gpio_addr = gpio_a_addr;
    }else if (port == 'B') {
    gpio_addr = gpio_b_addr;
    }

    std::uintptr_t dout_addr = gpio_addr + dout_offset;
    std::uint32_t volatile* dout_register = 
      reinterpret_cast<std::uint32_t volatile*>(dout_addr);

    if (p_high) {
    *dout_register |= 1 << pin; //set bit
    }else {
    *dout_register &= ~(1 << pin); //clear bit
    }
  }

  bool driver_level() override {

    std::uintptr_t gpio_addr = 0; 

    if (port == 'A') {
    gpio_addr = gpio_a_addr;
    }else if (port == 'B') {
    gpio_addr = gpio_b_addr;
    }

    std::uintptr_t din_addr = gpio_addr + din_offset;
    std::uint32_t volatile* din_register = 
      reinterpret_cast<std::uint32_t volatile*>(din_addr);

    return (*din_register >> pin) & 1;
  }

  std::uint8_t port;
  std::uint8_t pin;
};

class mspm0_input_pin : public lab1::input_pin
{
public:
  mspm0_input_pin(std::uint8_t p_port, std::uint8_t p_pin)
      : port(p_port), pin(p_pin)
  {
  }

private:
  bool driver_configure(lab1::input_pin::settings const& p_settings) override
  {
    std::uint8_t pincm_index = get_pincm_index(port, pin);
    std::uintptr_t gpio_addr = 0;

    if (port =='A') {
    gpio_addr = gpio_a_addr;
    }else if (port == 'B') {
    gpio_addr = gpio_b_addr;
    }
  std::uintptr_t power_enable_addr = gpio_addr + power_enable_offset;
  std::uint32_t volatile* power_enable_register =
    reinterpret_cast<std::uint32_t volatile*>(power_enable_addr);

  *power_enable_register = (0x26 << 24) | 1;

  std::uintptr_t doe_addr = gpio_addr + doe_offset;
  std::uint32_t volatile* doe_register = 
    reinterpret_cast<std::uint32_t volatile*>(doe_addr);
  
  *doe_register &= ~(1 << pin);

  std::uint32_t pin_config = 
    1 << input_enable_bit | 1 << periph_conn_bit | 1;

  if (p_settings.resistor == lab1::pin_resistor::pull_up) {
    pin_config |= 1 <<pull_up_bit;
  }else if (p_settings.resistor == lab1::pin_resistor::pull_down) {
    pin_config |= 1 << pull_down_bit;
  }

  io_mux_array[pincm_index] = pin_config;

    return true;
  }

  bool driver_level() override
  {

     std::uintptr_t gpio_addr = 0; 

    if (port == 'A') {
    gpio_addr = gpio_a_addr;
    }else if (port == 'B') {
    gpio_addr = gpio_b_addr;
    }

    std::uintptr_t din_addr = gpio_addr + din_offset;
    std::uint32_t volatile* din_register = 
      reinterpret_cast<std::uint32_t volatile*>(din_addr);

    return (*din_register >> pin) & 1;
  
  }

  std::uint8_t port;
  std::uint8_t pin;
};

} //namespace

int main()
{
  std::printf("Hello, World\n");

  mspm0_output_pin red('B', 26);
  mspm0_output_pin green('B', 27);
  mspm0_output_pin blue ('B',22);

  lab1::output_pin::settings settings;

  red.configure(settings);
  green.configure(settings);
  blue.configure(settings);

  //red.level(true);
  //red.level(false);
  //bool red_state = red.level();
  //std::printf("Red state: %d\n", red_state);

  mspm0_input_pin s1('A', 18);

  lab1::input_pin::settings s1_settings;
  s1_settings.resistor = lab1::pin_resistor::pull_down;

  s1.configure(s1_settings);

  mspm0_input_pin s2('B', 21);

  lab1::input_pin::settings s2_settings;
  s2_settings.resistor = lab1::pin_resistor::pull_up;

  s2.configure(s2_settings);
  
  // TODO(lab1, step 2): Configure the RGB LED pins as outputs and the two
  // push-buttons (S1, S2) as inputs using the gpio:: driver you write in
  // gpio.cpp. See README.md for pin assignments and reference material.

  // TIP: the switches need pull resistor. See the user schematic of the launch
  // pad for which is needed for S1 and S2.

  while (true) {
    // TODO(lab1, step 2): implement the button -> LED color behavior
    // described in README.md
    //red.level(s1.level());
    //red.level(!s2.level());

    bool s1_pressed = s1.level();
    bool s2_pressed = !s2.level();

    red.level(false);
    green.level(false);
    blue.level(false);

    if (s1_pressed && s2_pressed) {
    blue.level(true);
    red.level(true);
    green.level(true);
    }
  }
}