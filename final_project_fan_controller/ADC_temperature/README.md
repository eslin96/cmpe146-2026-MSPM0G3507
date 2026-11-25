# PC Fan Controller: LM35 Temperature Measurement

This project reads an LM35 analog temperature sensor with the MSPM0G3507
ADC and publishes the converted temperature to a FreeRTOS queue. The
implementation is divided across:

- `lm35.hpp` - public LM35 and ADC interface
- `lm35.cpp` - ADC configuration, sample acquisition, and conversion
- `adc_temp.cpp` - FreeRTOS setup, temperature task, and application entry
  point

## Overview

The ADC is configured for a single 12-bit, unsigned software-triggered
conversion using channel 2 and VDDA as the reference voltage. The sensor
temperature task samples ADC0 once per second, converts the result to degrees
Celsius, and stores the latest value in a one-item queue.

```text
LM35 output
    |
    v
ADC0 channel 2
    |
    v
lm35::read_adc_code()
    |
    v
lm35::adc_code_to_celsius()
    |
    v
temperature_queue (one float)
```

The application prints each temperature over `std::printf()` for verification.

## Hardware

- MSPM0G3507 LaunchPad
- LM35 temperature sensor
- LM35 output connected to the analog pin selected by `IOMUX_PINCM55`
- Sensor `VCC` connected to a suitable positive supply
- Sensor ground connected to the LaunchPad ground
- No external ADC reference is required; the code uses VDDA

> [!NOTE]
> Confirm that the selected analog pin and supply voltage match the
> LaunchPad and sensor wiring before running the application.

The LM35 produces approximately 10 mV per degree Celsius:

```text
temperature (degrees C) = ADC voltage * 100
```

## ADC Configuration

`lm35::initialize_adc()` configures:

| Setting | Value |
| --- | --- |
| ADC | ADC0 |
| Resolution | 12-bit |
| Reference | VDDA |
| Conversion mode | Single, software triggered |
| ADC channel | 2 |
| Data format | Unsigned |
| Sample timer source | SCOMP0 |
| ADC clock | SYSOSC, divide by 1 |
| Sample time | 560 ADC cycles |

The ADC pin is selected by the generated board configuration with
`IOMUX_PINCM55`.

## Code Components

### `lm35.hpp`

Declares the `lm35` namespace API:

- `initialize_adc()` configures ADC0 and the analog input pin.
- `read_adc_code()` starts a conversion and returns a 12-bit result.
- `adc_code_to_celsius(adc_code, vdda_volts)` converts an ADC code to degrees
  Celsius.

### `lm35.cpp`

`lm35::initialize_adc()` resets and enables ADC0, configures its clock and
conversion memory, sets the sample time, and enables conversions.

`lm35::read_adc_code()` starts conversion and waits until conversion starts
before reading MEM0. The conversion uses the following voltage conversion:

```cpp
voltage = adc_code * vdda_volts / 4095.0f;
temperature_celsius = voltage * 100.0f;
```

The implementation assumes a 3.3 V VDDA when called from `adc_temp.cpp`.
Update the `vdda_volts` argument if the board's actual supply voltage differs.

### `adc_temp.cpp`

`main()` first initializes the system configuration and LM35 ADC interface,
then creates a one-item `float` queue. It creates `temperature_task()`, starts
the FreeRTOS scheduler, and does not return during normal operation.

`temperature_task()` runs continuously:

1. Reads the ADC code.
2. Converts the code to degrees Celsius.
3. Overwrites the temperature queue with the latest value.
4. Prints the temperature.
5. Waits for one second.

Using `xQueueOverwrite()` ensures that the queue always contains only the
most recent temperature. Queue creation or task creation failures are reported
through `std::printf()` and halt the application.

## Build and Run

1. Build the project.
2. Flash it to an MSPM0G3507 LaunchPad.
3. Open a serial debug console for the project's debug output.
4. Verify that the temperature reports approximately 0 degrees Celsius when
   the LM35 output is near 0 V and 25 degrees Celsius near 250 mV.

## Notes

- The ADC code has a maximum value of 4095 because it is configured for
  12-bit unsigned conversion.
- The conversion is repeated every second.
- The debug print can be removed after the sensor readings are verified.
