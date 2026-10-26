// UiPanel.cpp  (Hazer-TX)
// Draws the panel mimic: a blue "LCD" with two text lines plus a small info line
// (receiver version, signal strength, value echo), and six touch buttons laid out
// like the machine (FUNCTION UP DOWN / TIMER START-STOP MANUAL).
#include <Arduino.h>
#include "Config.h"
#include "HazerProtocol.h"
#include "Display.h"
#include "HazerState.h"
#include "Radio.h"
#include "UiPanel.h"

// ---------------- layout ----------------
static const int BX[3] = {8, 112, 216};
static const int BY[2] = {104, 170};
static const int BW = 96, BH = 58;
static const char* BL[6] = {"FUNCTION", "UP", "DOWN", "TIMER", "START/STOP", "MANUAL"};

static int btnAt(int x, int y) {
  for (int i = 0; i < 6; i++) {
    int cx = BX[i % 3], cy = BY[i / 3];
    if (x >= cx && x < cx + BW && y >= cy && y < cy + BH) return i;
  }
  return -1;
}

// ---------------- state ----------------
static bool needFull = true;
static bool ignoreTouch = false;
static int  pressedBtn = -1;

void uiPanelInvalidate() {
  needFull = true;
  ignoreTouch = true;
}

// ---------------- touch ----------------
UiResult uiPanelUpdate(bool touched, int32_t tx, int32_t ty, uint32_t now) {
  static int      heldBtn = -1;
  static uint32_t lastTouchMs = 0, nextRepeat = 0, lcdHoldStart = 0;

  if (ignoreTouch) {                 // wait for finger release after returning from another screen
    if (!touched) ignoreTouch = false;
    return UI_NONE;
  }

  int hit = -1;
  if (touched) {
    lastTouchMs = now;
    hit = btnAt(tx, ty);
  } else if (now - lastTouchMs < 60) {
    hit = heldBtn;                   // short release debounce
  }

  // Hidden gesture: hold the blue LCD area to open PIN entry
  if (touched && ty < 100 && hit < 0) {
    if (lcdHoldStart == 0) lcdHoldStart = now;
    if (now - lcdHoldStart >= CONFIG_HOLD_MS) {
      lcdHoldStart = 0;
      if (heldBtn >= 0) { hazerButtonUp(heldBtn); heldBtn = -1; }
      pressedBtn = -1;
      return UI_REQUEST_CONFIG;
    }
  } else {
    lcdHoldStart = 0;
  }

  if (hit != heldBtn) {
    if (heldBtn >= 0) hazerButtonUp(heldBtn);
    heldBtn = hit;
    if (hit >= 0) { hazerButtonDown(hit); nextRepeat = now + 450; }
  } else if ((hit == 1 || hit == 2) && now >= nextRepeat) {
    hazerAdjust(hit == 1 ? +1 : -1);
    nextRepeat = now + 100;
  }
  pressedBtn = heldBtn;
  return UI_NONE;
}

// ---------------- drawing ----------------
static void drawButtons(int pressed, uint8_t active) {
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(textdatum_t::middle_center);
  for (int i = 0; i < 6; i++) {
    uint16_t bg = (i == pressed) ? lcd.color565(80, 200, 80)
                : (active & (1 << i)) ? lcd.color565(230, 140, 20)
                : lcd.color565(60, 60, 70);
    lcd.fillRoundRect(BX[i % 3], BY[i / 3], BW, BH, 8, bg);
    lcd.drawRoundRect(BX[i % 3], BY[i / 3], BW, BH, 8, TFT_WHITE);
    lcd.setTextColor(TFT_WHITE, bg);
    lcd.drawString(BL[i], BX[i % 3] + BW / 2, BY[i / 3] + BH / 2);
  }
}

static void drawBars(int level) {
  for (int i = 0; i < 4; i++) {
    int h = 4 + i * 4;
    uint16_t col = (i < level) ? lcd.color565(120, 255, 140) : lcd.color565(15, 60, 130);
    lcd.fillRect(250 + i * 7, 34 - h, 5, h, col);
  }
}

static void drawLcd(const char* l1, const char* l2, const char* info, bool infoRed, bool out, int bars) {
  uint16_t blue = lcd.color565(20, 90, 200);
  lcd.fillRoundRect(8, 8, 304, 88, 8, blue);
  lcd.drawRoundRect(8, 8, 304, 88, 8, TFT_WHITE);
  lcd.setFont(&fonts::Font4);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setTextColor(TFT_WHITE, blue);
  lcd.drawString(l1, 150, 26);
  lcd.drawString(l2, 160, 56);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(infoRed ? lcd.color565(255, 190, 80) : lcd.color565(190, 215, 255), blue);
  lcd.drawString(info, 160, 83);
  drawBars(bars);
  if (out) lcd.fillCircle(296, 22, 7, TFT_RED);
}

static int rssiToBars(int rssi) {
  if (rssi > -55) return 4;
  if (rssi > -65) return 3;
  if (rssi > -75) return 2;
  if (rssi > -85) return 1;
  return 0;
}

// cache of what is currently on screen
static char    c1[28] = "", c2[36] = "", cInfo[48] = "";
static bool    cOut = false, cInfoRed = false;
static int     cBars = -1, cPressed = -2;
static uint8_t cActive = 255;

void uiPanelRender(uint32_t now) {
  if (needFull) {
    lcd.fillScreen(TFT_BLACK);
    c1[0] = c2[0] = cInfo[0] = 0;
    cBars = -1; cPressed = -2; cActive = 255;
    needFull = false;
  }

  RadioStatus rs = radioGetStatus();

  // numeric RSSI is sampled once a second so the display doesn't flicker
  static uint32_t lastSample = 0;
  static int shownRssi = -127;
  if (now - lastSample >= 1000) { lastSample = now; shownRssi = rs.linkUp ? rs.rssi : -127; }

  char l1[28], l2[36], info[48];
  info[0] = 0;
  bool infoRed = false;
  const char* err = radioError();
  int page = hazerPage();

  if (page == PG_MAIN) {
    snprintf(l2, sizeof(l2), "Vol %d%%  Fan %d%%", hazerVolPct(), hazerFanPct());
    if (err) {
      snprintf(l1, sizeof(l1), "Radio Error");
      snprintf(l2, sizeof(l2), "%s", err);
    } else if (!rs.linkUp) {
      snprintf(l1, sizeof(l1), "No Link");
      snprintf(l2, sizeof(l2), "Searching for receiver");
    } else if (rs.flags & HZ_FLAG_CONFIG) {
      snprintf(l1, sizeof(l1), "Rx Config Mode");
      snprintf(l2, sizeof(l2), "Haze disabled");
    } else if (rs.flags & HZ_FLAG_FAILSAFE) {
      snprintf(l1, sizeof(l1), "Rx Fail-safe");
      snprintf(l2, sizeof(l2), "Waiting for data");
    } else if (hazerManualHeld()) {
      snprintf(l1, sizeof(l1), "Manual Haze");
    } else if (hazerOnActive()) {
      snprintf(l1, sizeof(l1), "Haze On");
    } else if (hazerTimerActive()) {
      snprintf(l1, sizeof(l1), hazerTimerPhase() ? "Timer Haze" : "Timer Wait");
      snprintf(l2, sizeof(l2), "%lds", hazerTimerRemaining(now));
    } else {
      snprintf(l1, sizeof(l1), "Ready Haze");
    }
  } else {
    switch (page) {
      case PG_INTERVAL: snprintf(l1, sizeof(l1), "Interval Set"); snprintf(l2, sizeof(l2), "%d s", hazerInterval()); break;
      case PG_DURATION: snprintf(l1, sizeof(l1), "Duration Set"); snprintf(l2, sizeof(l2), "%d s", hazerDuration()); break;
      case PG_VOLUME:   snprintf(l1, sizeof(l1), "Volume Set");   snprintf(l2, sizeof(l2), "%d%%", hazerVolPct()); break;
      default:          snprintf(l1, sizeof(l1), "FanSpeed Set"); snprintf(l2, sizeof(l2), "%d%%", hazerFanPct()); break;
    }
  }

  // info line: receiver version, signal, value echo, fail-safe flag
  if (rs.linkUp) {
    int echoE = rs.echo[HZV_EMIT] >= 128 ? 1 : 0;
    int echoF = (rs.echo[HZV_FAN] * 100 + 127) / 255;
    int echoV = (rs.echo[HZV_VOL] * 100 + 127) / 255;
    if (rs.flags & HZ_FLAG_FAILSAFE) {
      snprintf(info, sizeof(info), "FAILSAFE  RX v%u.%u  %ddBm", rs.fwMajor, rs.fwMinor, shownRssi);
      infoRed = true;
    } else {
      snprintf(info, sizeof(info), "RX v%u.%u %ddBm  Echo E%d F%d V%d",
               rs.fwMajor, rs.fwMinor, shownRssi, echoE, echoF, echoV);
    }
  } else {
    snprintf(info, sizeof(info), "RX: no link");
    infoRed = true;
  }

  bool out = hazerEmit() && rs.linkUp;
  int bars = rs.linkUp ? rssiToBars(shownRssi) : 0;

  uint8_t active = 0;
  if (hazerTimerActive())  active |= (1 << 3);
  if (hazerOnActive())     active |= (1 << 4);
  if (hazerManualHeld())   active |= (1 << 5);

  if (strcmp(l1, c1) || strcmp(l2, c2) || strcmp(info, cInfo) ||
      out != cOut || infoRed != cInfoRed || bars != cBars) {
    drawLcd(l1, l2, info, infoRed, out, bars);
    strcpy(c1, l1); strcpy(c2, l2); strcpy(cInfo, info);
    cOut = out; cInfoRed = infoRed; cBars = bars;
  }
  if (pressedBtn != cPressed || active != cActive) {
    drawButtons(pressedBtn, active);
    cPressed = pressedBtn; cActive = active;
  }
}
