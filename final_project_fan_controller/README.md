# CMPE 146 Smart PC Fan Controller

## Project Overview
This project uses the MSPM0G3507 microcontroller to control a 4-pin 12V PC fan based on temperature. The system uses a 25 kHz PWM signal to adjust fan speed and tachometer feedback to measure actual RPM.

FreeRTOS will manage the different tasks, and an OLED display will show temperature, fan speed, and fault information.

## Team Responsibilities

- **Person 1: (Sonal)** External temperature sensor and ADC
- **Person 2: (Eslin)** PWM generation and fan speed control
- **Person 3: (Kaden)** Tachometer and RPM measurement
- **Person 4: (Dylan)** OLED display
- **Person 5: (Koby)** FreeRTOS integration and system testing

## Development

Each member will develop and test their assigned component separately before integration.

The final system will combine the components using FreeRTOS tasks and communication mechanisms.
