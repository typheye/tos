#ifndef REC_HAL_SD_LIMITS_H
#define REC_HAL_SD_LIMITS_H

/*
 * REC-only bound for STM32 HAL's ACMD41 voltage negotiation loop.
 *
 * ST's default SDMMC_MAX_VOLT_TRIAL is 65535 command attempts and is not tied
 * to a millisecond deadline.  On a card that does not answer ACMD41 promptly,
 * HAL_SD_Init() can therefore block REC for roughly 30 seconds even though USB
 * interrupts continue running.  Compile only REC's private hal_sd.c object with
 * a smaller retry count; SYSTEM and CubeMX-generated sources remain untouched.
 */
#include "stm32f4xx.h"
#include "stm32f4xx_ll_sdmmc.h"
#undef SDMMC_MAX_VOLT_TRIAL
#define SDMMC_MAX_VOLT_TRIAL 2048U

#endif /* REC_HAL_SD_LIMITS_H */
