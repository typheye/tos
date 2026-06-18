#include "rec_msc.h"

#include "sbl_common.h"
#include "sbl_usb.h"
#include "sdio.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"

#define REC_MSC_VID              0x0483U
#define REC_MSC_PID              0x5754U
#define REC_MSC_LANGID           0x0409U
#define REC_MSC_CFG_SIZE         32U
#define REC_MSC_IN_EP            0x81U
#define REC_MSC_OUT_EP           0x01U
#define REC_MSC_EP_MPS           64U
#define REC_MSC_BLOCK_SIZE       512U
#define REC_MSC_IO_BLOCKS        8U
#define REC_MSC_IO_BUFFER_SIZE   (REC_MSC_BLOCK_SIZE * REC_MSC_IO_BLOCKS)
#define REC_MSC_ENUM_GRACE_MS    6000U
#define REC_MSC_RETRY_MS         500U
#define REC_MSC_READY_RETRY_MS    50U
#define REC_MSC_PROTOCOL_FALLBACK_MS 1200U
#define REC_MSC_MEDIA_WAIT_MS         1500U

#define MSC_REQ_BOT_RESET        0xFFU
#define MSC_REQ_GET_MAX_LUN      0xFEU
#define MSC_CBW_SIGNATURE        0x43425355UL
#define MSC_CSW_SIGNATURE        0x53425355UL

#define SCSI_TEST_UNIT_READY     0x00U
#define SCSI_REQUEST_SENSE       0x03U
#define SCSI_INQUIRY             0x12U
#define SCSI_MODE_SENSE6         0x1AU
#define SCSI_START_STOP_UNIT     0x1BU
#define SCSI_PREVENT_ALLOW       0x1EU
#define SCSI_READ_FORMAT_CAP     0x23U
#define SCSI_READ_CAPACITY10     0x25U
#define SCSI_READ10              0x28U
#define SCSI_WRITE10             0x2AU
#define SCSI_VERIFY10            0x2FU
#define SCSI_SYNC_CACHE10        0x35U
#define SCSI_MODE_SENSE10        0x5AU
#define SCSI_READ_CAPACITY16     0x9EU
#define SCSI_REPORT_LUNS         0xA0U

#define SCSI_SENSE_NO_SENSE      0x00U
#define SCSI_SENSE_NOT_READY     0x02U
#define SCSI_SENSE_MEDIUM_ERROR  0x03U
#define SCSI_SENSE_ILLEGAL_REQ   0x05U
#define SCSI_SENSE_UNIT_ATTENTION 0x06U
#define SCSI_ASC_LUN_NOT_READY   0x04U
#define SCSI_ASCQ_BECOMING_READY 0x01U
#define SCSI_ASC_NO_MEDIUM       0x3AU
#define SCSI_ASC_MEDIUM_CHANGED  0x28U
#define SCSI_ASC_INVALID_CMD     0x20U
#define SCSI_ASC_INVALID_FIELD   0x24U
#define SCSI_ASC_LBA_RANGE       0x21U
#define SCSI_ASC_WRITE_FAULT     0x03U
#define SCSI_ASC_READ_ERROR      0x11U

typedef enum {
  REC_MSC_STAGE_CBW = 0,
  REC_MSC_STAGE_DATA_IN,
  REC_MSC_STAGE_DATA_OUT,
  REC_MSC_STAGE_CSW,
  REC_MSC_STAGE_READ_WAIT,
  REC_MSC_STAGE_WRITE_WAIT,
  REC_MSC_STAGE_SYNC_WAIT,
  REC_MSC_STAGE_MEDIA_WAIT,
  REC_MSC_STAGE_STALL,
  REC_MSC_STAGE_RECOVERY,
} REC_MSC_Stage;

typedef struct {
  uint8_t cbw[31];
  uint8_t csw[13];
  uint8_t rx[REC_MSC_EP_MPS];
  volatile uint8_t sense_key;
  volatile uint8_t sense_asc;
  volatile uint8_t sense_ascq;
  volatile uint8_t stage;
  volatile uint8_t started;
  volatile uint8_t storage_ready;
  volatile uint8_t storage_initializing;
  volatile uint8_t storage_backend;
  volatile uint8_t storage_attach_pending;
  volatile uint8_t unit_attention;
  volatile uint8_t control_seen;
  volatile uint8_t cbw_seen;
  volatile uint8_t inquiry_seen;
  volatile uint8_t media_probe_enabled;
  volatile uint8_t csw_status;
  volatile uint8_t bot_generation;
  volatile uint8_t io_generation;
  volatile uint32_t tag;
  volatile uint32_t residue;
  volatile uint32_t card_blocks;
  volatile uint32_t pending_blocks;
  volatile uint32_t tx_len;
  volatile uint32_t tx_pos;
  volatile uint32_t read_lba;
  volatile uint32_t read_blocks;
  volatile uint32_t write_lba;
  volatile uint32_t write_expected;
  volatile uint32_t write_received;
  volatile uint32_t sector_pos;
  volatile uint32_t io_blocks;
  volatile uint32_t enum_deadline;
  volatile uint32_t protocol_fallback_at;
  volatile uint32_t media_probe_at;
  volatile uint32_t media_wait_deadline;
  volatile uint32_t retry_at;
  uint8_t * volatile tx_buf;
  uint8_t *sector_buf;
} REC_MSC_Context;

enum {
  REC_MSC_BACKEND_NONE = 0,
  REC_MSC_BACKEND_SD = 1,
};

static USBD_HandleTypeDef rec_msc_dev;
static REC_MSC_Context rec_msc;
static uint8_t rec_msc_sector_storage[REC_MSC_IO_BUFFER_SIZE]
    __attribute__((aligned(4)));
static uint8_t rec_msc_str_desc[64] __ALIGN_END;
static uint8_t rec_msc_cfg_desc[REC_MSC_CFG_SIZE] __ALIGN_END;
static uint8_t rec_msc_ep0_reply[2] __attribute__((aligned(4)));

static uint8_t REC_USBD_MSC_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t REC_USBD_MSC_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t REC_USBD_MSC_Setup(USBD_HandleTypeDef *pdev,
                                  USBD_SetupReqTypedef *req);
static uint8_t REC_USBD_MSC_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t REC_USBD_MSC_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *REC_USBD_MSC_GetFSCfgDesc(uint16_t *length);
static uint8_t *REC_USBD_MSC_GetDeviceQualifierDesc(uint16_t *length);
static void rec_msc_dispatch_scsi(USBD_HandleTypeDef *pdev);

static uint8_t *REC_MSC_DeviceDescriptor(USBD_SpeedTypeDef speed,
                                         uint16_t *length);
static uint8_t *REC_MSC_LangIDStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length);
static uint8_t *REC_MSC_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed,
                                                  uint16_t *length);
static uint8_t *REC_MSC_ProductStrDescriptor(USBD_SpeedTypeDef speed,
                                             uint16_t *length);
static uint8_t *REC_MSC_SerialStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length);
static uint8_t *REC_MSC_ConfigStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length);
static uint8_t *REC_MSC_InterfaceStrDescriptor(USBD_SpeedTypeDef speed,
                                               uint16_t *length);

static const USBD_ClassTypeDef REC_USBD_MSC_CLASS REC_CONST = {
    REC_USBD_MSC_Init,
    REC_USBD_MSC_DeInit,
    REC_USBD_MSC_Setup,
    NULL,
    NULL,
    REC_USBD_MSC_DataIn,
    REC_USBD_MSC_DataOut,
    NULL,
    NULL,
    NULL,
    REC_USBD_MSC_GetFSCfgDesc,
    REC_USBD_MSC_GetFSCfgDesc,
    REC_USBD_MSC_GetFSCfgDesc,
    REC_USBD_MSC_GetDeviceQualifierDesc,
};

static const USBD_DescriptorsTypeDef REC_MSC_Desc REC_CONST = {
    REC_MSC_DeviceDescriptor,
    REC_MSC_LangIDStrDescriptor,
    REC_MSC_ManufacturerStrDescriptor,
    REC_MSC_ProductStrDescriptor,
    REC_MSC_SerialStrDescriptor,
    REC_MSC_ConfigStrDescriptor,
    REC_MSC_InterfaceStrDescriptor,
};

__ALIGN_BEGIN static const uint8_t rec_msc_device_desc[USB_LEN_DEV_DESC] REC_CONST __ALIGN_END = {
    0x12, USB_DESC_TYPE_DEVICE, 0x00, 0x02,
    0x00, 0x00, 0x00, USB_MAX_EP0_SIZE,
    LOBYTE(REC_MSC_VID), HIBYTE(REC_MSC_VID),
    LOBYTE(REC_MSC_PID), HIBYTE(REC_MSC_PID),
    0x02, 0x01,
    USBD_IDX_MFC_STR, USBD_IDX_PRODUCT_STR, USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION,
};

__ALIGN_BEGIN static const uint8_t rec_msc_lang_desc[USB_LEN_LANGID_STR_DESC] REC_CONST __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC, USB_DESC_TYPE_STRING,
    LOBYTE(REC_MSC_LANGID), HIBYTE(REC_MSC_LANGID),
};

__ALIGN_BEGIN static const uint8_t rec_msc_cfg_desc_template[REC_MSC_CFG_SIZE] REC_CONST __ALIGN_END = {
    0x09, USB_DESC_TYPE_CONFIGURATION,
    LOBYTE(REC_MSC_CFG_SIZE), HIBYTE(REC_MSC_CFG_SIZE),
    0x01, 0x01, 0x00, 0x80, 0x32,

    0x09, USB_DESC_TYPE_INTERFACE,
    0x00, 0x00, 0x02, 0x08, 0x06, 0x50, 0x00,

    0x07, USB_DESC_TYPE_ENDPOINT, REC_MSC_OUT_EP, USBD_EP_TYPE_BULK,
    LOBYTE(REC_MSC_EP_MPS), HIBYTE(REC_MSC_EP_MPS), 0x00,

    0x07, USB_DESC_TYPE_ENDPOINT, REC_MSC_IN_EP, USBD_EP_TYPE_BULK,
    LOBYTE(REC_MSC_EP_MPS), HIBYTE(REC_MSC_EP_MPS), 0x00,
};

__ALIGN_BEGIN static const uint8_t rec_msc_qualifier_desc[USB_LEN_DEV_QUALIFIER_DESC] REC_CONST __ALIGN_END = {
    USB_LEN_DEV_QUALIFIER_DESC, USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02, 0x00, 0x00, 0x00, USB_MAX_EP0_SIZE, 0x01, 0x00,
};

static uint16_t rec_msc_strlen(const char *s) {
  uint16_t n = 0U;
  while (s && s[n]) {
    n++;
  }
  return n;
}

static void rec_msc_get_string(const char *ascii, uint16_t *length) {
  uint16_t len = rec_msc_strlen(ascii);
  if (len > 31U) {
    len = 31U;
  }
  rec_msc_str_desc[0] = (uint8_t)((len * 2U) + 2U);
  rec_msc_str_desc[1] = USB_DESC_TYPE_STRING;
  for (uint16_t i = 0U; i < len; ++i) {
    rec_msc_str_desc[2U + (i * 2U)] = (uint8_t)ascii[i];
    rec_msc_str_desc[3U + (i * 2U)] = 0U;
  }
  *length = rec_msc_str_desc[0];
}

static uint8_t *REC_MSC_DeviceDescriptor(USBD_SpeedTypeDef speed,
                                         uint16_t *length) {
  (void)speed;
  *length = sizeof(rec_msc_device_desc);
  return (uint8_t *)rec_msc_device_desc;
}

static uint8_t *REC_MSC_LangIDStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length) {
  (void)speed;
  *length = sizeof(rec_msc_lang_desc);
  return (uint8_t *)rec_msc_lang_desc;
}

static uint8_t *REC_MSC_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed,
                                                  uint16_t *length) {
  (void)speed;
  rec_msc_get_string("Typheye", length);
  return rec_msc_str_desc;
}

static uint8_t *REC_MSC_ProductStrDescriptor(USBD_SpeedTypeDef speed,
                                             uint16_t *length) {
  (void)speed;
  rec_msc_get_string("TOS USB Disk", length);
  return rec_msc_str_desc;
}

static uint8_t *REC_MSC_SerialStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length) {
  (void)speed;
  rec_msc_get_string("TOS-REC-0002", length);
  return rec_msc_str_desc;
}

static uint8_t *REC_MSC_ConfigStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length) {
  (void)speed;
  rec_msc_get_string("Mass Storage", length);
  return rec_msc_str_desc;
}

static uint8_t *REC_MSC_InterfaceStrDescriptor(USBD_SpeedTypeDef speed,
                                               uint16_t *length) {
  (void)speed;
  rec_msc_get_string("Mass Storage", length);
  return rec_msc_str_desc;
}

static uint8_t *REC_USBD_MSC_GetFSCfgDesc(uint16_t *length) {
  *length = sizeof(rec_msc_cfg_desc);
  return rec_msc_cfg_desc;
}

static uint8_t *REC_USBD_MSC_GetDeviceQualifierDesc(uint16_t *length) {
  *length = sizeof(rec_msc_qualifier_desc);
  return (uint8_t *)rec_msc_qualifier_desc;
}

static uint32_t rec_msc_get_le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t rec_msc_get_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint16_t rec_msc_get_be16(const uint8_t *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void rec_msc_put_le32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

static void rec_msc_put_be32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static void rec_msc_put_be64(uint8_t *p, uint64_t v) {
  p[0] = (uint8_t)(v >> 56);
  p[1] = (uint8_t)(v >> 48);
  p[2] = (uint8_t)(v >> 40);
  p[3] = (uint8_t)(v >> 32);
  p[4] = (uint8_t)(v >> 24);
  p[5] = (uint8_t)(v >> 16);
  p[6] = (uint8_t)(v >> 8);
  p[7] = (uint8_t)v;
}

static void rec_msc_zero(uint8_t *p, uint32_t n) {
  while (n--) {
    *p++ = 0U;
  }
}

static void rec_msc_copy(uint8_t *dst, const uint8_t *src, uint32_t n) {
  while (n--) {
    *dst++ = *src++;
  }
}

static void rec_msc_set_sense_ex(uint8_t key, uint8_t asc, uint8_t ascq) {
  rec_msc.sense_key = key;
  rec_msc.sense_asc = asc;
  rec_msc.sense_ascq = ascq;
}

static void rec_msc_set_sense(uint8_t key, uint8_t asc) {
  rec_msc_set_sense_ex(key, asc, 0U);
}

static void rec_msc_storage_mark_offline(uint8_t key, uint8_t asc) {
  uint8_t had_media =
      (uint8_t)(rec_msc.storage_backend == REC_MSC_BACKEND_SD ||
                rec_msc.storage_attach_pending);

  rec_msc.storage_ready = 0U;
  rec_msc.storage_initializing = 0U;
  rec_msc.storage_backend = REC_MSC_BACKEND_NONE;
  rec_msc.storage_attach_pending = 0U;
  rec_msc.pending_blocks = 0U;
  rec_msc.card_blocks = 0U;
  if (had_media) {
    rec_msc.unit_attention = 1U;
  }
  rec_msc_set_sense(key, asc);
}

/* All SDIO block operations run from REC_MSC_Tick() in thread context. USB
 * callbacks only inspect cached media state.  The USB device is therefore
 * fully enumerated first and presents a normal removable LUN with no medium;
 * SD identification then runs in the background and is attached only at a BOT
 * command boundary. */
static uint8_t rec_msc_storage_probe(void) {
  uint32_t blocks = 0U;

  rec_msc.storage_initializing = 1U;
  rec_msc_set_sense_ex(SCSI_SENSE_NOT_READY, SCSI_ASC_LUN_NOT_READY,
                       SCSI_ASCQ_BECOMING_READY);
  if (!REC_BlockInit(&blocks) || blocks == 0U) {
    if (blocks > 0U) {
      /* Card identification succeeded but it has not reached TRANSFER yet.
       * Keep the live handle and retry the short ready check. */
      rec_msc.pending_blocks = blocks;
      rec_msc.storage_initializing = 1U;
    } else {
      REC_BlockRelease();
      rec_msc_storage_mark_offline(SCSI_SENSE_NOT_READY,
                                   SCSI_ASC_NO_MEDIUM);
    }
    return 0U;
  }

  /* Persist one diagnostic snapshot before host and firmware can both touch
   * the FAT volume.  Failure to mount/write the log is intentionally ignored. */
  REC_MscLogReady(blocks);
  rec_msc.pending_blocks = blocks;
  __DMB();
  /* Publish this flag last.  A CBW can arrive in the USB interrupt as soon as
   * REC_MscLogReady() returns; seeing attach_pending must imply that geometry
   * and the live SD handle are already complete. */
  rec_msc.storage_attach_pending = 1U;
  rec_msc.storage_initializing = 0U;
  return 1U;
}

static uint8_t rec_msc_apply_pending_storage(void) {
  if (!rec_msc.storage_attach_pending || rec_msc.pending_blocks == 0U) {
    return 0U;
  }

  rec_msc.storage_backend = REC_MSC_BACKEND_SD;
  rec_msc.card_blocks = rec_msc.pending_blocks;
  rec_msc.pending_blocks = 0U;
  rec_msc.storage_attach_pending = 0U;
  rec_msc.storage_initializing = 0U;
  rec_msc.storage_ready = 1U;
  rec_msc.unit_attention = 1U;
  rec_msc_set_sense(SCSI_SENSE_NO_SENSE, 0U);
  __DMB();
  return 1U;
}

static uint8_t rec_msc_command_needs_media(uint8_t opcode) {
  switch (opcode) {
    case SCSI_TEST_UNIT_READY:
    case SCSI_READ_FORMAT_CAP:
    case SCSI_READ_CAPACITY10:
    case SCSI_READ10:
    case SCSI_WRITE10:
    case SCSI_SYNC_CACHE10:
    case SCSI_READ_CAPACITY16:
      return 1U;
    default:
      return 0U;
  }
}

static void rec_msc_resume_waiting_media(void) {
  uint8_t attached = 0U;

  if (rec_msc.stage != REC_MSC_STAGE_MEDIA_WAIT ||
      !rec_msc.storage_attach_pending) {
    return;
  }

  /* The waiting CBW is already owned by the class, so only the cached media
   * state is switched under the USB IRQ lock.  The command is dispatched after
   * interrupts are re-enabled. */
  NVIC_DisableIRQ(OTG_FS_IRQn);
  if (rec_msc.stage == REC_MSC_STAGE_MEDIA_WAIT) {
    attached = rec_msc_apply_pending_storage();
  }
  NVIC_EnableIRQ(OTG_FS_IRQn);

  if (attached) {
    rec_msc.media_wait_deadline = 0U;
    rec_msc_dispatch_scsi(&rec_msc_dev);
  }
}

static uint8_t rec_msc_storage_online_cached(void) {
  return (rec_msc.storage_backend == REC_MSC_BACKEND_SD &&
          rec_msc.storage_ready && rec_msc.card_blocks > 0U) ? 1U : 0U;
}

static uint8_t rec_msc_storage_geometry_cached(void) {
  return rec_msc_storage_online_cached();
}

static void rec_msc_prepare_cbw(USBD_HandleTypeDef *pdev) {
  rec_msc.media_wait_deadline = 0U;
  __DMB();
  rec_msc.stage = REC_MSC_STAGE_CBW;
  (void)USBD_LL_PrepareReceive(pdev, REC_MSC_OUT_EP, rec_msc.cbw,
                               sizeof(rec_msc.cbw));
}

static void rec_msc_send_csw(USBD_HandleTypeDef *pdev) {
  rec_msc_put_le32(&rec_msc.csw[0], MSC_CSW_SIGNATURE);
  rec_msc_put_le32(&rec_msc.csw[4], rec_msc.tag);
  rec_msc_put_le32(&rec_msc.csw[8], rec_msc.residue);
  rec_msc.csw[12] = rec_msc.csw_status;
  __DMB();
  rec_msc.stage = REC_MSC_STAGE_CSW;
  (void)USBD_LL_Transmit(pdev, REC_MSC_IN_EP, rec_msc.csw,
                         sizeof(rec_msc.csw));
}

static void rec_msc_send_next_in(USBD_HandleTypeDef *pdev) {
  uint32_t left = rec_msc.tx_len - rec_msc.tx_pos;
  uint32_t n = (left > REC_MSC_EP_MPS) ? REC_MSC_EP_MPS : left;

  if (n > 0U) {
    (void)USBD_LL_Transmit(pdev, REC_MSC_IN_EP,
                           &rec_msc.tx_buf[rec_msc.tx_pos], n);
    rec_msc.tx_pos += n;
    if (rec_msc.residue >= n) {
      rec_msc.residue -= n;
    } else {
      rec_msc.residue = 0U;
    }
    return;
  }

  if (rec_msc.read_blocks > 0U) {
    /* Leave the IN endpoint unqueued so the USB core returns NAK while the
     * main loop performs the next SDIO read. */
    rec_msc.io_generation = rec_msc.bot_generation;
    __DMB();
    rec_msc.stage = REC_MSC_STAGE_READ_WAIT;
    return;
  }

  rec_msc_send_csw(pdev);
}

static void rec_msc_send_data(USBD_HandleTypeDef *pdev, uint8_t *buf,
                              uint32_t len) {
  if (len > rec_msc.residue) {
    len = rec_msc.residue;
  }
  __DMB();
  rec_msc.stage = REC_MSC_STAGE_DATA_IN;
  rec_msc.tx_buf = buf;
  rec_msc.tx_len = len;
  rec_msc.tx_pos = 0U;
  rec_msc_send_next_in(pdev);
}

static void rec_msc_fail_ex(USBD_HandleTypeDef *pdev, uint8_t key,
                            uint8_t asc, uint8_t ascq) {
  rec_msc_set_sense_ex(key, asc, ascq);
  rec_msc.csw_status = 1U;
  if (rec_msc.residue == 0U) {
    rec_msc_send_csw(pdev);
    return;
  }

  /* A CSW must not be injected into a data phase.  For example, if READ
   * CAPACITY fails and we transmit the 13-byte CSW immediately, Windows reads
   * its first eight bytes as capacity data and the BOT stream becomes
   * permanently desynchronised.  Terminate the failed data phase with a bulk
   * stall; after the host clears the IN halt, Setup() sends the failed CSW. */
  if ((rec_msc.cbw[12] & 0x80U) == 0U) {
    (void)USBD_LL_StallEP(pdev, REC_MSC_OUT_EP);
  }
  (void)USBD_LL_StallEP(pdev, REC_MSC_IN_EP);
  __DMB();
  rec_msc.stage = REC_MSC_STAGE_STALL;
}

static void rec_msc_fail(USBD_HandleTypeDef *pdev, uint8_t key, uint8_t asc) {
  rec_msc_fail_ex(pdev, key, asc, 0U);
}

static void rec_msc_fail_media(USBD_HandleTypeDef *pdev) {
  if (rec_msc.storage_initializing) {
    rec_msc_fail_ex(pdev, SCSI_SENSE_NOT_READY, SCSI_ASC_LUN_NOT_READY,
                    SCSI_ASCQ_BECOMING_READY);
  } else {
    rec_msc_fail(pdev, SCSI_SENSE_NOT_READY, SCSI_ASC_NO_MEDIUM);
  }
}

static void rec_msc_scsi_inquiry(USBD_HandleTypeDef *pdev,
                                 const uint8_t *cmd) {
  rec_msc.inquiry_seen = 1U;
  static const uint8_t vendor[] REC_CONST = "Typheye ";
  static const uint8_t product[] REC_CONST = "USB SD DISK     ";
  static const uint8_t rev[] REC_CONST = "0001";
  static const uint8_t unit_serial[] REC_CONST = "TOSREC000001";
  uint32_t len;

  /* Match ST's official MSC SCSI implementation: Windows commonly asks for
   * the supported VPD-page list and the unit serial number while creating the
   * disk device.  Returning a standard INQUIRY payload for an EVPD request is
   * protocol-invalid and can make the storage stack retry with long delays. */
  if ((cmd[1] & 0x01U) != 0U) {
    rec_msc_zero(rec_msc.sector_buf, 32U);
    rec_msc.sector_buf[0] = 0x00U;
    if (cmd[2] == 0x00U) {
      rec_msc.sector_buf[1] = 0x00U;
      rec_msc.sector_buf[3] = 3U;
      rec_msc.sector_buf[4] = 0x00U;
      rec_msc.sector_buf[5] = 0x80U;
      rec_msc.sector_buf[6] = 0x83U;
      len = 7U;
    } else if (cmd[2] == 0x80U) {
      rec_msc.sector_buf[1] = 0x80U;
      rec_msc.sector_buf[3] = sizeof(unit_serial) - 1U;
      rec_msc_copy(&rec_msc.sector_buf[4], unit_serial,
                   sizeof(unit_serial) - 1U);
      len = 4U + (sizeof(unit_serial) - 1U);
    } else if (cmd[2] == 0x83U) {
      rec_msc.sector_buf[1] = 0x83U;
      rec_msc.sector_buf[3] = 16U;
      rec_msc.sector_buf[4] = 0x02U; /* ASCII identifier. */
      rec_msc.sector_buf[5] = 0x01U; /* Vendor-specific device identifier. */
      rec_msc.sector_buf[7] = sizeof(unit_serial) - 1U;
      rec_msc_copy(&rec_msc.sector_buf[8], unit_serial,
                   sizeof(unit_serial) - 1U);
      len = 20U;
    } else {
      rec_msc_fail(pdev, SCSI_SENSE_ILLEGAL_REQ,
                   SCSI_ASC_INVALID_FIELD);
      return;
    }
    if (cmd[4] < len) {
      len = cmd[4];
    }
    rec_msc_send_data(pdev, rec_msc.sector_buf, len);
    return;
  }

  if (cmd[2] != 0U) {
    rec_msc_fail(pdev, SCSI_SENSE_ILLEGAL_REQ, SCSI_ASC_INVALID_FIELD);
    return;
  }

  rec_msc_zero(rec_msc.sector_buf, 36U);
  rec_msc.sector_buf[0] = 0x00U;
  /* The LUN exists before an SD card is ready, so it must be reported as
   * removable.  Windows then keeps polling TEST UNIT READY while the USB MSC
   * device itself remains stable; once SD is attached it re-reads capacity and
   * creates the disk/volume without a USB disconnect. */
  rec_msc.sector_buf[1] = 0x80U;
  rec_msc.sector_buf[2] = 0x04U;
  rec_msc.sector_buf[3] = 0x02U;
  rec_msc.sector_buf[4] = 31U;
  rec_msc_copy(&rec_msc.sector_buf[8], vendor, 8U);
  rec_msc_copy(&rec_msc.sector_buf[16], product, 16U);
  rec_msc_copy(&rec_msc.sector_buf[32], rev, 4U);
  len = 36U;
  if (cmd[4] < len) {
    len = cmd[4];
  }
  rec_msc_send_data(pdev, rec_msc.sector_buf, len);
}

static void rec_msc_scsi_request_sense(USBD_HandleTypeDef *pdev) {
  rec_msc_zero(rec_msc.sector_buf, 18U);
  rec_msc.sector_buf[0] = 0x70U;
  rec_msc.sector_buf[2] = rec_msc.sense_key;
  rec_msc.sector_buf[7] = 10U;
  rec_msc.sector_buf[12] = rec_msc.sense_asc;
  rec_msc.sector_buf[13] = rec_msc.sense_ascq;
  rec_msc_send_data(pdev, rec_msc.sector_buf, 18U);
  rec_msc_set_sense(SCSI_SENSE_NO_SENSE, 0U);
}

static void rec_msc_scsi_read_capacity(USBD_HandleTypeDef *pdev) {
  if (!rec_msc_storage_geometry_cached()) {
    rec_msc_fail_media(pdev);
    return;
  }
  rec_msc_put_be32(&rec_msc.sector_buf[0], rec_msc.card_blocks - 1U);
  rec_msc_put_be32(&rec_msc.sector_buf[4], REC_MSC_BLOCK_SIZE);
  rec_msc_send_data(pdev, rec_msc.sector_buf, 8U);
}

static void rec_msc_scsi_read_format_cap(USBD_HandleTypeDef *pdev) {
  if (!rec_msc_storage_geometry_cached()) {
    rec_msc_fail_media(pdev);
    return;
  }
  rec_msc_zero(rec_msc.sector_buf, 12U);
  rec_msc.sector_buf[3] = 8U;
  /* Match ST's current MSC SCSI implementation. */
  rec_msc_put_be32(&rec_msc.sector_buf[4], rec_msc.card_blocks);
  rec_msc.sector_buf[8] = 0x02U;
  rec_msc.sector_buf[9] = 0x00U;
  rec_msc.sector_buf[10] = 0x02U;
  rec_msc.sector_buf[11] = 0x00U;
  rec_msc_send_data(pdev, rec_msc.sector_buf, 12U);
}

static void rec_msc_scsi_read_capacity16(USBD_HandleTypeDef *pdev,
                                         const uint8_t *cmd) {
  uint32_t alloc_len = rec_msc_get_be32(&cmd[10]);
  uint32_t len = alloc_len < 32U ? alloc_len : 32U;
  if ((cmd[1] & 0x1FU) != 0x10U) {
    rec_msc_fail(pdev, SCSI_SENSE_ILLEGAL_REQ, SCSI_ASC_INVALID_FIELD);
    return;
  }
  if (!rec_msc_storage_geometry_cached()) {
    rec_msc_fail_media(pdev);
    return;
  }
  rec_msc_zero(rec_msc.sector_buf, 32U);
  rec_msc_put_be64(&rec_msc.sector_buf[0],
                   (uint64_t)rec_msc.card_blocks - 1ULL);
  rec_msc_put_be32(&rec_msc.sector_buf[8], REC_MSC_BLOCK_SIZE);
  rec_msc_send_data(pdev, rec_msc.sector_buf, len);
}

static void rec_msc_scsi_report_luns(USBD_HandleTypeDef *pdev,
                                     const uint8_t *cmd) {
  uint32_t alloc_len = rec_msc_get_be32(&cmd[6]);
  uint32_t len = alloc_len < 16U ? alloc_len : 16U;
  rec_msc_zero(rec_msc.sector_buf, 16U);
  rec_msc.sector_buf[3] = 8U;
  rec_msc_send_data(pdev, rec_msc.sector_buf, len);
}

static void rec_msc_scsi_mode_sense6(USBD_HandleTypeDef *pdev) {
  rec_msc_zero(rec_msc.sector_buf, 4U);
  rec_msc.sector_buf[0] = 3U;
  rec_msc_send_data(pdev, rec_msc.sector_buf, 4U);
}

static void rec_msc_scsi_mode_sense10(USBD_HandleTypeDef *pdev) {
  rec_msc_zero(rec_msc.sector_buf, 8U);
  /* Eight-byte MODE PARAMETER HEADER(10), no block descriptor/pages. */
  rec_msc.sector_buf[1] = 6U;
  rec_msc_send_data(pdev, rec_msc.sector_buf, 8U);
}

static void rec_msc_scsi_read10(USBD_HandleTypeDef *pdev, const uint8_t *cmd) {
  uint32_t lba = rec_msc_get_be32(&cmd[2]);
  uint32_t blocks = rec_msc_get_be16(&cmd[7]);
  uint32_t max_blocks = rec_msc.residue / REC_MSC_BLOCK_SIZE;

  if (!rec_msc_storage_online_cached()) {
    rec_msc_fail_media(pdev);
    return;
  }
  if (blocks > max_blocks) {
    blocks = max_blocks;
  }
  if (blocks == 0U) {
    rec_msc_send_csw(pdev);
    return;
  }
  if (lba >= rec_msc.card_blocks || blocks > (rec_msc.card_blocks - lba)) {
    rec_msc_fail(pdev, SCSI_SENSE_ILLEGAL_REQ, SCSI_ASC_LBA_RANGE);
    return;
  }
  rec_msc.read_lba = lba;
  rec_msc.read_blocks = blocks;
  rec_msc.tx_len = 0U;
  rec_msc.tx_pos = 0U;
  rec_msc.io_generation = rec_msc.bot_generation;
  __DMB();
  rec_msc.stage = REC_MSC_STAGE_READ_WAIT;
}

static void rec_msc_scsi_write10(USBD_HandleTypeDef *pdev, const uint8_t *cmd) {
  uint32_t lba = rec_msc_get_be32(&cmd[2]);
  uint32_t blocks = rec_msc_get_be16(&cmd[7]);
  uint32_t max_bytes = blocks * REC_MSC_BLOCK_SIZE;

  if (!rec_msc_storage_online_cached()) {
    rec_msc_fail_media(pdev);
    return;
  }
  if (blocks == 0U || rec_msc.residue == 0U) {
    rec_msc_send_csw(pdev);
    return;
  }
  if (lba >= rec_msc.card_blocks || blocks > (rec_msc.card_blocks - lba)) {
    rec_msc_fail(pdev, SCSI_SENSE_ILLEGAL_REQ, SCSI_ASC_LBA_RANGE);
    return;
  }
  if (rec_msc.residue != max_bytes) {
    rec_msc_fail(pdev, SCSI_SENSE_ILLEGAL_REQ, SCSI_ASC_INVALID_FIELD);
    return;
  }
  rec_msc.write_lba = lba;
  rec_msc.write_expected = max_bytes;
  rec_msc.write_received = 0U;
  rec_msc.sector_pos = 0U;
  rec_msc.io_blocks = 0U;
  __DMB();
  rec_msc.stage = REC_MSC_STAGE_DATA_OUT;
  (void)USBD_LL_PrepareReceive(pdev, REC_MSC_OUT_EP, rec_msc.rx,
                               REC_MSC_EP_MPS);
}

static void rec_msc_dispatch_scsi(USBD_HandleTypeDef *pdev) {
  const uint8_t *cmd = &rec_msc.cbw[15];
  rec_msc.csw_status = 0U;
  rec_msc.read_blocks = 0U;
  rec_msc.tx_len = 0U;
  rec_msc.tx_pos = 0U;

  /* Do not fail a capacity/readiness command while the first bounded SD probe
   * is running.  A failed data-in command stalls Bulk IN by BOT rules; Windows
   * can leave that recovery pending for about 30 seconds, which also prevented
   * the old main-loop code from ever observing an idle CBW boundary.  Keep the
   * current CBW in NAK instead and resume it as soon as the probe completes. */
  if (!rec_msc_storage_online_cached() &&
      rec_msc_command_needs_media(cmd[0])) {
    uint32_t now = HAL_GetTick();
    uint8_t probe_due =
        (uint8_t)(rec_msc.retry_at == 0U ||
                  (int32_t)(now - rec_msc.retry_at) >= 0);
    if (rec_msc.storage_initializing || rec_msc.storage_attach_pending ||
        probe_due) {
      rec_msc.media_probe_enabled = 1U;
      rec_msc.media_probe_at = now;
      rec_msc.media_wait_deadline = now + REC_MSC_MEDIA_WAIT_MS;
      __DMB();
      rec_msc.stage = REC_MSC_STAGE_MEDIA_WAIT;
      return;
    }
  }

  /* Report insertion/removal on TEST UNIT READY, which has no data phase and
   * therefore cannot desynchronise BOT with a capacity payload/CSW stall.  If
   * the host asks for capacity directly, it simply receives current geometry. */
  if (rec_msc.unit_attention && cmd[0] == SCSI_TEST_UNIT_READY) {
    rec_msc.unit_attention = 0U;
    rec_msc_fail(pdev, SCSI_SENSE_UNIT_ATTENTION,
                 SCSI_ASC_MEDIUM_CHANGED);
    return;
  }

  switch (cmd[0]) {
    case SCSI_TEST_UNIT_READY:
      if (rec_msc_storage_online_cached()) {
        rec_msc_send_csw(pdev);
      } else {
        rec_msc_fail_media(pdev);
      }
      break;
    case SCSI_INQUIRY:
      rec_msc_scsi_inquiry(pdev, cmd);
      break;
    case SCSI_REQUEST_SENSE:
      rec_msc_scsi_request_sense(pdev);
      break;
    case SCSI_READ_CAPACITY10:
      rec_msc_scsi_read_capacity(pdev);
      break;
    case SCSI_READ_FORMAT_CAP:
      rec_msc_scsi_read_format_cap(pdev);
      break;
    case SCSI_MODE_SENSE6:
      rec_msc_scsi_mode_sense6(pdev);
      break;
    case SCSI_MODE_SENSE10:
      rec_msc_scsi_mode_sense10(pdev);
      break;
    case SCSI_READ_CAPACITY16:
      rec_msc_scsi_read_capacity16(pdev, cmd);
      break;
    case SCSI_REPORT_LUNS:
      rec_msc_scsi_report_luns(pdev, cmd);
      break;
    case SCSI_READ10:
      rec_msc_scsi_read10(pdev, cmd);
      break;
    case SCSI_WRITE10:
      rec_msc_scsi_write10(pdev, cmd);
      break;
    case SCSI_VERIFY10:
    case SCSI_PREVENT_ALLOW:
      rec_msc_send_csw(pdev);
      break;
    case SCSI_START_STOP_UNIT:
      /* Windows may issue LoEj/Start=0 during normal probing or when the shell
       * refreshes the drive.  Do not treat it as physical media removal; REC
       * has no tray and dropping SD here makes the next capacity/read command
       * look like an empty/no-media disk. */
      rec_msc_send_csw(pdev);
      break;
    case SCSI_SYNC_CACHE10:
      if (!rec_msc_storage_online_cached()) {
        rec_msc_fail_media(pdev);
      } else {
        rec_msc.io_generation = rec_msc.bot_generation;
        __DMB();
        rec_msc.stage = REC_MSC_STAGE_SYNC_WAIT;
      }
      break;
    default:
      rec_msc_fail(pdev, SCSI_SENSE_ILLEGAL_REQ, SCSI_ASC_INVALID_CMD);
      break;
  }
}

static void rec_msc_parse_cbw(USBD_HandleTypeDef *pdev, uint32_t len) {
  if (len != sizeof(rec_msc.cbw) ||
      rec_msc_get_le32(&rec_msc.cbw[0]) != MSC_CBW_SIGNATURE ||
      rec_msc.cbw[13] != 0U || rec_msc.cbw[14] == 0U ||
      rec_msc.cbw[14] > 16U) {
    (void)USBD_LL_StallEP(pdev, REC_MSC_IN_EP);
    (void)USBD_LL_StallEP(pdev, REC_MSC_OUT_EP);
    rec_msc.stage = REC_MSC_STAGE_RECOVERY;
    return;
  }
  rec_msc.tag = rec_msc_get_le32(&rec_msc.cbw[4]);
  rec_msc.residue = rec_msc_get_le32(&rec_msc.cbw[8]);
  rec_msc.cbw_seen = 1U;

  /* A valid CBW is the protocol confirmation required before touching SDIO.
   * Start the probe immediately; waiting for GET_MAX_LUN + INQUIRY + a quiet
   * CBW window starved on Windows because it continuously polls an empty LUN. */
  if (!rec_msc_storage_online_cached()) {
    rec_msc.media_probe_enabled = 1U;
    rec_msc.media_probe_at = HAL_GetTick();
  }

  /* If the thread completed SD identification between two CBWs, publish the
   * new geometry here, inside the USB callback, before decoding this command. */
  if (rec_msc.storage_attach_pending) {
    (void)rec_msc_apply_pending_storage();
  }
  rec_msc_dispatch_scsi(pdev);
}

static void rec_msc_handle_write_data(USBD_HandleTypeDef *pdev, uint32_t len) {
  uint32_t pos = 0U;
  while (pos < len && rec_msc.write_received < rec_msc.write_expected) {
    uint32_t room = REC_MSC_IO_BUFFER_SIZE - rec_msc.sector_pos;
    uint32_t left = len - pos;
    uint32_t n = (left < room) ? left : room;
    uint32_t expected_left = rec_msc.write_expected - rec_msc.write_received;
    if (n > expected_left) {
      n = expected_left;
    }
    rec_msc_copy(&rec_msc.sector_buf[rec_msc.sector_pos],
                 &rec_msc.rx[pos], n);
    rec_msc.sector_pos += n;
    rec_msc.write_received += n;
    pos += n;
    if (rec_msc.residue >= n) {
      rec_msc.residue -= n;
    } else {
      rec_msc.residue = 0U;
    }
    if (rec_msc.sector_pos == REC_MSC_IO_BUFFER_SIZE ||
        rec_msc.write_received == rec_msc.write_expected) {
      if ((rec_msc.sector_pos % REC_MSC_BLOCK_SIZE) != 0U) {
        rec_msc.csw_status = 1U;
        rec_msc_set_sense(SCSI_SENSE_ILLEGAL_REQ, SCSI_ASC_INVALID_FIELD);
        rec_msc_send_csw(pdev);
        return;
      }
      /* Do not issue SDIO commands from the USB OUT callback.  Leave the OUT
       * endpoint unarmed so the host sees NAK until REC_MSC_Tick() commits the
       * accumulated multi-sector chunk. */
      rec_msc.io_blocks = rec_msc.sector_pos / REC_MSC_BLOCK_SIZE;
      rec_msc.io_generation = rec_msc.bot_generation;
      __DMB();
      rec_msc.stage = REC_MSC_STAGE_WRITE_WAIT;
      return;
    }
  }

  if (rec_msc.write_received >= rec_msc.write_expected) {
    if (rec_msc.sector_pos != 0U) {
      rec_msc.csw_status = 1U;
      rec_msc_set_sense(SCSI_SENSE_ILLEGAL_REQ, SCSI_ASC_LBA_RANGE);
    }
    rec_msc_send_csw(pdev);
  } else {
    (void)USBD_LL_PrepareReceive(pdev, REC_MSC_OUT_EP, rec_msc.rx,
                                 REC_MSC_EP_MPS);
  }
}

static uint8_t REC_USBD_MSC_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
  (void)cfgidx;
  rec_msc.bot_generation++;
  rec_msc.stage = REC_MSC_STAGE_CBW;
  rec_msc.control_seen = 0U;
  rec_msc.cbw_seen = 0U;
  rec_msc.inquiry_seen = 0U;
  rec_msc.media_probe_enabled = 0U;
  rec_msc.protocol_fallback_at =
      HAL_GetTick() + REC_MSC_PROTOCOL_FALLBACK_MS;
  rec_msc.media_probe_at = 0U;
  rec_msc.media_wait_deadline = 0U;
  rec_msc.csw_status = 0U;
  rec_msc.residue = 0U;
  rec_msc_set_sense(SCSI_SENSE_NO_SENSE, 0U);
  pdev->pClassData = &rec_msc;
  pdev->pClassDataCmsit[pdev->classId] = &rec_msc;
  (void)USBD_LL_OpenEP(pdev, REC_MSC_IN_EP, USBD_EP_TYPE_BULK,
                       REC_MSC_EP_MPS);
  (void)USBD_LL_OpenEP(pdev, REC_MSC_OUT_EP, USBD_EP_TYPE_BULK,
                       REC_MSC_EP_MPS);
  /* Keep the core endpoint bookkeeping consistent with ST's official MSC
   * class.  Standard CLEAR_FEATURE handling consults these flags; leaving
   * them unset can turn a recoverable BOT halt into a host-side timeout. */
  pdev->ep_in[REC_MSC_IN_EP & 0x0FU].is_used = 1U;
  pdev->ep_out[REC_MSC_OUT_EP & 0x0FU].is_used = 1U;
  rec_msc_prepare_cbw(pdev);
  return (uint8_t)USBD_OK;
}

static uint8_t REC_USBD_MSC_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
  (void)cfgidx;
  (void)USBD_LL_CloseEP(pdev, REC_MSC_IN_EP);
  (void)USBD_LL_CloseEP(pdev, REC_MSC_OUT_EP);
  pdev->ep_in[REC_MSC_IN_EP & 0x0FU].is_used = 0U;
  pdev->ep_out[REC_MSC_OUT_EP & 0x0FU].is_used = 0U;
  pdev->pClassData = NULL;
  pdev->pClassDataCmsit[pdev->classId] = NULL;
  return (uint8_t)USBD_OK;
}

static uint8_t REC_USBD_MSC_Setup(USBD_HandleTypeDef *pdev,
                                  USBD_SetupReqTypedef *req) {
  switch (req->bmRequest & USB_REQ_TYPE_MASK) {
    case USB_REQ_TYPE_CLASS:
      if (req->bRequest == MSC_REQ_GET_MAX_LUN && req->wValue == 0U &&
          req->wLength == 1U && (req->bmRequest & 0x80U) != 0U) {
        /* EP0 transmission is asynchronous.  Never pass a pointer to a stack
         * variable that ceases to exist when Setup() returns. */
        rec_msc.control_seen = 1U;
        rec_msc_ep0_reply[0] = 0U;
        (void)USBD_CtlSendData(pdev, rec_msc_ep0_reply, 1U);
      } else if (req->bRequest == MSC_REQ_BOT_RESET && req->wValue == 0U &&
                 req->wLength == 0U && (req->bmRequest & 0x80U) == 0U) {
        rec_msc.bot_generation++;
        (void)USBD_LL_FlushEP(pdev, REC_MSC_IN_EP);
        (void)USBD_LL_FlushEP(pdev, REC_MSC_OUT_EP);
        (void)USBD_LL_ClearStallEP(pdev, REC_MSC_IN_EP);
        (void)USBD_LL_ClearStallEP(pdev, REC_MSC_OUT_EP);
        rec_msc.csw_status = 0U;
        rec_msc.residue = 0U;
        rec_msc.read_blocks = 0U;
        rec_msc.write_expected = 0U;
        rec_msc.write_received = 0U;
        rec_msc.sector_pos = 0U;
        rec_msc_prepare_cbw(pdev);
      } else {
        USBD_CtlError(pdev, req);
        return (uint8_t)USBD_FAIL;
      }
      break;
    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest) {
        case USB_REQ_CLEAR_FEATURE:
          if (rec_msc.stage == REC_MSC_STAGE_RECOVERY) {
            /* An invalid CBW requires the class-specific BOT reset sequence;
             * clearing endpoint halts alone must not resume the stream. */
            (void)USBD_LL_StallEP(pdev, REC_MSC_IN_EP);
            (void)USBD_LL_StallEP(pdev, REC_MSC_OUT_EP);
          } else if (rec_msc.stage == REC_MSC_STAGE_STALL &&
                     ((uint8_t)req->wIndex & 0x80U) != 0U) {
            rec_msc_send_csw(pdev);
          }
          break;
        case USB_REQ_GET_STATUS:
          rec_msc_ep0_reply[0] = 0U;
          rec_msc_ep0_reply[1] = 0U;
          (void)USBD_CtlSendData(pdev, rec_msc_ep0_reply, 2U);
          break;
        case USB_REQ_GET_INTERFACE:
          rec_msc_ep0_reply[0] = 0U;
          (void)USBD_CtlSendData(pdev, rec_msc_ep0_reply, 1U);
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

static uint8_t REC_USBD_MSC_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum) {
  (void)epnum;
  if (rec_msc.stage == REC_MSC_STAGE_DATA_IN) {
    rec_msc_send_next_in(pdev);
  } else if (rec_msc.stage == REC_MSC_STAGE_CSW) {
    rec_msc_prepare_cbw(pdev);
  }
  return (uint8_t)USBD_OK;
}

static uint8_t REC_USBD_MSC_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum) {
  uint32_t len = USBD_LL_GetRxDataSize(pdev, epnum);
  if (rec_msc.stage == REC_MSC_STAGE_CBW) {
    rec_msc_parse_cbw(pdev, len);
  } else if (rec_msc.stage == REC_MSC_STAGE_DATA_OUT) {
    if (len > REC_MSC_EP_MPS) {
      len = REC_MSC_EP_MPS;
    }
    rec_msc_handle_write_data(pdev, len);
  } else {
    rec_msc_prepare_cbw(pdev);
  }
  return (uint8_t)USBD_OK;
}

REC_CODE uint8_t REC_MSC_IsConfigured(void) {
  return (uint8_t)(rec_msc.started &&
                   rec_msc_dev.dev_state == USBD_STATE_CONFIGURED);
}

REC_CODE uint8_t REC_MSC_IsStarted(void) {
  return rec_msc.started ? 1U : 0U;
}

REC_CODE uint8_t REC_MSC_Start(void) {
  uint32_t now;

  if (rec_msc.started) {
    return 1U;
  }
  rec_msc.sector_buf = rec_msc_sector_storage;

  rec_msc.storage_ready = 0U;
  rec_msc.storage_initializing = 0U;
  rec_msc.storage_backend = REC_MSC_BACKEND_NONE;
  rec_msc.storage_attach_pending = 0U;
  rec_msc.unit_attention = 0U;
  rec_msc.control_seen = 0U;
  rec_msc.cbw_seen = 0U;
  rec_msc.inquiry_seen = 0U;
  rec_msc.media_probe_enabled = 0U;
  rec_msc.card_blocks = 0U;
  rec_msc.pending_blocks = 0U;
  rec_msc.stage = REC_MSC_STAGE_CBW;
  rec_msc.csw_status = 0U;
  rec_msc.residue = 0U;
  rec_msc.read_blocks = 0U;
  rec_msc.write_expected = 0U;
  rec_msc.write_received = 0U;
  rec_msc.sector_pos = 0U;
  rec_msc.io_blocks = 0U;
  rec_msc.bot_generation = 0U;
  rec_msc.io_generation = 0U;
  rec_msc_set_sense(SCSI_SENSE_NO_SENSE, 0U);
  rec_msc_copy(rec_msc_cfg_desc, rec_msc_cfg_desc_template,
               sizeof(rec_msc_cfg_desc));

  /* Start one stable removable MSC device immediately.  Until SD is ready the
   * LUN reports NO MEDIUM/BECOMING READY; USB enumeration and BOT protocol are
   * therefore independent from card identification, and no fake filesystem or
   * cached 64 KiB geometry has to be replaced later. */
  if (USBD_Init(&rec_msc_dev, (USBD_DescriptorsTypeDef *)&REC_MSC_Desc,
                DEVICE_FS) != USBD_OK) {
    (void)USBD_DeInit(&rec_msc_dev);
    rec_msc.retry_at = HAL_GetTick() + REC_MSC_RETRY_MS;
    return 0U;
  }
  /* CubeMX's shared PCD MSP callback installs OTG_FS at priority 0.  Lower it
   * before connecting the pull-up so SysTick and the SDIO polling timebase can
   * continue while the host enumerates the MSC device. */
  HAL_NVIC_SetPriority(OTG_FS_IRQn, 6U, 0U);
  if (USBD_RegisterClass(&rec_msc_dev,
                         (USBD_ClassTypeDef *)&REC_USBD_MSC_CLASS) != USBD_OK) {
    (void)USBD_DeInit(&rec_msc_dev);
    rec_msc.retry_at = HAL_GetTick() + REC_MSC_RETRY_MS;
    return 0U;
  }
  if (USBD_Start(&rec_msc_dev) != USBD_OK) {
    (void)USBD_Stop(&rec_msc_dev);
    (void)USBD_DeInit(&rec_msc_dev);
    rec_msc.retry_at = HAL_GetTick() + REC_MSC_RETRY_MS;
    return 0U;
  }
  rec_msc.started = 1U;
  now = HAL_GetTick();
  rec_msc.enum_deadline = now + REC_MSC_ENUM_GRACE_MS;
  rec_msc.protocol_fallback_at = now + REC_MSC_PROTOCOL_FALLBACK_MS;
  rec_msc.media_probe_at = 0U;
  rec_msc.media_wait_deadline = 0U;
  rec_msc.retry_at = 0U;
  return 1U;
}

REC_CODE void REC_MSC_Stop(void) {
  if (rec_msc.started) {
    (void)USBD_Stop(&rec_msc_dev);
    (void)USBD_DeInit(&rec_msc_dev);
  }
  rec_msc.started = 0U;
  REC_BlockRelease();
  rec_msc.storage_initializing = 0U;
  rec_msc.storage_attach_pending = 0U;
  rec_msc.pending_blocks = 0U;
  rec_msc_storage_mark_offline(SCSI_SENSE_NO_SENSE, 0U);
  rec_msc.stage = REC_MSC_STAGE_CBW;
  rec_msc.control_seen = 0U;
  rec_msc.cbw_seen = 0U;
  rec_msc.inquiry_seen = 0U;
  rec_msc.media_probe_enabled = 0U;
  rec_msc.enum_deadline = 0U;
  rec_msc.protocol_fallback_at = 0U;
  rec_msc.media_probe_at = 0U;
  rec_msc.media_wait_deadline = 0U;
}

REC_CODE void REC_MSC_Tick(void) {
  uint32_t now;
  uint8_t generation;
  uint8_t ok;

  now = HAL_GetTick();
  if (!rec_msc.started) {
    if (rec_msc.retry_at == 0U ||
        (int32_t)(now - rec_msc.retry_at) >= 0) {
      rec_msc.retry_at = now + REC_MSC_RETRY_MS;
      (void)REC_MSC_Start();
    }
    return;
  }

  /* A valid CBW, not a timing gap, enables SD probing.  The probe deliberately
   * runs regardless of the instantaneous BOT stage: requiring stage==CBW made
   * the main loop lose every race to Windows' next command and could postpone
   * initialization until the host's roughly 30-second error-recovery timeout. */
  uint8_t probe_ran = 0U;
  if (rec_msc.media_probe_enabled && REC_MSC_IsConfigured() &&
      rec_msc.storage_backend != REC_MSC_BACKEND_SD &&
      !rec_msc.storage_attach_pending &&
      (int32_t)(now - rec_msc.media_probe_at) >= 0 &&
      (rec_msc.retry_at == 0U || (int32_t)(now - rec_msc.retry_at) >= 0)) {
    probe_ran = 1U;
    if (rec_msc_storage_probe()) {
      rec_msc.retry_at = 0U;
    } else {
      now = HAL_GetTick();
      rec_msc.retry_at = now +
          (rec_msc.pending_blocks > 0U ? REC_MSC_READY_RETRY_MS
                                       : REC_MSC_RETRY_MS);
      rec_msc.media_probe_at = rec_msc.retry_at;
    }
  }

  if (REC_MSC_IsConfigured() && rec_msc.storage_attach_pending) {
    rec_msc_resume_waiting_media();
  }

  /* If the bounded probe found no medium, finish the held command now instead
   * of keeping Bulk IN/OUT NAK until the host times out.  A card that identified
   * but has not reached TRANSFER receives short 50 ms retries, with an absolute
   * cap for the current CBW. */
  if (rec_msc.stage == REC_MSC_STAGE_MEDIA_WAIT &&
      !rec_msc.storage_attach_pending) {
    now = HAL_GetTick();
    if ((probe_ran && rec_msc.pending_blocks == 0U) ||
        (rec_msc.media_wait_deadline != 0U &&
         (int32_t)(now - rec_msc.media_wait_deadline) >= 0)) {
      rec_msc.storage_initializing = 0U;
      rec_msc.retry_at = now + REC_MSC_RETRY_MS;
      rec_msc.media_wait_deadline = 0U;
      rec_msc_dispatch_scsi(&rec_msc_dev);
    }
  }

  if (rec_msc.stage == REC_MSC_STAGE_READ_WAIT) {
    uint32_t blocks = rec_msc.read_blocks;
    if (blocks > REC_MSC_IO_BLOCKS) {
      blocks = REC_MSC_IO_BLOCKS;
    }
    generation = rec_msc.io_generation;
    ok = REC_BlockRead(rec_msc.read_lba, rec_msc.sector_buf, blocks);
    if (generation != rec_msc.bot_generation ||
        rec_msc.stage != REC_MSC_STAGE_READ_WAIT) {
      return;
    }
    if (!ok) {
      REC_BlockRelease();
      rec_msc_storage_mark_offline(SCSI_SENSE_MEDIUM_ERROR,
                                   SCSI_ASC_READ_ERROR);
      rec_msc.retry_at = HAL_GetTick() + REC_MSC_RETRY_MS;
      rec_msc.csw_status = 1U;
      rec_msc.read_blocks = 0U;
      rec_msc_send_csw(&rec_msc_dev);
      return;
    }
    rec_msc.read_lba += blocks;
    rec_msc.read_blocks -= blocks;
    rec_msc.tx_buf = rec_msc.sector_buf;
    rec_msc.tx_len = blocks * REC_MSC_BLOCK_SIZE;
    rec_msc.tx_pos = 0U;
    __DMB();
    rec_msc.stage = REC_MSC_STAGE_DATA_IN;
    rec_msc_send_next_in(&rec_msc_dev);
    return;
  }

  if (rec_msc.stage == REC_MSC_STAGE_WRITE_WAIT) {
    generation = rec_msc.io_generation;
    ok = REC_BlockWrite(rec_msc.write_lba, rec_msc.sector_buf,
                        rec_msc.io_blocks);
    if (generation != rec_msc.bot_generation ||
        rec_msc.stage != REC_MSC_STAGE_WRITE_WAIT) {
      return;
    }
    if (!ok) {
      REC_BlockRelease();
      rec_msc_storage_mark_offline(SCSI_SENSE_MEDIUM_ERROR,
                                   SCSI_ASC_WRITE_FAULT);
      rec_msc.retry_at = HAL_GetTick() + REC_MSC_RETRY_MS;
      rec_msc.csw_status = 1U;
      rec_msc.write_received = rec_msc.write_expected;
      rec_msc.sector_pos = 0U;
      rec_msc_send_csw(&rec_msc_dev);
      return;
    }
    rec_msc.write_lba += rec_msc.io_blocks;
    rec_msc.io_blocks = 0U;
    rec_msc.sector_pos = 0U;
    if (rec_msc.write_received >= rec_msc.write_expected) {
      rec_msc_send_csw(&rec_msc_dev);
    } else {
      __DMB();
      rec_msc.stage = REC_MSC_STAGE_DATA_OUT;
      (void)USBD_LL_PrepareReceive(&rec_msc_dev, REC_MSC_OUT_EP,
                                   rec_msc.rx, REC_MSC_EP_MPS);
    }
    return;
  }

  if (rec_msc.stage == REC_MSC_STAGE_SYNC_WAIT) {
    generation = rec_msc.io_generation;
    ok = REC_BlockSync();
    if (generation != rec_msc.bot_generation ||
        rec_msc.stage != REC_MSC_STAGE_SYNC_WAIT) {
      return;
    }
    if (!ok) {
      REC_BlockRelease();
      rec_msc_storage_mark_offline(SCSI_SENSE_MEDIUM_ERROR,
                                   SCSI_ASC_WRITE_FAULT);
      rec_msc.retry_at = HAL_GetTick() + REC_MSC_RETRY_MS;
      rec_msc.csw_status = 1U;
    }
    rec_msc_send_csw(&rec_msc_dev);
    return;
  }

  if (REC_MSC_IsConfigured()) {
    rec_msc.enum_deadline = now + REC_MSC_ENUM_GRACE_MS;
    return;
  }
  /* Do not auto-disconnect active REC MSC.  Windows may still be probing the
   * device or retrying after a medium error; forcing a USB stop/start here
   * turns a recoverable empty/no-medium state into an Unknown Device. */
}
