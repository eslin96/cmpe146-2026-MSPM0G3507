# cmpe146-2026-MSPM0G3507

Repo containing the labs for CmpE 146 2026 by Khalil Estell.

## Getting your workspace set up

This repo holds multiple Code Composer Studio (Theia) projects - one per lab,
plus a shared `freertos_kernel` project - meant to be opened together as a
single workspace:

1. Install CCS along with the **MSPM0-SDK** (2.10.00.04) and **SysConfig**
   products - these are what let CCS resolve this repo's
   `${COM_TI_MSPM0_SDK_*}` build variables.
2. Open CCS Theia and go to the **Explorer** panel.
3. Click the **"..."** (overflow) menu at the top of the Explorer panel.
4. Select **"Add Folder to Workspace..."**.
5. Select the folder you cloned this repo into
   (`cmpe146-2026-MSPM0G3507`).

CCS discovers every project inside the folder you add - each `labN_*/`
project and `freertos_kernel/` - and adds them all to your workspace in one
step. You should not need to import each lab project individually.

## Repository layout

```text
cmpe146-2026-MSPM0G3507/
├── hal/                        Shared hardware abstraction interfaces (pure
│                               virtual classes) - one header per peripheral
│                               category, used across all labs. Given, don't
│                               modify.
├── third_party/                Small vendored header-only utilities: bit
│                               manipulation helpers (libhal-util) and a
│                               std::span polyfill for this toolchain.
├── freertos_kernel/            Shared CCS project that builds the FreeRTOS
│                               kernel + POSIX layer as a library. Referenced
│                               by any lab that needs FreeRTOS (see that
│                               lab's .project) - you shouldn't need to build
│                               this one directly.
├── lab1_gpio/                  Lab 1: GPIO driver, from scratch.
├── lab2_interrupt_timer_pwm/   Lab 2: interrupts, timer, PWM, from scratch.
├── lab3_spi/                   Lab 3: SPI driver + NOR flash.
├── lab4_i2c/                   Lab 4: I2C driver + BMI160 IMU.
├── lab5_adc_dac/                Lab 5: ADC/DAC drivers + FreeRTOS.
├── lab6_uart_crc/               Lab 6: UART driver + CRC-32.
├── style.md                     This codebase's C++ style guide.
└── .clang-format, .clang-tidy,  Formatting, lint, and spell-check config
    .cspell.json,                used by this repo's pre-commit hooks.
    .pre-commit-config.yaml
```

Each `labN_*/` folder is a self-contained CCS project:

- `.project` / `.cproject` / `.ccsproject` / `.settings/` - CCS project
  metadata, generated/maintained by CCS, don't hand-edit.
- `targetConfigs/` - debug probe + device configuration.
- `labN_*.syscfg` - SysConfig: clock tree and debug pins only. The
  peripheral that lab is teaching is deliberately left unconfigured here -
  you write that driver by hand.
- `labN_*.cpp` - where you write your driver classes and the lab's
  `main()`, against the shared interfaces in `../hal/`.
- `README.md` - that lab's assignment and grading rubric.
