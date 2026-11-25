#pragma once

#include <cstdint>

#include "FreeRTOS.h"
#include "queue.h"

#include "mspm0_tachometer.hpp"

namespace fan_controller {

/**
 * @brief Parameters passed to rpm_task().
 *
 * output_queue must have a length of one because rpm_task uses
 * xQueueOverwrite() to keep the newest reading available to the controller
 * and display tasks.
 */
struct rpm_task_context
{
  mspm0_tachometer* tachometer;
  QueueHandle_t output_queue;
  std::uint32_t measurement_window_ms;
};

/**
 * @brief Periodically publish the latest tachometer reading.
 *
 * The task waits for one complete measurement window before publishing its
 * first result, then repeats at that same interval.
 */
void rpm_task(void* p_parameter);

}  // namespace fan_controller
