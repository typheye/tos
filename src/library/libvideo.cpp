#include "include/libvideo.h"

#include "core/manager/include/settings_manager.h"
#include "core/sys/include/sysfonts.h"
#include "core/sys/include/syswatchdog.h"
#include "dram.h"
#include "ff.h"
#include "hardware/include/lcd.h"
#include "stm32f4xx_hal.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace {
constexpr uint16_t kVersion = 1U;
constexpr uint16_t kHeaderSize = 40U;
constexpr uint16_t kFlagLoop = 0x0001U;
constexpr uint8_t kCodecRaw = 1U;
constexpr uint8_t kCodecRle = 2U;
constexpr uint32_t kRleReadBufferSize = 2048U;
constexpr uint32_t kFramePixels = LCD_WIDTH * LCD_HEIGHT;
constexpr uint32_t kRawPayloadSize = kFramePixels * 2U;
constexpr uint32_t kMaxRlePayloadSize = kFramePixels * 4U;

uint8_t rleReadBuffer[kRleReadBufferSize] __attribute__((aligned(4)));
uint16_t measuredFps = 0U;
char dashboardPerf[28] = "FPS:   0F | CPU: 100%";
char dashboardMemory[28] = "RAM:   0% | CRM:   0%";

void video_put_pixel(uint16_t *pixels, uint16_t tile_y, uint16_t tile_h,
                     uint16_t x, uint16_t y, uint16_t color) {
  if (x >= LCD_WIDTH || y < tile_y || y >= tile_y + tile_h)
    return;
  pixels[static_cast<uint32_t>(y - tile_y) * LCD_WIDTH + x] = color;
}

uint16_t ascii12_string_width(const char *text) {
  const size_t length = strlen(text);
  return length == 0U ? 0U : static_cast<uint16_t>(length * 7U - 1U);
}

void sample_dashboard(void) {
  SysDramStats_t ram{};
  SysDramStats_t ccm{};
  (void)SysDram_GetStats(SYSDRAM_REGION_RAM, &ram);
  (void)SysDram_GetStats(SYSDRAM_REGION_CCM, &ccm);
  constexpr uint32_t ram_total = 128U * 1024U;
  constexpr uint32_t ccm_total = 64U * 1024U;
  uint32_t ram_used = (ram_total > ram.total ? ram_total - ram.total : 0U) +
                      ram.used;
  uint32_t ccm_used = (ccm_total > ccm.total ? ccm_total - ccm.total : 0U) +
                      ccm.used;
  if (ram_used > ram_total)
    ram_used = ram_total;
  if (ccm_used > ccm_total)
    ccm_used = ccm_total;
  const unsigned ram_pct = (ram_used * 100U + ram_total / 2U) / ram_total;
  const unsigned ccm_pct = (ccm_used * 100U + ccm_total / 2U) / ccm_total;
  snprintf(dashboardPerf, sizeof(dashboardPerf), "FPS: %3uF | CPU: %3u%%",
           static_cast<unsigned>(measuredFps > 999U ? 999U : measuredFps),
           100U);
  snprintf(dashboardMemory, sizeof(dashboardMemory),
           "RAM: %3u%% | CRM: %3u%%", ram_pct, ccm_pct);
}

void draw_ascii12(uint16_t *pixels, uint16_t tile_y, uint16_t tile_h,
                  uint16_t x, uint16_t y, const char *text,
                  uint16_t color) {
  const pFONT font = ASCII_Font12;
  for (; *text != '\0'; ++text, x += font->Width + 1U) {
    const char ch = (*text >= 32 && *text <= 126) ? *text : '?';
    const uint8_t *glyph =
        font->pTable + static_cast<uint16_t>(ch - 32) * font->Bytes;
    uint16_t pixel = 0U;
    uint8_t column = 0U;
    for (uint16_t index = 0U; index < font->Bytes; ++index) {
      uint8_t bits = glyph[index];
      for (uint8_t bit = 0U; bit < 8U; ++bit) {
        if ((bits & 0x01U) != 0U) {
          video_put_pixel(
              pixels, tile_y, tile_h, static_cast<uint16_t>(x + column),
              static_cast<uint16_t>(y + pixel / font->Width), color);
        }
        ++pixel;
        ++column;
        if (column == font->Width) {
          column = 0U;
          break;
        }
        bits >>= 1U;
      }
    }
  }
}

void draw_dashboard(uint16_t *pixels, uint16_t tile_y, uint16_t tile_h) {
  if (!SM_Debug_Dashboard())
    return;
  const uint16_t y0 = LCD_HEIGHT - 29U;
  if (tile_y + tile_h <= y0 || tile_y >= LCD_HEIGHT)
    return;

  const uint16_t perf_width = ascii12_string_width(dashboardPerf);
  const uint16_t mem_width = ascii12_string_width(dashboardMemory);
  const uint16_t width =
      static_cast<uint16_t>((perf_width > mem_width ? perf_width : mem_width) +
                            8U);
  const uint16_t x0 = static_cast<uint16_t>(LCD_WIDTH - width);
  for (uint16_t y = y0; y < LCD_HEIGHT; ++y) {
    uint16_t *span =
        pixels + static_cast<uint32_t>(y - tile_y) * LCD_WIDTH + x0;
    memset(span, 0, static_cast<size_t>(width) * sizeof(uint16_t));
  }
  draw_ascii12(pixels, tile_y, tile_h, x0 + 4U, y0 + 2U, dashboardPerf,
               0x07E0U);
  draw_ascii12(pixels, tile_y, tile_h, x0 + 4U, y0 + 15U, dashboardMemory,
               0xFFE0U);
}

#pragma pack(push, 1)
struct FileHeader {
  char magic[4];
  uint16_t version;
  uint16_t header_size;
  uint16_t width;
  uint16_t height;
  uint16_t fps;
  uint16_t flags;
  uint32_t frame_count;
  uint32_t codec;
  uint32_t reserved[4];
};

struct FrameHeader {
  char magic[4];
  uint32_t timestamp_ms;
  uint8_t codec;
  uint8_t flags;
  uint16_t reserved;
  uint32_t payload_size;
  uint32_t pixel_count;
};
#pragma pack(pop)

static_assert(sizeof(FileHeader) == 40U, "TVID file header layout mismatch");
static_assert(sizeof(FrameHeader) == 20U, "TVID frame header layout mismatch");

bool pumped(LibVideoPumpFn pump, void *user) {
  SysWatchdog_Tick();
  return pump && pump(user) != 0;
}

bool read_exact(FIL *file, void *dst, UINT size) {
  UINT read = 0U;
  return f_read(file, dst, size, &read) == FR_OK && read == size;
}

uint16_t wire_to_native(uint8_t high, uint8_t low) {
  return static_cast<uint16_t>((static_cast<uint16_t>(high) << 8U) | low);
}

LibVideoResult render_raw(FIL *file, const FrameHeader &frame,
                          LibVideoPumpFn pump, void *user) {
  if (frame.payload_size != kRawPayloadSize)
    return LIBVIDEO_INVALID;

  for (uint16_t y = 0U; y < LCD_HEIGHT; y += TILE_HEIGHT) {
    const uint16_t h = static_cast<uint16_t>(
        (y + TILE_HEIGHT <= LCD_HEIGHT) ? TILE_HEIGHT : LCD_HEIGHT - y);
    LCD_BeginTileRender(y, h);
    uint16_t *pixels = LCD_GetFrameBuffer();
    const UINT bytes = static_cast<UINT>(LCD_WIDTH * h * 2U);
    if (!read_exact(file, pixels, bytes))
      return LIBVIDEO_IO_ERROR;

    /* TVID stores RGB565 as two wire-order bytes. TOS transfers the tile
     * buffer as 16-bit values, so convert each pair to its host value. */
    const uint8_t *raw = reinterpret_cast<const uint8_t *>(pixels);
    const uint32_t count = static_cast<uint32_t>(LCD_WIDTH) * h;
    for (uint32_t i = 0U; i < count; ++i)
      pixels[i] = wire_to_native(raw[i * 2U], raw[i * 2U + 1U]);

    draw_dashboard(pixels, y, h);
    LCD_EndTileRender();
    if (pumped(pump, user))
      return LIBVIDEO_ABORTED;
  }
  return LIBVIDEO_OK;
}

LibVideoResult render_rle(FIL *file, const FrameHeader &frame,
                          LibVideoPumpFn pump, void *user) {
  uint32_t payload_read = 0U;
  uint32_t buffered_pos = 0U;
  uint32_t buffered_len = 0U;
  uint32_t pixels_done = 0U;
  uint16_t run_left = 0U;
  uint16_t run_color = 0U;

  for (uint16_t y = 0U; y < LCD_HEIGHT; y += TILE_HEIGHT) {
    const uint16_t h = static_cast<uint16_t>(
        (y + TILE_HEIGHT <= LCD_HEIGHT) ? TILE_HEIGHT : LCD_HEIGHT - y);
    LCD_BeginTileRender(y, h);
    uint16_t *pixels = LCD_GetFrameBuffer();
    const uint32_t tile_pixels = static_cast<uint32_t>(LCD_WIDTH) * h;
    uint32_t out = 0U;

    while (out < tile_pixels) {
      if (run_left == 0U) {
        if (buffered_pos == buffered_len) {
          const uint32_t remaining = frame.payload_size - payload_read;
          const UINT request = static_cast<UINT>(
              remaining < kRleReadBufferSize ? remaining : kRleReadBufferSize);
          if (request == 0U || !read_exact(file, rleReadBuffer, request))
            return LIBVIDEO_IO_ERROR;
          payload_read += request;
          buffered_pos = 0U;
          buffered_len = request;
        }
        if (buffered_len - buffered_pos < 4U)
          return LIBVIDEO_INVALID;
        const uint8_t *run = &rleReadBuffer[buffered_pos];
        buffered_pos += 4U;
        run_left = static_cast<uint16_t>(run[0] | (run[1] << 8U));
        run_color = wire_to_native(run[2], run[3]);
        if (run_left == 0U)
          return LIBVIDEO_INVALID;
      }

      const uint32_t room = tile_pixels - out;
      const uint16_t take =
          run_left < room ? run_left : static_cast<uint16_t>(room);
      for (uint16_t i = 0U; i < take; ++i)
        pixels[out + i] = run_color;
      out += take;
      run_left = static_cast<uint16_t>(run_left - take);
      pixels_done += take;
    }

    draw_dashboard(pixels, y, h);
    LCD_EndTileRender();
    if (pumped(pump, user))
      return LIBVIDEO_ABORTED;
  }

  if (pixels_done != frame.pixel_count || pixels_done != kFramePixels ||
      run_left != 0U || payload_read != frame.payload_size ||
      buffered_pos != buffered_len) {
    return LIBVIDEO_INVALID;
  }
  return LIBVIDEO_OK;
}

LibVideoResult wait_until(uint32_t deadline, LibVideoPumpFn pump, void *user) {
  while (static_cast<int32_t>(HAL_GetTick() - deadline) < 0) {
    if (pumped(pump, user))
      return LIBVIDEO_ABORTED;
    HAL_Delay(1U);
  }
  return LIBVIDEO_OK;
}
} // namespace

extern "C" LibVideoResult LibVideo_Play(const char *path, LibVideoPumpFn pump,
                                          void *user) {
  FIL file{};
  FRESULT fr = f_open(&file, path, FA_READ);
  if (fr == FR_NO_FILE || fr == FR_NO_PATH)
    return LIBVIDEO_NOT_FOUND;
  if (fr != FR_OK)
    return LIBVIDEO_IO_ERROR;

  FileHeader header{};
  if (!read_exact(&file, &header, sizeof(header))) {
    f_close(&file);
    return LIBVIDEO_IO_ERROR;
  }
  if (std::memcmp(header.magic, "TVID", 4U) != 0 ||
      header.version != kVersion || header.header_size != kHeaderSize ||
      header.width != LCD_WIDTH || header.height != LCD_HEIGHT ||
      header.fps == 0U || header.fps > 60U || header.frame_count == 0U ||
      (header.codec != kCodecRaw && header.codec != kCodecRle)) {
    f_close(&file);
    return LIBVIDEO_INVALID;
  }

  const uint64_t file_size = static_cast<uint64_t>(f_size(&file));
  const uint64_t minimum_size =
      static_cast<uint64_t>(header.header_size) +
      static_cast<uint64_t>(header.frame_count) * sizeof(FrameHeader);
  if (file_size < minimum_size) {
    f_close(&file);
    return LIBVIDEO_INVALID;
  }

  const FSIZE_t first_frame = header.header_size;
  measuredFps = 0U;
  sample_dashboard();
  do {
    if (f_lseek(&file, first_frame) != FR_OK) {
      f_close(&file);
      return LIBVIDEO_IO_ERROR;
    }
    const uint32_t loop_start = HAL_GetTick();
    uint32_t fps_window_start = 0U;
    uint32_t fps_completed = 0U;
    bool fps_window_started = false;
    for (uint32_t index = 0U; index < header.frame_count; ++index) {
      FrameHeader frame{};
      if (!read_exact(&file, &frame, sizeof(frame))) {
        f_close(&file);
        return LIBVIDEO_IO_ERROR;
      }
      if (std::memcmp(frame.magic, "FRM1", 4U) != 0 ||
          frame.pixel_count != kFramePixels ||
          (frame.codec != kCodecRaw && frame.codec != kCodecRle)) {
        f_close(&file);
        return LIBVIDEO_INVALID;
      }

      const bool payload_valid =
          frame.codec == kCodecRaw
              ? frame.payload_size == kRawPayloadSize
              : frame.payload_size >= 4U &&
                    frame.payload_size <= kMaxRlePayloadSize &&
                    (frame.payload_size & 3U) == 0U;
      const uint64_t payload_offset = static_cast<uint64_t>(f_tell(&file));
      if (!payload_valid || payload_offset > file_size ||
          static_cast<uint64_t>(frame.payload_size) >
              file_size - payload_offset) {
        f_close(&file);
        return LIBVIDEO_INVALID;
      }

      LibVideoResult result =
          wait_until(loop_start + frame.timestamp_ms, pump, user);
      if (result != LIBVIDEO_OK) {
        f_close(&file);
        return result;
      }
      if (pumped(pump, user)) {
        f_close(&file);
        return LIBVIDEO_ABORTED;
      }
      result = frame.codec == kCodecRaw
                                  ? render_raw(&file, frame, pump, user)
                                  : render_rle(&file, frame, pump, user);
      if (result != LIBVIDEO_OK) {
        f_close(&file);
        return result;
      }

      /* Count only completed frame presentations. Measuring before render
       * reports a frame that may still take tens of milliseconds to transfer
       * and makes a struggling decoder look faster than the panel. */
      const uint32_t presented_at = HAL_GetTick();
      if (!fps_window_started) {
        fps_window_start = presented_at;
        fps_window_started = true;
      } else {
        ++fps_completed;
        const uint32_t elapsed = presented_at - fps_window_start;
        if (elapsed >= 1000U) {
          measuredFps = static_cast<uint16_t>(
              (fps_completed * 1000U + elapsed / 2U) / elapsed);
          sample_dashboard();
          fps_completed = 0U;
          fps_window_start = presented_at;
        }
      }
    }
  } while ((header.flags & kFlagLoop) != 0U);

  f_close(&file);
  return LIBVIDEO_OK;
}

extern "C" const char *LibVideo_ResultText(LibVideoResult result) {
  switch (result) {
  case LIBVIDEO_OK: return "PLAYBACK COMPLETE";
  case LIBVIDEO_ABORTED: return "ABORTED";
  case LIBVIDEO_NOT_FOUND: return "TEST.TVID NOT FOUND";
  case LIBVIDEO_INVALID: return "INVALID TEST.TVID";
  case LIBVIDEO_IO_ERROR: return "VIDEO IO ERROR";
  case LIBVIDEO_NO_MEMORY: return "OUT OF MEMORY";
  default: return "VIDEO ERROR";
  }
}
