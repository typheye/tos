/**
 ******************************************************************************
 * @file    libui.h
 * @brief   Shared UI drawing helpers.
 ******************************************************************************
 */

#ifndef LIBUI_H
#define LIBUI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void UI_DrawFrameTitle(const char *title);
void UI_DrawFrameStatusIcons(void);
void UI_DrawMenuCard(int idx, int sel, int cy, const char *text);
void UI_DrawMenuCardEx(int idx, int sel, int cy, const char *text, bool grey);
void UI_DrawMenuValue(int idx, int sel, int cy, const char *label,
                      const char *value, bool editing);
void UI_DrawMenuValueEx(int idx, int sel, int cy, const char *label,
                        const char *value, bool editing, bool grey);
void UI_DrawMenuBool(int idx, int sel, int cy, const char *label, bool on,
                     bool editing);
void UI_DrawStdFooter(void);
int UI_MenuVisibleStart(int count, int sel, int visible);
void UI_DrawSimpleMenu(const char *title, const char **items, int count,
                       int sel);
int UI_MenuLoop(const char *title, const char **items, int count,
                int start_sel);

#ifdef __cplusplus
}
#endif

#endif
