# Lab 5 - ADC + DAC (FreeRTOS)

For this lab, you will:

1. Implement an ADC driver using DriverLib, with DMA-driven sampling.
2. Implement a DAC driver from scratch, using the reference manual.
3. Use FreeRTOS to run ADC sampling and DAC output as two concurrent tasks
   connected by a queue.

## Use cases

Consider this a component of a larger product. Products where this
sample-average-and-output pipeline (or ones like it) would be used include:

- Analog gauges and meters driven from a sensor reading
- Audio level metering and simple signal conditioning
- Data acquisition systems that condition a sensor signal for another
  analog system downstream
- Thermostats and other feedback controllers that turn a sensed value into
  an analog control signal

## Overview

This lab requires FreeRTOS. Write an ADC driver against the `adc16_stream`
interface (`adc.hpp`) and a DAC driver against the `dac16` interface
(`dac.hpp`), then use them from two FreeRTOS tasks:

1. An ADC task fills a 64-sample buffer from the on-board temperature sensor
   in a single DMA-backed transfer, averages the buffer, and sends the
   average onto a queue.
2. A DAC task waits on that queue, scales each value it receives to fill the
   DAC's full output range (0V to Vdd), and writes it out.
3. ADC task should sleep the thread for 1 second between samples.

Each of these should be its own class function (or set of class functions)
added to the `adc_driver` and `dac_driver` classes within the
`lab5_adc_dac.cpp` file.

`adc16_stream` fills an entire buffer per call, rather than returning one
sample at a time - the ADC driver must DMA to configure a
single transfer that fills the whole buffer, put the calling task to sleep
using FreeRTOS for the duration of that transfer, and let the DMA
transfer-complete interrupt wake it once the buffer is full (one sleep per
buffer, not one per sample). The DAC driver must be written from scratch
against the register-level user's guide, no DriverLib for that one.

## Learning Objectives

- Implement a buffer-filling ADC driver, using DriverLib and DMA, that frees
  the CPU (via putting its calling task to sleep) for the duration of a
  whole buffered transfer rather than per sample.
- Implement a DAC driver from register-level code.
- Use FreeRTOS tasks and a queue to move data between a producer and a
  consumer running concurrently.

## Hardware

Board: [LP-MSPM0G3507 LaunchPad](https://www.ti.com/tool/LP-MSPM0G3507).
Sample the on-board temperature sensor channel on the ADC, and route the DAC
output to a pin you can probe or measure.

## Grading Rubric (60 pt)

1. **stream ADC16 interface implementation - 15 pts.** ADC driver correctly
   implements the `adc16_stream` interface using DriverLib and DMA, filling
   the whole given buffer in a single transfer and putting its calling task
   to sleep until the DMA transfer-complete interrupt wakes it.
2. **DAC16 interface implementation - 15 pts.** DAC driver correctly
   implements the `dac16` interface, written from register-level code.
3. **FreeRTOS application - 20 pts.** ADC and DAC tasks run concurrently,
   connected by a queue, with the DAC task scaling each averaged sample to
   the DAC's full output range.
4. **Code submission - 10 pts.** Submit a PR to your git repo targeting your
   `main` branch.

## Reference material

- **[MSPM0G3507 datasheet](https://www.ti.com/lit/ds/symlink/mspm0g3507.pdf)** -
  ADC and DAC electrical characteristics and pin-to-peripheral function table.
- **[MSPM0G350x Technical Reference Manual](https://www.ti.com/lit/ug/slau846e/slau846e.pdf)** -
  ADC12, DAC12, and DMA chapters.
- **MSPM0 SDK DriverLib documentation** - ADC12 and DMA APIs.
