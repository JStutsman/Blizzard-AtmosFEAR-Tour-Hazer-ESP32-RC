// UiNumpad.cpp  (Hazer-TX)
// 4x4 pad: digits, DEL, CLR, CANCEL, OK (0 is a wide key). OK only accepts values in range.
#include <Arduino.h>
#include "Config.h"
#include "Display.h"
#include "Theme.h"
#include "UiNumpad.h"

enum { K_DEL = 10, K_CLR, K_CANCEL, K_OK, K_COUNT };

// ---------------- layout ----------------
static const int KX[4] = {6, 84, 162, 240}, KW = 74, KH = 40;
static const int KY[4] = {68, 112, 156, 200};

struct Key { int id, col, row, span; const char* label; };
static const Key KEYS[] = {
  {1, 0, 0, 1, "1"}, {2, 1, 0, 1, "2"}, {3, 2, 0, 1, "3"}, {K_DEL, 3, 0, 1, "DEL"},
  {4, 0, 1, 1, "4"}, {5, 1, 1, 1, "5"}, {6, 2, 1, 1, "6"}, {K_CLR, 3, 1, 1, "CLR"},
  {7, 0, 2, 1, "7"}, {8, 1, 2, 1, "8"}, {9, 2, 2, 1, "9"}, {K_CANCEL, 3, 2, 1, "CANCEL"},
  {0, 0, 3, 3, "0"},                                       {K_OK, 3, 3, 1, "OK"},
};
static const int KEY_COUNT = sizeof(KEYS) / sizeof(KEYS[0]);

static int keyW(const Key& k) { return KX[k.col + k.span - 1] + KW - KX[k.col]; }

static int keyAt(int x, int y) {
  for (int i = 0; i < KEY_COUNT; i++) {
    const Key& k = KEYS[i];
    if (x >= KX[k.col] && x < KX[k.col] + keyW(k) && y >= KY[k.row] && y < KY[k.row] + KH) return k.id;
  }
  return -1;
}

// ---------------- state ----------------
static bool       active = false, full = false, valueDirty = false;
static char       title[28];
static char       digits[5];                   // typed so far
static uint8_t    nDigits = 0, maxDigits = 3;
static int        minV = 0, maxV = 0;
static bool       rangeMsg = false;            // "Range a-b" shown after a refused OK
static NumpadDone doneCb = nullptr;
static bool       wasDown = false;
static uint32_t   lastTouchMs = 0, lastAct = 0;

void uiNumpadStart(const char* t, int value, int lo, int hi, NumpadDone done) {
  snprintf(title, sizeof(title), "%s", t);
  minV = lo; maxV = hi; doneCb = done;
  maxDigits = hi >= 1000 ? 4 : hi >= 100 ? 3 : hi >= 10 ? 2 : 1;
  nDigits = 0; digits[0] = 0;
  if (value >= lo && value <= hi) nDigits = snprintf(digits, sizeof(digits), "%d", value);
  rangeMsg = false;
  active = true; full = true;
  wasDown = true;                              // ignore the touch that opened the pad
  lastAct = lastTouchMs = millis();
}

bool uiNumpadActive() { return active; }

static void close(bool ok, int value) {
  active = false;
  if (doneCb) doneCb(ok, value);
}

static void press(int id) {
  if (id >= 0 && id <= 9) {
    if (nDigits == 1 && digits[0] == '0') nDigits = 0;   // no leading zero
    if (nDigits < maxDigits) { digits[nDigits++] = '0' + id; digits[nDigits] = 0; }
  } else if (id == K_DEL) {
    if (nDigits) digits[--nDigits] = 0;
  } else if (id == K_CLR) {
    nDigits = 0; digits[0] = 0;
  } else if (id == K_CANCEL) {
    close(false, 0);
    return;
  } else if (id == K_OK) {
    int v = nDigits ? atoi(digits) : -1;
    if (v >= minV && v <= maxV) { close(true, v); return; }
    rangeMsg = true;
  }
  if (id != K_OK) rangeMsg = false;
  valueDirty = true;
}

// ---------------- touch ----------------
void uiNumpadUpdate(bool touched, int32_t tx, int32_t ty, uint32_t now) {
  if (touched) lastTouchMs = lastAct = now;
  bool down = touched || now - lastTouchMs < 60; // short release debounce
  bool tap  = down && !wasDown;
  wasDown = down;

  if (now - lastAct > PIN_IDLE_TIMEOUT_MS) { close(false, 0); return; }
  if (tap) press(keyAt(tx, ty));
}

// ---------------- drawing ----------------
static void drawValue() {
  const Theme& t = theme();
  lcd.fillRect(0, 30, 320, 34, t.screenBg);
  lcd.setFont(&fonts::Font4);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setTextColor(t.text, t.screenBg);
  lcd.drawString(nDigits ? digits : "_", 160, 46);
  char r[24];
  if (rangeMsg) snprintf(r, sizeof(r), "Range %d-%d", minV, maxV);   // refused OK
  else          snprintf(r, sizeof(r), "%d-%d", minV, maxV);
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(textdatum_t::middle_right);
  lcd.setTextColor(rangeMsg ? t.warn : t.textDim, t.screenBg);
  lcd.drawString(r, 312, 46);
}

void uiNumpadRender(uint32_t now) {
  if (full) {
    full = false;
    const Theme& t = theme();
    lcd.fillScreen(t.screenBg);
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setTextColor(t.text, t.screenBg);
    lcd.drawString(title, 160, 14);
    for (int i = 0; i < KEY_COUNT; i++) {
      const Key& k = KEYS[i];
      uint16_t bg = k.id == K_OK ? t.btnConfirm : t.btn;
      int x = KX[k.col], y = KY[k.row], w = keyW(k);
      lcd.fillRoundRect(x, y, w, KH, 6, bg);
      lcd.drawRoundRect(x, y, w, KH, 6, t.border);
      lcd.setFont(k.label[1] ? &fonts::Font2 : &fonts::Font4);   // words small, digits large
      lcd.setTextColor(t.text, bg);
      lcd.drawString(k.label, x + w / 2, y + KH / 2);
    }
    valueDirty = true;
  }
  if (valueDirty) { valueDirty = false; drawValue(); }
}
