# TOS Cloud API Incremental Notes: HID Commands

This document describes only the incremental cloud protocol additions for the
STM32 Custom HID bridge.

## Heartbeat Additions

Endpoint unchanged:

```http
POST /v1/device/heartbeat
```

Heartbeat JSON now includes:

```json
{
  "hid": "idle"
}
```

`hid` values:

- `idle`: USB HID is configured and no cloud HID command is running.
- `busy`: a cloud HID command is currently being sent in small non-blocking steps.
- `no_usb`: USB is not configured by the host PC.
- `error`: last HID command failed. The device continues running and later commands may still work.

The command ACK means the command was accepted into the local HID queue, not
that every key/mouse report has already finished on the PC.

## Command Envelope

Commands are still delivered through the existing command array:

```json
{
  "cmds": [
    {
      "id": "cmd-1001",
      "action": "hid_type",
      "params": {
        "text": "hello"
      }
    }
  ]
}
```

The firmware accepts parameters either at the top level or inside `params`.

ACK endpoint unchanged:

```http
POST /v1/device/{device_id}/commands/ack
```

ACK body:

```json
{
  "acks": [
    {
      "id": "cmd-1001",
      "ok": true,
      "err": ""
    }
  ]
}
```

Common `err` values:

- `usb not configured`
- `hid busy`
- `missing text`
- `bad key`
- `bad combo`
- `missing wheel`
- `unknown hid action`

## Supported HID Actions

### Type Text

Actions:

- `hid_type`
- `type_text`
- `keyboard_type`

Example:

```json
{
  "id": "cmd-type-1",
  "action": "hid_type",
  "params": {
    "text": "TOS HID OK\n",
    "delay": 20,
    "hold": 25
  }
}
```

Parameters:

- `text`: ASCII text, max 96 bytes per command.
- `delay`: optional gap between keys in ms, clamped to 5..250.
- `hold`: optional key hold time in ms, clamped to 5..80.

Unsupported characters are skipped and logged locally.

### Hotkey

Actions:

- `hid_hotkey`
- `hotkey`
- `shortcut`

Example:

```json
{
  "id": "cmd-hotkey-1",
  "action": "hid_hotkey",
  "params": {
    "combo": "ctrl+shift+esc",
    "hold": 40
  }
}
```

Supported modifiers in `combo`:

- `ctrl`
- `shift`
- `alt`
- `win`, `gui`, `meta`, `cmd`

Supported key names:

- letters `a`..`z`
- digits `0`..`9`
- `enter`, `esc`, `backspace`, `tab`, `space`
- `delete`, `left`, `right`, `up`, `down`
- `capslock`
- `f1`..`f12`

### Single Key

Actions:

- `hid_key`
- `keyboard_key`
- `tap_key`

Examples:

```json
{
  "id": "cmd-key-1",
  "action": "hid_key",
  "params": {
    "key": "enter"
  }
}
```

```json
{
  "id": "cmd-key-2",
  "action": "hid_key",
  "params": {
    "key_code": 76,
    "modifier": "ctrl+alt"
  }
}
```

`key_code` is a raw USB HID keyboard usage ID. Prefer symbolic `key` where
possible.

### Mouse Move

Actions:

- `hid_mouse`
- `mouse_move`
- `mouse`

Example:

```json
{
  "id": "cmd-mouse-1",
  "action": "hid_mouse",
  "params": {
    "x": 20,
    "y": -10
  }
}
```

Parameters:

- `x`: relative movement, clamped to -127..127.
- `y`: relative movement, clamped to -127..127.
- `wheel`: optional wheel delta, clamped to -127..127.
- `button` or `buttons`: optional held buttons.
- `hold`: optional button hold time in ms, clamped to 0..200.

### Mouse Click

Actions:

- `hid_click`
- `mouse_click`
- `click`

Example:

```json
{
  "id": "cmd-click-1",
  "action": "hid_click",
  "params": {
    "button": "left",
    "hold": 35
  }
}
```

Button names:

- `left`
- `right`
- `middle`

### Mouse Scroll

Actions:

- `hid_scroll`
- `mouse_scroll`
- `scroll`

Example:

```json
{
  "id": "cmd-scroll-1",
  "action": "hid_scroll",
  "params": {
    "wheel": -4
  }
}
```

### Vendor Text

Actions:

- `hid_vendor`
- `vendor_text`
- `vendor`

Example:

```json
{
  "id": "cmd-vendor-1",
  "action": "hid_vendor",
  "params": {
    "text": "ping"
  }
}
```

The payload is sent as Custom HID vendor report ID `0x10`, max 63 bytes.

### Release All HID Inputs

Actions:

- `hid_release`
- `release_hid`
- `release`

Example:

```json
{
  "id": "cmd-release-1",
  "action": "hid_release"
}
```

This sends keyboard release and mouse release reports.

## Runtime Rules

- Only one HID cloud command can be queued at a time.
- HID sending is non-blocking and runs from the normal firmware tick path.
- If a HID command is already active, the next HID command ACK returns `hid busy`.
- HID commands never reinitialize USB and never reset the device.
- If USB is unplugged or not enumerated, commands fail with `usb not configured`.
- Cloud heartbeat and ACK transport remain single-request-at-a-time through the existing ESP8266 network manager.
