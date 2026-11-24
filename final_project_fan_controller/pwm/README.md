# PWM Fan Control

## Overview

This module generates a nominal 25 kHz PWM signal for the smart PC fan controller using the MSPM0G3507. The portable fan driver accepts a duty cycle from 0 to 100 percent and uses the PWM interface provided in `hal/timer.hpp`.

## Implementation

- `pwm/fan_pwm.hpp` and `pwm/fan_pwm.cpp`: portable fan driver, with no MCU register access.
- `fan_pwm_test/mspm0_pwm.hpp` and `fan_pwm_test/mspm0_pwm.cpp`: MSPM0-specific implementation of `lab2::pwm` using direct register access.
- `pwm/fan_control.hpp` and `pwm/fan_control.cpp`: portable temperature-to-RPM mapping and feedback control.
- `fan_pwm_test/main.cpp`: standalone controller test using simulated readings, not part of the final FreeRTOS application.

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

## PWM Driver Integration Instructions

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

## PWM Verification

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

## Physical Fan Integration Checks

The actual fan was not available during testing. Verify the fan model, PWM input voltage, polarity, open-drain/open-collector requirements, and any needed interface circuit before connection. Use a suitable external fan power supply with a shared ground; do not connect 12 V to an MCU pin. A constant LOW PWM command is not proof that the physical fan stops. Verify actual fan response and startup behavior during integration.

## Automatic Fan Speed Control

The `fan_control` class receives temperature in degrees Celsius and measured RPM. It calculates the target RPM using a linear temperature curve and adjusts the previous PWM duty cycle based on the speed error. The class does not access MCU registers or depend on FreeRTOS.

### Initial configuration

These are provisional test settings, not measured limits of the physical fan. Agree on final values with the team and tune the controller on the actual fan.

| Parameter | Test value |
|---|---:|
| Minimum temperature | 30 C |
| Maximum temperature | 60 C |
| Minimum target RPM | 800 |
| Maximum target RPM | 2000 |
| Minimum commanded duty cycle | 30% |
| Maximum commanded duty cycle | 100% |
| Duty adjustment per update | 5 percentage points |
| RPM deadband | +/-100 RPM |
| Proposed control update interval | 500 ms |

Temperatures at or below 30 C request 800 RPM; temperatures at or above 60 C request 2000 RPM. Between these limits, the target increases linearly (45 C requests 1400 RPM). If measured RPM is more than 100 RPM below the target, duty increases by 5 percentage points. If it is more than 100 RPM above the target, duty decreases by 5 percentage points. Otherwise, duty is unchanged. Duty is limited to 30-100% during normal control.

The controller starts at 100% duty. Invalid configuration, invalid readings, or non-finite temperature cause a 100% cooling request and a result with `control_valid == false`. Normal updates resume when valid readings return, provided the configuration is valid.

### Integration example

Compile `fan_control.cpp`, `fan_pwm.cpp`, and `mspm0_pwm.cpp`. Include their corresponding headers. Do not compile the standalone test application or its startup files into the team application. The following example belongs in the control task or its initialization code, using the appropriate include paths.

```cpp
fan_controller::mspm0_pwm pwm_hardware(25'000U);
fan_controller::fan_pwm fan(pwm_hardware);

fan_controller::fan_control_config config{
  30.0F, 60.0F, 800U, 2000U, 30U, 5U, 100U
};

fan_controller::fan_control controller(fan, config);

// Example: one update using fresh sensor readings.
auto result = controller.update(45.0F, 1800U, true, true);
```

Create the objects once and call `update()` periodically from a single control task. Use real sensor values and validity flags in the final application, not the example constants. Do not create a new controller on every iteration because it stores the previous duty cycle. No other task should write to the same PWM channel.

| Interface | C++ type | Meaning |
|---|---|---|
| Temperature | `float` | Degrees Celsius |
| Actual RPM | `std::uint32_t` | Measured fan speed |
| Temperature valid | `bool` | Fresh, valid temperature reading |
| RPM valid | `bool` | Fresh, valid RPM reading |
| `result.target_rpm` | `std::uint32_t` | Requested speed |
| `result.duty_cycle` | `std::uint8_t` | Commanded duty, 0-100% |
| `result.control_valid` | `bool` | Whether normal feedback control is active |

Person 1 supplies temperature, Person 3 supplies RPM, and Person 5 connects these inputs and controller results to the FreeRTOS tasks and queues. Person 4 can display the latest results. The integration task must check freshness and validity, including missing sensor data. Queue names, message layouts, task priorities, and update intervals still require team agreement.

The CCS test project links the `pwm` folder. If the linked path is specific to the original computer, update it to the local checkout when importing the test project.

## Controller Verification

The controller compiled and ran on the MSPM0G3507 in CCS. Temperature and RPM inputs were simulated through the debugger WATCH panel. These tests verify the control calculations and returned duty commands, not the physical fan response or stability of the complete feedback loop. The earlier logic analyzer captures verify the separate PWM hardware implementation.

| Test | Simulated input or change | Observed result |
|---|---|---|
| [1. Initial mapping](verification/controller/01-initial-mapping.png) | 45 C, 1400 RPM | Target 1400, duty 100% |
| [2. Above target](verification/controller/02-decrease-duty.png) | 45 C, 1800 RPM | Duty 100% to 95% |
| [3. Below target](verification/controller/03-increase-duty.png) | 45 C, 1000 RPM | Duty 95% to 100% |
| [4. Deadband](verification/controller/04-deadband.png) | 45 C, 1450 RPM | Duty remains 100% |
| [5. Temperature decrease](verification/controller/05-temperature-decrease.png) | 30 C, 1450 RPM | Target 800, duty 95% |
| [6. Temperature increase](verification/controller/06-temperature-increase.png) | 60 C, 1450 RPM | Target 2000, duty 100% |
| [7. Upper limits](verification/controller/07-upper-limits.png) | 75 C, 1000 RPM | Target limited to 2000, duty 100% |
| [8. Lower limits](verification/controller/08-lower-limits.png) | 20 C, 1800 RPM, repeated updates | Target limited to 800, duty limited to 30% |
| [9A. Invalid temperature](verification/controller/09-invalid-temperature.png) | Temperature validity false | Target 2000, duty 100% |
| [Temperature recovery](verification/controller/10-temperature-recovery.png) | Both readings valid, 45 C, 1800 RPM | Target 1400, duty 95% |
| [9B. Invalid RPM](verification/controller/11-invalid-rpm.png) | RPM validity false | Target 2000, duty 100% |
| [RPM recovery](verification/controller/12-rpm-recovery.png) | Both readings valid, 45 C, 1800 RPM | Target 1400, duty 95% |

### Reproduce the test

Build and debug `fan_pwm_test`. Add `temperature`, `actual_rpm`, `temperature_valid`, `rpm_valid`, `target_rpm`, `duty_cycle`, `update_control`, and `completed_updates` to WATCH. Set inputs first, then change `update_control` to a new integer and confirm `completed_updates` matches it. One new request performs one control update. Repeat the request with a different number for each successive duty adjustment. This debugger-only mechanism is not part of the production task.

## Remaining Team Integration

- Verify the physical fan interface and correct PWM polarity before connecting the fan.
- Confirm timer and pin allocation, including the tachometer timer and input pin.
- Confirm the fan's usable RPM range, minimum starting duty, temperature thresholds, and feedback update interval. Tune the duty adjustment and deadband on the physical system.
- Integrate the temperature and RPM measurements, control task, display, and heartbeat through FreeRTOS.
- Agree on stall detection ownership and timeout policy. A valid zero RPM or a tachometer timeout needs to be evaluated against the commanded speed and startup allowance. The controller's invalid-reading response is not a complete stall alarm.
- Verify actual speed tracking, physical fault behavior, and standalone operation.

The PWM driver and simulated control logic are ready for integration. Physical fan behavior, stall detection, and full-system operation remain unverified.
