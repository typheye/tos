#ifndef KEYBOARD_HPP
#define KEYBOARD_HPP

// Opens an ASCII keyboard overlay for password entry.
// title: shown at top (e.g. "WiFi Password")
// max_len: max password length (1~24)
// out: buffer to receive the password (caller-allocated, must be >= max_len+1)
// Returns: true if user confirmed (pressed Connect), false if cancelled.
bool keyboard_open(const char *title, char *out, int max_len);

#endif
