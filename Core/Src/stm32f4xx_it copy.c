/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    stm32f4xx_it.c
 * @brief   Interrupt Service Routines.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static uint8_t esp8266_line_buffer[256];
static uint16_t esp8266_line_index = 0;

uint8_t esp8266_global_buffer[512];
uint16_t esp8266_global_index = 0;
uint8_t esp8266_data_ready = 0;

volatile uint32_t uart2_rx_count = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void copy_to_global_buffer(uint8_t *line, uint16_t len) {
  if (len > 512)
    len = 512;
  memcpy(esp8266_global_buffer, line, len);
  esp8266_global_index = len;
  esp8266_data_ready = 1;
}
/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_sdio_rx;
extern DMA_HandleTypeDef hdma_sdio_tx;
extern DMA_HandleTypeDef hdma_spi1_rx;
extern DMA_HandleTypeDef hdma_spi1_tx;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/

void NMI_Handler(void) {
  while (1) {
  }
}

void HardFault_Handler(void) {
  while (1) {
  }
}

void MemManage_Handler(void) {
  while (1) {
  }
}

void BusFault_Handler(void) {
  while (1) {
  }
}

void UsageFault_Handler(void) {
  while (1) {
  }
}

void SVC_Handler(void) {}

void DebugMon_Handler(void) {}

void PendSV_Handler(void) {}

void SysTick_Handler(void) { HAL_IncTick(); }

/******************************************************************************/
/* STM32F4xx Peripheral Interrupt Handlers                                    */
/******************************************************************************/

void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }

void USART2_IRQHandler(void) {
  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET) {
    uint8_t data = (uint8_t)(huart2.Instance->DR & 0xFF);

    uart2_rx_count++;

    if (esp8266_line_index < sizeof(esp8266_line_buffer) - 1) {
      esp8266_line_buffer[esp8266_line_index++] = data;
    }

    if (data == '\n' && esp8266_line_index > 0) {
      esp8266_line_buffer[esp8266_line_index] = '\0';
      copy_to_global_buffer(esp8266_line_buffer, esp8266_line_index);
      esp8266_line_index = 0;
    }

    if (esp8266_line_index >= sizeof(esp8266_line_buffer)) {
      esp8266_line_index = 0;
    }

    __HAL_UART_CLEAR_FLAG(&huart2, UART_FLAG_RXNE);
  }

  HAL_UART_IRQHandler(&huart2);
}

void DMA2_Stream0_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_spi1_rx); }

void DMA2_Stream3_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_sdio_rx); }

void DMA2_Stream5_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_spi1_tx); }

void DMA2_Stream6_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_sdio_tx); }

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */