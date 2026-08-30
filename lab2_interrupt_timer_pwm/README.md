# Lab 2 - Interrupt, Timers and PWM

In this lab you will:

1. Implement a steady clock, edge-triggered GPIO interrupt, and PWM
   driver from scratch - TI DriverLib is not allowed.
2. Debounce a switch interrupt in software.
3. Build a simplified rhythm game that coordinates all four drivers
   (including `gpio` from lab 1).

This document is the complete assignment: what to build, how it's
graded, and how to get started.

## Overview

Build a simplified
[Taiko no Tatsujin](https://www.youtube.com/watch?v=fAhLc4QtqvA)-style
rhythm game. The RGB LED cycles through RED, BLUE, and OFF at a fixed
beats-per-minute:

- RED - press S1
- BLUE - press S2
- OFF - press nothing

Wrong button, missed window, or a press during OFF ends the game. Score each
correct press by reaction time:

  1. Perfect
  2. Great
  3. Okay
  4. Bad

Print the final score when the game ends (loss or level complete).

BPM is your choice. Hardcode it or read it from STDIN. Encode the color
sequence however you like (array(s), generated) but try and keep it fair and
engaging. The lab instructor and TA will play the game to confirm it
works.

## Learning Objectives

- Implement a steady clock (time base) driver from scratch.
- Implement an edge-triggered GPIO interrupt driver from scratch.
- Implement a PWM driver from scratch.
- Debounce a switch interrupt signal in software.
- Coordinate multiple drivers (GPIO, clock, interrupt, PWM) to build a
  working game.

## Hardware

Board: [LP-MSPM0G3507 LaunchPad](https://www.ti.com/tool/LP-MSPM0G3507).
Same LED wiring as `lab1_gpio`:

| Color | Pin  |
| ----- | ---- |
| Red   | PB26 |
| Green | PB27 |
| Blue  | PB22 |

S1/S2 pins: see
[lab1_gpio/README.md](../lab1_gpio/README.md#hardware) or the
[LP-MSPM0G3507 User's Guide & Schematic](https://www.ti.com/lit/slau873)
(SLAU873).

## Grading Rubric (50 pt)

1. **Steady clock - 10 pts.** Implemented and working.
2. **Edge-triggered interrupt - 10 pts.** Implemented and working.
3. **PWM - 10 pts.** Implemented and working.
4. **Switch debouncing - 10 pts.** Working debounce system.
5. **Reaction game - 10 pts.** Works as expected; the instructor and TA
   will play it to confirm.

## Reference material

- **[MSPM0G350x Technical Reference Manual](https://www.ti.com/lit/ug/slau846e/slau846e.pdf)** -
  General Purpose Input/Output (GPIO), Input/Output Multiplexer (IOMUX),
  and General Purpose Timer (GPTIMER) chapters.
- **Armv6-M Architecture Reference Manual** - SysTick and Nested
  Vectored Interrupt Controller (NVIC), if you use them.
- **[MSPM0G3507 datasheet](https://www.ti.com/lit/ds/symlink/mspm0g3507.pdf)** -
  pin-to-peripheral function table (which pins carry which timer's PWM
  output).
- **[LP-MSPM0G3507 User's Guide](https://www.ti.com/lit/slau873)** -
  on-board LED/button wiring.
- **`style.md` §S.10 (Memory-Mapped I/O)** - the pattern this codebase
  uses for declaring and accessing hardware registers safely.

## HINT: DEBOUNCE

A closed switch vibrates, connecting and disconnecting the contact pad
multiple times before settling - each vibration fires the interrupt
again. Accept all of them and the game fails on the first press.
Disable the edge interrupt after the first pulse, then re-enable it on
the next LED color cycle.
