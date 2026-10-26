// Theme.h  (Hazer-TX) - screen colors for normal and dark mode, and the backlight
#pragma once
#include <Arduino.h>

struct Theme {
  uint16_t screenBg, text, textDim, border;           // black screens, labels, outlines
  uint16_t lcdBg, lcdText, infoOk, warn;              // blue "LCD" area and info line
  uint16_t btn, btnLocked, btnPressed, btnActive;     // panel / keypad buttons
  uint16_t btnConfirm, btnTake;                       // CONFIRM and take-control buttons
  uint16_t barOn, barOff, alert;                      // signal bars, red dot / X
};

const Theme& theme();
bool themeDark();
void themeSetDark(bool dark);           // switches palette
void themeSetBacklight(uint8_t pct);    // 0-100 % (no-op if unchanged)
