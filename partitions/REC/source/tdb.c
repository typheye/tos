#include "rec_tdb.h"

#include "ff.h"
#include "common.h"
#include "hw.h"
#include "manifest.h"
#include "stm32f407xx.h"

#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"

#define TDB_USB_VID                 0x0483U
#define TDB_USB_PID                 0x5756U
#define TDB_USB_LANGID              0x0409U
#define TDB_USB_CDC_CONFIG_SIZE       75U
#define TDB_USB_CDC_DATA_IN_EP      0x81U
#define TDB_USB_CDC_DATA_OUT_EP     0x01U
#define TDB_USB_CDC_CMD_EP          0x82U
#define TDB_USB_CDC_DATA_MPS          64U
#define TDB_USB_CDC_CMD_MPS            8U
#define TDB_USB_RX_RING_SIZE        2048U
#define TDB_USB_LINE_SIZE            384U
#define TDB_USB_TX_SIZE               64U
#define TDB_RAW_CHUNK_SIZE          1024U
#define TDB_FILE_BUFFER_SIZE        1024U
#define TDB_OUTPUT_SIZE             4096U
#define TDB_PATH_SIZE                128U
#define TDB_ARG_MAX                   10U
#define TDB_TX_TIMEOUT_MS           3000U

#define CDC_REQ_SET_LINE_CODING     0x20U
#define CDC_REQ_GET_LINE_CODING     0x21U
#define CDC_REQ_SET_CONTROL_LINE    0x22U

#define TDB_FLASH_RECORD_MAGIC      0x544F5301UL
#define TDB_SETTINGS_MAGIC          0x544F5300UL
#define TDB_SETTINGS_AREA_SIZE      TOS_USERDATA_SETTINGS_SIZE
#define TDB_SETTINGS_MAX_RECORD     4096U

#define TDB_ALIGN4(v) (((v) + 3U) & ~3U)

typedef struct {
  uint8_t rx_buf[TDB_USB_CDC_DATA_MPS];
  uint8_t tx_buf[TDB_USB_TX_SIZE];
  uint8_t ring[TDB_USB_RX_RING_SIZE];
  uint8_t line[TDB_USB_LINE_SIZE];
  volatile uint16_t head;
  volatile uint16_t tail;
  uint16_t line_len;
  uint8_t line_overflow;
  volatile uint8_t tx_busy;
  uint8_t cmd_opcode;
  uint8_t line_coding[7];
} TDB_CDC_Handle;

typedef struct {
  FIL file;
  char final_path[TDB_PATH_SIZE];
  char temp_path[TDB_PATH_SIZE];
  uint32_t expected_size;
  uint32_t expected_crc;
  uint32_t received;
  uint32_t running_crc;
  uint32_t chunk_len;
  uint32_t chunk_received;
  uint32_t chunk_crc;
  uint32_t chunk_offset;
  uint8_t active;
  volatile uint8_t rx_raw;
} TDB_PushContext;

typedef struct __attribute__((packed, aligned(4))) {
  uint32_t magic;
  uint32_t crc;
  uint32_t datasize;
} TDB_FlashRecordHeader;

typedef struct __attribute__((packed)) {
  char ssid[24];
  char pwd[32];
} TDB_SavedNet;

typedef struct __attribute__((packed, aligned(4))) {
  uint32_t magic;
  uint32_t crc;
  uint8_t disp_auto;
  uint8_t disp_bright;
  uint8_t disp_dir;
  char wlan_ssid[24];
  char wlan_pwd[32];
  char hs_ssid[24];
  char hs_pwd[32];
  uint8_t wlan_on;
  uint8_t wlan_auto_conn;
  uint8_t debug_dashboard;
  uint8_t debug_log_com;
  uint8_t pad1[1];
  uint8_t saved_count;
  TDB_SavedNet saved[10];
  uint8_t time_auto_sync;
  uint8_t time_style_24h;
  uint8_t hotspot_auto_close;
  uint8_t boot_gfx;
  uint8_t pad2[2];
  char hotspot_ip[16];
} TDB_Settings;

_Static_assert(sizeof(TDB_Settings) == 712U, "TDB settings layout mismatch");

static USBD_HandleTypeDef tdb_usb_dev;
static TDB_CDC_Handle tdb_cdc;
static TDB_PushContext tdb_push;
static uint8_t tdb_raw_buf[TDB_RAW_CHUNK_SIZE] __attribute__((aligned(4)));
static uint8_t tdb_file_buf[TDB_FILE_BUFFER_SIZE] __attribute__((aligned(4)));
static char tdb_output[TDB_OUTPUT_SIZE];
static char tdb_cwd[TDB_PATH_SIZE] = "0:/";
static uint32_t tdb_output_len;
static uint8_t tdb_output_truncated;
static uint8_t tdb_usb_started;
static uint8_t tdb_banner_sent;
static uint8_t tdb_reboot_pending;
static uint8_t tdb_str_desc[64] __attribute__((aligned(4)));

static uint8_t TDB_USBD_CDC_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t TDB_USBD_CDC_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t TDB_USBD_CDC_Setup(USBD_HandleTypeDef *pdev,
                                  USBD_SetupReqTypedef *req);
static uint8_t TDB_USBD_CDC_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t TDB_USBD_CDC_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t TDB_USBD_CDC_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *TDB_USBD_CDC_GetFSCfgDesc(uint16_t *length);
static uint8_t *TDB_USBD_CDC_GetDeviceQualifierDesc(uint16_t *length);

static uint8_t *TDB_USB_DeviceDescriptor(USBD_SpeedTypeDef speed,
                                         uint16_t *length);
static uint8_t *TDB_USB_LangIDStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length);
static uint8_t *TDB_USB_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed,
                                                  uint16_t *length);
static uint8_t *TDB_USB_ProductStrDescriptor(USBD_SpeedTypeDef speed,
                                             uint16_t *length);
static uint8_t *TDB_USB_SerialStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length);
static uint8_t *TDB_USB_ConfigStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length);
static uint8_t *TDB_USB_InterfaceStrDescriptor(USBD_SpeedTypeDef speed,
                                               uint16_t *length);

static const USBD_ClassTypeDef TDB_USBD_CDC_CLASS REC_CONST = {
    TDB_USBD_CDC_Init,
    TDB_USBD_CDC_DeInit,
    TDB_USBD_CDC_Setup,
    NULL,
    TDB_USBD_CDC_EP0_RxReady,
    TDB_USBD_CDC_DataIn,
    TDB_USBD_CDC_DataOut,
    NULL,
    NULL,
    NULL,
    TDB_USBD_CDC_GetFSCfgDesc,
    TDB_USBD_CDC_GetFSCfgDesc,
    TDB_USBD_CDC_GetFSCfgDesc,
    TDB_USBD_CDC_GetDeviceQualifierDesc,
};

static const USBD_DescriptorsTypeDef TDB_USB_Desc REC_CONST = {
    TDB_USB_DeviceDescriptor,
    TDB_USB_LangIDStrDescriptor,
    TDB_USB_ManufacturerStrDescriptor,
    TDB_USB_ProductStrDescriptor,
    TDB_USB_SerialStrDescriptor,
    TDB_USB_ConfigStrDescriptor,
    TDB_USB_InterfaceStrDescriptor,
};

__ALIGN_BEGIN static const uint8_t tdb_usb_device_desc[USB_LEN_DEV_DESC] REC_CONST __ALIGN_END = {
    0x12, USB_DESC_TYPE_DEVICE, 0x00, 0x02,
    0x02, 0x02, 0x01, USB_MAX_EP0_SIZE,
    LOBYTE(TDB_USB_VID), HIBYTE(TDB_USB_VID),
    LOBYTE(TDB_USB_PID), HIBYTE(TDB_USB_PID),
    0x00, 0x01,
    USBD_IDX_MFC_STR, USBD_IDX_PRODUCT_STR, USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION,
};

__ALIGN_BEGIN static const uint8_t tdb_usb_lang_desc[USB_LEN_LANGID_STR_DESC] REC_CONST __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC, USB_DESC_TYPE_STRING,
    LOBYTE(TDB_USB_LANGID), HIBYTE(TDB_USB_LANGID),
};

__ALIGN_BEGIN static const uint8_t tdb_usb_cfg_desc[TDB_USB_CDC_CONFIG_SIZE] REC_CONST __ALIGN_END = {
    0x09, USB_DESC_TYPE_CONFIGURATION,
    LOBYTE(TDB_USB_CDC_CONFIG_SIZE), HIBYTE(TDB_USB_CDC_CONFIG_SIZE),
    0x02, 0x01, 0x00, 0xC0, 0x32,

    0x08, USB_DESC_TYPE_IAD, 0x00, 0x02, 0x02, 0x02, 0x01, 0x00,

    0x09, USB_DESC_TYPE_INTERFACE, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    0x05, 0x24, 0x00, 0x10, 0x01,
    0x05, 0x24, 0x01, 0x00, 0x01,
    0x04, 0x24, 0x02, 0x02,
    0x05, 0x24, 0x06, 0x00, 0x01,
    0x07, USB_DESC_TYPE_ENDPOINT, TDB_USB_CDC_CMD_EP, USBD_EP_TYPE_INTR,
    LOBYTE(TDB_USB_CDC_CMD_MPS), HIBYTE(TDB_USB_CDC_CMD_MPS), 0x10,

    0x09, USB_DESC_TYPE_INTERFACE, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    0x07, USB_DESC_TYPE_ENDPOINT, TDB_USB_CDC_DATA_OUT_EP, USBD_EP_TYPE_BULK,
    LOBYTE(TDB_USB_CDC_DATA_MPS), HIBYTE(TDB_USB_CDC_DATA_MPS), 0x00,
    0x07, USB_DESC_TYPE_ENDPOINT, TDB_USB_CDC_DATA_IN_EP, USBD_EP_TYPE_BULK,
    LOBYTE(TDB_USB_CDC_DATA_MPS), HIBYTE(TDB_USB_CDC_DATA_MPS), 0x00,
};

__ALIGN_BEGIN static const uint8_t tdb_usb_qualifier_desc[USB_LEN_DEV_QUALIFIER_DESC] REC_CONST __ALIGN_END = {
    USB_LEN_DEV_QUALIFIER_DESC, USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02, 0x02, 0x02, 0x01, USB_MAX_EP0_SIZE, 0x01, 0x00,
};

static uint16_t tdb_strlen(const char *s) {
  uint16_t n = 0U;
  while (s && s[n]) n++;
  return n;
}

static uint8_t tdb_ascii_equal(const char *a, const char *b) {
  uint32_t i = 0U;
  if (!a || !b) return 0U;
  while (a[i] && b[i]) {
    char ca = a[i];
    char cb = b[i];
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + ('a' - 'A'));
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + ('a' - 'A'));
    if (ca != cb) return 0U;
    i++;
  }
  return (uint8_t)(a[i] == 0 && b[i] == 0);
}

static uint8_t tdb_ascii_starts(const uint8_t *text, const char *prefix) {
  uint32_t i = 0U;
  while (prefix[i]) {
    uint8_t a = text[i];
    uint8_t b = (uint8_t)prefix[i];
    if (a >= 'A' && a <= 'Z') a = (uint8_t)(a + ('a' - 'A'));
    if (b >= 'A' && b <= 'Z') b = (uint8_t)(b + ('a' - 'A'));
    if (a != b) return 0U;
    i++;
  }
  return 1U;
}

static void tdb_memzero(void *dst, uint32_t len) {
  uint8_t *p = (uint8_t *)dst;
  while (len--) *p++ = 0U;
}

static void tdb_memcpy(void *dst, const void *src, uint32_t len) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  while (len--) *d++ = *s++;
}

static uint8_t tdb_memeq(const void *a, const void *b, uint32_t len) {
  const uint8_t *pa = (const uint8_t *)a;
  const uint8_t *pb = (const uint8_t *)b;
  while (len--) {
    if (*pa++ != *pb++) return 0U;
  }
  return 1U;
}

static void tdb_strcopy(char *dst, const char *src, uint32_t cap) {
  uint32_t i = 0U;
  if (!dst || cap == 0U) return;
  if (!src) src = "";
  while (src[i] && i + 1U < cap) {
    dst[i] = src[i];
    i++;
  }
  dst[i] = 0;
}

static uint32_t tdb_crc32_update(uint32_t crc, const uint8_t *data,
                                 uint32_t len) {
  while (len--) {
    crc ^= *data++;
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
  }
  return crc;
}

static uint32_t tdb_crc32(const uint8_t *data, uint32_t len) {
  return ~tdb_crc32_update(0xFFFFFFFFUL, data, len);
}

static uint32_t tdb_parse_u32(const uint8_t *text, uint32_t *out) {
  uint32_t value = 0U;
  uint32_t i = 0U;
  if (!text || !out || text[0] < '0' || text[0] > '9') return 0U;
  while (text[i] >= '0' && text[i] <= '9') {
    value = value * 10U + (uint32_t)(text[i] - '0');
    i++;
  }
  *out = value;
  return i;
}

static uint32_t tdb_parse_hex32(const uint8_t *text, uint32_t *out) {
  uint32_t value = 0U;
  uint32_t i = 0U;
  uint32_t digits = 0U;
  if (!text || !out) return 0U;
  if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) i = 2U;
  for (;;) {
    uint8_t c = text[i];
    uint8_t n;
    if (c >= '0' && c <= '9') n = (uint8_t)(c - '0');
    else if (c >= 'a' && c <= 'f') n = (uint8_t)(c - 'a' + 10U);
    else if (c >= 'A' && c <= 'F') n = (uint8_t)(c - 'A' + 10U);
    else break;
    value = (value << 4) | n;
    i++;
    digits++;
  }
  if (digits == 0U) return 0U;
  *out = value;
  return i;
}

static const uint8_t *tdb_skip_space(const uint8_t *p) {
  while (p && (*p == ' ' || *p == '\t')) p++;
  return p;
}

static uint8_t tdb_hex_nibble(uint8_t c, uint8_t *out) {
  if (c >= '0' && c <= '9') *out = (uint8_t)(c - '0');
  else if (c >= 'a' && c <= 'f') *out = (uint8_t)(c - 'a' + 10U);
  else if (c >= 'A' && c <= 'F') *out = (uint8_t)(c - 'A' + 10U);
  else return 0U;
  return 1U;
}

static uint8_t tdb_hex_decode(const uint8_t *hex, char *out, uint32_t cap) {
  uint32_t n = 0U;
  if (!hex || !out || cap == 0U) return 0U;
  while (hex[0] && hex[1]) {
    uint8_t hi, lo;
    if (n + 1U >= cap || !tdb_hex_nibble(hex[0], &hi) ||
        !tdb_hex_nibble(hex[1], &lo)) return 0U;
    out[n++] = (char)((hi << 4) | lo);
    hex += 2;
  }
  if (*hex != 0U) return 0U;
  out[n] = 0;
  return 1U;
}

static char *tdb_append_u32(char *p, uint32_t value) {
  char tmp[10];
  uint8_t n = 0U;
  if (value == 0U) {
    *p++ = '0';
    return p;
  }
  while (value && n < sizeof(tmp)) {
    tmp[n++] = (char)('0' + value % 10U);
    value /= 10U;
  }
  while (n) *p++ = tmp[--n];
  return p;
}

static char *tdb_append_hex32(char *p, uint32_t value) {
  static const char hex[] REC_CONST = "0123456789ABCDEF";
  *p++ = '0';
  *p++ = 'x';
  for (int8_t shift = 28; shift >= 0; shift -= 4) {
    *p++ = hex[(value >> (uint8_t)shift) & 0xFU];
  }
  return p;
}

static uint8_t tdb_send_status(const char *status, const char *message);
static char *tdb_append_uid_word(char *p, uint32_t value) {
  static const char hex[] REC_CONST = "0123456789ABCDEF";
  for (int8_t shift = 28; shift >= 0; shift -= 4)
    *p++ = hex[(value >> (uint8_t)shift) & 0xFU];
  return p;
}

static void tdb_device_id(char id[25]) {
  const uint32_t *uid = (const uint32_t *)UID_BASE;
  char *p = id;
  p = tdb_append_uid_word(p, uid[0]);
  p = tdb_append_uid_word(p, uid[1]);
  p = tdb_append_uid_word(p, uid[2]);
  *p = '\0';
}

static void tdb_send_identity(void) {
  char message[40] = "TDB/1 REC ";
  tdb_device_id(message + 10);
  (void)tdb_send_status("OKAY", message);
}


static void tdb_usb_get_string(const char *ascii, uint16_t *length) {
  uint16_t len = tdb_strlen(ascii);
  if (len > 31U) len = 31U;
  tdb_str_desc[0] = (uint8_t)((len * 2U) + 2U);
  tdb_str_desc[1] = USB_DESC_TYPE_STRING;
  for (uint16_t i = 0U; i < len; ++i) {
    tdb_str_desc[2U + i * 2U] = (uint8_t)ascii[i];
    tdb_str_desc[3U + i * 2U] = 0U;
  }
  *length = tdb_str_desc[0];
}

static uint8_t *TDB_USB_DeviceDescriptor(USBD_SpeedTypeDef speed,
                                         uint16_t *length) {
  (void)speed;
  *length = sizeof(tdb_usb_device_desc);
  return (uint8_t *)tdb_usb_device_desc;
}

static uint8_t *TDB_USB_LangIDStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length) {
  (void)speed;
  *length = sizeof(tdb_usb_lang_desc);
  return (uint8_t *)tdb_usb_lang_desc;
}

static uint8_t *TDB_USB_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed,
                                                  uint16_t *length) {
  (void)speed;
  tdb_usb_get_string("Typheye", length);
  return tdb_str_desc;
}

static uint8_t *TDB_USB_ProductStrDescriptor(USBD_SpeedTypeDef speed,
                                             uint16_t *length) {
  (void)speed;
  tdb_usb_get_string("TOS Debug Bridge", length);
  return tdb_str_desc;
}

static uint8_t *TDB_USB_SerialStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length) {
  (void)speed;
  tdb_usb_get_string("TOS-TDB-0001", length);
  return tdb_str_desc;
}

static uint8_t *TDB_USB_ConfigStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length) {
  (void)speed;
  tdb_usb_get_string("TDB CDC Config", length);
  return tdb_str_desc;
}

static uint8_t *TDB_USB_InterfaceStrDescriptor(USBD_SpeedTypeDef speed,
                                               uint16_t *length) {
  (void)speed;
  tdb_usb_get_string("TOS Debug Bridge", length);
  return tdb_str_desc;
}

static uint8_t *TDB_USBD_CDC_GetFSCfgDesc(uint16_t *length) {
  *length = sizeof(tdb_usb_cfg_desc);
  return (uint8_t *)tdb_usb_cfg_desc;
}

static uint8_t *TDB_USBD_CDC_GetDeviceQualifierDesc(uint16_t *length) {
  *length = sizeof(tdb_usb_qualifier_desc);
  return (uint8_t *)tdb_usb_qualifier_desc;
}

static void tdb_ring_push(uint8_t value) {
  uint16_t next = (uint16_t)((tdb_cdc.head + 1U) % TDB_USB_RX_RING_SIZE);
  if (next != tdb_cdc.tail) {
    tdb_cdc.ring[tdb_cdc.head] = value;
    tdb_cdc.head = next;
  }
}

static uint8_t tdb_ring_pop(uint8_t *value) {
  if (tdb_cdc.tail == tdb_cdc.head) return 0U;
  *value = tdb_cdc.ring[tdb_cdc.tail];
  tdb_cdc.tail = (uint16_t)((tdb_cdc.tail + 1U) % TDB_USB_RX_RING_SIZE);
  return 1U;
}

static uint8_t TDB_USBD_CDC_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
  (void)cfgidx;
  tdb_cdc.head = 0U;
  tdb_cdc.tail = 0U;
  tdb_cdc.line_len = 0U;
  tdb_cdc.line_overflow = 0U;
  tdb_cdc.tx_busy = 0U;
  tdb_cdc.cmd_opcode = 0U;
  tdb_cdc.line_coding[0] = 0x00U;
  tdb_cdc.line_coding[1] = 0xC2U;
  tdb_cdc.line_coding[2] = 0x01U;
  tdb_cdc.line_coding[3] = 0x00U;
  tdb_cdc.line_coding[4] = 0x00U;
  tdb_cdc.line_coding[5] = 0x00U;
  tdb_cdc.line_coding[6] = 0x08U;

  pdev->pClassData = &tdb_cdc;
  pdev->pClassDataCmsit[pdev->classId] = &tdb_cdc;
  (void)USBD_LL_OpenEP(pdev, TDB_USB_CDC_DATA_IN_EP, USBD_EP_TYPE_BULK,
                       TDB_USB_CDC_DATA_MPS);
  (void)USBD_LL_OpenEP(pdev, TDB_USB_CDC_DATA_OUT_EP, USBD_EP_TYPE_BULK,
                       TDB_USB_CDC_DATA_MPS);
  (void)USBD_LL_OpenEP(pdev, TDB_USB_CDC_CMD_EP, USBD_EP_TYPE_INTR,
                       TDB_USB_CDC_CMD_MPS);
  pdev->ep_in[TDB_USB_CDC_DATA_IN_EP & 0x0FU].is_used = 1U;
  pdev->ep_out[TDB_USB_CDC_DATA_OUT_EP & 0x0FU].is_used = 1U;
  pdev->ep_in[TDB_USB_CDC_CMD_EP & 0x0FU].is_used = 1U;
  (void)USBD_LL_PrepareReceive(pdev, TDB_USB_CDC_DATA_OUT_EP,
                               tdb_cdc.rx_buf, TDB_USB_CDC_DATA_MPS);
  return (uint8_t)USBD_OK;
}

static uint8_t TDB_USBD_CDC_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
  (void)cfgidx;
  (void)USBD_LL_CloseEP(pdev, TDB_USB_CDC_DATA_IN_EP);
  (void)USBD_LL_CloseEP(pdev, TDB_USB_CDC_DATA_OUT_EP);
  (void)USBD_LL_CloseEP(pdev, TDB_USB_CDC_CMD_EP);
  pdev->ep_in[TDB_USB_CDC_DATA_IN_EP & 0x0FU].is_used = 0U;
  pdev->ep_out[TDB_USB_CDC_DATA_OUT_EP & 0x0FU].is_used = 0U;
  pdev->ep_in[TDB_USB_CDC_CMD_EP & 0x0FU].is_used = 0U;
  pdev->pClassData = NULL;
  pdev->pClassDataCmsit[pdev->classId] = NULL;
  tdb_cdc.tx_busy = 0U;
  return (uint8_t)USBD_OK;
}

static uint8_t TDB_USBD_CDC_Setup(USBD_HandleTypeDef *pdev,
                                  USBD_SetupReqTypedef *req) {
  uint16_t status = 0U;
  switch (req->bmRequest & USB_REQ_TYPE_MASK) {
  case USB_REQ_TYPE_CLASS:
    switch (req->bRequest) {
    case CDC_REQ_SET_LINE_CODING:
      tdb_cdc.cmd_opcode = req->bRequest;
      (void)USBD_CtlPrepareRx(pdev, tdb_cdc.line_coding,
                              req->wLength < 7U ? req->wLength : 7U);
      break;
    case CDC_REQ_GET_LINE_CODING:
      (void)USBD_CtlSendData(pdev, tdb_cdc.line_coding,
                             req->wLength < 7U ? req->wLength : 7U);
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

static uint8_t TDB_USBD_CDC_EP0_RxReady(USBD_HandleTypeDef *pdev) {
  (void)pdev;
  tdb_cdc.cmd_opcode = 0U;
  return (uint8_t)USBD_OK;
}

static uint8_t TDB_USBD_CDC_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum) {
  (void)pdev;
  (void)epnum;
  tdb_cdc.tx_busy = 0U;
  return (uint8_t)USBD_OK;
}

static uint8_t TDB_USBD_CDC_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum) {
  uint32_t len = USBD_LL_GetRxDataSize(pdev, epnum);
  if (len > TDB_USB_CDC_DATA_MPS) len = TDB_USB_CDC_DATA_MPS;
  for (uint32_t i = 0U; i < len; ++i) {
    if (tdb_push.rx_raw) {
      if (tdb_push.chunk_received < tdb_push.chunk_len) {
        tdb_raw_buf[tdb_push.chunk_received++] = tdb_cdc.rx_buf[i];
      }
    } else {
      tdb_ring_push(tdb_cdc.rx_buf[i]);
    }
  }
  (void)USBD_LL_PrepareReceive(pdev, TDB_USB_CDC_DATA_OUT_EP,
                               tdb_cdc.rx_buf, TDB_USB_CDC_DATA_MPS);
  return (uint8_t)USBD_OK;
}

REC_CODE uint8_t REC_TDB_IsConfigured(void) {
  return (uint8_t)(tdb_usb_started &&
                   tdb_usb_dev.dev_state == USBD_STATE_CONFIGURED);
}

REC_CODE uint8_t REC_TDB_IsStarted(void) { return tdb_usb_started; }

static uint8_t tdb_usb_write_packet(const uint8_t *data, uint16_t len) {
  uint32_t start = HAL_GetTick();
  if (!data || len == 0U || len > TDB_USB_TX_SIZE) return 0U;
  while (tdb_cdc.tx_busy) {
    if (!REC_TDB_IsConfigured() ||
        (uint32_t)(HAL_GetTick() - start) >= TDB_TX_TIMEOUT_MS) return 0U;
  }
  tdb_memcpy(tdb_cdc.tx_buf, data, len);
  tdb_cdc.tx_busy = 1U;
  if (USBD_LL_Transmit(&tdb_usb_dev, TDB_USB_CDC_DATA_IN_EP,
                       tdb_cdc.tx_buf, len) != USBD_OK) {
    tdb_cdc.tx_busy = 0U;
    return 0U;
  }
  start = HAL_GetTick();
  while (tdb_cdc.tx_busy) {
    if (!REC_TDB_IsConfigured() ||
        (uint32_t)(HAL_GetTick() - start) >= TDB_TX_TIMEOUT_MS) return 0U;
  }
  return 1U;
}

static uint8_t tdb_usb_write_all(const uint8_t *data, uint32_t len) {
  while (len) {
    uint16_t chunk = len > TDB_USB_TX_SIZE ? TDB_USB_TX_SIZE : (uint16_t)len;
    if (!tdb_usb_write_packet(data, chunk)) return 0U;
    data += chunk;
    len -= chunk;
  }
  return 1U;
}

static uint8_t tdb_usb_write_text(const char *text) {
  return tdb_usb_write_all((const uint8_t *)text, tdb_strlen(text));
}

static uint8_t tdb_send_status(const char *prefix, const char *message) {
  char line[128];
  char *p = line;
  while (*prefix && p + 1 < line + sizeof(line)) *p++ = *prefix++;
  if (message && message[0] && p + 1 < line + sizeof(line)) {
    *p++ = ' ';
    while (*message && p + 2 < line + sizeof(line)) *p++ = *message++;
  }
  *p++ = '\r';
  *p++ = '\n';
  return tdb_usb_write_all((const uint8_t *)line, (uint32_t)(p - line));
}

static uint8_t tdb_send_frame(const uint8_t *data, uint32_t len,
                              uint8_t success, const char *message) {
  char header[64];
  char *p = header;
  uint32_t crc = tdb_crc32(data, len);
  const char *word = "DATA ";
  while (*word) *p++ = *word++;
  p = tdb_append_u32(p, len);
  *p++ = ' ';
  p = tdb_append_hex32(p, crc);
  *p++ = '\r';
  *p++ = '\n';
  if (!tdb_usb_write_all((const uint8_t *)header, (uint32_t)(p - header)))
    return 0U;
  if (len && !tdb_usb_write_all(data, len)) return 0U;
  return tdb_send_status(success ? "OKAY" : "FAIL", message);
}

static uint8_t tdb_read_line(uint8_t *line, uint16_t cap) {
  uint8_t c;
  while (tdb_ring_pop(&c)) {
    if (c == '\r') continue;
    if (c == '\n') {
      if (tdb_cdc.line_overflow) {
        static const char overflow[] REC_CONST = "__LINE_TOO_LONG__";
        tdb_strcopy((char *)line, overflow, cap);
      } else {
        line[tdb_cdc.line_len] = 0U;
      }
      tdb_cdc.line_len = 0U;
      tdb_cdc.line_overflow = 0U;
      return line[0] != 0U;
    }
    if (!tdb_cdc.line_overflow) {
      if (tdb_cdc.line_len + 1U < cap) {
        line[tdb_cdc.line_len++] = c;
      } else {
        tdb_cdc.line_overflow = 1U;
      }
    }
  }
  return 0U;
}

static void tdb_output_reset(void) {
  tdb_output_len = 0U;
  tdb_output_truncated = 0U;
  tdb_output[0] = 0;
}

static void tdb_output_bytes(const char *data, uint32_t len) {
  if (!data || len == 0U || tdb_output_truncated) return;
  if (len > TDB_OUTPUT_SIZE - 1U - tdb_output_len) {
    len = TDB_OUTPUT_SIZE - 1U - tdb_output_len;
    tdb_output_truncated = 1U;
  }
  tdb_memcpy(&tdb_output[tdb_output_len], data, len);
  tdb_output_len += len;
  tdb_output[tdb_output_len] = 0;
}

static void tdb_output_text(const char *text) {
  tdb_output_bytes(text, tdb_strlen(text));
}

static void tdb_output_u32(uint32_t value) {
  char temp[10];
  char *end = tdb_append_u32(temp, value);
  tdb_output_bytes(temp, (uint32_t)(end - temp));
}

static void tdb_output_hex32(uint32_t value) {
  char temp[10];
  char *end = tdb_append_hex32(temp, value);
  tdb_output_bytes(temp, (uint32_t)(end - temp));
}

static void tdb_output_finish(void) {
  static const char note[] REC_CONST = "\r\n... output truncated; use pull for large files\r\n";
  if (tdb_output_truncated) {
    uint32_t n = tdb_strlen(note);
    if (n < TDB_OUTPUT_SIZE) {
      uint32_t start = TDB_OUTPUT_SIZE - 1U - n;
      tdb_memcpy(&tdb_output[start], note, n);
      tdb_output_len = start + n;
      tdb_output[tdb_output_len] = 0;
    }
  }
}

static const char *tdb_fresult_name(FRESULT fr) {
  switch (fr) {
  case FR_OK: return "FR_OK";
  case FR_DISK_ERR: return "FR_DISK_ERR";
  case FR_INT_ERR: return "FR_INT_ERR";
  case FR_NOT_READY: return "FR_NOT_READY";
  case FR_NO_FILE: return "FR_NO_FILE";
  case FR_NO_PATH: return "FR_NO_PATH";
  case FR_INVALID_NAME: return "FR_INVALID_NAME";
  case FR_DENIED: return "FR_DENIED";
  case FR_EXIST: return "FR_EXIST";
  case FR_INVALID_OBJECT: return "FR_INVALID_OBJECT";
  case FR_WRITE_PROTECTED: return "FR_WRITE_PROTECTED";
  case FR_INVALID_DRIVE: return "FR_INVALID_DRIVE";
  case FR_NOT_ENABLED: return "FR_NOT_ENABLED";
  case FR_NO_FILESYSTEM: return "FR_NO_FILESYSTEM";
  case FR_MKFS_ABORTED: return "FR_MKFS_ABORTED";
  case FR_TIMEOUT: return "FR_TIMEOUT";
  case FR_LOCKED: return "FR_LOCKED";
  case FR_NOT_ENOUGH_CORE: return "FR_NOT_ENOUGH_CORE";
  case FR_TOO_MANY_OPEN_FILES: return "FR_TOO_MANY_OPEN_FILES";
  case FR_INVALID_PARAMETER: return "FR_INVALID_PARAMETER";
  default: return "FR_UNKNOWN";
  }
}

static uint8_t tdb_fs_ensure(void) {
  if (REC_FsIsMounted()) return 1U;
  return REC_FsMount();
}

static void tdb_fs_error(const char *what, FRESULT fr) {
  tdb_output_text("error: ");
  tdb_output_text(what);
  tdb_output_text(": ");
  tdb_output_text(tdb_fresult_name(fr));
  tdb_output_text("\r\n");
  if (fr == FR_DISK_ERR || fr == FR_NOT_READY || fr == FR_INVALID_OBJECT) {
    REC_FsUnmount();
  }
}

static uint8_t tdb_path_pop(char *out, uint32_t *len) {
  if (*len <= 3U) return 1U;
  while (*len > 3U && out[*len - 1U] != '/') (*len)--;
  if (*len > 3U) (*len)--;
  out[*len] = 0;
  return 1U;
}

static uint8_t tdb_make_path(const char *input, char *out, uint32_t cap) {
  char source[TDB_PATH_SIZE];
  uint32_t out_len;
  uint32_t i = 0U;
  if (!out || cap < 4U) return 0U;
  if (!input || input[0] == 0) {
    tdb_strcopy(out, tdb_cwd, cap);
    return 1U;
  }
  tdb_strcopy(source, input, sizeof(source));
  if ((source[0] == '0' && source[1] == ':' &&
       (source[2] == '/' || source[2] == '\\')) ||
      source[0] == '/' || source[0] == '\\') {
    out[0] = '0'; out[1] = ':'; out[2] = '/'; out[3] = 0;
    out_len = 3U;
    if (source[0] == '0') i = 3U;
    else i = 1U;
  } else {
    tdb_strcopy(out, tdb_cwd, cap);
    out_len = tdb_strlen(out);
  }
  while (source[i]) {
    char component[66];
    uint32_t n = 0U;
    while (source[i] == '/' || source[i] == '\\') i++;
    while (source[i] && source[i] != '/' && source[i] != '\\') {
      if (source[i] == ':' || n + 1U >= sizeof(component)) return 0U;
      component[n++] = source[i++];
    }
    component[n] = 0;
    if (n == 0U || (n == 1U && component[0] == '.')) continue;
    if (n == 2U && component[0] == '.' && component[1] == '.') {
      (void)tdb_path_pop(out, &out_len);
      continue;
    }
    if (out_len > 3U) {
      if (out_len + 1U >= cap) return 0U;
      out[out_len++] = '/';
    }
    if (out_len + n >= cap) return 0U;
    tdb_memcpy(&out[out_len], component, n);
    out_len += n;
    out[out_len] = 0;
  }
  return 1U;
}

static uint8_t tdb_tokenize(char *text, char *argv[], uint8_t max_args) {
  char *src = text;
  char *dst = text;
  uint8_t argc = 0U;
  while (*src) {
    char quote = 0;
    while (*src == ' ' || *src == '\t') src++;
    if (!*src || argc >= max_args) break;
    argv[argc++] = dst;
    while (*src) {
      char c = *src++;
      if (!quote && (c == ' ' || c == '\t')) break;
      if (!quote && (c == '\'' || c == '"')) {
        quote = c;
        continue;
      }
      if (quote && c == quote) {
        quote = 0;
        continue;
      }
      if (c == '\\' && *src) c = *src++;
      *dst++ = c;
    }
    *dst++ = 0;
  }
  return argc;
}

static uint8_t tdb_shell_ls(uint8_t argc, char *argv[]) {
  DIR dir;
  FILINFO info;
  FRESULT fr;
  char path[TDB_PATH_SIZE];
  if (!tdb_fs_ensure()) {
    tdb_output_text("error: storage not ready: ");
    tdb_output_text(REC_FatLastError());
    tdb_output_text("\r\n");
    return 0U;
  }
  if (!tdb_make_path(argc > 1U ? argv[1] : tdb_cwd, path, sizeof(path))) {
    tdb_output_text("error: invalid path\r\n");
    return 0U;
  }
  fr = f_opendir(&dir, path);
  if (fr != FR_OK) {
    tdb_fs_error("ls", fr);
    return 0U;
  }
  for (;;) {
    fr = f_readdir(&dir, &info);
    if (fr != FR_OK || info.fname[0] == 0) break;
    tdb_output_text(info.fname);
    if (info.fattrib & AM_DIR) {
      tdb_output_text("/\r\n");
    } else {
      tdb_output_text("\t");
      tdb_output_u32((uint32_t)info.fsize);
      tdb_output_text(" bytes\r\n");
    }
    if (tdb_output_truncated) break;
  }
  (void)f_closedir(&dir);
  if (fr != FR_OK) {
    tdb_fs_error("ls", fr);
    return 0U;
  }
  return 1U;
}

static uint8_t tdb_shell_cd(uint8_t argc, char *argv[]) {
  FILINFO info;
  FRESULT fr;
  char path[TDB_PATH_SIZE];
  if (argc < 2U) {
    tdb_output_text(tdb_cwd);
    tdb_output_text("\r\n");
    return 1U;
  }
  if (!tdb_fs_ensure()) {
    tdb_output_text("error: storage not ready: ");
    tdb_output_text(REC_FatLastError());
    tdb_output_text("\r\n");
    return 0U;
  }
  if (!tdb_make_path(argv[1], path, sizeof(path))) {
    tdb_output_text("error: invalid path\r\n");
    return 0U;
  }
  fr = f_stat(path, &info);
  if (fr != FR_OK || (info.fattrib & AM_DIR) == 0U) {
    if (fr == FR_OK) fr = FR_NO_PATH;
    tdb_fs_error("cd", fr);
    return 0U;
  }
  tdb_strcopy(tdb_cwd, path, sizeof(tdb_cwd));
  return 1U;
}

static uint8_t tdb_shell_mkdir(uint8_t argc, char *argv[]) {
  char path[TDB_PATH_SIZE];
  FRESULT fr;
  if (argc < 2U || !tdb_fs_ensure()) {
    tdb_output_text(argc < 2U ? "usage: mkdir <path>\r\n" :
                                "error: storage not ready\r\n");
    return 0U;
  }
  if (!tdb_make_path(argv[1], path, sizeof(path))) {
    tdb_output_text("error: invalid path\r\n");
    return 0U;
  }
  fr = f_mkdir(path);
  if (fr != FR_OK && fr != FR_EXIST) {
    tdb_fs_error("mkdir", fr);
    return 0U;
  }
  return 1U;
}

static uint8_t tdb_shell_rm(uint8_t argc, char *argv[]) {
  char path[TDB_PATH_SIZE];
  FRESULT fr;
  if (argc < 2U || !tdb_fs_ensure()) {
    tdb_output_text(argc < 2U ? "usage: rm <path>\r\n" :
                                "error: storage not ready\r\n");
    return 0U;
  }
  if (!tdb_make_path(argv[1], path, sizeof(path))) {
    tdb_output_text("error: invalid path\r\n");
    return 0U;
  }
  fr = f_unlink(path);
  if (fr != FR_OK) {
    tdb_fs_error("rm", fr);
    return 0U;
  }
  return 1U;
}

static uint8_t tdb_shell_mv(uint8_t argc, char *argv[]) {
  char old_path[TDB_PATH_SIZE];
  char new_path[TDB_PATH_SIZE];
  FRESULT fr;
  if (argc < 3U || !tdb_fs_ensure()) {
    tdb_output_text(argc < 3U ? "usage: mv <src> <dst>\r\n" :
                                "error: storage not ready\r\n");
    return 0U;
  }
  if (!tdb_make_path(argv[1], old_path, sizeof(old_path)) ||
      !tdb_make_path(argv[2], new_path, sizeof(new_path))) {
    tdb_output_text("error: invalid path\r\n");
    return 0U;
  }
  fr = f_rename(old_path, new_path);
  if (fr != FR_OK) {
    tdb_fs_error("mv", fr);
    return 0U;
  }
  return 1U;
}

static uint8_t tdb_shell_cp(uint8_t argc, char *argv[]) {
  FIL src;
  FIL dst;
  char src_path[TDB_PATH_SIZE];
  char dst_path[TDB_PATH_SIZE];
  FRESULT fr;
  UINT br = 0U;
  UINT bw = 0U;
  if (argc < 3U || !tdb_fs_ensure()) {
    tdb_output_text(argc < 3U ? "usage: cp <src> <dst>\r\n" :
                                "error: storage not ready\r\n");
    return 0U;
  }
  if (!tdb_make_path(argv[1], src_path, sizeof(src_path)) ||
      !tdb_make_path(argv[2], dst_path, sizeof(dst_path))) {
    tdb_output_text("error: invalid path\r\n");
    return 0U;
  }
  fr = f_open(&src, src_path, FA_READ);
  if (fr != FR_OK) {
    tdb_fs_error("cp source", fr);
    return 0U;
  }
  fr = f_open(&dst, dst_path, FA_CREATE_ALWAYS | FA_WRITE);
  if (fr != FR_OK) {
    (void)f_close(&src);
    tdb_fs_error("cp destination", fr);
    return 0U;
  }
  do {
    fr = f_read(&src, tdb_file_buf, sizeof(tdb_file_buf), &br);
    if (fr == FR_OK && br) {
      fr = f_write(&dst, tdb_file_buf, br, &bw);
      if (fr == FR_OK && bw != br) fr = FR_DISK_ERR;
    }
  } while (fr == FR_OK && br != 0U);
  if (fr == FR_OK) fr = f_sync(&dst);
  (void)f_close(&src);
  (void)f_close(&dst);
  if (fr != FR_OK) {
    tdb_fs_error("cp", fr);
    return 0U;
  }
  return 1U;
}

static uint8_t tdb_shell_cat(uint8_t argc, char *argv[]) {
  FIL file;
  char path[TDB_PATH_SIZE];
  FRESULT fr;
  UINT br = 0U;
  if (argc < 2U || !tdb_fs_ensure()) {
    tdb_output_text(argc < 2U ? "usage: cat <file>\r\n" :
                                "error: storage not ready\r\n");
    return 0U;
  }
  if (!tdb_make_path(argv[1], path, sizeof(path))) {
    tdb_output_text("error: invalid path\r\n");
    return 0U;
  }
  fr = f_open(&file, path, FA_READ);
  if (fr != FR_OK) {
    tdb_fs_error("cat", fr);
    return 0U;
  }
  do {
    fr = f_read(&file, tdb_file_buf, sizeof(tdb_file_buf), &br);
    if (fr == FR_OK && br) tdb_output_bytes((const char *)tdb_file_buf, br);
  } while (fr == FR_OK && br && !tdb_output_truncated);
  (void)f_close(&file);
  if (fr != FR_OK) {
    tdb_fs_error("cat", fr);
    return 0U;
  }
  return 1U;
}

static uint8_t tdb_shell_stat(uint8_t argc, char *argv[]) {
  FILINFO info;
  char path[TDB_PATH_SIZE];
  FRESULT fr;
  if (argc < 2U || !tdb_fs_ensure()) {
    tdb_output_text(argc < 2U ? "usage: stat <path>\r\n" :
                                "error: storage not ready\r\n");
    return 0U;
  }
  if (!tdb_make_path(argv[1], path, sizeof(path))) {
    tdb_output_text("error: invalid path\r\n");
    return 0U;
  }
  fr = f_stat(path, &info);
  if (fr != FR_OK) {
    tdb_fs_error("stat", fr);
    return 0U;
  }
  tdb_output_text("path="); tdb_output_text(path); tdb_output_text("\r\n");
  tdb_output_text("type=");
  tdb_output_text((info.fattrib & AM_DIR) ? "directory\r\n" : "file\r\n");
  tdb_output_text("size="); tdb_output_u32((uint32_t)info.fsize);
  tdb_output_text("\r\nattr="); tdb_output_hex32(info.fattrib);
  tdb_output_text("\r\n");
  return 1U;
}

static uint8_t tdb_shell_df(void) {
  DWORD free_clusters = 0U;
  FATFS *fs = NULL;
  FRESULT fr;
  uint32_t free_kib;
  uint32_t total_kib;
  if (!tdb_fs_ensure()) {
    tdb_output_text("error: storage not ready\r\n");
    return 0U;
  }
  fr = f_getfree("0:", &free_clusters, &fs);
  if (fr != FR_OK || !fs) {
    tdb_fs_error("df", fr == FR_OK ? FR_INVALID_OBJECT : fr);
    return 0U;
  }
  free_kib = (uint32_t)((free_clusters * fs->csize) / 2U);
  total_kib = (uint32_t)(((fs->n_fatent - 2U) * fs->csize) / 2U);
  tdb_output_text("total_kib="); tdb_output_u32(total_kib);
  tdb_output_text("\r\nfree_kib="); tdb_output_u32(free_kib);
  tdb_output_text("\r\n");
  return 1U;
}

static uint8_t tdb_settings_header_valid(uint32_t addr,
                                         TDB_FlashRecordHeader **out) {
  TDB_FlashRecordHeader *hdr;
  uint32_t total;
  if (addr > TOS_PART_USERDATA_ADDRESS + TDB_SETTINGS_AREA_SIZE -
                 sizeof(TDB_FlashRecordHeader)) return 0U;
  hdr = (TDB_FlashRecordHeader *)addr;
  if (hdr->magic != TDB_FLASH_RECORD_MAGIC || hdr->datasize == 0U ||
      hdr->datasize > TDB_SETTINGS_MAX_RECORD) return 0U;
  total = sizeof(*hdr) + hdr->datasize;
  if (addr + total > TOS_PART_USERDATA_ADDRESS + TDB_SETTINGS_AREA_SIZE)
    return 0U;
  if (tdb_crc32((const uint8_t *)(addr + sizeof(*hdr)), hdr->datasize) !=
      hdr->crc) return 0U;
  if (out) *out = hdr;
  return 1U;
}

static uint32_t tdb_settings_find_last(TDB_FlashRecordHeader **out_hdr) {
  uint32_t addr = TOS_PART_USERDATA_ADDRESS;
  uint32_t last = 0U;
  TDB_FlashRecordHeader *hdr = NULL;
  while (addr < TOS_PART_USERDATA_ADDRESS + TDB_SETTINGS_AREA_SIZE -
                    sizeof(TDB_FlashRecordHeader)) {
    if (!tdb_settings_header_valid(addr, &hdr)) break;
    last = addr;
    addr += TDB_ALIGN4(sizeof(*hdr) + hdr->datasize);
  }
  if (out_hdr) {
    *out_hdr = last ? (TDB_FlashRecordHeader *)last : NULL;
  }
  return last;
}

static void tdb_settings_defaults(TDB_Settings *s) {
  tdb_memzero(s, sizeof(*s));
  s->magic = TDB_SETTINGS_MAGIC;
  s->disp_auto = 1U;
  s->disp_bright = 10U;
  tdb_strcopy(s->hs_ssid, "TOS-Hotspot", sizeof(s->hs_ssid));
  tdb_strcopy(s->hs_pwd, "12345678", sizeof(s->hs_pwd));
  s->time_auto_sync = 1U;
  s->time_style_24h = 1U;
  s->hotspot_auto_close = 1U;
  s->boot_gfx = 1U;
  tdb_strcopy(s->hotspot_ip, "192.168.4.1", sizeof(s->hotspot_ip));
}

static void tdb_settings_sanitize(TDB_Settings *s) {
  s->wlan_ssid[sizeof(s->wlan_ssid) - 1U] = 0;
  s->wlan_pwd[sizeof(s->wlan_pwd) - 1U] = 0;
  s->hs_ssid[sizeof(s->hs_ssid) - 1U] = 0;
  s->hs_pwd[sizeof(s->hs_pwd) - 1U] = 0;
  s->hotspot_ip[sizeof(s->hotspot_ip) - 1U] = 0;
  s->disp_auto = s->disp_auto ? 1U : 0U;
  s->wlan_on = s->wlan_on ? 1U : 0U;
  s->wlan_auto_conn = s->wlan_auto_conn ? 1U : 0U;
  s->debug_dashboard = s->debug_dashboard ? 1U : 0U;
  s->debug_log_com = s->debug_log_com ? 1U : 0U;
  s->time_auto_sync = s->time_auto_sync ? 1U : 0U;
  s->time_style_24h = s->time_style_24h ? 1U : 0U;
  s->hotspot_auto_close = s->hotspot_auto_close ? 1U : 0U;
  s->boot_gfx = s->boot_gfx ? 1U : 0U;
  if (s->disp_bright < 1U || s->disp_bright > 10U) s->disp_bright = 10U;
  if (s->disp_dir > 1U) s->disp_dir = 0U;
  if (s->saved_count > 10U) s->saved_count = 10U;
  s->magic = TDB_SETTINGS_MAGIC;
  s->crc = 0U;
}

static uint8_t tdb_settings_load(TDB_Settings *s) {
  TDB_FlashRecordHeader *hdr = NULL;
  uint32_t addr = tdb_settings_find_last(&hdr);
  uint32_t copy;
  if (!s) return 0U;
  tdb_settings_defaults(s);
  if (!addr || !hdr) return 0U;
  copy = hdr->datasize < sizeof(*s) ? hdr->datasize : sizeof(*s);
  tdb_memcpy(s, (const void *)(addr + sizeof(*hdr)), copy);
  if ((s->magic & 0xFFFFFF00UL) != TDB_SETTINGS_MAGIC) {
    tdb_settings_defaults(s);
    return 0U;
  }
  tdb_settings_sanitize(s);
  return 1U;
}

static uint8_t tdb_flash_range_erased(uint32_t addr, uint32_t len) {
  const uint8_t *p = (const uint8_t *)addr;
  while (len--) if (*p++ != 0xFFU) return 0U;
  return 1U;
}

static uint8_t tdb_settings_append(TDB_Settings *s) {
  TDB_FlashRecordHeader *last_hdr = NULL;
  TDB_FlashRecordHeader hdr;
  uint32_t last = tdb_settings_find_last(&last_hdr);
  uint32_t next = TOS_PART_USERDATA_ADDRESS;
  uint32_t slot = TDB_ALIGN4(sizeof(hdr) + sizeof(*s));
  const uint32_t *words;
  tdb_settings_sanitize(s);
  if (last && last_hdr) next = last + TDB_ALIGN4(sizeof(*last_hdr) +
                                                 last_hdr->datasize);
  if (next + slot > TOS_PART_USERDATA_ADDRESS + TDB_SETTINGS_AREA_SIZE ||
      !tdb_flash_range_erased(next, slot)) return 0U;
  hdr.magic = TDB_FLASH_RECORD_MAGIC;
  hdr.datasize = sizeof(*s);
  hdr.crc = tdb_crc32((const uint8_t *)s, sizeof(*s));
  if (!SBL_FlashUnlock()) return 0U;
  words = (const uint32_t *)&hdr;
  for (uint32_t i = 0U; i < sizeof(hdr) / 4U; ++i) {
    if (!SBL_FlashProgramWord(next + i * 4U, words[i])) {
      SBL_FlashLock();
      return 0U;
    }
  }
  words = (const uint32_t *)s;
  for (uint32_t i = 0U; i < sizeof(*s) / 4U; ++i) {
    if (!SBL_FlashProgramWord(next + sizeof(hdr) + i * 4U, words[i])) {
      SBL_FlashLock();
      return 0U;
    }
  }
  SBL_FlashLock();
  SBL_FlashFlushCaches();
  return tdb_settings_header_valid(next, NULL);
}

static uint8_t tdb_parse_bool_text(const char *text, uint8_t *value) {
  if (tdb_ascii_equal(text, "1") || tdb_ascii_equal(text, "true") ||
      tdb_ascii_equal(text, "on") || tdb_ascii_equal(text, "yes")) {
    *value = 1U;
    return 1U;
  }
  if (tdb_ascii_equal(text, "0") || tdb_ascii_equal(text, "false") ||
      tdb_ascii_equal(text, "off") || tdb_ascii_equal(text, "no")) {
    *value = 0U;
    return 1U;
  }
  return 0U;
}

static void tdb_settings_output_pair(const char *key, const char *value) {
  tdb_output_text(key); tdb_output_text("="); tdb_output_text(value);
  tdb_output_text("\r\n");
}

static void tdb_settings_output_bool(const char *key, uint8_t value) {
  tdb_settings_output_pair(key, value ? "true" : "false");
}

static uint8_t tdb_settings_get_value(TDB_Settings *s, const char *key) {
  if (tdb_ascii_equal(key, "disp.auto")) tdb_settings_output_bool(key, s->disp_auto);
  else if (tdb_ascii_equal(key, "disp.bright")) {
    tdb_output_text(key); tdb_output_text("="); tdb_output_u32(s->disp_bright); tdb_output_text("\r\n");
  } else if (tdb_ascii_equal(key, "disp.dir")) {
    tdb_output_text(key); tdb_output_text("="); tdb_output_u32(s->disp_dir); tdb_output_text("\r\n");
  } else if (tdb_ascii_equal(key, "wlan.on")) tdb_settings_output_bool(key, s->wlan_on);
  else if (tdb_ascii_equal(key, "wlan.autoconnect")) tdb_settings_output_bool(key, s->wlan_auto_conn);
  else if (tdb_ascii_equal(key, "wlan.ssid")) tdb_settings_output_pair(key, s->wlan_ssid);
  else if (tdb_ascii_equal(key, "wlan.password")) tdb_settings_output_pair(key, s->wlan_pwd);
  else if (tdb_ascii_equal(key, "time.autosync")) tdb_settings_output_bool(key, s->time_auto_sync);
  else if (tdb_ascii_equal(key, "time.24h")) tdb_settings_output_bool(key, s->time_style_24h);
  else if (tdb_ascii_equal(key, "debug.dashboard")) tdb_settings_output_bool(key, s->debug_dashboard);
  else if (tdb_ascii_equal(key, "debug.logcom")) tdb_settings_output_bool(key, s->debug_log_com);
  else if (tdb_ascii_equal(key, "boot.gfx")) tdb_settings_output_bool(key, s->boot_gfx);
  else if (tdb_ascii_equal(key, "hotspot.ssid")) tdb_settings_output_pair(key, s->hs_ssid);
  else if (tdb_ascii_equal(key, "hotspot.password")) tdb_settings_output_pair(key, s->hs_pwd);
  else if (tdb_ascii_equal(key, "hotspot.ip")) tdb_settings_output_pair(key, s->hotspot_ip);
  else if (tdb_ascii_equal(key, "hotspot.autoclose")) tdb_settings_output_bool(key, s->hotspot_auto_close);
  else if (tdb_ascii_equal(key, "saved.count")) {
    tdb_output_text(key); tdb_output_text("="); tdb_output_u32(s->saved_count); tdb_output_text("\r\n");
  } else return 0U;
  return 1U;
}

static uint8_t tdb_settings_set_value(TDB_Settings *s, const char *key,
                                      const char *value) {
  uint8_t b;
  uint32_t n;
  if (tdb_ascii_equal(key, "disp.auto") && tdb_parse_bool_text(value, &b)) s->disp_auto = b;
  else if (tdb_ascii_equal(key, "disp.bright")) {
    if (!tdb_parse_u32((const uint8_t *)value, &n) || n < 1U || n > 10U) return 0U;
    s->disp_bright = (uint8_t)n;
  } else if (tdb_ascii_equal(key, "disp.dir")) {
    if (!tdb_parse_u32((const uint8_t *)value, &n) || n > 1U) return 0U;
    s->disp_dir = (uint8_t)n;
  } else if (tdb_ascii_equal(key, "wlan.on") && tdb_parse_bool_text(value, &b)) s->wlan_on = b;
  else if (tdb_ascii_equal(key, "wlan.autoconnect") && tdb_parse_bool_text(value, &b)) s->wlan_auto_conn = b;
  else if (tdb_ascii_equal(key, "wlan.ssid")) tdb_strcopy(s->wlan_ssid, value, sizeof(s->wlan_ssid));
  else if (tdb_ascii_equal(key, "wlan.password")) tdb_strcopy(s->wlan_pwd, value, sizeof(s->wlan_pwd));
  else if (tdb_ascii_equal(key, "time.autosync") && tdb_parse_bool_text(value, &b)) s->time_auto_sync = b;
  else if (tdb_ascii_equal(key, "time.24h") && tdb_parse_bool_text(value, &b)) s->time_style_24h = b;
  else if (tdb_ascii_equal(key, "debug.dashboard") && tdb_parse_bool_text(value, &b)) s->debug_dashboard = b;
  else if (tdb_ascii_equal(key, "debug.logcom") && tdb_parse_bool_text(value, &b)) s->debug_log_com = b;
  else if (tdb_ascii_equal(key, "boot.gfx") && tdb_parse_bool_text(value, &b)) s->boot_gfx = b;
  else if (tdb_ascii_equal(key, "hotspot.ssid")) tdb_strcopy(s->hs_ssid, value, sizeof(s->hs_ssid));
  else if (tdb_ascii_equal(key, "hotspot.password")) tdb_strcopy(s->hs_pwd, value, sizeof(s->hs_pwd));
  else if (tdb_ascii_equal(key, "hotspot.ip")) tdb_strcopy(s->hotspot_ip, value, sizeof(s->hotspot_ip));
  else if (tdb_ascii_equal(key, "hotspot.autoclose") && tdb_parse_bool_text(value, &b)) s->hotspot_auto_close = b;
  else return 0U;
  return 1U;
}

static uint8_t tdb_shell_settings(uint8_t argc, char *argv[]) {
  TDB_Settings current;
  TDB_Settings before;
  uint8_t found = tdb_settings_load(&current);
  if (argc < 2U || tdb_ascii_equal(argv[1], "list")) {
    if (!found) tdb_output_text("warning: no valid settings record; showing defaults\r\n");
    (void)tdb_settings_get_value(&current, "disp.auto");
    (void)tdb_settings_get_value(&current, "disp.bright");
    (void)tdb_settings_get_value(&current, "disp.dir");
    (void)tdb_settings_get_value(&current, "wlan.on");
    (void)tdb_settings_get_value(&current, "wlan.autoconnect");
    (void)tdb_settings_get_value(&current, "wlan.ssid");
    (void)tdb_settings_get_value(&current, "time.autosync");
    (void)tdb_settings_get_value(&current, "time.24h");
    (void)tdb_settings_get_value(&current, "debug.dashboard");
    (void)tdb_settings_get_value(&current, "debug.logcom");
    (void)tdb_settings_get_value(&current, "boot.gfx");
    (void)tdb_settings_get_value(&current, "hotspot.ssid");
    (void)tdb_settings_get_value(&current, "hotspot.ip");
    (void)tdb_settings_get_value(&current, "hotspot.autoclose");
    (void)tdb_settings_get_value(&current, "saved.count");
    return 1U;
  }
  if (tdb_ascii_equal(argv[1], "get")) {
    if (argc < 3U || !tdb_settings_get_value(&current, argv[2])) {
      tdb_output_text("usage: settings get <key>\r\n");
      return 0U;
    }
    return 1U;
  }
  if (tdb_ascii_equal(argv[1], "set")) {
    if (argc < 4U) {
      tdb_output_text("usage: settings set <key> <value>\r\n");
      return 0U;
    }
    before = current;
    if (!tdb_settings_set_value(&current, argv[2], argv[3])) {
      tdb_output_text("error: unknown key or invalid value\r\n");
      return 0U;
    }
    tdb_settings_sanitize(&current);
    if (tdb_memeq(&before, &current, sizeof(current))) {
      tdb_output_text("unchanged\r\n");
      return 1U;
    }
    if (!tdb_settings_append(&current)) {
      tdb_output_text("error: settings journal full or flash write failed\r\n");
      return 0U;
    }
    tdb_output_text("saved; reboot TOS to apply\r\n");
    return 1U;
  }
  tdb_output_text("usage: settings [list|get <key>|set <key> <value>]\r\n");
  return 0U;
}

static uint8_t tdb_shell_exec(char *command) {
  char *argv[TDB_ARG_MAX];
  uint8_t argc = tdb_tokenize(command, argv, TDB_ARG_MAX);
  uint8_t ok = 1U;
  tdb_output_reset();
  if (argc == 0U) return 1U;
  if (tdb_ascii_equal(argv[0], "help")) {
    tdb_output_text("pwd ls [path] cd <path> stat <path> df\r\n");
    tdb_output_text("mkdir <path> rm <path> mv <src> <dst> cp <src> <dst>\r\n");
    tdb_output_text("cat <file> mount umount settings [list|get|set] reboot\r\n");
    tdb_output_text("Use host commands 'tdb push' and 'tdb pull' for file transfer.\r\n");
  } else if (tdb_ascii_equal(argv[0], "pwd")) {
    tdb_output_text(tdb_cwd); tdb_output_text("\r\n");
  } else if (tdb_ascii_equal(argv[0], "ls")) ok = tdb_shell_ls(argc, argv);
  else if (tdb_ascii_equal(argv[0], "cd")) ok = tdb_shell_cd(argc, argv);
  else if (tdb_ascii_equal(argv[0], "mkdir")) ok = tdb_shell_mkdir(argc, argv);
  else if (tdb_ascii_equal(argv[0], "rm")) ok = tdb_shell_rm(argc, argv);
  else if (tdb_ascii_equal(argv[0], "mv")) ok = tdb_shell_mv(argc, argv);
  else if (tdb_ascii_equal(argv[0], "cp")) ok = tdb_shell_cp(argc, argv);
  else if (tdb_ascii_equal(argv[0], "cat")) ok = tdb_shell_cat(argc, argv);
  else if (tdb_ascii_equal(argv[0], "stat")) ok = tdb_shell_stat(argc, argv);
  else if (tdb_ascii_equal(argv[0], "df")) ok = tdb_shell_df();
  else if (tdb_ascii_equal(argv[0], "mount")) {
    REC_FsUnmount();
    if (REC_FsMount()) tdb_output_text("mounted\r\n");
    else {
      tdb_output_text("error: "); tdb_output_text(REC_FatLastError());
      tdb_output_text("\r\n"); ok = 0U;
    }
  } else if (tdb_ascii_equal(argv[0], "umount")) {
    REC_FsUnmount(); tdb_strcopy(tdb_cwd, "0:/", sizeof(tdb_cwd));
    tdb_output_text("unmounted\r\n");
  } else if (tdb_ascii_equal(argv[0], "settings")) {
    ok = tdb_shell_settings(argc, argv);
  } else if (tdb_ascii_equal(argv[0], "reboot")) {
    tdb_output_text("rebooting\r\n");
    tdb_reboot_pending = 1U;
  } else {
    tdb_output_text("error: unknown command; run help\r\n");
    ok = 0U;
  }
  tdb_output_finish();
  return ok;
}

static void tdb_push_abort(uint8_t remove_temp) {
  if (tdb_push.active) {
    (void)f_close(&tdb_push.file);
    if (remove_temp) (void)f_unlink(tdb_push.temp_path);
  }
  tdb_memzero(&tdb_push, sizeof(tdb_push));
}

static uint8_t tdb_push_begin(const uint8_t *line) {
  const uint8_t *p = tdb_skip_space(line + 9U);
  uint32_t size = 0U;
  uint32_t crc = 0U;
  uint32_t n;
  char requested[TDB_PATH_SIZE];
  FRESULT fr;
  n = tdb_parse_u32(p, &size);
  if (n == 0U) return tdb_send_status("FAIL", "bad push size");
  p = tdb_skip_space(p + n);
  n = tdb_parse_hex32(p, &crc);
  if (n == 0U) return tdb_send_status("FAIL", "bad push crc");
  p = tdb_skip_space(p + n);
  if (!tdb_hex_decode(p, requested, sizeof(requested)) ||
      !tdb_fs_ensure()) return tdb_send_status("FAIL", "storage/path unavailable");
  tdb_push_abort(1U);
  if (!tdb_make_path(requested, tdb_push.final_path,
                     sizeof(tdb_push.final_path)))
    return tdb_send_status("FAIL", "invalid path");
  if (tdb_strlen(tdb_push.final_path) + 8U >= sizeof(tdb_push.temp_path))
    return tdb_send_status("FAIL", "path too long");
  tdb_strcopy(tdb_push.temp_path, tdb_push.final_path,
              sizeof(tdb_push.temp_path));
  {
    uint32_t len = tdb_strlen(tdb_push.temp_path);
    tdb_strcopy(&tdb_push.temp_path[len], ".tdbtmp",
                sizeof(tdb_push.temp_path) - len);
  }
  (void)f_unlink(tdb_push.temp_path);
  fr = f_open(&tdb_push.file, tdb_push.temp_path,
              FA_CREATE_ALWAYS | FA_WRITE);
  if (fr != FR_OK) {
    tdb_memzero(&tdb_push, sizeof(tdb_push));
    return tdb_send_status("FAIL", tdb_fresult_name(fr));
  }
  tdb_push.expected_size = size;
  tdb_push.expected_crc = crc;
  tdb_push.running_crc = 0xFFFFFFFFUL;
  tdb_push.active = 1U;
  return tdb_send_status("OKAY", "READY 1024");
}

static uint8_t tdb_push_data(const uint8_t *line) {
  const uint8_t *p = tdb_skip_space(line + 8U);
  uint32_t len = 0U;
  uint32_t offset = 0U;
  uint32_t crc = 0U;
  uint32_t n;
  if (!tdb_push.active) return tdb_send_status("FAIL", "no active push");
  n = tdb_parse_u32(p, &len);
  if (!n) return tdb_send_status("FAIL", "bad chunk length");
  p = tdb_skip_space(p + n);
  n = tdb_parse_u32(p, &offset);
  if (!n) return tdb_send_status("FAIL", "bad chunk offset");
  p = tdb_skip_space(p + n);
  if (!tdb_parse_hex32(p, &crc)) return tdb_send_status("FAIL", "bad chunk crc");
  if (len == 0U || len > TDB_RAW_CHUNK_SIZE || offset != tdb_push.received ||
      len > tdb_push.expected_size - tdb_push.received)
    return tdb_send_status("FAIL", "invalid chunk");
  tdb_push.chunk_len = len;
  tdb_push.chunk_received = 0U;
  tdb_push.chunk_crc = crc;
  tdb_push.chunk_offset = offset;
  tdb_push.rx_raw = 1U;
  return tdb_send_status("OKAY", "SEND");
}

static uint8_t tdb_push_end(void) {
  FRESULT fr;
  uint32_t crc;
  if (!tdb_push.active || tdb_push.rx_raw)
    return tdb_send_status("FAIL", "push incomplete");
  crc = ~tdb_push.running_crc;
  if (tdb_push.received != tdb_push.expected_size ||
      crc != tdb_push.expected_crc) {
    tdb_push_abort(1U);
    return tdb_send_status("FAIL", "size/crc mismatch");
  }
  fr = f_sync(&tdb_push.file);
  if (fr == FR_OK) fr = f_close(&tdb_push.file);
  tdb_push.active = 0U;
  if (fr == FR_OK) {
    (void)f_unlink(tdb_push.final_path);
    fr = f_rename(tdb_push.temp_path, tdb_push.final_path);
  }
  if (fr != FR_OK) {
    (void)f_unlink(tdb_push.temp_path);
    tdb_memzero(&tdb_push, sizeof(tdb_push));
    return tdb_send_status("FAIL", tdb_fresult_name(fr));
  }
  tdb_memzero(&tdb_push, sizeof(tdb_push));
  return tdb_send_status("OKAY", "PUSHED");
}

static uint8_t tdb_pull_file(const uint8_t *line) {
  const uint8_t *p = tdb_skip_space(line + 4U);
  char requested[TDB_PATH_SIZE];
  char path[TDB_PATH_SIZE];
  FIL file;
  FRESULT fr;
  UINT br = 0U;
  uint32_t size;
  uint32_t crc = 0xFFFFFFFFUL;
  char header[64];
  char *h;
  if (!tdb_hex_decode(p, requested, sizeof(requested)) || !tdb_fs_ensure() ||
      !tdb_make_path(requested, path, sizeof(path)))
    return tdb_send_status("FAIL", "storage/path unavailable");
  fr = f_open(&file, path, FA_READ);
  if (fr != FR_OK) return tdb_send_status("FAIL", tdb_fresult_name(fr));
  size = (uint32_t)f_size(&file);
  do {
    fr = f_read(&file, tdb_file_buf, sizeof(tdb_file_buf), &br);
    if (fr == FR_OK && br) crc = tdb_crc32_update(crc, tdb_file_buf, br);
  } while (fr == FR_OK && br);
  if (fr == FR_OK) fr = f_lseek(&file, 0U);
  if (fr != FR_OK) {
    (void)f_close(&file);
    return tdb_send_status("FAIL", tdb_fresult_name(fr));
  }
  crc = ~crc;
  h = header;
  *h++ = 'D'; *h++ = 'A'; *h++ = 'T'; *h++ = 'A'; *h++ = ' ';
  h = tdb_append_u32(h, size); *h++ = ' ';
  h = tdb_append_hex32(h, crc); *h++ = '\r'; *h++ = '\n';
  if (!tdb_usb_write_all((const uint8_t *)header, (uint32_t)(h - header))) {
    (void)f_close(&file);
    return 0U;
  }
  do {
    fr = f_read(&file, tdb_file_buf, sizeof(tdb_file_buf), &br);
    if (fr == FR_OK && br && !tdb_usb_write_all(tdb_file_buf, br)) {
      fr = FR_TIMEOUT;
      break;
    }
  } while (fr == FR_OK && br);
  (void)f_close(&file);
  return tdb_send_status(fr == FR_OK ? "OKAY" : "FAIL",
                         fr == FR_OK ? "PULLED" : tdb_fresult_name(fr));
}

static void tdb_handle_command(uint8_t *line) {
  if (tdb_ascii_equal((char *)line, "__LINE_TOO_LONG__")) {
    (void)tdb_send_status("FAIL", "command line too long");
  } else if (tdb_ascii_equal((char *)line, "HELLO")) {
    tdb_send_identity();
  } else if (tdb_ascii_equal((char *)line, "INFO")) {
    tdb_output_reset();
    tdb_output_text("product=TOS Debug Bridge\r\nprotocol=1\r\nmode=REC\r\ncwd=");
    {
      char id[25]; tdb_device_id(id);
      tdb_output_text("serial="); tdb_output_text(id); tdb_output_text("\r\n");
    }
    tdb_output_text(tdb_cwd);
    tdb_output_text("\r\nmounted=");
    tdb_output_text(REC_FsIsMounted() ? "true\r\n" : "false\r\n");
    (void)tdb_send_frame((const uint8_t *)tdb_output, tdb_output_len, 1U, "INFO");
  } else if (tdb_ascii_starts(line, "SHELL ")) {
    char command[TDB_USB_LINE_SIZE];
    uint8_t ok;
    if (!tdb_hex_decode(tdb_skip_space(line + 5U), command, sizeof(command))) {
      (void)tdb_send_status("FAIL", "bad shell encoding");
      return;
    }
    ok = tdb_shell_exec(command);
    (void)tdb_send_frame((const uint8_t *)tdb_output, tdb_output_len, ok,
                         ok ? "SHELL" : "SHELL");
  } else if (tdb_ascii_starts(line, "PUSHBEGIN ")) {
    (void)tdb_push_begin(line);
  } else if (tdb_ascii_starts(line, "PUSHDATA ")) {
    (void)tdb_push_data(line);
  } else if (tdb_ascii_equal((char *)line, "PUSHEND")) {
    (void)tdb_push_end();
  } else if (tdb_ascii_equal((char *)line, "ABORT")) {
    tdb_push_abort(1U);
    (void)tdb_send_status("OKAY", "ABORTED");
  } else if (tdb_ascii_starts(line, "PULL ")) {
    (void)tdb_pull_file(line);
  } else {
    (void)tdb_send_status("FAIL", "unknown command");
  }
}

REC_CODE uint8_t REC_TDB_Start(void) {
  if (tdb_usb_started) return 1U;
  tdb_banner_sent = 0U;
  tdb_reboot_pending = 0U;
  tdb_push_abort(0U);
  tdb_strcopy(tdb_cwd, "0:/", sizeof(tdb_cwd));
  if (USBD_Init(&tdb_usb_dev, (USBD_DescriptorsTypeDef *)&TDB_USB_Desc,
                DEVICE_FS) != USBD_OK) {
    (void)USBD_DeInit(&tdb_usb_dev);
    return 0U;
  }
  if (USBD_RegisterClass(&tdb_usb_dev,
                         (USBD_ClassTypeDef *)&TDB_USBD_CDC_CLASS) != USBD_OK) {
    (void)USBD_DeInit(&tdb_usb_dev);
    return 0U;
  }
  if (USBD_Start(&tdb_usb_dev) != USBD_OK) {
    (void)USBD_Stop(&tdb_usb_dev);
    (void)USBD_DeInit(&tdb_usb_dev);
    return 0U;
  }
  tdb_usb_started = 1U;
  return 1U;
}

REC_CODE void REC_TDB_Stop(void) {
  tdb_push_abort(1U);
  REC_FsUnmount();
  if (tdb_usb_started) {
    (void)USBD_Stop(&tdb_usb_dev);
    (void)USBD_DeInit(&tdb_usb_dev);
  }
  tdb_usb_started = 0U;
  tdb_banner_sent = 0U;
}

REC_CODE void REC_TDB_Tick(void) {
  if (!REC_TDB_IsConfigured()) {
    tdb_banner_sent = 0U;
    return;
  }
  if (!tdb_banner_sent && !tdb_cdc.tx_busy) {
    tdb_banner_sent = tdb_usb_write_text("TOS-TDB READY 1\r\n");
    return;
  }
  if (tdb_push.rx_raw && tdb_push.chunk_received >= tdb_push.chunk_len) {
    UINT bw = 0U;
    FRESULT fr;
    uint32_t crc = tdb_crc32(tdb_raw_buf, tdb_push.chunk_len);
    tdb_push.rx_raw = 0U;
    if (crc != tdb_push.chunk_crc ||
        tdb_push.chunk_offset != tdb_push.received) {
      tdb_push_abort(1U);
      (void)tdb_send_status("FAIL", "chunk crc/offset mismatch");
      return;
    }
    fr = f_write(&tdb_push.file, tdb_raw_buf, tdb_push.chunk_len, &bw);
    if (fr != FR_OK || bw != tdb_push.chunk_len) {
      tdb_push_abort(1U);
      (void)tdb_send_status("FAIL", fr == FR_OK ? "short write" :
                                                   tdb_fresult_name(fr));
      return;
    }
    tdb_push.running_crc = tdb_crc32_update(tdb_push.running_crc,
                                             tdb_raw_buf,
                                             tdb_push.chunk_len);
    tdb_push.received += tdb_push.chunk_len;
    (void)tdb_send_status("OKAY", "DATA");
    return;
  }
  if (!tdb_cdc.tx_busy && tdb_read_line(tdb_cdc.line, sizeof(tdb_cdc.line))) {
    tdb_handle_command(tdb_cdc.line);
  }
  if (tdb_reboot_pending && !tdb_cdc.tx_busy) {
    tdb_reboot_pending = 0U;
    SBL_DelayMs(80U);
    SBL_SystemReboot();
  }
}
