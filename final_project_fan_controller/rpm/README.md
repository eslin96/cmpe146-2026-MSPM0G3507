# RPM / Tachometer Measurement

This module is Person 3's portion of the fan controller. It counts falling
edges from the fan's open-collector tachometer output and publishes a
whole-number RPM reading for the control, display, and fault-detection tasks.

## Files

- `rpm_measurement.hpp/.cpp`: portable pulse-count-to-RPM calculation.
- `mspm0_tachometer.hpp/.cpp`: MSPM0 GPIO input and interrupt driver.
- `rpm_task.hpp/.cpp`: FreeRTOS task that publishes the newest reading.

## Default hardware connection

The default configuration is `PA14` (`PINCM36`, `GPIOA` pin 14) with a falling
edge interrupt. Connect the fan tachometer wire to PA14 through the LaunchPad
pin-extension header, connect the fan supply ground to LaunchPad ground, and
use a pull-up to 3.3 V. Do not connect a 12 V fan signal directly to an MCU
pin. The fan's tach output must be open-collector/open-drain or level-shifted
to a safe 3.3 V signal.

The default assumes two tachometer pulses per revolution, which is common but
must be checked against the fan datasheet:

```cpp
fan_controller::mspm0_tachometer tachometer(
  fan_controller::mspm0_tachometer::launchpad_pa14(2U));
if (!tachometer.initialize()) {
  // Report the configuration failure.
}
```

Use a different `pulses_per_revolution` value if the fan specifies another
number. The configuration structure also permits another GPIO pin and edge
polarity if PA14 is needed by another part of the final design.

## FreeRTOS integration

Create a one-element queue because `rpm_task` overwrites it with the newest
reading:

```cpp
QueueHandle_t rpm_queue = xQueueCreate(
  1, sizeof(fan_controller::rpm_reading));

fan_controller::rpm_task_context rpm_context{
  &tachometer,
  rpm_queue,
  500U
};

xTaskCreate(fan_controller::rpm_task,
            "RPM",
            configMINIMAL_STACK_SIZE,
            &rpm_context,
            tskIDLE_PRIORITY + 1U,
            nullptr);
```

The 500 ms window gives the controller the same update cadence documented in
the PWM module. A reading is calculated as

```text
RPM = pulse_count * 60,000 / (pulses_per_revolution * window_ms)
```

The calculation rounds to the nearest whole RPM. A completed window with no
pulses returns `{0, true}`. The integration/fault task should decide whether
zero RPM is acceptable for the current commanded duty cycle; zero RPM by
itself is not automatically an invalid sensor reading.

## GPIO ISR integration

MSPM0 GPIOA and GPIOB share the GPIO interrupt vector. This module therefore
does not define `GROUP1_IRQHandler`, which avoids colliding with the team's
button handler. The final application's existing `GROUP1_IRQHandler` should
dispatch the configured pin to the driver and clear the GPIO interrupt group
after it is handled:

```cpp
extern fan_controller::mspm0_tachometer tachometer;

extern "C" void GROUP1_IRQHandler(void)
{
  std::uint32_t const status = DL_Interrupt_getStatusGroup(
    DL_INTERRUPT_GROUP_1, DL_INTERRUPT_GROUP1_GPIOA);

  if ((status & DL_INTERRUPT_GROUP1_GPIOA) != 0U) {
    if (tachometer.handle_gpio_interrupt()) {
      DL_Interrupt_clearGroup(
        DL_INTERRUPT_GROUP_1, DL_INTERRUPT_GROUP1_GPIOA);
      return;
    }

    // Process any other GPIOA sources owned by the integrated application.
  }
}
```

If the integration code already handles the GPIO group, call
`handle_gpio_interrupt()` from that handler instead of adding another ISR.
Compile all four `.cpp` files in this directory with the final application;
do not add another `main()` for this module.
