/**
 ******************************************************************************
 * @file    usb.c
 * @author  Typheye
 * @brief   SBL USB CDC fastboot protocol implementation.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "usb.h"

#include "build.h"
#include "flash.h"
#include "hw.h"
#include "state.h"

#include "stm32f407xx.h"

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
} SBL_USB_CDC_Handle_t;

typedef struct {
  SBL_FlashSession_t session;
  const SBL_FlashPartition_t *part;
  uint8_t *chunk_buf;
  uint32_t chunk_len;
  uint32_t chunk_received;
  uint32_t chunk_crc;
  uint32_t chunk_offset;
  uint8_t rx_raw;
  uint8_t active;
} SBL_USB_FlashContext_t;

static USBD_HandleTypeDef sbl_usb_dev;
static SBL_USB_CDC_Handle_t sbl_cdc;
static SBL_USB_FlashContext_t sbl_flash_ctx;
static uint8_t sbl_flash_chunk[SBL_FLASH_CHUNK_SIZE] __attribute__((aligned(4)));
static uint8_t sbl_usb_started;
static uint8_t sbl_usb_banner_sent;

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

static SBL_CODE uint8_t sbl_is_space(uint8_t c) {
  return c == ' ' || c == '\t';
}

static SBL_CODE uint32_t sbl_parse_u32(const uint8_t *text, uint32_t *out) {
  uint32_t value = 0U;
  uint32_t i = 0U;
  if (!text || !out || text[0] == 0U) {
    return 0U;
  }
  while (text[i] >= '0' && text[i] <= '9') {
    value = (value * 10U) + (uint32_t)(text[i] - '0');
    i++;
  }
  if (i == 0U) {
    return 0U;
  }
  *out = value;
  return i;
}

static SBL_CODE uint32_t sbl_parse_hex32(const uint8_t *text, uint32_t *out) {
  uint32_t value = 0U;
  uint32_t i = 0U;
  if (!text || !out || text[0] == 0U) {
    return 0U;
  }
  if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    i = 2U;
  }
  {
    uint32_t digits = 0U;
    while (1) {
      uint8_t c = text[i];
      uint8_t nibble;
      if (c >= '0' && c <= '9') {
        nibble = (uint8_t)(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        nibble = (uint8_t)(c - 'a' + 10U);
      } else if (c >= 'A' && c <= 'F') {
        nibble = (uint8_t)(c - 'A' + 10U);
      } else {
        break;
      }
      value = (value << 4) | nibble;
      i++;
      digits++;
    }
    if (digits == 0U) {
      return 0U;
    }
  }
  *out = value;
  return i;
}

static SBL_CODE uint8_t sbl_hex_to_bin(const uint8_t *hex, uint8_t *bin,
                                        uint32_t bin_len) {
  uint32_t i;
  if (!hex || !bin || bin_len == 0U) return 0U;
  for (i = 0U; i < bin_len; ++i) {
    uint8_t hi = hex[i * 2U];
    uint8_t lo = hex[i * 2U + 1U];
    uint8_t nib_hi, nib_lo;
    if (hi >= '0' && hi <= '9') {
      nib_hi = (uint8_t)(hi - '0');
    } else if (hi >= 'a' && hi <= 'f') {
      nib_hi = (uint8_t)(hi - 'a' + 10U);
    } else if (hi >= 'A' && hi <= 'F') {
      nib_hi = (uint8_t)(hi - 'A' + 10U);
    } else {
      return 0U;
    }
    if (lo >= '0' && lo <= '9') {
      nib_lo = (uint8_t)(lo - '0');
    } else if (lo >= 'a' && lo <= 'f') {
      nib_lo = (uint8_t)(lo - 'a' + 10U);
    } else if (lo >= 'A' && lo <= 'F') {
      nib_lo = (uint8_t)(lo - 'A' + 10U);
    } else {
      return 0U;
    }
    bin[i] = (uint8_t)((nib_hi << 4U) | nib_lo);
  }
  return 1U;
}

static SBL_CODE uint8_t sbl_secure_memcmp(const uint8_t *a, const uint8_t *b,
                                           uint32_t len) {
  uint8_t diff = 0U;
  for (uint32_t i = 0U; i < len; ++i) {
    diff |= a[i] ^ b[i];
  }
  return diff == 0U ? 1U : 0U;
}

static SBL_CODE const uint8_t *sbl_skip_space(const uint8_t *p) {
  while (p && sbl_is_space(*p)) {
    p++;
  }
  return p;
}

static SBL_CODE void sbl_copy_token(char *dst, uint32_t dst_len,
                                    const uint8_t *src, uint32_t len) {
  uint32_t n = (len + 1U < dst_len) ? len : (dst_len - 1U);
  for (uint32_t i = 0U; i < n; ++i) {
    dst[i] = (char)src[i];
  }
  dst[n] = 0;
}

static SBL_CODE void sbl_hex32(char *dst, uint32_t val) {
  for (int i = 7; i >= 0; --i) {
    uint8_t nib = (uint8_t)(val & 0xFU);
    dst[i] = (char)(nib + (nib < 10U ? '0' : 'A' - 10));
    val >>= 4;
  }
}

static SBL_CODE void sbl_flash_reset(void) {
  sbl_flash_ctx.chunk_buf = 0;
  SBL_FlashAbort(&sbl_flash_ctx.session);
  sbl_flash_ctx.part = 0;
  sbl_flash_ctx.chunk_len = 0U;
  sbl_flash_ctx.chunk_received = 0U;
  sbl_flash_ctx.chunk_crc = 0U;
  sbl_flash_ctx.chunk_offset = 0U;
  sbl_flash_ctx.rx_raw = 0U;
  sbl_flash_ctx.active = 0U;
}

static SBL_CODE uint8_t sbl_flash_parse_begin(const uint8_t *line) {
  const uint8_t *p = line + 10U;
  const uint8_t *name_start;
  uint32_t name_len = 0U;
  uint32_t size = 0U;
  uint32_t crc = 0U;
  char part_name[16];

  if (!SBL_StateUnlocked()) {
    SBL_USB_WriteText("FAIL bootloader locked\r\n");
    return 1U;
  }

  p = sbl_skip_space(p);
  name_start = p;
  while (*p && !sbl_is_space(*p)) {
    p++;
    name_len++;
  }
  if (name_len == 0U) {
    SBL_USB_WriteText("FAIL missing partition\r\n");
    return 1U;
  }
  sbl_copy_token(part_name, sizeof(part_name), name_start, name_len);
  p = sbl_skip_space(p);
  p += sbl_parse_u32(p, &size);
  p = sbl_skip_space(p);
  if (size == 0U || sbl_parse_hex32(p, &crc) == 0U) {
    SBL_USB_WriteText("FAIL bad flash begin args\r\n");
    return 1U;
  }

  sbl_flash_reset();
  sbl_flash_ctx.part = SBL_FlashFindPartition(part_name);
  if (!sbl_flash_ctx.part) {
    SBL_USB_WriteText("FAIL unknown partition\r\n");
    return 1U;
  }
  if (!sbl_flash_ctx.part->allow_flash) {
    SBL_USB_WriteText("FAIL flash disabled for partition\r\n");
    return 1U;
  }
  if (!SBL_FlashBegin(&sbl_flash_ctx.session, sbl_flash_ctx.part, size, crc)) {
    SBL_USB_WriteText("FAIL flash begin rejected\r\n");
    return 1U;
  }
  sbl_flash_ctx.chunk_buf = sbl_flash_chunk;
  sbl_flash_ctx.active = 1U;
  SBL_USB_WriteText("OKAY READY 16384\r\n");
  return 1U;
}

static SBL_CODE uint8_t sbl_flash_parse_data(const uint8_t *line) {
  const uint8_t *p = line + 9U;
  uint32_t len = 0U;
  uint32_t offset = 0U;
  uint32_t crc = 0U;

  if (!sbl_flash_ctx.active) {
    SBL_USB_WriteText("FAIL no active flash\r\n");
    return 1U;
  }
  p = sbl_skip_space(p);
  {
    uint32_t n = sbl_parse_u32(p, &len);
    if (n == 0U) {
      SBL_USB_WriteText("FAIL bad chunk length\r\n");
      return 1U;
    }
    p += n;
  }
  p = sbl_skip_space(p);
  {
    uint32_t n = sbl_parse_u32(p, &offset);
    if (n == 0U) {
      SBL_USB_WriteText("FAIL bad chunk offset\r\n");
      return 1U;
    }
    p += n;
  }
  p = sbl_skip_space(p);
  if (sbl_parse_hex32(p, &crc) == 0U) {
    SBL_USB_WriteText("FAIL bad chunk crc\r\n");
    return 1U;
  }
  if (len == 0U || len > SBL_FlashChunkSize() || (len & 3U) != 0U) {
    SBL_USB_WriteText("FAIL invalid chunk size\r\n");
    return 1U;
  }
  sbl_flash_ctx.chunk_len = len;
  sbl_flash_ctx.chunk_received = 0U;
  sbl_flash_ctx.chunk_crc = crc;
  sbl_flash_ctx.chunk_offset = offset;
  sbl_flash_ctx.rx_raw = 1U;
  SBL_USB_WriteText("OKAY SEND\r\n");
  return 1U;
}

static SBL_CODE uint8_t sbl_flash_parse_erase(const uint8_t *line) {
  const uint8_t *p = line + 5U;
  const uint8_t *name_start;
  uint32_t name_len = 0U;
  char part_name[16];
  const SBL_FlashPartition_t *part;

  if (!SBL_StateUnlocked()) {
    SBL_USB_WriteText("FAIL bootloader locked\r\n");
    return 1U;
  }
  p = sbl_skip_space(p);
  name_start = p;
  while (*p && !sbl_is_space(*p)) {
    p++;
    name_len++;
  }
  if (name_len == 0U) {
    SBL_USB_WriteText("FAIL missing partition\r\n");
    return 1U;
  }
  sbl_copy_token(part_name, sizeof(part_name), name_start, name_len);
  part = SBL_FlashFindPartition(part_name);
  if (!part) {
    SBL_USB_WriteText("FAIL unknown partition\r\n");
    return 1U;
  }
  if (!part->allow_erase) {
    SBL_USB_WriteText("FAIL erase disabled for partition\r\n");
    return 1U;
  }
  if (!SBL_FlashErasePartition(part)) {
    SBL_USB_WriteText("FAIL erase failed\r\n");
    return 1U;
  }
  SBL_USB_WriteText("OKAY ERASED\r\n");
  return 1U;
}

static SBL_CODE uint8_t sbl_flash_parse_end(void) {
  uint8_t requires_reset;
  if (!sbl_flash_ctx.active) {
    SBL_USB_WriteText("FAIL no active flash\r\n");
    return 1U;
  }
  if (!SBL_FlashFinalize(&sbl_flash_ctx.session)) {
    sbl_flash_reset();
    SBL_USB_WriteText("FAIL flash verify failed\r\n");
    return 1U;
  }
  requires_reset = sbl_flash_ctx.session.requires_reset;
  sbl_flash_reset();
  SBL_USB_WriteTextWait(requires_reset ? "OKAY STAGED; REBOOTING\r\n"
                                       : "OKAY FLASHED\r\n");
  if (requires_reset) {
    SBL_DelayMs(100U);
    SBL_SystemRebootTo(SBL_BOOT_TARGET_FASTBOOT);
  }
  return 1U;
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
  sbl_flash_reset();

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
    if (sbl_flash_ctx.rx_raw) {
      if (sbl_flash_ctx.chunk_received < sbl_flash_ctx.chunk_len &&
          sbl_flash_ctx.chunk_buf) {
        sbl_flash_ctx.chunk_buf[sbl_flash_ctx.chunk_received++] = sbl_cdc.rx_buf[i];
      }
    } else {
      sbl_ring_push(sbl_cdc.rx_buf[i]);
    }
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
    (void)USBD_DeInit(&sbl_usb_dev);
    return 0U;
  }
  if (USBD_RegisterClass(&sbl_usb_dev, &SBL_USBD_CDC_CLASS) != USBD_OK) {
    (void)USBD_DeInit(&sbl_usb_dev);
    return 0U;
  }
  if (USBD_Start(&sbl_usb_dev) != USBD_OK) {
    (void)USBD_Stop(&sbl_usb_dev);
    (void)USBD_DeInit(&sbl_usb_dev);
    return 0U;
  }
  sbl_usb_started = 1U;
  return 1U;
}

SBL_CODE void SBL_USB_DisconnectPulse(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  (void)RCC->AHB1ENR;

  GPIOA->MODER &= ~(3UL << (12U * 2U));
  GPIOA->MODER |=  (1UL << (12U * 2U));
  GPIOA->OTYPER &= ~(1UL << 12U);
  GPIOA->PUPDR &= ~(3UL << (12U * 2U));
  GPIOA->BSRR = (1UL << (12U + 16U));
  SBL_DelayMs(80U);
}

SBL_CODE void SBL_USB_DeInit(void) {
  if (sbl_usb_started) {
    (void)USBD_Stop(&sbl_usb_dev);
    (void)USBD_DeInit(&sbl_usb_dev);
  }
  sbl_usb_started = 0U;
  sbl_usb_banner_sent = 0U;
  sbl_flash_reset();
}

SBL_CODE uint8_t SBL_USB_IsBusy(void) {
  return (uint8_t)(sbl_flash_ctx.active || sbl_flash_ctx.rx_raw);
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
  while (sbl_cdc.tx_busy) {}
  SBL_USB_WriteText("version-bootloader:" SBL_BUILD_VERSION_BOOTLOADER "\r\n");
  while (sbl_cdc.tx_busy) {}
  SBL_USB_WriteText("version-baseband:" SBL_BUILD_VERSION_BASEBAND "\r\n");
  while (sbl_cdc.tx_busy) {}
  /* STM32 96-bit UID as hex serial */
  {
    char serial[25];
    const uint32_t *uid = (const uint32_t *)0x1FFF7A10U;
    sbl_hex32(serial + 0, uid[0]);
    sbl_hex32(serial + 8, uid[1]);
    sbl_hex32(serial + 16, uid[2]);
    serial[24] = '\0';
    SBL_USB_WriteText("serialno:");
    while (sbl_cdc.tx_busy) {}
    SBL_USB_WriteText(serial);
    while (sbl_cdc.tx_busy) {}
    SBL_USB_WriteText("\r\n");
    while (sbl_cdc.tx_busy) {}
  }
  SBL_USB_WriteText(SBL_StateUnlocked() ? "unlocked:yes\r\n" : "unlocked:no\r\n");
  while (sbl_cdc.tx_busy) {}
}
static SBL_CODE void sbl_send_value(const char *value) {
  SBL_USB_WriteTextWait("OKAY");
  SBL_USB_WriteTextWait(value ? value : "");
  SBL_USB_WriteTextWait("\r\n");
}

static SBL_CODE void sbl_serial(char serial[25]) {
  const uint32_t *uid = (const uint32_t *)0x1FFF7A10U;
  sbl_hex32(serial + 0, uid[0]);
  sbl_hex32(serial + 8, uid[1]);
  sbl_hex32(serial + 16, uid[2]);
  serial[24] = '\0';
}

static SBL_CODE void sbl_send_partition_info(const SBL_FlashPartition_t *part) {
  char size[11] = "0x00000000";
  sbl_hex32(size + 2, part->size);
  SBL_USB_WriteTextWait("INFOpartition:");
  SBL_USB_WriteTextWait(part->name);
  SBL_USB_WriteTextWait(":size:");
  SBL_USB_WriteTextWait(size);
  SBL_USB_WriteTextWait(part->allow_flash ? ":flash:yes" : ":flash:no");
  SBL_USB_WriteTextWait(part->allow_erase ? ":erase:yes" : ":erase:no");
  SBL_USB_WriteTextWait(part->staged ? ":staged:yes\r\n" : ":staged:no\r\n");
}

static SBL_CODE void sbl_send_partitions(void) {
  for (uint32_t i = 0U; i < SBL_FlashPartitionCount(); ++i)
    sbl_send_partition_info(SBL_FlashPartitionAt(i));
  SBL_USB_WriteTextWait("OKAY\r\n");
}

static SBL_CODE void sbl_send_getvar(const char *name) {
  char serial[25];
  if (sbl_streq((const uint8_t *)name, "product"))
    sbl_send_value(SBL_BUILD_PRODUCT_NAME);
  else if (sbl_streq((const uint8_t *)name, "version"))
    sbl_send_value(SBL_BUILD_VERSION);
  else if (sbl_streq((const uint8_t *)name, "version-bootloader"))
    sbl_send_value(SBL_BUILD_VERSION_BOOTLOADER);
  else if (sbl_streq((const uint8_t *)name, "version-baseband"))
    sbl_send_value(SBL_BUILD_VERSION_BASEBAND);
  else if (sbl_streq((const uint8_t *)name, "serialno")) {
    sbl_serial(serial); sbl_send_value(serial);
  } else if (sbl_streq((const uint8_t *)name, "unlocked"))
    sbl_send_value(SBL_StateUnlocked() ? "yes" : "no");
  else if (sbl_streq((const uint8_t *)name, "all"))
    sbl_send_partitions();
  else {
    static const char prefix[] = "partition-size:";
    uint32_t i = 0U;
    while (prefix[i] && name[i] == prefix[i]) ++i;
    if (!prefix[i]) {
      const SBL_FlashPartition_t *part = SBL_FlashFindPartition(name + i);
      if (part) {
        char size[11] = "0x00000000";
        sbl_hex32(size + 2, part->size); sbl_send_value(size); return;
      }
    }
    SBL_USB_WriteTextWait("FAILunknown variable\r\n");
  }
}


static SBL_CODE void sbl_handle_command(const uint8_t *line) {
  if ((line[0] == 'F' || line[0] == 'f') &&
      (line[1] == 'L' || line[1] == 'l') &&
      (line[2] == 'A' || line[2] == 'a') &&
      (line[3] == 'S' || line[3] == 's') &&
      (line[4] == 'H' || line[4] == 'h') &&
      (line[5] == 'B' || line[5] == 'b')) {
    (void)sbl_flash_parse_begin(line);
  } else if ((line[0] == 'F' || line[0] == 'f') &&
             (line[1] == 'L' || line[1] == 'l') &&
             (line[2] == 'A' || line[2] == 'a') &&
             (line[3] == 'S' || line[3] == 's') &&
             (line[4] == 'H' || line[4] == 'h') &&
             (line[5] == 'D' || line[5] == 'd')) {
    (void)sbl_flash_parse_data(line);
  } else if ((line[0] == 'E' || line[0] == 'e') &&
             (line[1] == 'R' || line[1] == 'r') &&
             (line[2] == 'A' || line[2] == 'a') &&
             (line[3] == 'S' || line[3] == 's') &&
             (line[4] == 'E' || line[4] == 'e')) {
    (void)sbl_flash_parse_erase(line);
  } else if (sbl_streq(line, "FLASHEND") || sbl_streq(line, "flashend")) {
    (void)sbl_flash_parse_end();
  } else if ((line[0] == 'G' || line[0] == 'g') &&
             (line[1] == 'E' || line[1] == 'e') &&
             (line[2] == 'T' || line[2] == 't') &&
             (line[3] == 'V' || line[3] == 'v') &&
             (line[4] == 'A' || line[4] == 'a') &&
             (line[5] == 'R' || line[5] == 'r') && line[6] == ' ') {
    sbl_send_getvar((const char *)(line + 7U));
  } else if (sbl_streq(line, "OEM PARTITIONS") ||
             sbl_streq(line, "oem partitions")) {
    sbl_send_partitions();
  } else if (sbl_streq(line, "INFO") || sbl_streq(line, "info")) {
    sbl_send_info();
  } else if (sbl_streq(line, "REBOOT RECOVERY") ||
             sbl_streq(line, "reboot recovery")) {
    SBL_USB_WriteTextWait("OK rebooting recovery\r\n");
    SBL_DelayMs(80U);
    SBL_SystemRebootTo(SBL_BOOT_TARGET_RECOVERY);
  } else if (sbl_streq(line, "REBOOT BOOTLOADER") ||
             sbl_streq(line, "reboot bootloader")) {
    SBL_USB_WriteTextWait("OK rebooting bootloader\r\n");
    SBL_DelayMs(80U);
    SBL_SystemRebootTo(SBL_BOOT_TARGET_FASTBOOT);
  } else if (sbl_streq(line, "REBOOT") || sbl_streq(line, "reboot")) {
    SBL_USB_WriteTextWait("OK rebooting\r\n");
    SBL_DelayMs(80U);
    SBL_SystemReboot();
  } else if (sbl_streq(line, "OEM UNLOCK") ||
             sbl_streq(line, "oem unlock")) {
    if (SBL_StateUnlocked()) {
      SBL_USB_WriteTextWait("INFO bootloader already unlocked\r\n");
      SBL_USB_WriteTextWait("OKAY already unlocked\r\n");
    } else if (SBL_StateSetUnlocked(1U)) {
      SBL_USB_WriteTextWait("OKAY bootloader unlocked\r\n");
      SBL_DelayMs(120U);
      SBL_SystemReboot();
    } else {
      SBL_USB_WriteTextWait("FAIL flash write failed\r\n");
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
      SBL_DelayMs(120U);
      SBL_SystemReboot();
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
  if (sbl_flash_ctx.rx_raw && sbl_flash_ctx.chunk_received >= sbl_flash_ctx.chunk_len) {
    sbl_flash_ctx.rx_raw = 0U;
    if (!SBL_FlashWriteChunk(&sbl_flash_ctx.session,
                             sbl_flash_ctx.chunk_offset,
                             sbl_flash_ctx.chunk_buf,
                             sbl_flash_ctx.chunk_len,
                             sbl_flash_ctx.chunk_crc)) {
      sbl_flash_reset();
      SBL_USB_WriteText("FAIL flash write failed\r\n");
      return;
    }
    SBL_USB_WriteTextWait("OKAY DATA\r\n");
    return;
  }
  if (!sbl_cdc.tx_busy && sbl_read_line(sbl_cdc.line, sizeof(sbl_cdc.line))) {
    sbl_handle_command(sbl_cdc.line);
  }
}
