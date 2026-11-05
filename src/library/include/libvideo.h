#ifndef TOS_LIBVIDEO_H
#define TOS_LIBVIDEO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*LibVideoPumpFn)(void *user);

typedef enum {
  LIBVIDEO_OK = 0,
  LIBVIDEO_ABORTED,
  LIBVIDEO_NOT_FOUND,
  LIBVIDEO_INVALID,
  LIBVIDEO_IO_ERROR,
  LIBVIDEO_NO_MEMORY
} LibVideoResult;

/* Plays a TVID stream. The pump callback is serviced during I/O, rendering,
 * and frame pacing; returning non-zero aborts playback immediately. */
LibVideoResult LibVideo_Play(const char *path, LibVideoPumpFn pump, void *user);
const char *LibVideo_ResultText(LibVideoResult result);

#ifdef __cplusplus
}
#endif

#endif
