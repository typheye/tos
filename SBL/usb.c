#include "sbl_usb.h"

#include "build.h"
#include "sbl_hw.h"
#include "sbl_state.h"

#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"

#define SBL_USB_VID                 0x0483U
#define SBL_USB_PID                 0x5751U
#define SBL_USB_LANGID              0x0409U
#define SBL_USB_CDC_CONFIG_SIZE       75U
#define SBL_USB_CDC_DATA_IN_EP      0x81U
#define SBL_USB_CDC_DATA_OUT_EP     0x01U
#define SBL_USB_CDC_CMD_EP          0x82U
#define SBL_USB_CDC_DATA_MPS          64U
#define SBL_USB_CDC_CMD_MPS            8U
#define SBL_USB_RX_RING_SIZE         256U
#define SBL_USB_TX_SIZE               96U
#define SBL_USB_LINE_SIZE             80U

#define CDC_REQ_SET_LINE_CODING     0x20U
#define CDC_REQ_GET_LINE_CODING     0x21U
#define CDC_REQ_SET_CONTROL_LINE    0x22U

typedef struct {
  uint8_t rx_buf[SBL_USB_CDC_DATA_MPS];
  uint8_t tx_buf[SBL_USB_TX_SIZE];
  uint8_t ring[SBL_USB_RX_RING_SIZE];
  uint8_t line[SBL_USB_LINE_SIZE];
  volatile uint16_t head;
  volatile uint16_t tail;
  volatile uint8_t tx_busy;
  uint8_t cmd_opcode;
  uint8_t line_coding[7];
} SBL_USB_CDC_Handle;

static USBD_HandleTypeDef sbl_usb_dev;
static SBL_USB_CDC_Handle sbl_cdc;
static uint8_t sbl_usb_started;
static uint8_t sbl_usb_banner_sent;
static volatile uint8_t sbl_usb_unlock_pending;
static volatile uint8_t sbl_usb_reload_pending;

static uint8_t SBL_USBD_CDC_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t SBL_USBD_CDC_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t SBL_USBD_CDC_Setup(USBD_HandleTypeDef *pdev,
                                  USBD_SetupReqTypedef *req);
static uint8_t SBL_USBD_CDC_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t SBL_USBD_CDC_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t SBL_USBD_CDC_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *SBL_USBD_CDC_GetFSCfgDesc(uint16_t *length);
static uint8_t *SBL_USBD_CDC_GetDeviceQualifierDesc(uint16_t *length);

static uint8_t *SBL_USB_DeviceDescriptor(USBD_SpeedTypeDef speed,
                                         uint16_t *length);
static uint8_t *SBL_USB_LangIDStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length);
static uint8_t *SBL_USB_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed,
                                                  uint16_t *length);
static uint8_t *SBL_USB_ProductStrDescriptor(USBD_SpeedTypeDef speed,
                                             uint16_t *length);
static uint8_t *SBL_USB_SerialStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length);
static uint8_t *SBL_USB_ConfigStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length);
static uint8_t *SBL_USB_InterfaceStrDescriptor(USBD_SpeedTypeDef speed,
                                               uint16_t *length);

USBD_ClassTypeDef SBL_USBD_CDC_CLASS SBL_CONST = {
    SBL_USBD_CDC_Init,
    SBL_USBD_CDC_DeInit,
    SBL_USBD_CDC_Setup,
    NULL,
    SBL_USBD_CDC_EP0_RxReady,
    SBL_USBD_CDC_DataIn,
    SBL_USBD_CDC_DataOut,
    NULL,
    NULL,
    NULL,
    SBL_USBD_CDC_GetFSCfgDesc,
    SBL_USBD_CDC_GetFSCfgDesc,
    SBL_USBD_CDC_GetFSCfgDesc,
    SBL_USBD_CDC_GetDeviceQualifierDesc,
};

static USBD_DescriptorsTypeDef SBL_USB_Desc SBL_CONST = {
    SBL_USB_DeviceDescriptor,
    SBL_USB_LangIDStrDescriptor,
    SBL_USB_ManufacturerStrDescriptor,
    SBL_USB_ProductStrDescriptor,
    SBL_USB_SerialStrDescriptor,
    SBL_USB_ConfigStrDescriptor,
    SBL_USB_InterfaceStrDescriptor,
};

__ALIGN_BEGIN static uint8_t sbl_usb_device_desc[USB_LEN_DEV_DESC] SBL_CONST __ALIGN_END = {
    0x12, USB_DESC_TYPE_DEVICE, 0x00, 0x02,
    0x02, 0x02, 0x01, USB_MAX_EP0_SIZE,
    LOBYTE(SBL_USB_VID), HIBYTE(SBL_USB_VID),
    LOBYTE(SBL_USB_PID), HIBYTE(SBL_USB_PID),
    0x00, 0x01,
    USBD_IDX_MFC_STR, USBD_IDX_PRODUCT_STR, USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION,
};

__ALIGN_BEGIN static uint8_t sbl_usb_lang_desc[USB_LEN_LANGID_STR_DESC] SBL_CONST __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC, USB_DESC_TYPE_STRING,
    LOBYTE(SBL_USB_LANGID), HIBYTE(SBL_USB_LANGID),
};

__ALIGN_BEGIN static uint8_t sbl_usb_cfg_desc[SBL_USB_CDC_CONFIG_SIZE] SBL_CONST __ALIGN_END = {
    0x09, USB_DESC_TYPE_CONFIGURATION,
    LOBYTE(SBL_USB_CDC_CONFIG_SIZE), HIBYTE(SBL_USB_CDC_CONFIG_SIZE),
    0x02, 0x01, 0x00, 0xC0, 0x32,

    0x08, USB_DESC_TYPE_IAD, 0x00, 0x02, 0x02, 0x02, 0x01, 0x00,

    0x09, USB_DESC_TYPE_INTERFACE, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    0x05, 0x24, 0x00, 0x10, 0x01,
    0x05, 0x24, 0x01, 0x00, 0x01,
    0x04, 0x24, 0x02, 0x02,
    0x05, 0x24, 0x06, 0x00, 0x01,
    0x07, USB_DESC_TYPE_ENDPOINT, SBL_USB_CDC_CMD_EP, USBD_EP_TYPE_INTR,
    LOBYTE(SBL_USB_CDC_CMD_MPS), HIBYTE(SBL_USB_CDC_CMD_MPS), 0x10,

    0x09, USB_DESC_TYPE_INTERFACE, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    0x07, USB_DESC_TYPE_ENDPOINT, SBL_USB_CDC_DATA_OUT_EP, USBD_EP_TYPE_BULK,
    LOBYTE(SBL_USB_CDC_DATA_MPS), HIBYTE(SBL_USB_CDC_DATA_MPS), 0x00,
    0x07, USB_DESC_TYPE_ENDPOINT, SBL_USB_CDC_DATA_IN_EP, USBD_EP_TYPE_BULK,
    LOBYTE(SBL_USB_CDC_DATA_MPS), HIBYTE(SBL_USB_CDC_DATA_MPS), 0x00,
};

__ALIGN_BEGIN static uint8_t sbl_usb_qualifier_desc[USB_LEN_DEV_QUALIFIER_DESC] SBL_CONST __ALIGN_END = {
    USB_LEN_DEV_QUALIFIER_DESC, USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02, 0x02, 0x02, 0x01, USB_MAX_EP0_SIZE, 0x01, 0x00,
};

__ALIGN_BEGIN static uint8_t sbl_usb_str_desc[64] __ALIGN_END;

static SBL_CODE uint16_t sbl_strlen(const char *s) {
  uint16_t n = 0U;
  while (s && s[n]) {
    n++;
  }
  return n;
}

static SBL_CODE uint8_t sbl_streq(const uint8_t *a, const char *b) {
  uint16_t i = 0U;
  while (b[i]) {
    if (a[i] != (uint8_t)b[i]) {
      return 0U;
    }
    i++;
  }
  return a[i] == 0U;
}

static SBL_CODE void sbl_usb_get_string(const char *ascii, uint16_t *length) {
  uint16_t len = sbl_strlen(ascii);
  if (len > 31U) {
    len = 31U;
  }
  sbl_usb_str_desc[0] = (uint8_t)((len * 2U) + 2U);
  sbl_usb_str_desc[1] = USB_DESC_TYPE_STRING;
  for (uint16_t i = 0U; i < len; i++) {
    sbl_usb_str_desc[2U + (i * 2U)] = (uint8_t)ascii[i];
    sbl_usb_str_desc[3U + (i * 2U)] = 0U;
  }
  *length = sbl_usb_str_desc[0];
}

static SBL_CODE uint8_t *SBL_USB_DeviceDescriptor(USBD_SpeedTypeDef speed,
                                                  uint16_t *length) {
  (void)speed;
  *length = sizeof(sbl_usb_device_desc);
  return sbl_usb_device_desc;
}

static SBL_CODE uint8_t *SBL_USB_LangIDStrDescriptor(USBD_SpeedTypeDef speed,
                                                     uint16_t *length) {
  (void)speed;
  *length = sizeof(sbl_usb_lang_desc);
  return sbl_usb_lang_desc;
}

static SBL_CODE uint8_t *SBL_USB_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed,
                                                           uint16_t *length) {
  (void)speed;
  sbl_usb_get_string("Typheye", length);
  return sbl_usb_str_desc;
}

static SBL_CODE uint8_t *SBL_USB_ProductStrDescriptor(USBD_SpeedTypeDef speed,
                                                      uint16_t *length) {
  (void)speed;
  sbl_usb_get_string("TOS SBL FASTBOOT", length);
  return sbl_usb_str_desc;
}

static SBL_CODE uint8_t *SBL_USB_SerialStrDescriptor(USBD_SpeedTypeDef speed,
                                                     uint16_t *length) {
  (void)speed;
  sbl_usb_get_string("TOS-SBL-0001", length);
  return sbl_usb_str_desc;
}

static SBL_CODE uint8_t *SBL_USB_ConfigStrDescriptor(USBD_SpeedTypeDef speed,
                                                     uint16_t *length) {
  (void)speed;
  sbl_usb_get_string("SBL CDC Config", length);
  return sbl_usb_str_desc;
}

static SBL_CODE uint8_t *SBL_USB_InterfaceStrDescriptor(USBD_SpeedTypeDef speed,
                                                        uint16_t *length) {
  (void)speed;
  sbl_usb_get_string("SBL CDC ACM", length);
  return sbl_usb_str_desc;
}

static SBL_CODE uint8_t *SBL_USBD_CDC_GetFSCfgDesc(uint16_t *length) {
  *length = sizeof(sbl_usb_cfg_desc);
  return sbl_usb_cfg_desc;
}

static SBL_CODE uint8_t *SBL_USBD_CDC_GetDeviceQualifierDesc(uint16_t *length) {
  *length = sizeof(sbl_usb_qualifier_desc);
  return sbl_usb_qualifier_desc;
}

static SBL_CODE void sbl_ring_push(uint8_t v) {
  uint16_t next = (uint16_t)((sbl_cdc.head + 1U) % SBL_USB_RX_RING_SIZE);
  if (next != sbl_cdc.tail) {
    sbl_cdc.ring[sbl_cdc.head] = v;
    sbl_cdc.head = next;
  }
}

static SBL_CODE uint8_t sbl_ring_pop(uint8_t *v) {
  if (sbl_cdc.tail == sbl_cdc.head) {
    return 0U;
  }
  *v = sbl_cdc.ring[sbl_cdc.tail];
  sbl_cdc.tail = (uint16_t)((sbl_cdc.tail + 1U) % SBL_USB_RX_RING_SIZE);
  return 1U;
}

static SBL_CODE uint8_t SBL_USBD_CDC_Init(USBD_HandleTypeDef *pdev,
                                          uint8_t cfgidx) {
  (void)cfgidx;
  sbl_cdc.head = 0U;
  sbl_cdc.tail = 0U;
  sbl_cdc.tx_busy = 0U;
  sbl_cdc.cmd_opcode = 0U;
  sbl_cdc.line_coding[0] = 0x00U;
  sbl_cdc.line_coding[1] = 0xC2U;
  sbl_cdc.line_coding[2] = 0x01U;
  sbl_cdc.line_coding[3] = 0x00U;
  sbl_cdc.line_coding[4] = 0x00U;
  sbl_cdc.line_coding[5] = 0x00U;
  sbl_cdc.line_coding[6] = 0x08U;

  pdev->pClassData = &sbl_cdc;
  pdev->pClassDataCmsit[pdev->classId] = &sbl_cdc;

  (void)USBD_LL_OpenEP(pdev, SBL_USB_CDC_DATA_IN_EP, USBD_EP_TYPE_BULK,
                       SBL_USB_CDC_DATA_MPS);
  (void)USBD_LL_OpenEP(pdev, SBL_USB_CDC_DATA_OUT_EP, USBD_EP_TYPE_BULK,
                       SBL_USB_CDC_DATA_MPS);
  (void)USBD_LL_OpenEP(pdev, SBL_USB_CDC_CMD_EP, USBD_EP_TYPE_INTR,
                       SBL_USB_CDC_CMD_MPS);
  (void)USBD_LL_PrepareReceive(pdev, SBL_USB_CDC_DATA_OUT_EP,
                               sbl_cdc.rx_buf, SBL_USB_CDC_DATA_MPS);
  return (uint8_t)USBD_OK;
}

static SBL_CODE uint8_t SBL_USBD_CDC_DeInit(USBD_HandleTypeDef *pdev,
                                            uint8_t cfgidx) {
  (void)cfgidx;
  (void)USBD_LL_CloseEP(pdev, SBL_USB_CDC_DATA_IN_EP);
  (void)USBD_LL_CloseEP(pdev, SBL_USB_CDC_DATA_OUT_EP);
  (void)USBD_LL_CloseEP(pdev, SBL_USB_CDC_CMD_EP);
  pdev->pClassData = NULL;
  pdev->pClassDataCmsit[pdev->classId] = NULL;
  sbl_cdc.tx_busy = 0U;
  return (uint8_t)USBD_OK;
}

static SBL_CODE uint8_t SBL_USBD_CDC_Setup(USBD_HandleTypeDef *pdev,
                                           USBD_SetupReqTypedef *req) {
  uint16_t status = 0U;

  switch (req->bmRequest & USB_REQ_TYPE_MASK) {
  case USB_REQ_TYPE_CLASS:
    switch (req->bRequest) {
    case CDC_REQ_SET_LINE_CODING:
      sbl_cdc.cmd_opcode = req->bRequest;
      (void)USBD_CtlPrepareRx(pdev, sbl_cdc.line_coding,
                              (req->wLength < 7U) ? req->wLength : 7U);
      break;
    case CDC_REQ_GET_LINE_CODING:
      (void)USBD_CtlSendData(pdev, sbl_cdc.line_coding,
                             (req->wLength < 7U) ? req->wLength : 7U);
      break;
    case CDC_REQ_SET_CONTROL_LINE:
      break;
    default:
      USBD_CtlError(pdev, req);
      return (uint8_t)USBD_FAIL;
    }
    break;
  case USB_REQ_TYPE_STANDARD:
    switch (req->bRequest) {
    case USB_REQ_GET_STATUS:
      (void)USBD_CtlSendData(pdev, (uint8_t *)&status, 2U);
      break;
    case USB_REQ_GET_INTERFACE:
      (void)USBD_CtlSendData(pdev, (uint8_t *)&status, 1U);
      break;
    case USB_REQ_SET_INTERFACE:
      break;
    default:
      USBD_CtlError(pdev, req);
      return (uint8_t)USBD_FAIL;
    }
    break;
  default:
    USBD_CtlError(pdev, req);
    return (uint8_t)USBD_FAIL;
  }

  return (uint8_t)USBD_OK;
}

static SBL_CODE uint8_t SBL_USBD_CDC_EP0_RxReady(USBD_HandleTypeDef *pdev) {
  (void)pdev;
  sbl_cdc.cmd_opcode = 0U;
  return (uint8_t)USBD_OK;
}

static SBL_CODE uint8_t SBL_USBD_CDC_DataIn(USBD_HandleTypeDef *pdev,
                                            uint8_t epnum) {
  (void)pdev;
  (void)epnum;
  sbl_cdc.tx_busy = 0U;
  return (uint8_t)USBD_OK;
}

static SBL_CODE uint8_t SBL_USBD_CDC_DataOut(USBD_HandleTypeDef *pdev,
                                             uint8_t epnum) {
  uint32_t len = USBD_LL_GetRxDataSize(pdev, epnum);
  if (len > SBL_USB_CDC_DATA_MPS) {
    len = SBL_USB_CDC_DATA_MPS;
  }
  for (uint32_t i = 0U; i < len; i++) {
    sbl_ring_push(sbl_cdc.rx_buf[i]);
  }
  (void)USBD_LL_PrepareReceive(pdev, SBL_USB_CDC_DATA_OUT_EP,
                               sbl_cdc.rx_buf, SBL_USB_CDC_DATA_MPS);
  return (uint8_t)USBD_OK;
}

SBL_CODE uint8_t SBL_USB_IsConfigured(void) {
  return (uint8_t)(sbl_usb_started &&
                   sbl_usb_dev.dev_state == USBD_STATE_CONFIGURED);
}

SBL_CODE uint8_t SBL_USB_Write(const uint8_t *data, uint16_t len) {
  if (!SBL_USB_IsConfigured() || !data || len == 0U || sbl_cdc.tx_busy) {
    return 0U;
  }
  if (len > SBL_USB_TX_SIZE) {
    len = SBL_USB_TX_SIZE;
  }
  for (uint16_t i = 0U; i < len; i++) {
    sbl_cdc.tx_buf[i] = data[i];
  }
  sbl_cdc.tx_busy = 1U;
  if (USBD_LL_Transmit(&sbl_usb_dev, SBL_USB_CDC_DATA_IN_EP,
                       sbl_cdc.tx_buf, len) != USBD_OK) {
    sbl_cdc.tx_busy = 0U;
    return 0U;
  }
  return 1U;
}

SBL_CODE uint8_t SBL_USB_WriteText(const char *text) {
  return SBL_USB_Write((const uint8_t *)text, sbl_strlen(text));
}

SBL_CODE uint8_t SBL_USB_WriteTextWait(const char *text) {
  for (uint16_t i = 0U; i < 200U; i++) {
    if (!sbl_cdc.tx_busy) {
      return SBL_USB_WriteText(text);
    }
    SBL_DelayMs(1U);
  }
  return 0U;
}

SBL_CODE uint8_t SBL_USB_Init(void) {
  if (sbl_usb_started) {
    return 1U;
  }

  sbl_usb_banner_sent = 0U;
  if (USBD_Init(&sbl_usb_dev, &SBL_USB_Desc, DEVICE_FS) != USBD_OK) {
    return 0U;
  }
  if (USBD_RegisterClass(&sbl_usb_dev, &SBL_USBD_CDC_CLASS) != USBD_OK) {
    return 0U;
  }
  if (USBD_Start(&sbl_usb_dev) != USBD_OK) {
    return 0U;
  }
  sbl_usb_started = 1U;
  return 1U;
}

SBL_CODE void SBL_USB_DeInit(void) {
  if (!sbl_usb_started) {
    return;
  }
  (void)USBD_Stop(&sbl_usb_dev);
  (void)USBD_DeInit(&sbl_usb_dev);
  sbl_usb_started = 0U;
  sbl_usb_banner_sent = 0U;
  sbl_usb_unlock_pending = 0U;
  sbl_usb_reload_pending = 0U;
}

SBL_CODE uint8_t SBL_USB_ConsumeUnlockRequest(void) {
  if (!sbl_usb_unlock_pending) {
    return 0U;
  }
  sbl_usb_unlock_pending = 0U;
  return 1U;
}

SBL_CODE uint8_t SBL_USB_ConsumeBootloaderReloadRequest(void) {
  if (!sbl_usb_reload_pending) {
    return 0U;
  }
  sbl_usb_reload_pending = 0U;
  return 1U;
}

SBL_CODE void SBL_USB_SendUnlockResult(uint8_t accepted, uint8_t flash_ok) {
  if (accepted) {
    if (flash_ok) {
      SBL_USB_WriteTextWait("OKAY bootloader unlocked\r\n");
    } else {
      SBL_USB_WriteTextWait("FAIL flash write failed\r\n");
    }
  } else {
    SBL_USB_WriteTextWait("FAIL unlock canceled\r\n");
  }
}

static SBL_CODE uint8_t sbl_read_line(uint8_t *line, uint16_t max_len) {
  uint8_t c;
  uint16_t len = 0U;
  while (sbl_ring_pop(&c)) {
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      line[len] = 0U;
      return len > 0U;
    }
    if (len < (uint16_t)(max_len - 1U)) {
      line[len++] = c;
    }
  }
  return 0U;
}

static SBL_CODE void sbl_send_info(void) {
  SBL_USB_WriteText("product_name:" SBL_BUILD_PRODUCT_NAME "\r\n");
  while (sbl_cdc.tx_busy) {
  }
  SBL_USB_WriteText("version:" SBL_BUILD_VERSION "\r\n");
  while (sbl_cdc.tx_busy) {
  }
  SBL_USB_WriteText("version-bootloader:" SBL_BUILD_VERSION_BOOTLOADER "\r\n");
  while (sbl_cdc.tx_busy) {
  }
  SBL_USB_WriteText(SBL_StateUnlocked() ? "unlocked:yes\r\n" : "unlocked:no\r\n");
}

static SBL_CODE void sbl_handle_command(const uint8_t *line) {
  if (sbl_streq(line, "PING") || sbl_streq(line, "ping")) {
    SBL_USB_WriteText("PONG\r\n");
  } else if (sbl_streq(line, "INFO") || sbl_streq(line, "info")) {
    sbl_send_info();
  } else if (sbl_streq(line, "REBOOT") || sbl_streq(line, "reboot")) {
    SBL_USB_WriteText("OK rebooting\r\n");
    SBL_DelayMs(80U);
    SBL_SystemReboot();
  } else if (sbl_streq(line, "OEM UNLOCK") ||
             sbl_streq(line, "oem unlock")) {
    if (SBL_StateUnlocked()) {
      SBL_USB_WriteText("INFO bootloader already unlocked\r\n");
      while (sbl_cdc.tx_busy) {
      }
      SBL_USB_WriteText("OKAY already unlocked\r\n");
    } else {
      sbl_usb_unlock_pending = 1U;
      SBL_USB_WriteText("INFO unlock confirmation required\r\n");
    }
  } else if (sbl_streq(line, "OEM LOCK") ||
             sbl_streq(line, "oem lock")) {
    if (!SBL_StateUnlocked()) {
      SBL_USB_WriteText("INFO bootloader already locked\r\n");
      while (sbl_cdc.tx_busy) {
      }
      SBL_USB_WriteText("OKAY already locked\r\n");
    } else if (SBL_StateSetUnlocked(0U)) {
      SBL_USB_WriteTextWait("OKAY bootloader locked\r\n");
      sbl_usb_reload_pending = 1U;
    } else {
      SBL_USB_WriteTextWait("FAIL flash write failed\r\n");
    }
  } else {
    SBL_USB_WriteText("ERR unknown command\r\n");
  }
}

SBL_CODE void SBL_USB_Tick(void) {
  if (!SBL_USB_IsConfigured()) {
    sbl_usb_banner_sent = 0U;
    return;
  }
  if (!sbl_usb_banner_sent && !sbl_cdc.tx_busy) {
    sbl_usb_banner_sent = SBL_USB_WriteText("TOS-SBL CDC READY\r\n");
  }
  if (!sbl_cdc.tx_busy && sbl_read_line(sbl_cdc.line, sizeof(sbl_cdc.line))) {
    sbl_handle_command(sbl_cdc.line);
  }
}
