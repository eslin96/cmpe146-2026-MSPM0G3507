#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <array>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "../hal/adc.hpp"
#include "../hal/dac.hpp"

// TODO(lab5, step 1): Make this class inherit from and implement the
// `lab5::adc16_stream` interface (`adc.hpp`) using DriverLib, wired to the
// on-board temperature sensor channel. read() should configure a single DMA
// transfer that fills the entire given buffer with consecutive conversions,
// put the calling FreeRTOS task to sleep (e.g. block on a semaphore or task
// notification) for the duration of that transfer, and let the DMA
// transfer-complete interrupt wake it once the buffer is full - one sleep
// per buffer, not one per sample.
class adc_driver
{};

// TODO(lab5, step 2): Make this class inherit from and implement the
// `lab5::dac16` interface (`dac.hpp`), written from register-level code
// against the MSPM0G350x user's guide - no DriverLib for this one.
class dac_driver
{};

namespace {
constexpr std::size_t samples_per_average = 64;
constexpr std::size_t queue_size = 1;

StaticQueue_t sample_queue_control_block;
std::array<std::uint16_t, queue_size> sample_queue_storage;
QueueHandle_t sample_queue;

void adc_task(void*)
{
  // TODO(lab5, step 3): Construct an adc_driver. Every iteration, declare a
  // local `std::array<std::uint16_t, samples_per_average>` buffer, fill it
  // with one call to adc_driver::read() (one DMA transfer, one task sleep),
  // average the buffer's contents, and send the average onto sample_queue
  // for the DAC task to consume.
  while (true) {
  }
}

void dac_task(void*)
{
  // TODO(lab5, step 4): Construct a dac_driver. Every iteration, block on
  // sample_queue for the next averaged sample, scale it to fill the DAC's
  // full output range (0V to Vdd), and write it out.
  while (true) {
  }
}
}  // namespace

int main()
{
  std::printf("Hello, World\n");

  sample_queue = xQueueCreateStatic(
    sample_queue_storage.size(),
    sizeof(sample_queue_storage[0]),
    reinterpret_cast<std::uint8_t*>(sample_queue_storage.data()),
    &sample_queue_control_block);

  // TODO(lab5, step 5): Create the ADC and DAC tasks with xTaskCreate(),
  // then start the scheduler with vTaskStartScheduler(). That call does not
  // return under normal operation, so nothing belongs after it.
}
