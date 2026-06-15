#include "sbl_common.h"

extern uint32_t HAL_GetTick(void);

SBL_CODE void SBL_Delay(volatile uint32_t loops) {
  while (loops--) {
    __NOP();
  }
}

SBL_CODE void SBL_DelayMs(uint32_t ms) {
  uint32_t start = HAL_GetTick();
  while ((uint32_t)(HAL_GetTick() - start) < ms) {
    __NOP();
  }
}

SBL_CODE void SBL_GpioSet(GPIO_TypeDef *port, uint32_t pin) {
  port->BSRR = (1UL << pin);
}

SBL_CODE void SBL_GpioReset(GPIO_TypeDef *port, uint32_t pin) {
  port->BSRR = (1UL << (pin + 16U));
}
