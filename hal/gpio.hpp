#pragma once

#include <cstdint>

namespace lab1 {
/**
 * @brief Set of possible pin mode resistor settings.
 *
 * See each enumeration to get more details about when and how these should be
 * used.
 *
 */
enum class pin_resistor : uint8_t
{
  /// No pull up. This will cause the pin to float. This may be desirable if the
  /// pin has an external resistor attached or if the signal is sensitive to
  /// external devices like resistors.
  none = 0,
  /// Pull the pin down to devices GND. This will ensure that the voltage read
  /// by the pin when there is no signal on the pin is LOW (or false).
  pull_down,
  /// See pull down explanation, but in this case the pin is pulled up to VCC,
  /// also called VDD on some systems.
  pull_up,
};

/**
 * @brief Digital input pin hardware abstraction interface.
 *
 * Use this to read a pin and determine if the voltage on it is HIGH or LOW.
 *
 */
class input_pin
{
public:
  /// Generic settings for input pins
  struct settings
  {
    /// Pull resistor for an input pin
    pin_resistor resistor = pin_resistor::pull_up;
  };

  /**
   * @brief Configure the input pin to match the settings supplied
   *
   * @param p_settings - settings to apply to input pin
   *
   * @return true - if the settings are valid
   * @return false - if the settings could not be achieved
   */
  bool configure(settings const& p_settings)
  {
    return driver_configure(p_settings);
  }

  /**
   * @brief Read the state of the input pin
   *
   * @return bool - true indicates HIGH voltage level and false
   * indicates LOW voltage level
   */
  [[nodiscard]] bool level()
  {
    return driver_level();
  }

private:
  virtual bool driver_configure(settings const& p_settings) = 0;
  virtual bool driver_level() = 0;
};

/**
 * @brief Digital output pin hardware abstraction.
 *
 * Use this to drive a pin HIGH or LOW in order to send a control signal or turn
 * off or on an LED.
 *
 */
class output_pin
{
public:
  /// Generic settings for output pins
  struct settings
  {
    /// Pull resistor for the pin. This generally only helpful when open
    /// drain is enabled.
    pin_resistor resistor = pin_resistor::none;

    /// Starting level of the output pin. HIGH voltage defined as true and LOW
    /// voltage defined as false.
    bool open_drain = false;
  };

  /**
   * @brief Configure the output pin to match the settings supplied
   *
   * @param p_settings - settings to apply to output pin
   *
   * @return true - if the settings are valid
   * @return false - if the settings could not be achieved
   */
  bool configure(settings const& p_settings)
  {
    return driver_configure(p_settings);
  }

  /**
   * @brief Set the state of the pin
   *
   * @param p_high - if true then the pin state is set to HIGH voltage. If
   * false, the pin state is set to LOW voltage.
   */
  void level(bool p_high)
  {
    driver_level(p_high);
  }

  /**
   * @brief Read the current state of the output pin from hardware
   *
   * Implementations must read the pin state from hardware and will not simply
   * cache the results from the execution of `level(bool)`.
   *
   * This pin may not equal the state set by `level(bool)` when the pin is
   * configured as open-drain.
   *
   * @return true - if the level of the pin is HIGH
   * @return false - if the level of the pin is LOW
   */
  [[nodiscard]] bool level()
  {
    return driver_level();
  }

private:
  virtual bool driver_configure(settings const& p_settings) = 0;
  virtual void driver_level(bool p_high) = 0;
  virtual bool driver_level() = 0;
};

/**
 * @brief Callback interface for `edge_triggered_interrupt`.
 *
 * Implement this interface and pass a pointer to it to
 * `edge_triggered_interrupt::on_trigger()` to be notified when the pin's
 * configured trigger condition occurs.
 *
 */
struct edge_triggered_callback
{
  /**
   * @brief Invoked after the pin's configured trigger edge has occurred
   *
   * @param p_state - true if the pin's state was HIGH when the interrupt was
   * triggered, false if it was LOW.
   */
  virtual void callback(bool p_state) = 0;
};

/**
 * @brief Digital interrupt pin hardware abstraction.
 *
 * Use this to automatically call a function when a pin's state transitions.
 *
 * The transition states are:
 *
 *   - falling edge: the pin reads a transition from HIGH to LOW
 *   - rising edge: the pin reads a transition from LOW to HIGH
 *   - both: the pin reads any state change
 *
 */
class edge_triggered_interrupt
{
public:
  /**
   * @brief The condition in which an interrupt is triggered.
   *
   */
  enum class trigger_edge : uint8_t
  {
    /// Trigger the interrupt when a pin transitions from HIGH to LOW.
    falling = 0,
    /// Trigger the interrupt when a pin transitions from LOW to HIGH.
    rising,
    /// Trigger the interrupt when a pin transitions state in either direction.
    both,
  };

  /// Generic settings for interrupt pins
  struct settings
  {
    /// Pull resistor for the interrupt pin. It is highly advised to set this
    /// to something other than `none`, or to attach an external pull
    /// resistor, in order to prevent spurious interrupts from firing.
    pin_resistor resistor = pin_resistor::pull_up;

    /// The trigger condition that will invoke the callback.
    trigger_edge trigger = trigger_edge::rising;
  };

  /**
   * @brief Configure the interrupt pin to match the settings supplied
   *
   * @param p_settings - settings to apply to interrupt pin
   * @return true - if the settings are valid
   * @return false - if the settings could not be achieved
   */
  bool configure(settings const& p_settings)
  {
    return driver_configure(p_settings);
  }

  /**
   * @brief Set the callback to run when the interrupt triggers
   *
   * Any state transitions that occur before this function is called are
   * lost.
   *
   * @param p_callback - object whose `callback()` is invoked when the
   * trigger condition occurs. Pass `nullptr` to clear a previously set
   * callback.
   */
  void on_trigger(edge_triggered_callback* p_callback)
  {
    driver_on_trigger(p_callback);
  }

private:
  virtual bool driver_configure(settings const& p_settings) = 0;
  virtual void driver_on_trigger(edge_triggered_callback* p_callback) = 0;
};
}  // namespace lab1
