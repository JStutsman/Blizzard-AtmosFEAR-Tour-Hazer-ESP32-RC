// Display.cpp  (Hazer-TX)
#include "Display.h"

LGFX lcd;

void displayBegin() {
  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(255);
  lcd.fillScreen(TFT_BLACK);
#if TOUCH_CAL_ON_BOOT
  uint16_t cal[8];
  lcd.calibrateTouch(cal, TFT_WHITE, TFT_BLACK, 15);
  for (int i = 0; i < 8; i++) Serial.printf("%u%s", cal[i], i < 7 ? "," : "\n");
  lcd.fillScreen(TFT_BLACK);
#endif
}
