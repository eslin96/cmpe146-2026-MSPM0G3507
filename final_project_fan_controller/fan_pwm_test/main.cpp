/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../pwm/fan_control.hpp"
#include "../pwm/fan_pwm.hpp"
#include "mspm0_pwm.hpp"

// Simulate temperature and RPM readings through the debugger.
float volatile temperature = 45.0F;
std::uint32_t volatile actual_rpm = 1400U;

// Simulate the validity of the sensor readings.
bool volatile temperature_valid = true;
bool volatile rpm_valid = true;

// Request a controller update through the debugger.
std::uint32_t volatile update_control = 1U;

// Store the last completed update.
std::uint32_t volatile completed_updates = 0U;

// Display the controller results through the debugger.
std::uint32_t volatile target_rpm = 0U;
std::uint8_t volatile duty_cycle = 0U;

int main(void)
{
  // Create a PWM hardware driver running at 25 kHz.
  fan_controller::mspm0_pwm pwm_hardware(25'000U);

  // Connect the portable fan driver to the hardware driver.
  fan_controller::fan_pwm fan(pwm_hardware);

  // Configure the temperature curve and RPM controller.
  fan_controller::fan_control_config config{ 30.0F, 60.0F, 800U, 2000U,
                                             30U,   5U,    100U };

  // Create the closed-loop fan controller.
  fan_controller::fan_control controller(fan, config);

  while (1) {
    // Read the requested update number.
    std::uint32_t request = update_control;

    // Update the controller when a new request is received.
    if (request != completed_updates) {
      auto result = controller.update(
        temperature, actual_rpm, temperature_valid, rpm_valid);

      // Store the controller results.
      target_rpm = result.target_rpm;
      duty_cycle = result.duty_cycle;

      // Mark the requested update as completed.
      completed_updates = request;
    }
  }
}
