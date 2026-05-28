#include "keyboard.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/libpd.h"
#include <cstdio>
#include <cstring>

extern KeyManager keyManager;
extern LCD boardLCD;

// Key layout — 5 rows, each rendered as 5~10 small cards per row.
// UP/DOWN snakes through every slot in reading order.
// Each slot has a display label and the actual char to append.
struct KeySlot {
  const char *label;  // display text (e.g. "Q" or "DEL" or "SP")
  char append;        // 0 = special key, otherwise ASCII char to add
  bool wide;          // true = take 2 slots width
};

// Row definitions
static const KeySlot row0[] = {
  {"Q",'Q'},{"W",'W'},{"E",'E'},{"R",'R'},{"T",'T'},{"Y",'Y'},{"U",'U'},{"I",'I'},{"O",'O'},{"P",'P'},
};
static const KeySlot row1[] = {
  {"A",'A'},{"S",'S'},{"D",'D'},{"F",'F'},{"G",'G'},{"H",'H'},{"J",'J'},{"K",'K'},{"L",'L'},
};
static const KeySlot row2[] = {
  {"@",'@'},{"Z",'Z'},{"X",'X'},{"C",'C'},{"V",'V'},{"B",'B'},{"N",'N'},{"M",'M'},{"!",'!'},
};
static const KeySlot row3[] = {
  {"1",'1'},{"2",'2'},{"3",'3'},{"4",'4'},{"5",'5'},{"6",'6'},{"7",'7'},{"8",'8'},{"9",'9'},{"0",'0'},
};
static const KeySlot row4[] = {
  {".",'.'},{"-",'-'},{"_",'_'},{"DEL",0},{"SP",' '},{"OK",0},
};

struct RowDef { const KeySlot *slots; int count; };
static const RowDef rows[] = {
  {row0,10},{row1,9},{row2,9},{row3,10},{row4,6},
};
#define NUM_ROWS 5

// Flattened slot index for UP/DOWN navigation
static int flat_count(void) {
  int n = 0;
  for (int r = 0; r < NUM_ROWS; r++) n += rows[r].count;
  return n;
}

static bool get_flat(int idx, KeySlot *out, int *out_row, int *out_col) {
  int n = 0;
  for (int r = 0; r < NUM_ROWS; r++) {
    if (idx < n + rows[r].count) {
      *out = rows[r].slots[idx - n];
      *out_row = r;
      *out_col = idx - n;
      return true;
    }
    n += rows[r].count;
  }
  return false;
}

// ============ Draw ============

static void draw_kb(const char *title, const char *pwd, int pwd_len, int sel) {
  PD_Init();
  PD_FillScreen(TOS_BG);
  PD_DrawFrame();

  // Title
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, title);

  // Password field (masked)
  PD_SetFont(FONT_ASCII_20);
  PD_SetColor(TOS_TEXT);
  char mask[25];
  for (int i = 0; i < pwd_len; i++) mask[i] = '*';
  mask[pwd_len] = '\0';
  PD_DrawString(22, 28, mask);
  // Cursor
  PD_SetColor(TOS_ACCENT);
  PD_SetFont(FONT_ASCII_20);
  PD_DrawString(22 + pwd_len * 12 + 2, 28, "_");

  // Key rows — small cards
  int base_y = 56;
  int row_h = 28;
  int row_gap = 30;

  int flat_idx = 0;
  for (int r = 0; r < NUM_ROWS; r++) {
    int n = rows[r].count;
    // Evenly distribute across 224px usable width
    int total_w = 224;
    int slot_w = total_w / n;
    int cy = base_y + r * row_gap;

    for (int c = 0; c < n; c++) {
      KeySlot ks = rows[r].slots[c];
      int card_w = ks.wide ? slot_w * 2 - 2 : slot_w - 2;
      int cx = 8 + c * slot_w + 1;

      bool selected = (flat_idx == sel);
      uint32_t bg = selected ? TOS_ACCENT : TOS_CARD_BG;
      uint32_t fg = selected ? TOS_TEXT   : TOS_TEXT_SEC;

      PD_DrawAngledCard(cx, cy, card_w, 22, 4, bg);
      PD_SetColor(fg);
      PD_SetFont(FONT_ASCII_16);

      // Centre text in card
      uint16_t tw = PD_GetStringWidth(ks.label);
      uint16_t th = PD_GetCharHeight();
      PD_DrawString(cx + (card_w - tw) / 2, cy + (22 - th) / 2 + 1, ks.label);

      flat_idx++;
    }
  }

  // Footer
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_GREY);
  PD_DrawString(8, 220, "UP/DOWN: move");
  PD_DrawString(130, 220, "ENTER: select");

  LCD_Flush();
}

// ============ Public ============

bool keyboard_open(const char *title, char *out, int max_len) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  int pwd_len = 0;
  memset(out, 0, max_len + 1);
  int total = flat_count();
  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % total;
      HAL_Delay(120);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + total) % total;
      HAL_Delay(120);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      KeySlot ks; int r, c;
      if (get_flat(sel, &ks, &r, &c)) {
        if (ks.append == 0 && strcmp(ks.label, "DEL") == 0) {
          // Backspace
          if (pwd_len > 0) pwd_len--;
        } else if (ks.append == 0 && strcmp(ks.label, "OK") == 0) {
          // Done
          out[pwd_len] = '\0';
          printf("[KB] Password entered: %s\r\n", out);
          return true;
        } else if (ks.append != 0 && pwd_len < max_len) {
          // Append character
          out[pwd_len++] = ks.append;
          out[pwd_len] = '\0';
        }
      }
      HAL_Delay(150);
    }
    le = ce;

    // Long-press UP+DOWN to cancel
    bool up = keyManager.collision_A8.isPressed(), down = keyManager.collision_D0.isPressed();
    static uint32_t et = 0; static bool ea = false;
    if (up && down && !ea) { et = HAL_GetTick(); ea = true; }
    else if (up && down && ea) { if (HAL_GetTick() - et > 700) { out[0] = '\0'; return false; } }
    else if (!up && !down) { ea = false; }

    if (HAL_GetTick() - lu > 100) { lu = HAL_GetTick(); draw_kb(title, out, pwd_len, sel); }
    HAL_Delay(20);
  }
}
