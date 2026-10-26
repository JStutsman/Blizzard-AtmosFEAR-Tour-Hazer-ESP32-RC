// UiSettings.cpp  (Hazer-TX)
// Gear screen: the remote's own settings that are not part of the hazer's menu.
// One row per setting with its value and two step buttons; DONE (or inactivity) closes.
// Receivers, display and backlight apply as you tap; DMX addresses are sent on close,
// so holding +/- doesn't make the receiver rewrite its flash on every step.
#include <Arduino.h>
#include "Config.h"
#include "HazerProtocol.h"
#include "Display.h"
#include "HazerState.h"
#include "Radio.h"
#include "FixtureTable.h"
#include "Theme.h"
#include "UiPanel.h"
#include "UiSettings.h"

enum { ROW_TARGET, ROW_DMX1, ROW_DMX2, ROW_DISPLAY, ROW_BACKLIGHT, ROW_COUNT };
static const char* ROW_LABEL[ROW_COUNT]  = {"Receivers", "RX1 DMX", "RX2 DMX", "Display", "Backlight"};
static const bool  ROW_CHOICE[ROW_COUNT] = {true, false, false, true, false};   // < > (no repeat) vs - +

// ---------------- layout ----------------
static const int ROW_Y0 = 34, ROW_H = 34, BTN_W = 44, BTN_H = 30;
static const int BTN_X[2] = {214, 266};          // step down, step up
static const int DONE_Y = 206, DONE_H = 30;

static bool rowBtnAt(int x, int y, int* row, int* dir) {
  for (int r = 0; r < ROW_COUNT; r++) {
    int y0 = ROW_Y0 + r * ROW_H;
    if (y < y0 || y >= y0 + BTN_H) continue;
    for (int b = 0; b < 2; b++)
      if (x >= BTN_X[b] && x < BTN_X[b] + BTN_W) { *row = r; *dir = b ? +1 : -1; return true; }
  }
  return false;
}

// ---------------- state ----------------
static bool     active = false;
static bool     full = false;                    // redraw the whole screen
static bool     rowDirty[ROW_COUNT];
static int      dmxEdit[DMX_RX_COUNT];           // address being edited, 0 = not known yet
static bool     dmxEdited[DMX_RX_COUNT];         // changed here, sent on close
static bool     wasDown = false;
static int      heldRow = -1, heldDir = 0;
static uint32_t lastTouchMs = 0, lastAct = 0, nextRepeat = 0, holdStart = 0;

void uiSettingsStart() {
  for (int i = 0; i < DMX_RX_COUNT; i++) { dmxEdit[i] = hazerDmxWanted(i); dmxEdited[i] = false; }
  active  = true;
  full    = true;
  wasDown = true;                                // ignore the touch that opened this screen
  heldRow = -1;
  lastAct = lastTouchMs = millis();
}

bool uiSettingsActive() { return active; }

static void close() {
  for (int i = 0; i < DMX_RX_COUNT; i++) if (dmxEdited[i]) hazerSetDmxWanted(i, dmxEdit[i]);
  hazerSaveSettings();
  active = false;
  uiPanelInvalidate();
}

static void step(int row, int dir, uint32_t now) {
  switch (row) {
    case ROW_TARGET:    hazerSetTargetMask((hazerTargetMask() - 1 + (dir > 0 ? 1 : 2)) % 3 + 1); break;   // wraps
    case ROW_DISPLAY:   hazerSetDarkMode(!hazerDarkMode()); rowDirty[ROW_BACKLIGHT] = true; break;
    case ROW_BACKLIGHT: hazerSetBacklightPct(hazerBacklightPct() + dir * 5); break;
    default: {
      int i = row - ROW_DMX1;
      if (!dmxEdit[i]) return;                   // not known yet
      int n = (now - holdStart > 2000) ? 10 : 1; // speed up long holds
      dmxEdit[i] = constrain(dmxEdit[i] + dir * n, 1, 510);   // hazer uses 3 channels: base..base+2
      dmxEdited[i] = true;
      break;
    }
  }
}

// ---------------- touch ----------------
void uiSettingsUpdate(bool touched, int32_t tx, int32_t ty, uint32_t now) {
  // DMX rows start with the address set here, else the one the receiver reports
  for (int i = 0; i < DMX_RX_COUNT && i < RECEIVER_COUNT; i++) {
    if (dmxEdit[i]) continue;
    RadioStatus rs = radioGetStatus(i);
    if (rs.linkUp && rs.base >= 1 && rs.base <= 510) { dmxEdit[i] = rs.base; rowDirty[ROW_DMX1 + i] = true; }
  }

  if (touched) lastTouchMs = lastAct = now;
  bool down = touched || now - lastTouchMs < 60; // short release debounce
  bool tap  = down && !wasDown;
  wasDown = down;

  if (now - lastAct > PIN_IDLE_TIMEOUT_MS) { close(); return; }

  if (!down) {
    if (heldRow >= 0) { rowDirty[heldRow] = true; heldRow = -1; }   // release highlight
    return;
  }
  if (tap) {
    if (ty >= DONE_Y) { close(); return; }
    int row, dir;
    if (rowBtnAt(tx, ty, &row, &dir)) {
      heldRow = row; heldDir = dir;
      holdStart = now; nextRepeat = now + 450;
      step(row, dir, now);
      rowDirty[row] = true;
    }
  } else if (heldRow >= 0 && !ROW_CHOICE[heldRow] && now >= nextRepeat) {
    step(heldRow, heldDir, now);
    rowDirty[heldRow] = true;
    nextRepeat = now + 100;
  }
}

// ---------------- drawing ----------------
static void valueText(int row, char* buf, size_t len) {
  static const char* TARGET_NAMES[] = {"", "RX 1", "RX 2", "RX 1 + 2"};
  switch (row) {
    case ROW_TARGET:    snprintf(buf, len, "%s", TARGET_NAMES[hazerTargetMask() & 3]); break;
    case ROW_DISPLAY:   snprintf(buf, len, "%s", hazerDarkMode() ? "Dark" : "Normal"); break;
    case ROW_BACKLIGHT: snprintf(buf, len, "%d%%", hazerBacklightPct()); break;
    default: {
      int i = row - ROW_DMX1;
      if (dmxEdit[i]) snprintf(buf, len, "%d%s", dmxEdit[i], dmxEdited[i] ? " *" : "");   // * = sent on DONE
      else            snprintf(buf, len, "--");
      break;
    }
  }
}

static void drawButton(int x, int y, int w, int h, const char* label, uint16_t bg) {
  lcd.fillRoundRect(x, y, w, h, 6, bg);
  lcd.drawRoundRect(x, y, w, h, 6, theme().border);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setTextColor(theme().text, bg);
  lcd.drawString(label, x + w / 2, y + h / 2);
}

static void drawRow(int r) {
  const Theme& t = theme();
  int y0 = ROW_Y0 + r * ROW_H, cy = y0 + BTN_H / 2;
  lcd.fillRect(0, y0, 320, BTN_H, t.screenBg);

  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(textdatum_t::middle_left);
  lcd.setTextColor(t.textDim, t.screenBg);
  lcd.drawString(ROW_LABEL[r], 10, cy);

  char v[20];
  valueText(r, v, sizeof(v));
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(t.text, t.screenBg);
  lcd.drawString(v, 92, cy);

  for (int b = 0; b < 2; b++) {
    const char* label = ROW_CHOICE[r] ? (b ? ">" : "<") : (b ? "+" : "-");
    bool pressed = heldRow == r && heldDir == (b ? 1 : -1);
    drawButton(BTN_X[b], y0, BTN_W, BTN_H, label, pressed ? t.btnPressed : t.btn);
  }
}

void uiSettingsRender(uint32_t now) {
  static bool shownDark = false;
  if (themeDark() != shownDark) { shownDark = themeDark(); full = true; }   // palette changed

  if (full) {
    full = false;
    lcd.fillScreen(theme().screenBg);
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setTextColor(theme().text, theme().screenBg);
    lcd.drawString("Settings", 160, 16);
    drawButton(10, DONE_Y, 300, DONE_H, "DONE", theme().btnConfirm);
    for (int r = 0; r < ROW_COUNT; r++) rowDirty[r] = true;
  }
  for (int r = 0; r < ROW_COUNT; r++) {
    if (!rowDirty[r]) continue;
    rowDirty[r] = false;
    drawRow(r);
  }
}
