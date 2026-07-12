/**
 ******************************************************************************
 * @file    hid_manager.c
 * @author  Typheye
 * @brief   Cloud-facing HID command manager implementation.
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

#include "include/hid_manager.h"

#define HIDM_TEXT_MAX          96U
#define HIDM_VENDOR_TEXT_MAX   63U
#define HIDM_DEFAULT_HOLD_MS   25U
#define HIDM_DEFAULT_GAP_MS    20U
#define HIDM_MAX_COMMAND_MS    20000U
#define HIDM_SHORT_COMMAND_MS  3000U

#define HIDM_MOD_LCTRL   0x01U
#define HIDM_MOD_LSHIFT  0x02U
#define HIDM_MOD_LALT    0x04U
#define HIDM_MOD_LGUI    0x08U

#define HIDM_MOUSE_LEFT    0x01U
#define HIDM_MOUSE_RIGHT   0x02U
#define HIDM_MOUSE_MIDDLE  0x04U

#define HIDM_KEY_A       0x04U
#define HIDM_KEY_1       0x1EU
#define HIDM_KEY_0       0x27U
#define HIDM_KEY_ENTER   0x28U
#define HIDM_KEY_ESC     0x29U
#define HIDM_KEY_BACKSP  0x2AU
#define HIDM_KEY_TAB     0x2BU
#define HIDM_KEY_SPACE   0x2CU
#define HIDM_KEY_MINUS   0x2DU
#define HIDM_KEY_EQUAL   0x2EU
#define HIDM_KEY_LBRACE  0x2FU
#define HIDM_KEY_RBRACE  0x30U
#define HIDM_KEY_BSLASH  0x31U
#define HIDM_KEY_SEMI    0x33U
#define HIDM_KEY_QUOTE   0x34U
#define HIDM_KEY_GRAVE   0x35U
#define HIDM_KEY_COMMA   0x36U
#define HIDM_KEY_DOT     0x37U
#define HIDM_KEY_SLASH   0x38U
#define HIDM_KEY_CAPS    0x39U
#define HIDM_KEY_F1      0x3AU
#define HIDM_KEY_DELETE  0x4CU
#define HIDM_KEY_RIGHT   0x4FU
#define HIDM_KEY_LEFT    0x50U
#define HIDM_KEY_DOWN    0x51U
#define HIDM_KEY_UP      0x52U

typedef enum {
  HIDM_CMD_NONE = 0,
  HIDM_CMD_KEY,
  HIDM_CMD_TEXT,
  HIDM_CMD_MOUSE,
  HIDM_CMD_VENDOR,
  HIDM_CMD_RELEASE
} HidmCommandType;

typedef enum {
  HIDM_PHASE_IDLE = 0,
  HIDM_PHASE_KEY_DOWN,
  HIDM_PHASE_KEY_HOLD,
  HIDM_PHASE_KEY_UP,
  HIDM_PHASE_KEY_GAP,
  HIDM_PHASE_MOUSE_SEND,
  HIDM_PHASE_MOUSE_HOLD,
  HIDM_PHASE_MOUSE_RELEASE,
  HIDM_PHASE_VENDOR_SEND,
  HIDM_PHASE_RELEASE_KEY,
  HIDM_PHASE_RELEASE_MOUSE
} HidmPhase;

typedef struct {
  bool active;
  HidmCommandType type;
  HidmPhase phase;
  uint32_t next_ms;
  uint32_t deadline_ms;
  uint16_t hold_ms;
  uint16_t gap_ms;
  uint8_t modifier;
  uint8_t key;
  uint8_t buttons;
  int8_t x;
  int8_t y;
  int8_t wheel;
  char text[HIDM_TEXT_MAX + 1U];
  uint16_t text_pos;
} HidmCommand;

static HidmCommand g_hidm_cmd;
static const char *g_hidm_status = "idle";
static char g_hidm_last_error[40] = "ok";
static uint32_t g_hidm_report_seq = 0U;
static uint8_t g_hidm_report_dirty = 1U;
static uint8_t g_hidm_service_guard = 0U;

static bool hidm_time_due(uint32_t now, uint32_t target) {
  return (int32_t)(now - target) >= 0;
}

static int hidm_clamp_int(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static char hidm_lower_char(char c) {
  if (c >= 'A' && c <= 'Z') return (char)(c + ('a' - 'A'));
  return c;
}

static bool hidm_streq_ci(const char *a, const char *b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (hidm_lower_char(*a) != hidm_lower_char(*b)) return false;
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

static void hidm_lower_copy(char *dst, uint16_t dst_sz, const char *src) {
  uint16_t i = 0;
  if (!dst || dst_sz == 0U) return;
  if (!src) src = "";
  while (src[i] && i + 1U < dst_sz) {
    dst[i] = hidm_lower_char(src[i]);
    i++;
  }
  dst[i] = '\0';
}

static void hidm_trim_in_place(char *s) {
  char *end;
  if (!s) return;
  while (*s == ' ' || *s == '\t') s++;
  end = s + strlen(s);
  while (end > s && (end[-1] == ' ' || end[-1] == '\t')) {
    end--;
  }
  *end = '\0';
}

static bool hidm_contains_token(const char *s, const char *token) {
  char tmp[96];
  hidm_lower_copy(tmp, sizeof(tmp), s);
  return strstr(tmp, token) != NULL;
}

static bool hidm_action_is(const char *action, const char *a,
                           const char *b, const char *c) {
  return (a && hidm_streq_ci(action, a)) ||
         (b && hidm_streq_ci(action, b)) ||
         (c && hidm_streq_ci(action, c));
}

static bool hidm_json_str(const char *obj, const char *key,
                          char *out, int out_sz) {
  if (json_get_str(obj, key, out, out_sz)) return true;

  const char *params = json_find(obj, "params");
  if (params && *params == '{') {
    return json_get_str(params, key, out, out_sz);
  }
  return false;
}

static int hidm_json_int(const char *obj, const char *key, int defv) {
  int sentinel = 0x7FFFFFFF;
  int v = json_get_int(obj, key, sentinel);
  if (v != sentinel) return v;

  const char *params = json_find(obj, "params");
  if (params && *params == '{') {
    return json_get_int(params, key, defv);
  }
  return defv;
}

static bool hidm_usb_idle(void) {
  if (!HidManager_IsConfigured() || hUsbDeviceFS.pClassData == NULL) {
    return false;
  }

  USBD_CUSTOM_HID_HandleTypeDef *hhid =
      (USBD_CUSTOM_HID_HandleTypeDef *)hUsbDeviceFS.pClassData;
  return hhid->state == CUSTOM_HID_IDLE;
}

static bool hidm_ascii_to_key(char c, uint8_t *modifier, uint8_t *key) {
  uint8_t mod = 0U;
  uint8_t k = 0U;

  if (c >= 'a' && c <= 'z') {
    k = (uint8_t)(HIDM_KEY_A + (uint8_t)(c - 'a'));
  } else if (c >= 'A' && c <= 'Z') {
    mod = HIDM_MOD_LSHIFT;
    k = (uint8_t)(HIDM_KEY_A + (uint8_t)(c - 'A'));
  } else if (c >= '1' && c <= '9') {
    k = (uint8_t)(HIDM_KEY_1 + (uint8_t)(c - '1'));
  } else {
    switch (c) {
    case '0': k = HIDM_KEY_0; break;
    case '\n': k = HIDM_KEY_ENTER; break;
    case '\r': k = HIDM_KEY_ENTER; break;
    case '\t': k = HIDM_KEY_TAB; break;
    case ' ': k = HIDM_KEY_SPACE; break;
    case '-': k = HIDM_KEY_MINUS; break;
    case '_': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_MINUS; break;
    case '=': k = HIDM_KEY_EQUAL; break;
    case '+': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_EQUAL; break;
    case '[': k = HIDM_KEY_LBRACE; break;
    case '{': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_LBRACE; break;
    case ']': k = HIDM_KEY_RBRACE; break;
    case '}': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_RBRACE; break;
    case '\\': k = HIDM_KEY_BSLASH; break;
    case '|': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_BSLASH; break;
    case ';': k = HIDM_KEY_SEMI; break;
    case ':': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_SEMI; break;
    case '\'': k = HIDM_KEY_QUOTE; break;
    case '"': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_QUOTE; break;
    case '`': k = HIDM_KEY_GRAVE; break;
    case '~': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_GRAVE; break;
    case ',': k = HIDM_KEY_COMMA; break;
    case '<': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_COMMA; break;
    case '.': k = HIDM_KEY_DOT; break;
    case '>': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_DOT; break;
    case '/': k = HIDM_KEY_SLASH; break;
    case '?': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_SLASH; break;
    case '!': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_1; break;
    case '@': mod = HIDM_MOD_LSHIFT; k = (uint8_t)(HIDM_KEY_1 + 1U); break;
    case '#': mod = HIDM_MOD_LSHIFT; k = (uint8_t)(HIDM_KEY_1 + 2U); break;
    case '$': mod = HIDM_MOD_LSHIFT; k = (uint8_t)(HIDM_KEY_1 + 3U); break;
    case '%': mod = HIDM_MOD_LSHIFT; k = (uint8_t)(HIDM_KEY_1 + 4U); break;
    case '^': mod = HIDM_MOD_LSHIFT; k = (uint8_t)(HIDM_KEY_1 + 5U); break;
    case '&': mod = HIDM_MOD_LSHIFT; k = (uint8_t)(HIDM_KEY_1 + 6U); break;
    case '*': mod = HIDM_MOD_LSHIFT; k = (uint8_t)(HIDM_KEY_1 + 7U); break;
    case '(': mod = HIDM_MOD_LSHIFT; k = (uint8_t)(HIDM_KEY_1 + 8U); break;
    case ')': mod = HIDM_MOD_LSHIFT; k = HIDM_KEY_0; break;
    default: return false;
    }
  }

  if (modifier) *modifier = mod;
  if (key) *key = k;
  return k != 0U;
}

static bool hidm_parse_modifier(const char *s, uint8_t *mod_out) {
  uint8_t mod = 0U;
  if (s && s[0]) {
    if (hidm_contains_token(s, "ctrl") || hidm_contains_token(s, "control")) {
      mod |= HIDM_MOD_LCTRL;
    }
    if (hidm_contains_token(s, "shift")) mod |= HIDM_MOD_LSHIFT;
    if (hidm_contains_token(s, "alt")) mod |= HIDM_MOD_LALT;
    if (hidm_contains_token(s, "gui") || hidm_contains_token(s, "win") ||
        hidm_contains_token(s, "meta") || hidm_contains_token(s, "cmd")) {
      mod |= HIDM_MOD_LGUI;
    }
  }
  if (mod_out) *mod_out = mod;
  return true;
}

static bool hidm_parse_key_name(const char *name, uint8_t *modifier,
                                uint8_t *key) {
  char k[32];
  uint8_t mod = 0U;
  uint8_t usage = 0U;

  if (!name || !name[0]) return false;
  hidm_lower_copy(k, sizeof(k), name);

  if (k[1] == '\0') {
    if (!hidm_ascii_to_key(k[0], &mod, &usage)) return false;
  } else if (hidm_streq_ci(k, "enter") || hidm_streq_ci(k, "return")) {
    usage = HIDM_KEY_ENTER;
  } else if (hidm_streq_ci(k, "esc") || hidm_streq_ci(k, "escape")) {
    usage = HIDM_KEY_ESC;
  } else if (hidm_streq_ci(k, "backspace")) {
    usage = HIDM_KEY_BACKSP;
  } else if (hidm_streq_ci(k, "tab")) {
    usage = HIDM_KEY_TAB;
  } else if (hidm_streq_ci(k, "space")) {
    usage = HIDM_KEY_SPACE;
  } else if (hidm_streq_ci(k, "delete") || hidm_streq_ci(k, "del")) {
    usage = HIDM_KEY_DELETE;
  } else if (hidm_streq_ci(k, "left")) {
    usage = HIDM_KEY_LEFT;
  } else if (hidm_streq_ci(k, "right")) {
    usage = HIDM_KEY_RIGHT;
  } else if (hidm_streq_ci(k, "up")) {
    usage = HIDM_KEY_UP;
  } else if (hidm_streq_ci(k, "down")) {
    usage = HIDM_KEY_DOWN;
  } else if (hidm_streq_ci(k, "capslock") || hidm_streq_ci(k, "caps")) {
    usage = HIDM_KEY_CAPS;
  } else if (k[0] == 'f' && k[1] >= '1' && k[1] <= '9' && k[2] == '\0') {
    usage = (uint8_t)(HIDM_KEY_F1 + (uint8_t)(k[1] - '1'));
  } else if (k[0] == 'f' && k[1] == '1' && k[2] >= '0' && k[2] <= '2' &&
             k[3] == '\0') {
    usage = (uint8_t)(HIDM_KEY_F1 + 9U + (uint8_t)(k[2] - '0'));
  } else {
    return false;
  }

  if (modifier) *modifier = mod;
  if (key) *key = usage;
  return usage != 0U;
}

static bool hidm_parse_combo(const char *combo, uint8_t *modifier,
                             uint8_t *key) {
  char tmp[64];
  char *last;
  uint8_t mod = 0U;
  uint8_t key_mod = 0U;
  uint8_t usage = 0U;

  if (!combo || !combo[0]) return false;
  hidm_lower_copy(tmp, sizeof(tmp), combo);
  (void)hidm_parse_modifier(tmp, &mod);

  last = strrchr(tmp, '+');
  if (last) {
    last++;
  } else {
    last = tmp;
  }

  while (*last == ' ' || *last == '\t') last++;
  hidm_trim_in_place(last);
  if (!hidm_parse_key_name(last, &key_mod, &usage)) return false;
  mod |= key_mod;

  if (modifier) *modifier = mod;
  if (key) *key = usage;
  return true;
}

static uint8_t hidm_parse_buttons(const char *s, int fallback) {
  uint8_t buttons = 0U;
  if (s && s[0]) {
    if (hidm_contains_token(s, "left") || hidm_contains_token(s, "primary")) {
      buttons |= HIDM_MOUSE_LEFT;
    }
    if (hidm_contains_token(s, "right") || hidm_contains_token(s, "secondary")) {
      buttons |= HIDM_MOUSE_RIGHT;
    }
    if (hidm_contains_token(s, "middle") || hidm_contains_token(s, "wheel")) {
      buttons |= HIDM_MOUSE_MIDDLE;
    }
  }
  if (buttons == 0U && fallback > 0) {
    buttons = (uint8_t)(fallback & 0x07);
  }
  return buttons;
}

static void hidm_mark_report_dirty(const char *status) {
  if (!status || !status[0]) status = "idle";
  g_hidm_status = status;
  if (g_hidm_report_seq < 0xFFFFFFFFUL) g_hidm_report_seq++;
  else g_hidm_report_seq = 1U;
  g_hidm_report_dirty = 1U;
}

static void hidm_update_usb_idle_status(void) {
  if (g_hidm_cmd.active) return;
  if (!HidManager_IsConfigured()) {
    if (strcmp(g_hidm_status, "no_usb") != 0) {
      hidm_mark_report_dirty("no_usb");
    }
  } else if (strcmp(g_hidm_status, "no_usb") == 0) {
    snprintf(g_hidm_last_error, sizeof(g_hidm_last_error), "ok");
    hidm_mark_report_dirty("idle");
  }
}

static void hidm_set_error(const char *err) {
  if (!err || !err[0]) err = "error";
  snprintf(g_hidm_last_error, sizeof(g_hidm_last_error), "%s", err);
  hidm_mark_report_dirty("error");
}

static void hidm_complete(void) {
  LOG_I("HIDM", "Command done");
  memset(&g_hidm_cmd, 0, sizeof(g_hidm_cmd));
  snprintf(g_hidm_last_error, sizeof(g_hidm_last_error), "ok");
  hidm_mark_report_dirty(HidManager_IsConfigured() ? "idle" : "no_usb");
}

static void hidm_fail(const char *err) {
  LOG_W("HIDM", "Command failed: %s", err ? err : "error");
  memset(&g_hidm_cmd, 0, sizeof(g_hidm_cmd));
  hidm_set_error(err);
}

static bool hidm_send_ok(int8_t ret) {
  if (ret == (int8_t)USBD_OK) return true;
  if (ret == (int8_t)USBD_BUSY) return false;
  hidm_fail("usb send error");
  return false;
}

static bool hidm_send_keyboard(uint8_t modifier, uint8_t key) {
  if (!HidManager_IsConfigured()) {
    hidm_fail("usb not configured");
    return false;
  }
  if (!hidm_usb_idle()) return false;
  return hidm_send_ok(TOS_HID_SendKeyboard(modifier, key, 0U, 0U, 0U, 0U, 0U));
}

static bool hidm_send_keyboard_release(void) {
  if (!HidManager_IsConfigured()) {
    hidm_fail("usb not configured");
    return false;
  }
  if (!hidm_usb_idle()) return false;
  return hidm_send_ok(TOS_HID_ReleaseKeyboard());
}

static bool hidm_send_mouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
  if (!HidManager_IsConfigured()) {
    hidm_fail("usb not configured");
    return false;
  }
  if (!hidm_usb_idle()) return false;
  return hidm_send_ok(TOS_HID_SendMouse(buttons, x, y, wheel));
}

static bool hidm_send_vendor_text(const char *text) {
  uint16_t len = 0U;
  if (!HidManager_IsConfigured()) {
    hidm_fail("usb not configured");
    return false;
  }
  if (!hidm_usb_idle()) return false;

  while (text && text[len] && len < HIDM_VENDOR_TEXT_MAX) len++;
  return hidm_send_ok(TOS_HID_SendVendor((const uint8_t *)text, len));
}

static bool hidm_load_next_text_key(void) {
  while (g_hidm_cmd.text[g_hidm_cmd.text_pos]) {
    char c = g_hidm_cmd.text[g_hidm_cmd.text_pos++];
    uint8_t mod = 0U;
    uint8_t key = 0U;
    if (hidm_ascii_to_key(c, &mod, &key)) {
      g_hidm_cmd.modifier = mod;
      g_hidm_cmd.key = key;
      return true;
    }
    LOG_W("HIDM", "Skip unsupported char 0x%02X", (unsigned)(uint8_t)c);
  }
  return false;
}

static void hidm_queue_start(HidmCommandType type, HidmPhase phase,
                             uint32_t budget_ms) {
  uint32_t now = HAL_GetTick();
  g_hidm_cmd.active = true;
  g_hidm_cmd.type = type;
  g_hidm_cmd.phase = phase;
  g_hidm_cmd.next_ms = now;
  if (budget_ms > HIDM_MAX_COMMAND_MS) budget_ms = HIDM_MAX_COMMAND_MS;
  g_hidm_cmd.deadline_ms = now + budget_ms;
  snprintf(g_hidm_last_error, sizeof(g_hidm_last_error), "ok");
  hidm_mark_report_dirty("busy");
}

static bool hidm_queue_key(uint8_t modifier, uint8_t key, uint16_t hold_ms) {
  memset(&g_hidm_cmd, 0, sizeof(g_hidm_cmd));
  g_hidm_cmd.modifier = modifier;
  g_hidm_cmd.key = key;
  g_hidm_cmd.hold_ms = hold_ms;
  g_hidm_cmd.gap_ms = HIDM_DEFAULT_GAP_MS;
  hidm_queue_start(HIDM_CMD_KEY, HIDM_PHASE_KEY_DOWN, HIDM_SHORT_COMMAND_MS);
  LOG_I("HIDM", "Queue key mod=0x%02X key=0x%02X",
        (unsigned)modifier, (unsigned)key);
  return true;
}

static bool hidm_queue_text(const char *text, uint16_t hold_ms,
                            uint16_t gap_ms) {
  uint32_t budget;
  size_t len;

  memset(&g_hidm_cmd, 0, sizeof(g_hidm_cmd));
  snprintf(g_hidm_cmd.text, sizeof(g_hidm_cmd.text), "%s", text ? text : "");
  len = strlen(g_hidm_cmd.text);
  if (len == 0U) return false;

  g_hidm_cmd.hold_ms = hold_ms;
  g_hidm_cmd.gap_ms = gap_ms;
  g_hidm_cmd.text_pos = 0U;
  if (!hidm_load_next_text_key()) return false;

  budget = 3000U + (uint32_t)len * ((uint32_t)hold_ms + (uint32_t)gap_ms + 20U);
  hidm_queue_start(HIDM_CMD_TEXT, HIDM_PHASE_KEY_DOWN, budget);
  LOG_I("HIDM", "Queue text len=%u", (unsigned)len);
  return true;
}

static bool hidm_queue_mouse(uint8_t buttons, int8_t x, int8_t y,
                             int8_t wheel, uint16_t hold_ms) {
  memset(&g_hidm_cmd, 0, sizeof(g_hidm_cmd));
  g_hidm_cmd.buttons = buttons;
  g_hidm_cmd.x = x;
  g_hidm_cmd.y = y;
  g_hidm_cmd.wheel = wheel;
  g_hidm_cmd.hold_ms = hold_ms;
  hidm_queue_start(HIDM_CMD_MOUSE, HIDM_PHASE_MOUSE_SEND, HIDM_SHORT_COMMAND_MS);
  LOG_I("HIDM", "Queue mouse buttons=0x%02X x=%d y=%d wheel=%d",
        (unsigned)buttons, x, y, wheel);
  return true;
}

static bool hidm_queue_vendor(const char *text) {
  memset(&g_hidm_cmd, 0, sizeof(g_hidm_cmd));
  snprintf(g_hidm_cmd.text, sizeof(g_hidm_cmd.text), "%s", text ? text : "");
  hidm_queue_start(HIDM_CMD_VENDOR, HIDM_PHASE_VENDOR_SEND, HIDM_SHORT_COMMAND_MS);
  LOG_I("HIDM", "Queue vendor len=%u", (unsigned)strlen(g_hidm_cmd.text));
  return true;
}

static bool hidm_queue_release(void) {
  memset(&g_hidm_cmd, 0, sizeof(g_hidm_cmd));
  hidm_queue_start(HIDM_CMD_RELEASE, HIDM_PHASE_RELEASE_KEY, HIDM_SHORT_COMMAND_MS);
  LOG_I("HIDM", "Queue release");
  return true;
}

void HidManager_Init(void) {
  memset(&g_hidm_cmd, 0, sizeof(g_hidm_cmd));
  snprintf(g_hidm_last_error, sizeof(g_hidm_last_error), "ok");
  g_hidm_report_seq = 0U;
  g_hidm_report_dirty = 0U;
  hidm_mark_report_dirty(HidManager_IsConfigured() ? "idle" : "no_usb");
}

bool HidManager_IsConfigured(void) {
  return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED;
}

bool HidManager_IsBusy(void) {
  return g_hidm_cmd.active;
}

const char *HidManager_GetReportStatus(void) {
  hidm_update_usb_idle_status();
  if (g_hidm_cmd.active) return "busy";
  if (!HidManager_IsConfigured()) return "no_usb";
  return g_hidm_status ? g_hidm_status : "idle";
}

const char *HidManager_GetLastError(void) {
  return g_hidm_last_error;
}

uint32_t HidManager_GetReportSeq(void) {
  return g_hidm_report_seq;
}

bool HidManager_IsReportDirty(void) {
  hidm_update_usb_idle_status();
  return g_hidm_report_dirty != 0U;
}

void HidManager_ClearReportDirty(void) {
  g_hidm_report_dirty = 0U;
}

bool HidManager_QueueCloudCommand(const char *action, const char *json_obj,
                                  const char **err_out) {
  char text[HIDM_TEXT_MAX + 1U];
  char combo[64];
  char key_name[32];
  char mod_name[48];
  char button_name[32];
  uint8_t mod = 0U;
  uint8_t key = 0U;

  if (err_out) *err_out = "";
  if (!action || !json_obj) {
    if (err_out) *err_out = "bad hid command";
    return false;
  }

  if (HidManager_IsBusy()) {
    if (err_out) *err_out = "hid busy";
    return false;
  }

  if (!HidManager_IsConfigured()) {
    hidm_set_error("usb not configured");
    if (err_out) *err_out = "usb not configured";
    return false;
  }

  if (hidm_action_is(action, "hid_type", "type_text", "keyboard_type")) {
    int hold = hidm_clamp_int(hidm_json_int(json_obj, "hold", HIDM_DEFAULT_HOLD_MS), 5, 80);
    int gap = hidm_clamp_int(hidm_json_int(json_obj, "delay", HIDM_DEFAULT_GAP_MS), 5, 250);
    text[0] = '\0';
    if (!hidm_json_str(json_obj, "text", text, sizeof(text))) {
      if (err_out) *err_out = "missing text";
      return false;
    }
    if (!hidm_queue_text(text, (uint16_t)hold, (uint16_t)gap)) {
      if (err_out) *err_out = "bad text";
      return false;
    }
    return true;
  }

  if (hidm_action_is(action, "hid_hotkey", "hotkey", "shortcut")) {
    int hold = hidm_clamp_int(hidm_json_int(json_obj, "hold", HIDM_DEFAULT_HOLD_MS), 5, 120);
    combo[0] = '\0';
    if (!hidm_json_str(json_obj, "combo", combo, sizeof(combo)) ||
        !hidm_parse_combo(combo, &mod, &key)) {
      if (err_out) *err_out = "bad combo";
      return false;
    }
    return hidm_queue_key(mod, key, (uint16_t)hold);
  }

  if (hidm_action_is(action, "hid_key", "keyboard_key", "tap_key")) {
    int hold = hidm_clamp_int(hidm_json_int(json_obj, "hold", HIDM_DEFAULT_HOLD_MS), 5, 120);
    int code = hidm_json_int(json_obj, "key_code", -1);
    mod_name[0] = '\0';
    if (!hidm_json_str(json_obj, "modifiers", mod_name, sizeof(mod_name))) {
      (void)hidm_json_str(json_obj, "modifier", mod_name, sizeof(mod_name));
    }
    (void)hidm_parse_modifier(mod_name, &mod);

    if (code >= 1 && code <= 255) {
      key = (uint8_t)code;
    } else {
      key_name[0] = '\0';
      if (!hidm_json_str(json_obj, "key", key_name, sizeof(key_name)) ||
          !hidm_parse_key_name(key_name, NULL, &key)) {
        if (err_out) *err_out = "bad key";
        return false;
      }
    }
    return hidm_queue_key(mod, key, (uint16_t)hold);
  }

  if (hidm_action_is(action, "hid_mouse", "mouse_move", "mouse")) {
    int x = hidm_clamp_int(hidm_json_int(json_obj, "x", 0), -127, 127);
    int y = hidm_clamp_int(hidm_json_int(json_obj, "y", 0), -127, 127);
    int wheel = hidm_clamp_int(hidm_json_int(json_obj, "wheel", 0), -127, 127);
    int hold = hidm_clamp_int(hidm_json_int(json_obj, "hold", 0), 0, 200);
    int fallback_buttons = hidm_json_int(json_obj, "buttons", 0);
    button_name[0] = '\0';
    (void)hidm_json_str(json_obj, "button", button_name, sizeof(button_name));
    return hidm_queue_mouse(hidm_parse_buttons(button_name, fallback_buttons),
                            (int8_t)x, (int8_t)y, (int8_t)wheel,
                            (uint16_t)hold);
  }

  if (hidm_action_is(action, "hid_click", "mouse_click", "click")) {
    int hold = hidm_clamp_int(hidm_json_int(json_obj, "hold", 35), 5, 200);
    int fallback_buttons = hidm_json_int(json_obj, "buttons", HIDM_MOUSE_LEFT);
    button_name[0] = '\0';
    (void)hidm_json_str(json_obj, "button", button_name, sizeof(button_name));
    return hidm_queue_mouse(hidm_parse_buttons(button_name, fallback_buttons),
                            0, 0, 0, (uint16_t)hold);
  }

  if (hidm_action_is(action, "hid_scroll", "mouse_scroll", "scroll")) {
    int wheel = hidm_clamp_int(hidm_json_int(json_obj, "wheel", 0), -127, 127);
    if (wheel == 0) wheel = hidm_clamp_int(hidm_json_int(json_obj, "dy", 0), -127, 127);
    if (wheel == 0) {
      if (err_out) *err_out = "missing wheel";
      return false;
    }
    return hidm_queue_mouse(0U, 0, 0, (int8_t)wheel, 0U);
  }

  if (hidm_action_is(action, "hid_vendor", "vendor_text", "vendor")) {
    text[0] = '\0';
    if (!hidm_json_str(json_obj, "text", text, HIDM_VENDOR_TEXT_MAX + 1)) {
      if (err_out) *err_out = "missing text";
      return false;
    }
    return hidm_queue_vendor(text);
  }

  if (hidm_action_is(action, "hid_release", "release_hid", "release")) {
    return hidm_queue_release();
  }

  if (err_out) *err_out = "unknown hid action";
  return false;
}

void HidManager_ServiceTick(void) {
  uint32_t now;

  if (g_hidm_service_guard) return;
  g_hidm_service_guard = 1U;
  hidm_update_usb_idle_status();
  if (!g_hidm_cmd.active) {
    g_hidm_service_guard = 0U;
    return;
  }

  now = HAL_GetTick();
  if (!hidm_time_due(now, g_hidm_cmd.next_ms)) {
    g_hidm_service_guard = 0U;
    return;
  }
  if (hidm_time_due(now, g_hidm_cmd.deadline_ms)) {
    hidm_fail("usb timeout");
    g_hidm_service_guard = 0U;
    return;
  }

  switch (g_hidm_cmd.phase) {
  case HIDM_PHASE_KEY_DOWN:
    if (hidm_send_keyboard(g_hidm_cmd.modifier, g_hidm_cmd.key)) {
      g_hidm_cmd.phase = HIDM_PHASE_KEY_HOLD;
      g_hidm_cmd.next_ms = now + g_hidm_cmd.hold_ms;
    }
    break;

  case HIDM_PHASE_KEY_HOLD:
    if (hidm_send_keyboard_release()) {
      g_hidm_cmd.phase = HIDM_PHASE_KEY_GAP;
      g_hidm_cmd.next_ms = now + g_hidm_cmd.gap_ms;
    }
    break;

  case HIDM_PHASE_KEY_GAP:
    if (g_hidm_cmd.type == HIDM_CMD_TEXT) {
      if (hidm_load_next_text_key()) {
        g_hidm_cmd.phase = HIDM_PHASE_KEY_DOWN;
      } else {
        hidm_complete();
      }
    } else {
      hidm_complete();
    }
    break;

  case HIDM_PHASE_MOUSE_SEND:
    if (hidm_send_mouse(g_hidm_cmd.buttons, g_hidm_cmd.x,
                        g_hidm_cmd.y, g_hidm_cmd.wheel)) {
      if (g_hidm_cmd.buttons != 0U && g_hidm_cmd.hold_ms > 0U) {
        g_hidm_cmd.phase = HIDM_PHASE_MOUSE_HOLD;
        g_hidm_cmd.next_ms = now + g_hidm_cmd.hold_ms;
      } else {
        hidm_complete();
      }
    }
    break;

  case HIDM_PHASE_MOUSE_HOLD:
    if (hidm_send_mouse(0U, 0, 0, 0)) {
      hidm_complete();
    }
    break;

  case HIDM_PHASE_VENDOR_SEND:
    if (hidm_send_vendor_text(g_hidm_cmd.text)) {
      hidm_complete();
    }
    break;

  case HIDM_PHASE_RELEASE_KEY:
    if (hidm_send_keyboard_release()) {
      g_hidm_cmd.phase = HIDM_PHASE_RELEASE_MOUSE;
    }
    break;

  case HIDM_PHASE_RELEASE_MOUSE:
    if (hidm_send_mouse(0U, 0, 0, 0)) {
      hidm_complete();
    }
    break;

  case HIDM_PHASE_IDLE:
  case HIDM_PHASE_MOUSE_RELEASE:
  default:
    hidm_complete();
    break;
  }

  g_hidm_service_guard = 0U;
}

void HidManager_Tick(void) {
  SysWatchdog_Tick();
  HidManager_ServiceTick();
}
