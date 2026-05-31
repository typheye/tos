/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_custom_hid_if.c
  * @version        : v1.0_Cube
  * @brief          : USB Device Custom HID interface file.
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
#include "usbd_custom_hid_if.h"

/* USER CODE BEGIN INCLUDE */
#include <string.h>
/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief Usb device.
  * @{
  */

/** @addtogroup USBD_CUSTOM_HID
  * @{
  */

/** @defgroup USBD_CUSTOM_HID_Private_TypesDefinitions USBD_CUSTOM_HID_Private_TypesDefinitions
  * @brief Private types.
  * @{
  */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
  * @}
  */

/** @defgroup USBD_CUSTOM_HID_Private_Defines USBD_CUSTOM_HID_Private_Defines
  * @brief Private defines.
  * @{
  */

/* USER CODE BEGIN PRIVATE_DEFINES */
#if (USBD_CUSTOM_HID_REPORT_DESC_SIZE != 148U)
#error "CubeMX: set USB_DEVICE / Custom HID / Report Descriptor Size to 148."
#endif
#if (USBD_CUSTOMHID_OUTREPORT_BUF_SIZE < 64U)
#error "CubeMX: set USB_DEVICE / Custom HID / OUT Report Buffer Size to 64."
#endif
#if (CUSTOM_HID_EPIN_SIZE < 64U) || (CUSTOM_HID_EPOUT_SIZE < 64U)
#error "Custom HID EP IN/OUT size must be 64 bytes. Check USER CODE in usbd_conf.h."
#endif
/* USER CODE END PRIVATE_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_CUSTOM_HID_Private_Macros USBD_CUSTOM_HID_Private_Macros
  * @brief Private macros.
  * @{
  */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */

/** @defgroup USBD_CUSTOM_HID_Private_Variables USBD_CUSTOM_HID_Private_Variables
  * @brief Private variables.
  * @{
  */

/** Usb HID report descriptor. */
__ALIGN_BEGIN static uint8_t CUSTOM_HID_ReportDesc_FS[USBD_CUSTOM_HID_REPORT_DESC_SIZE] __ALIGN_END =
{
  /* USER CODE BEGIN 0 */
  /* Composite Custom HID report descriptor:
   *   Report ID 0x01: keyboard input + LED output
   *   Report ID 0x02: relative mouse input
   *   Report ID 0x10: vendor IN/OUT, 63-byte payload each way
   * CubeMX appends the final 0xC0 END_COLLECTION below USER CODE END 0.
   */
  0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01, 0x05, 0x07,
  0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
  0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
  0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
  0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06,
  0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00,
  0x29, 0x65, 0x81, 0x00, 0xC0, 0x05, 0x01, 0x09, 0x02, 0xA1,
  0x01, 0x85, 0x02, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19,
  0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75,
  0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x01, 0x05,
  0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25,
  0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06, 0xC0, 0xC0, 0x06,
  0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x10, 0x15, 0x00,
  0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x3F, 0x09, 0x02, 0x81,
  0x02, 0x95, 0x3F, 0x09, 0x03, 0x91, 0x02,
  /* USER CODE END 0 */
  0xC0    /*     END_COLLECTION	             */
};

/* USER CODE BEGIN PRIVATE_VARIABLES */
static uint8_t tos_hid_last_vendor_out[63];
static volatile uint16_t tos_hid_last_vendor_out_len = 0U;
static volatile uint8_t tos_hid_vendor_out_available = 0U;
/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CUSTOM_HID_Exported_Variables USBD_CUSTOM_HID_Exported_Variables
  * @brief Public variables.
  * @{
  */
extern USBD_HandleTypeDef hUsbDeviceFS;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */
/**
  * @}
  */

/** @defgroup USBD_CUSTOM_HID_Private_FunctionPrototypes USBD_CUSTOM_HID_Private_FunctionPrototypes
  * @brief Private functions declaration.
  * @{
  */

static int8_t CUSTOM_HID_Init_FS(void);
static int8_t CUSTOM_HID_DeInit_FS(void);
static int8_t CUSTOM_HID_OutEvent_FS(uint8_t event_idx, uint8_t state);

/**
  * @}
  */

USBD_CUSTOM_HID_ItfTypeDef USBD_CustomHID_fops_FS =
{
  CUSTOM_HID_ReportDesc_FS,
  CUSTOM_HID_Init_FS,
  CUSTOM_HID_DeInit_FS,
  CUSTOM_HID_OutEvent_FS
};

/** @defgroup USBD_CUSTOM_HID_Private_Functions USBD_CUSTOM_HID_Private_Functions
  * @brief Private functions.
  * @{
  */

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes the CUSTOM HID media low layer
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CUSTOM_HID_Init_FS(void)
{
  /* USER CODE BEGIN 4 */
  return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  DeInitializes the CUSTOM HID media low layer
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CUSTOM_HID_DeInit_FS(void)
{
  /* USER CODE BEGIN 5 */
  return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  Manage the CUSTOM HID class events
  * @param  event_idx: Event index
  * @param  state: Event state
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CUSTOM_HID_OutEvent_FS(uint8_t event_idx, uint8_t state)
{
  /* USER CODE BEGIN 6 */
  /* CubeMX default callback only passes the first two bytes. The full OUT
   * packet is still in the Custom HID class buffer, so read it from there
   * without changing generated middleware code.
   */
  USBD_CUSTOM_HID_HandleTypeDef *hhid =
      (USBD_CUSTOM_HID_HandleTypeDef *)hUsbDeviceFS.pClassData;

  if (hhid != NULL)
  {
    uint8_t *report = hhid->Report_buf;

    if (report[0] == TOS_HID_REPORT_ID_VENDOR)
    {
      memcpy(tos_hid_last_vendor_out, &report[1], sizeof(tos_hid_last_vendor_out));
      tos_hid_last_vendor_out_len = sizeof(tos_hid_last_vendor_out);
      tos_hid_vendor_out_available = 1U;
    }
    else
    {
      /* Keep LED/output reports harmless for now. event_idx/state are the
       * first two bytes if you later want keyboard LED status.
       */
      UNUSED(event_idx);
      UNUSED(state);
    }
  }

  /* Start next USB packet transfer once data processing is completed. */
  if (USBD_CUSTOM_HID_ReceivePacket(&hUsbDeviceFS) != (uint8_t)USBD_OK)
  {
    return -1;
  }

  return (USBD_OK);
  /* USER CODE END 6 */
}

/* USER CODE BEGIN 7 */
static int8_t TOS_HID_SendReport(const uint8_t *report, uint16_t len)
{
  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
  {
    return (int8_t)USBD_FAIL;
  }

  /* USBD_CUSTOM_HID_SendReport does not keep your buffer after transmit is
   * queued, but using static storage avoids stack lifetime surprises.
   */
  static uint8_t tx_buf[64];
  if (len > sizeof(tx_buf))
  {
    return (int8_t)USBD_FAIL;
  }

  memcpy(tx_buf, report, len);
  return (int8_t)USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, tx_buf, len);
}

int8_t TOS_HID_SendKeyboard(uint8_t modifier,
                            uint8_t key1, uint8_t key2, uint8_t key3,
                            uint8_t key4, uint8_t key5, uint8_t key6)
{
  uint8_t report[TOS_HID_KEYBOARD_REPORT_SIZE] = {
      TOS_HID_REPORT_ID_KEYBOARD,
      modifier,
      0x00U,
      key1, key2, key3, key4, key5, key6
  };
  return TOS_HID_SendReport(report, sizeof(report));
}

int8_t TOS_HID_ReleaseKeyboard(void)
{
  return TOS_HID_SendKeyboard(0U, 0U, 0U, 0U, 0U, 0U, 0U);
}

int8_t TOS_HID_SendMouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel)
{
  uint8_t report[TOS_HID_MOUSE_REPORT_SIZE] = {
      TOS_HID_REPORT_ID_MOUSE,
      buttons,
      (uint8_t)x,
      (uint8_t)y,
      (uint8_t)wheel
  };
  return TOS_HID_SendReport(report, sizeof(report));
}

int8_t TOS_HID_SendVendor(const uint8_t *payload, uint16_t len)
{
  uint8_t report[TOS_HID_VENDOR_REPORT_SIZE];
  memset(report, 0, sizeof(report));
  report[0] = TOS_HID_REPORT_ID_VENDOR;

  if (payload != NULL)
  {
    if (len > 63U)
    {
      len = 63U;
    }
    memcpy(&report[1], payload, len);
  }

  return TOS_HID_SendReport(report, sizeof(report));
}

uint8_t TOS_HID_GetLastVendorOut(uint8_t *payload, uint16_t max_len, uint16_t *out_len)
{
  uint16_t n = tos_hid_last_vendor_out_len;

  if (tos_hid_vendor_out_available == 0U)
  {
    return 0U;
  }

  if ((payload != NULL) && (max_len > 0U))
  {
    if (n > max_len)
    {
      n = max_len;
    }
    memcpy(payload, tos_hid_last_vendor_out, n);
  }

  if (out_len != NULL)
  {
    *out_len = n;
  }

  tos_hid_vendor_out_available = 0U;
  return 1U;
}
/* USER CODE END 7 */

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */

/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

