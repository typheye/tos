#include "init.h"

#include "sbl_hw.h"
#include "sbl_lcd.h"
#include "sbl_ui.h"
#include "sbl_usb.h"

#include "dma.h"
#include "gpio.h"
#include "spi.h"
#include "tim.h"

SBL_CODE void SBL_Run(void) {
  SBL_HwBootstrap();
  if (!SBL_IsFastbootRequested()) {
    return;
  }

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_TIM4_Init();

  SBL_LedsOff();
  (void)HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, 1000U);
  (void)SBL_USB_Init();
  SBL_UiRunFastboot();
}
