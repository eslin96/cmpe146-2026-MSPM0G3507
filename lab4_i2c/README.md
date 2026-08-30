# Lab 4 - I2C IMU

For this lab, you will:

1. Implement an I2C driver from scratch.
2. Use it to talk to an I2C accelerometer/IMU chip.
3. Use the accelerometer to determine the orientation of the chip

## Use cases

Consider this a component of a great product. Such products where this
technology would be used is:

- Smart phone orientation detection
- Drone orientation and balance
- Auto leveling robotics
- Level measurement device

## Overview

Write an I2C driver against the `i2c` interface (`i2c.hpp`), then use it to
implement the `accelerometer` interface (`accelerometer.hpp`) for the BMI160:

1. Read the X, Y, and Z acceleration values from the IMU.
2. Compute roll and pitch angles from the acceleration vector.
3. Continuously print the roll and pitch values to stdout.

Each of these should be its own class function (or set of class functions)
added to the `bmi160` driver within the `lab4_i2c.cpp` file.

DriverLib is allowed for this lab's I2C driver implementation — this is your
first chance to use a vendor-supplied API instead of writing register-level
code, so use it to get comfortable working with a vendor SDK to implement an
interface.

## Learning Objectives

- Implement an I2C driver, with SDA/SCL signal generation and device
  read/write, using DriverLib.
- Talk to a real I2C device (accelerometer/IMU) using your own driver.
- Convert raw accelerometer readings into roll and pitch angles.

## Hardware

Board: [LP-MSPM0G3507 LaunchPad](https://www.ti.com/tool/LP-MSPM0G3507).
Which I2C instance/pins you use is up to you - check the datasheet's
pin-to-peripheral function table for which pins carry I2C0/I2C1 signals.

IMU part: [Bosch BMI160](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi160-ds000.pdf)
6-axis accelerometer/gyroscope. This is the chip that will be provided to you
for this lab.

## Grading Rubric (60 pt)

1. **I2C implementation - 20 pts.** Produces correct SDA/SCL signals at the
   configured clock rate and can read from and write to a device on the bus.
2. **Accelerometer implementation - 20 pts.** Implements the `accelerometer`
   interface for the BMI160 and returns acceleration values for the X, Y, and
   Z axes.
3. **Roll and pitch - 10 pts.** Continuously computes and prints roll and
   pitch from the acceleration values.
4. **Code submission - 10 pts.** Submit a PR to your git repo targeting your
   `main` branch.

## Reference material

- **[MSPM0G3507 datasheet](https://www.ti.com/lit/ds/symlink/mspm0g3507.pdf)** -
  pin-to-peripheral function table for I2C pins.
- **[MSPM0G350x Technical Reference Manual](https://www.ti.com/lit/ug/slau846e/slau846e.pdf)** -
  I2C chapter.
- **[BMI160 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi160-ds000.pdf)** -
  command/power-mode, configuration, and acceleration output registers.
