#include <cstdint>
#include <cstdio>

#include "../hal/accelerometer.hpp"
#include "../hal/i2c.hpp"

// TODO(lab4, step 2): Make this class inherit from and implement the
// `lab4::accelerometer` interface (`accelerometer.hpp`). Note that the
// BMI160 boots into a low-power suspend mode, so before its acceleration
// registers hold live data you must write a "set accelerometer to normal
// power mode" command to its command register, and you may also want to set
// its output data rate/range via its accelerometer configuration register.
// See the BMI160 datasheet's register map (command, power mode, and
// acceleration data registers).
class bmi160
{
  // Fill this out...
};

int main()
{
  std::printf("Hello, World\n");

  // TODO(lab4, step 3): Construct an i2c driver object and use it to
  // construct a bmi160 object.

  while (true) {
    // TODO(lab4, step 3): Read the X, Y, Z acceleration values from the
    // bmi160 object, compute roll and pitch, and print them to stdout. This
    // runs every loop iteration so roll and pitch are reported continuously.
  }
}
