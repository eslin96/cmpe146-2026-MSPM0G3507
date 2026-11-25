#include <cstdio>
#include <cstdint>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lm35.hpp"
#include <ti/driverlib/driverlib.h>

#include "ti_msp_dl_config.h"

namespace {
QueueHandle_t temperature_queue = nullptr;
//reads and converts ADC and sends it to a queue
 void temperature_task(void* parameter) {
  (void)parameter;
    while (true) {
      const std::uint16_t adc_code = lm35::read_adc_code();
      const float temperature_celsius = lm35::adc_code_to_celsius(adc_code, 3.3f);
      xQueueOverwrite(temperature_queue, &temperature_celsius); // Send the temperature to the queue
      std::printf("Temperature: %.2f C\n", temperature_celsius);//remove it once readings are verified
      vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
  }
}  // namespace


int main()
{
  SYSCFG_DL_init();
  lm35::initialize_adc();
temperature_queue = xQueueCreate(1, sizeof(float));//creates space for one float value in the queue
if (temperature_queue == nullptr) {
    std::printf("Failed to create temperature queue\n");
     while(true){};
  }
if(xTaskCreate(temperature_task, "Temperature Task", configMINIMAL_STACK_SIZE, nullptr, tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
    std::printf("Failed to create temperature task\n");
    while(true){};
  }

vTaskStartScheduler();
  while (true) {
  }
  return 0;

}