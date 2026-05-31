#ifndef __THID_HPP
#define __THID_HPP

#include "main.h"
#include <cstddef>
#include <cstdint>

extern "C" {
#include "usb_device.h"
#include "usbd_custom_hid_if.h"
#include "usbd_customhid.h"
#include "usbd_def.h"
}

/*
 * THID is a small C++ wrapper around the CubeMX Custom HID USER CODE API.
 * Report layout is defined in USB_DEVICE/App/usbd_custom_hid_if.c:
 *   0x01: Keyboard IN report
 *   0x02: Mouse IN report
 *   0x10: Vendor IN/OUT report, 1-byte Report ID + 63-byte payload
 */
class THID {
public:
  enum Status : int8_t {
    OK = 0,
    NOT_CONFIGURED = -1,
    BUSY = -2,
    ERROR = -3,
    TIMEOUT = -4,
  };

  enum Modifier : uint8_t {
    MOD_LCTRL  = 0x01,
    MOD_LSHIFT = 0x02,
    MOD_LALT   = 0x04,
    MOD_LGUI   = 0x08,
    MOD_RCTRL  = 0x10,
    MOD_RSHIFT = 0x20,
    MOD_RALT   = 0x40,
    MOD_RGUI   = 0x80,
  };

  enum MouseButton : uint8_t {
    MOUSE_LEFT   = 0x01,
    MOUSE_RIGHT  = 0x02,
    MOUSE_MIDDLE = 0x04,
  };

  THID();

  void init();
  bool isConfigured() const;
  bool isTxIdle() const;
  Status waitReady(uint32_t timeout_ms = 200) const;

  Status sendKeyboard(uint8_t modifier,
                      uint8_t key1 = 0, uint8_t key2 = 0, uint8_t key3 = 0,
                      uint8_t key4 = 0, uint8_t key5 = 0, uint8_t key6 = 0);
  Status releaseKeyboard();
  Status tapKey(uint8_t modifier, uint8_t key, uint16_t hold_ms = 35);
  Status sendWinLock();
  Status typeAscii(const char *text, uint16_t per_key_delay_ms = 20);

  Status sendMouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel = 0);
  Status releaseMouse();
  Status clickMouse(uint8_t buttons = MOUSE_LEFT, uint16_t hold_ms = 35);
  Status moveMouse(int8_t x, int8_t y);
  Status scrollMouse(int8_t wheel);

  Status sendVendor(const uint8_t *payload, uint16_t len);
  Status sendVendorText(const char *text);
  bool receiveVendor(uint8_t *payload, uint16_t max_len, uint16_t *out_len);
  bool receiveVendorText(char *text, uint16_t max_len, uint16_t *out_len = nullptr);

  const char *statusText(Status st) const;

private:
  Status mapUsbResult(int8_t ret) const;
  bool asciiToKey(char c, uint8_t *modifier, uint8_t *key) const;
};

extern THID boardHID;

/* Frequently used HID keyboard usage IDs. */
#define HID_KEY_A       0x04U
#define HID_KEY_B       0x05U
#define HID_KEY_C       0x06U
#define HID_KEY_D       0x07U
#define HID_KEY_E       0x08U
#define HID_KEY_F       0x09U
#define HID_KEY_G       0x0AU
#define HID_KEY_H       0x0BU
#define HID_KEY_I       0x0CU
#define HID_KEY_J       0x0DU
#define HID_KEY_K       0x0EU
#define HID_KEY_L       0x0FU
#define HID_KEY_M       0x10U
#define HID_KEY_N       0x11U
#define HID_KEY_O       0x12U
#define HID_KEY_P       0x13U
#define HID_KEY_Q       0x14U
#define HID_KEY_R       0x15U
#define HID_KEY_S       0x16U
#define HID_KEY_T       0x17U
#define HID_KEY_U       0x18U
#define HID_KEY_V       0x19U
#define HID_KEY_W       0x1AU
#define HID_KEY_X       0x1BU
#define HID_KEY_Y       0x1CU
#define HID_KEY_Z       0x1DU
#define HID_KEY_1       0x1EU
#define HID_KEY_2       0x1FU
#define HID_KEY_3       0x20U
#define HID_KEY_4       0x21U
#define HID_KEY_5       0x22U
#define HID_KEY_6       0x23U
#define HID_KEY_7       0x24U
#define HID_KEY_8       0x25U
#define HID_KEY_9       0x26U
#define HID_KEY_0       0x27U
#define HID_KEY_ENTER   0x28U
#define HID_KEY_ESC     0x29U
#define HID_KEY_BACKSP  0x2AU
#define HID_KEY_TAB     0x2BU
#define HID_KEY_SPACE   0x2CU
#define HID_KEY_MINUS   0x2DU
#define HID_KEY_EQUAL   0x2EU
#define HID_KEY_LBRACE  0x2FU
#define HID_KEY_RBRACE  0x30U
#define HID_KEY_BSLASH  0x31U
#define HID_KEY_SEMI    0x33U
#define HID_KEY_QUOTE   0x34U
#define HID_KEY_GRAVE   0x35U
#define HID_KEY_COMMA   0x36U
#define HID_KEY_DOT     0x37U
#define HID_KEY_SLASH   0x38U

#endif /* __THID_HPP */
