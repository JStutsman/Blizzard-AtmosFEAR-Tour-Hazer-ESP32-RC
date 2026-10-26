// Theme.cpp  (Hazer-TX)
// Dark mode is for dark venues: dim text and muted buttons (pair it with a low backlight).
#include "Display.h"
#include "Theme.h"

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static const Theme NORMAL = {
  rgb(0, 0, 0),       rgb(255, 255, 255), rgb(150, 150, 160), rgb(255, 255, 255),
  rgb(20, 90, 200),   rgb(255, 255, 255), rgb(190, 215, 255), rgb(255, 190, 80),
  rgb(60, 60, 70),    rgb(40, 40, 45),    rgb(80, 200, 80),   rgb(230, 140, 20),
  rgb(30, 140, 60),   rgb(200, 110, 20),
  rgb(120, 255, 140), rgb(15, 60, 130),   rgb(255, 0, 0),
};

static const Theme DARK = {
  rgb(0, 0, 0),       rgb(170, 170, 180), rgb(100, 100, 110), rgb(70, 70, 80),
  rgb(8, 22, 50),     rgb(150, 175, 215), rgb(110, 135, 175), rgb(200, 140, 50),
  rgb(28, 28, 34),    rgb(18, 18, 22),    rgb(40, 110, 40),   rgb(140, 80, 10),
  rgb(20, 90, 40),    rgb(130, 70, 10),
  rgb(70, 170, 90),   rgb(15, 35, 70),    rgb(200, 30, 30),
};

static bool dark = false;

const Theme& theme() { return dark ? DARK : NORMAL; }
bool themeDark()     { return dark; }

void themeSetDark(bool d) { dark = d; }

void themeSetBacklight(uint8_t pct) {
  static int shown = -1;
  if (pct == shown) return;
  shown = pct;
  lcd.setBrightness((uint16_t)pct * 255 / 100);
}
