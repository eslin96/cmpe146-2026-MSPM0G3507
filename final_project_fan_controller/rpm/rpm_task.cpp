#include "rpm_task.hpp"

#include "task.h"

namespace fan_controller {

void rpm_task(void* p_parameter)
{
  auto* const context = static_cast<rpm_task_context*>(p_parameter);

  if (context == nullptr || context->tachometer == nullptr ||
      context->measurement_window_ms == 0U) {
    vTaskDelete(nullptr);
    return;
  }

  TickType_t period = pdMS_TO_TICKS(context->measurement_window_ms);
  if (period == 0U) {
    period = 1U;
  }

  // Delay first so the first result represents a complete measurement window.
  TickType_t next_wake = xTaskGetTickCount();

  while (true) {
    vTaskDelayUntil(&next_wake, period);

    rpm_reading const reading =
      context->tachometer->sample(context->measurement_window_ms);

    if (context->output_queue != nullptr) {
      // The queue is intentionally length one: consumers always receive the
      // newest RPM result rather than an old backlog of measurements.
      (void)xQueueOverwrite(context->output_queue, &reading);
    }
  }
}

}  // namespace fan_controller
