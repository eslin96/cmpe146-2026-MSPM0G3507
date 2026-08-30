# Lab 3 - SPI Flash Memory

Let consider this lab as a product that needs some additional features of.
Our research team has reported differences in performance based on flash memory
sizes, so they have asked that we include logs of the flash memory manufacturer
ID as well as its capacity is. It is your job to implement the needed parts to
communicate with the flash memory and to report the necessary information.

1. Implement an SPI driver from scratch
2. Use it to talk to a NOR flash memory chip over SPI.

## Overview

Write an SPI driver against the `spi` interface (`spi.hpp`), then use it
to:

1. Read and print the flash chip's manufacturer ID.
2. Read and print its memory capacity.
3. Read and print the first 32-byte block.
4. Take user input and write up to 32 bytes to that same block.

Each of these should be its own class function (or set of class functions)
added to the `flash_memory` driver within the `lab3_spi.cpp` file.

## Learning Objectives

- Implement an SPI driver from scratch, with a configurable bit rate.
- Drive a chip-select line with a plain GPIO output pin.
- Talk to a real SPI device (NOR flash) using your own driver.

## Hardware

Board: [LP-MSPM0G3507 LaunchPad](https://www.ti.com/tool/LP-MSPM0G3507).
Which SPI instance/pins you use is up to you - check the datasheet's
pin-to-peripheral function table for which pins carry SPI0/SPI1 signals,
and drive chip-select with a plain `output_pin`.

NOR flash part: Winbond W25Q32JV - 32 Mbit SPI NOR flash or similar.

## Grading Rubric (60 pt)

1. **SPI signaling - 15 pts.** Produces correct signals on the
   out/in/clock lines at the bit rate the application configures.
2. **ID + capacity - 15 pts.** Reads and prints both the manufacturer ID
   and memory capacity.
3. **Block read - 10 pts.** Reads and prints the first 32 bytes of the
   device.
4. **Block write - 10 pts.** Takes user input and writes up to 32 bytes
   to that first block.
5. **Code submission - 10 pts.** Submit a PR to your git repo targeting
   your `main` branch.

## Reference material

- **[MSPM0G3507 datasheet](https://www.ti.com/lit/ds/symlink/mspm0g3507.pdf)** -
  pin-to-peripheral function table for SPI pins.
- **[MSPM0G350x Technical Reference Manual](https://www.ti.com/lit/ug/slau846e/slau846e.pdf)** -
  SPI chapter.
- **[Winbond W25Q32JV datasheet](https://www.mouser.com/datasheet/2/949/w25q32jv_revg_03272018_plus-1489806.pdf)** -
  the NOR flash chip for this lab. Look for the JEDEC ID command
  (`0x9F`) for manufacturer/capacity, and the standard read/page-program
  commands.
