#include "include/keyboard.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/libpd.h"
#include "include/pot.hpp"
#include <cstdio>
#include <cstring>
#include "syslog.h"

extern KeyManager keyManager;
extern LCD boardLCD;
extern Potentiometer boardPot;

// ============ Key layout ============
// lo/hi = display labels; app_lo/app_hi = char to append (0=special)
struct KbKey {
  const char *lo, *hi;
  char app_lo, app_hi;
};
#define KL(lo, hi, lo_ch, hi_ch) {lo, hi, lo_ch, hi_ch}
#define KS(lo, hi) {lo, hi, 0, 0}

static const KbKey row0[] = {
    KL("q", "Q", 'q', 'Q'), KL("w", "W", 'w', 'W'), KL("e", "E", 'e', 'E'),
    KL("r", "R", 'r', 'R'), KL("t", "T", 't', 'T'), KL("y", "Y", 'y', 'Y'),
    KL("u", "U", 'u', 'U'), KL("i", "I", 'i', 'I'), KL("o", "O", 'o', 'O'),
    KL("p", "P", 'p', 'P'),
};
static const KbKey row1[] = {
    KL("a", "A", 'a', 'A'), KL("s", "S", 's', 'S'), KL("d", "D", 'd', 'D'),
    KL("f", "F", 'f', 'F'), KL("g", "G", 'g', 'G'), KL("h", "H", 'h', 'H'),
    KL("j", "J", 'j', 'J'), KL("k", "K", 'k', 'K'), KL("l", "L", 'l', 'L'),
};
static const KbKey row2[] = {
    KS("SF", "SF"),         KL("z", "Z", 'z', 'Z'), KL("x", "X", 'x', 'X'),
    KL("c", "C", 'c', 'C'), KL("v", "V", 'v', 'V'), KL("b", "B", 'b', 'B'),
    KL("n", "N", 'n', 'N'), KL("m", "M", 'm', 'M'), KS("<-", "<-"),
};
static const KbKey row3[] = {
    KL("1", "!", '1', '!'), KL("2", "@", '2', '@'), KL("3", "#", '3', '#'),
    KL("4", "$", '4', '$'), KL("5", "%", '5', '%'), KL("6", "^", '6', '^'),
    KL("7", "&", '7', '&'), KL("8", "*", '8', '*'), KL("9", "(", '9', '('),
    KL("0", ")", '0', ')'),
};
static const KbKey row4[] = {
    KL(".", ".", '.', '.'), KL("-", "-", '-', '-'), KL("_", "_", '_', '_'),
    KL("/", "/", '/', '/'), KL(":", ":", ':', ':'),
    KL(" ", " ", ' ', ' '), KS("OK", "OK"),
};

struct KbRow {
  const KbKey *keys;
  int n;
};
static const KbRow rows[] = {
    {row0, 10}, {row1, 9}, {row2, 9}, {row3, 10}, {row4, 7},
};
#define N_ROWS 5

static int flat_n(void) {
  int t = 0;
  for (int r = 0; r < N_ROWS; r++)
    t += rows[r].n;
  return t;
}
static const KbKey *flat_key(int idx, int *ro, int *co) {
  int off = 0;
  for (int r = 0; r < N_ROWS; r++) {
    if (idx < off + rows[r].n) {
      *ro = r;
      *co = idx - off;
      return &rows[r].keys[idx - off];
    }
    off += rows[r].n;
  }
  return NULL;
}
static int iabs(int x) { return x < 0 ? -x : x; }

// ============ Draw ============

static void draw_kb(const char *pwd, int len, int sel, bool shift) {
  LCD_FLUSH({
    PD_Init();
    PD_FillScreen(TOS_BG);
    PD_DrawFrame();

    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_ACCENT);
    PD_DrawString(22, 5, "KEY");

    // Password (16px font, right margin = left*1.5)
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_TEXT);
    int lmargin = 18, rmargin = 27;
    int max_chars = (240 - lmargin - rmargin) / 10; // ~19 chars
    int start = len > max_chars ? len - max_chars : 0;
    char disp[24];
    strncpy(disp, pwd + start, len - start);
    disp[len - start] = '\0';
    PD_DrawString(lmargin, 33, disp);

    // Blue underline beneath text
    PD_SetColor(TOS_ACCENT);
    PD_SetFill(true);
    PD_DrawRect(lmargin, 50, 240 - lmargin - rmargin, 2);
    PD_SetFill(false);

    // Key grid — symmetric margins
    int base_y = 56, row_h = 28, margin = 8;
    int flat_idx = 0;
    for (int r = 0; r < N_ROWS; r++) {
      int n = rows[r].n;
      int grid_w = 240 - margin * 2;
      int slot_w = grid_w / n;
      int cy = base_y + r * row_h;

      for (int c = 0; c < n; c++) {
        const KbKey *k = &rows[r].keys[c];
        int card_w = slot_w - 2;
        int cx = margin + c * slot_w + 1;

        bool sel_k = (flat_idx == sel);
        uint32_t bg = sel_k ? TOS_ACCENT : TOS_CARD_BG;
        uint32_t fg = sel_k ? TOS_TEXT : TOS_TEXT_SEC;

        PD_DrawAngledCard(cx, cy, card_w, 22, 4, bg);
        PD_SetColor(fg);
        PD_SetFont(FONT_ASCII_16);

        const char *label = shift ? k->hi : k->lo;
        uint16_t tw = PD_GetStringWidth(label);
        uint16_t th = PD_GetCharHeight();
        PD_DrawString(cx + (card_w - tw) / 2, cy + (22 - th) / 2 + 1, label);

        flat_idx++;
      }
    }

    PD_DrawFooterCenter("ENTER", NULL, "SELECT");
  });
}

// ============ Public ============

bool keyboard_open(const char *title, char *out, int max_len) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  int len = (int)strlen(out); /* preserve initial value */
  bool shift = true;
  int total = flat_n();
  // Init sel from pot to avoid double-highlight on first frame
  int pot0 = boardPot.readRaw();
  int sel = (total - 1) - (pot0 * total / 4096);
  if (sel < 0)
    sel = 0;
  if (sel >= total)
    sel = total - 1;
  uint8_t le = 0;
  uint32_t lu = 0;
  int last_pot = pot0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    // Pot fast navigation
    int pot = boardPot.readRaw();
    if (iabs(pot - last_pot) > 20) {
      int pos = (total - 1) - (pot * total / 4096);
      if (pos < 0)
        pos = 0;
      if (pos >= total)
        pos = total - 1;
      sel = pos;
    }
    last_pot = pot;

    // UP/DOWN fine
    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % total;
      HAL_Delay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + total) % total;
      HAL_Delay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      int r, c;
      const KbKey *k = flat_key(sel, &r, &c);
      if (k) {
        const char *label = shift ? k->hi : k->lo;

        if (strcmp(label, "OK") == 0) {
          out[len] = '\0';
          LOG_I("KB", "Done: %s", out);
          return true;
        } else if (strcmp(label, "<-") == 0) {
          if (len > 0)
            len--;
        } else if (strcmp(label, "SF") == 0) {
          shift = !shift;
        } else if (len < max_len) {
          char ch = shift ? k->app_hi : k->app_lo;
          if (ch != 0) {
            out[len++] = ch;
            out[len] = '\0';
          }
        }
      }
      HAL_Delay(150);
    }
    le = ce;

    // Long-press UP+DOWN cancel
    bool up = keyManager.collision_A8.isPressed(),
         down = keyManager.collision_D0.isPressed();
    static uint32_t et = 0;
    static bool ea = false;
    if (up && down && !ea) {
      et = HAL_GetTick();
      ea = true;
    } else if (up && down && ea) {
      if (HAL_GetTick() - et > 700) {
        out[0] = '\0';
        return false;
      }
    } else if (!up && !down) {
      ea = false;
    }

    if (HAL_GetTick() - lu > 80) {
      lu = HAL_GetTick();
      draw_kb(out, len, sel, shift);
    }
    HAL_Delay(15);
  }
}
