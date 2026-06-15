#include "init.h"

#include "sbl_hw.h"
#include "sbl_lcd.h"
#include "sbl_ui.h"

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
  SBL_LcdInit();
  SBL_UiRunFastboot();
}
