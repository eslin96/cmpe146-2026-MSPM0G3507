#include <cstdio>

#include "../hal/gpio.hpp"

int main()
{
  std::printf("Hello, World\n");

  // TODO(lab1, step 2): Configure the RGB LED pins as outputs and the two
  // push-buttons (S1, S2) as inputs using the gpio:: driver you write in
  // gpio.cpp. See README.md for pin assignments and reference material.

  // TIP: the switches need pull resistor. See the user schematic of the launch
  // pad for which is needed for S1 and S2.

  while (true) {
    // TODO(lab1, step 2): implement the button -> LED color behavior
    // described in README.md
  }
}
