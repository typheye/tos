#include "include/tvid_activity.hpp"
#include "core/manager/include/file_manager.h"
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/syswatchdog.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "gui/components/include/alert.hpp"
#include "library/include/libdly.h"
#include "library/include/libpd.h"
#include "library/include/libvideo.h"

extern KeyManager keyManager;

struct TvidInputState {
  uint8_t last_pressed;
};

static int tvid_pump(void *opaque) {
  TvidInputState *state = static_cast<TvidInputState *>(opaque);
  keyManager.tick();
  const uint8_t pressed =
      keyManager._btnEnter.getState() == KEY_PRESSED ? 1U : 0U;
  if (pressed && !state->last_pressed) {
    return 1;
  }
  state->last_pressed = pressed;
  TosApi_Tick();
  SysWatchdog_Tick();
  return 0;
}

static void tvid_release_enter(void) {
  while (keyManager._btnEnter.isPressed()) {
    TosApi_Tick();
    SysWatchdog_Tick();
    JPDelay(1U);
  }
  keyManager._btnEnter.tick();
  (void)keyManager._btnEnter.getState();
}

void tvid_test_activity(void) {
  TvidInputState input{0U};
  tvid_release_enter();
  if (FMCore_MountStorage(NULL, false) != FR_OK ||
      !FMCore_IsStorageMounted()) {
    alert_show("DEMO", "SD CARD OFFLINE");
    return;
  }

  LCD_SetDebugOverlaySuppressed(1U);
  LibVideoResult result =
      LibVideo_Play("0:/storage/test.tvid", tvid_pump, &input);
  LCD_SetDebugOverlaySuppressed(0U);
  if (result == LIBVIDEO_OK || result == LIBVIDEO_ABORTED)
    return;

  alert_show("DEMO", LibVideo_ResultText(result));
}
