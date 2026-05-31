#include "include/thid.hpp"
#include <cstring>

extern "C" USBD_HandleTypeDef hUsbDeviceFS;

THID boardHID;

THID::THID() {}

void THID::init() {
  /* USB itself is initialized by MX_USB_DEVICE_Init() before start_tos().
   * This method exists to keep the hardware-driver lifecycle consistent.
   */
}

bool THID::isConfigured() const {
  return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED;
}

bool THID::isTxIdle() const {
  if (!isConfigured() || hUsbDeviceFS.pClassData == nullptr) {
    return false;
  }
  USBD_CUSTOM_HID_HandleTypeDef *hhid =
      reinterpret_cast<USBD_CUSTOM_HID_HandleTypeDef *>(hUsbDeviceFS.pClassData);
  return hhid->state == CUSTOM_HID_IDLE;
}

THID::Status THID::waitReady(uint32_t timeout_ms) const {
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) <= timeout_ms) {
    if (!isConfigured()) {
      HAL_Delay(1);
      continue;
    }
    if (isTxIdle()) {
      return OK;
    }
    HAL_Delay(1);
  }
  return isConfigured() ? TIMEOUT : NOT_CONFIGURED;
}

THID::Status THID::mapUsbResult(int8_t ret) const {
  if (ret == static_cast<int8_t>(USBD_OK)) {
    return OK;
  }
  if (ret == static_cast<int8_t>(USBD_BUSY)) {
    return BUSY;
  }
  return ERROR;
}

THID::Status THID::sendKeyboard(uint8_t modifier,
                                uint8_t key1, uint8_t key2, uint8_t key3,
                                uint8_t key4, uint8_t key5, uint8_t key6) {
  Status ready = waitReady(200);
  if (ready != OK) {
    return ready;
  }
  return mapUsbResult(TOS_HID_SendKeyboard(modifier, key1, key2, key3,
                                           key4, key5, key6));
}

THID::Status THID::releaseKeyboard() {
  Status ready = waitReady(200);
  if (ready != OK) {
    return ready;
  }
  return mapUsbResult(TOS_HID_ReleaseKeyboard());
}

THID::Status THID::tapKey(uint8_t modifier, uint8_t key, uint16_t hold_ms) {
  Status st = sendKeyboard(modifier, key);
  if (st != OK) {
    return st;
  }
  HAL_Delay(hold_ms);
  st = releaseKeyboard();
  HAL_Delay(8);
  return st;
}

THID::Status THID::sendWinLock() {
  return tapKey(MOD_LGUI, HID_KEY_L, 40);
}

THID::Status THID::sendMouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
  Status ready = waitReady(200);
  if (ready != OK) {
    return ready;
  }
  return mapUsbResult(TOS_HID_SendMouse(buttons, x, y, wheel));
}

THID::Status THID::releaseMouse() {
  return sendMouse(0, 0, 0, 0);
}

THID::Status THID::clickMouse(uint8_t buttons, uint16_t hold_ms) {
  Status st = sendMouse(buttons, 0, 0, 0);
  if (st != OK) {
    return st;
  }
  HAL_Delay(hold_ms);
  st = releaseMouse();
  HAL_Delay(8);
  return st;
}

THID::Status THID::moveMouse(int8_t x, int8_t y) {
  return sendMouse(0, x, y, 0);
}

THID::Status THID::scrollMouse(int8_t wheel) {
  return sendMouse(0, 0, 0, wheel);
}

THID::Status THID::sendVendor(const uint8_t *payload, uint16_t len) {
  Status ready = waitReady(200);
  if (ready != OK) {
    return ready;
  }
  return mapUsbResult(TOS_HID_SendVendor(payload, len));
}

THID::Status THID::sendVendorText(const char *text) {
  if (text == nullptr) {
    return sendVendor(nullptr, 0);
  }
  uint16_t len = 0;
  while (text[len] != '\0' && len < 63U) {
    len++;
  }
  return sendVendor(reinterpret_cast<const uint8_t *>(text), len);
}

bool THID::receiveVendor(uint8_t *payload, uint16_t max_len, uint16_t *out_len) {
  return TOS_HID_GetLastVendorOut(payload, max_len, out_len) != 0U;
}

bool THID::receiveVendorText(char *text, uint16_t max_len, uint16_t *out_len) {
  if (text == nullptr || max_len == 0) {
    return false;
  }

  uint8_t tmp[63];
  uint16_t n = 0;
  if (!receiveVendor(tmp, sizeof(tmp), &n)) {
    return false;
  }

  while (n > 0 && tmp[n - 1] == 0U) {
    n--;
  }

  uint16_t copy_n = n;
  if (copy_n >= max_len) {
    copy_n = max_len - 1;
  }
  memcpy(text, tmp, copy_n);
  text[copy_n] = '\0';

  if (out_len != nullptr) {
    *out_len = copy_n;
  }
  return true;
}

bool THID::asciiToKey(char c, uint8_t *modifier, uint8_t *key) const {
  uint8_t mod = 0;
  uint8_t k = 0;

  if (c >= 'a' && c <= 'z') {
    k = HID_KEY_A + static_cast<uint8_t>(c - 'a');
  } else if (c >= 'A' && c <= 'Z') {
    mod = MOD_LSHIFT;
    k = HID_KEY_A + static_cast<uint8_t>(c - 'A');
  } else if (c >= '1' && c <= '9') {
    k = HID_KEY_1 + static_cast<uint8_t>(c - '1');
  } else {
    switch (c) {
    case '0': k = HID_KEY_0; break;
    case '\n': k = HID_KEY_ENTER; break;
    case '\r': k = HID_KEY_ENTER; break;
    case '\t': k = HID_KEY_TAB; break;
    case ' ': k = HID_KEY_SPACE; break;
    case '-': k = HID_KEY_MINUS; break;
    case '_': mod = MOD_LSHIFT; k = HID_KEY_MINUS; break;
    case '=': k = HID_KEY_EQUAL; break;
    case '+': mod = MOD_LSHIFT; k = HID_KEY_EQUAL; break;
    case '[': k = HID_KEY_LBRACE; break;
    case '{': mod = MOD_LSHIFT; k = HID_KEY_LBRACE; break;
    case ']': k = HID_KEY_RBRACE; break;
    case '}': mod = MOD_LSHIFT; k = HID_KEY_RBRACE; break;
    case '\\': k = HID_KEY_BSLASH; break;
    case '|': mod = MOD_LSHIFT; k = HID_KEY_BSLASH; break;
    case ';': k = HID_KEY_SEMI; break;
    case ':': mod = MOD_LSHIFT; k = HID_KEY_SEMI; break;
    case '\'': k = HID_KEY_QUOTE; break;
    case '"': mod = MOD_LSHIFT; k = HID_KEY_QUOTE; break;
    case '`': k = HID_KEY_GRAVE; break;
    case '~': mod = MOD_LSHIFT; k = HID_KEY_GRAVE; break;
    case ',': k = HID_KEY_COMMA; break;
    case '<': mod = MOD_LSHIFT; k = HID_KEY_COMMA; break;
    case '.': k = HID_KEY_DOT; break;
    case '>': mod = MOD_LSHIFT; k = HID_KEY_DOT; break;
    case '/': k = HID_KEY_SLASH; break;
    case '?': mod = MOD_LSHIFT; k = HID_KEY_SLASH; break;
    case '!': mod = MOD_LSHIFT; k = HID_KEY_1; break;
    case '@': mod = MOD_LSHIFT; k = HID_KEY_2; break;
    case '#': mod = MOD_LSHIFT; k = HID_KEY_3; break;
    case '$': mod = MOD_LSHIFT; k = HID_KEY_4; break;
    case '%': mod = MOD_LSHIFT; k = HID_KEY_5; break;
    case '^': mod = MOD_LSHIFT; k = HID_KEY_6; break;
    case '&': mod = MOD_LSHIFT; k = HID_KEY_7; break;
    case '*': mod = MOD_LSHIFT; k = HID_KEY_8; break;
    case '(': mod = MOD_LSHIFT; k = HID_KEY_9; break;
    case ')': mod = MOD_LSHIFT; k = HID_KEY_0; break;
    default: return false;
    }
  }

  if (modifier != nullptr) {
    *modifier = mod;
  }
  if (key != nullptr) {
    *key = k;
  }
  return k != 0U;
}

THID::Status THID::typeAscii(const char *text, uint16_t per_key_delay_ms) {
  if (text == nullptr) {
    return ERROR;
  }

  for (uint16_t i = 0; text[i] != '\0'; i++) {
    uint8_t mod = 0;
    uint8_t key = 0;
    if (!asciiToKey(text[i], &mod, &key)) {
      continue;
    }
    Status st = tapKey(mod, key, 25);
    if (st != OK) {
      return st;
    }
    HAL_Delay(per_key_delay_ms);
  }
  return OK;
}

const char *THID::statusText(Status st) const {
  switch (st) {
  case OK: return "OK";
  case NOT_CONFIGURED: return "USB not configured";
  case BUSY: return "USB busy";
  case ERROR: return "USB error";
  case TIMEOUT: return "USB timeout";
  default: return "Unknown";
  }
}
