# PWM Fan Control

## Overview

This module generates a nominal 25 kHz PWM signal for the smart PC fan controller using the MSPM0G3507. The portable fan driver accepts a duty cycle from 0 to 100 percent and uses the PWM interface provided in `hal/timer.hpp`.

## Implementation

- `pwm/fan_pwm.hpp` and `pwm/fan_pwm.cpp`: portable fan driver, with no MCU register access.
- `fan_pwm_test/mspm0_pwm.hpp` and `fan_pwm_test/mspm0_pwm.cpp`: MSPM0-specific implementation of `lab2::pwm` using direct register access.
- `fan_pwm_test/main.cpp`: standalone test application, not part of the final FreeRTOS application.

## Hardware Configuration

| Parameter | Value |
|---|---|
| MCU | MSPM0G3507 |
| Timer | TIMA1 |
| Channel | CCP0 |
| Output | PA10 (PINCM21, peripheral function 5) |
| Nominal timer clock | 32 MHz BUSCLK |
| Nominal PWM frequency | 25 kHz |
| LOAD | 1279 |
| Duty cycle | 0–100% |

## Integration Instructions

Include and compile the portable driver and the MSPM0 implementation in the final project. Do not include the standalone test `main.cpp`. Adjust include paths or source locations as needed, keeping the portable driver separate from the MCU-specific implementation.

```cpp
// Create the PWM hardware driver.
fan_controller::mspm0_pwm pwm_hardware(25'000U);

// Connect the portable fan driver to the hardware driver.
fan_controller::fan_pwm fan(pwm_hardware);

// Set the requested duty cycle.
fan.set_duty_cycle(50U);
```

Create the objects once and keep them alive for as long as the controlling task uses them. Only one task should own and update the PWM driver. Do not reset TIMA1 or reconfigure PA10 elsewhere. Verify that the final application uses the assumed 32 MHz timer clock and that the LaunchPad jumper routing exposes PA10 at the intended header.

`set_duty_cycle()` takes a `std::uint8_t` percentage; values above 100 are clamped to 100. `frequency()` returns the nominal configured frequency, not a measured frequency. The portable driver converts the percentage to the 16-bit PWM interface range (0–65535).

## Verification

Tested using Saleae Logic 2 at 24 MS/s on PA10. All five static duty-cycle tests passed. Runtime changes were made through a temporary debugger-controlled variable without rebuilding or restarting the MCU.

| Requested duty cycle | Observed output | Static | Runtime |
|---|---|---|---|
| 0% | Continuous LOW | Pass | Pass |
| 25% | 25–25.03% | Pass | Pass |
| 50% | 49.95–50.05% | Pass | Pass |
| 75% | 75–75.08% | Pass | Pass |
| 100% | Continuous HIGH | Pass | Pass |
| Return from 0% to 50% | 50% PWM | — | Pass |

Measured frequency at intermediate duty cycles: 25.210–25.237 kHz; measured period: 39.625–39.667 us. The frequency is approximately 0.84–0.95% above nominal, consistent with a small timer-clock offset. The duty-cycle variation is comparable to the analyzer's sampling resolution. No clock calibration was performed.

### Logic analyzer captures

- [0%](verification/00-percent.png)
- [25%](verification/25-percent.png)
- [50%](verification/50-percent.png)
- [75%](verification/75-percent.png)
- [100%](verification/100-percent.png)
- [Runtime return to 50%](verification/runtime-return-50.png)

## Remaining Integration Checks

The actual fan was not available during testing. Verify the fan model, PWM input voltage, polarity, open-drain/open-collector requirements, and any needed interface circuit before connection. Use a suitable external fan power supply with a shared ground; do not connect 12 V to an MCU pin. A constant LOW PWM command is not proof that the physical fan stops. Verify actual fan response and startup behavior during integration.

Temperature-to-RPM mapping, closed-loop RPM control, stall detection, and FreeRTOS task/queue integration are not verified by this PWM test. Agree on ownership and interfaces with the team before integrating.
