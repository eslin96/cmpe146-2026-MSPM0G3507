#pragma once

namespace lab4 {

/**
 * @brief Acceleration sensing hardware abstraction interface.
 *
 */
class accelerometer
{
public:
  /**
   * @brief Result from reading the accelerometer.
   *
   */
  struct read_t
  {
    /// Acceleration in the X axis, relative to the device's reference frame,
    /// in units of standard gravity (g).
    float x;
    /// Acceleration in the Y axis, relative to the device's reference frame,
    /// in units of standard gravity (g).
    float y;
    /// Acceleration in the Z axis, relative to the device's reference frame,
    /// in units of standard gravity (g).
    float z;
  };

  /**
   * @brief Read the latest acceleration sensed by the device
   *
   * @return read_t - acceleration data
   */
  [[nodiscard]] read_t read()
  {
    return driver_read();
  }

private:
  virtual read_t driver_read() = 0;
};
}  // namespace lab4
