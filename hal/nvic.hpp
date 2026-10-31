#pragma once

#include <ti/devices/msp/msp.h>

namespace cortex_m {

inline void nvic_enable_irq(int number)
{
  NVIC_EnableIRQ(static_cast<IRQn_Type>(number));
}

inline void nvic_disable_irq(int number)
{
  NVIC_DisableIRQ(static_cast<IRQn_Type>(number));
}

}