// Theme.h  (Hazer-TX) - screen colors and backlight for normal and dark mode
#pragma once
#include <Arduino.h>

struct Theme {
  uint16_t screenBg, text, textDim, border;           // black screens, labels, outlines
  uint16_t lcdBg, lcdText, infoOk, warn;              // blue "LCD" area and info line
  uint16_t btn, btnLocked, btnPressed, btnActive;     // panel / keypad buttons
  uint16_t btnConfirm, btnTake;                       // CONFIRM and take-control buttons
  uint16_t barOn, barOff, alert;                      // signal bars, red dot / X
  uint8_t  brightness;                                // backlight 0-255
};

const Theme& theme();
bool themeDark();
void themeSetDark(bool dark);   // switches palette and backlight (no-op if unchanged)
