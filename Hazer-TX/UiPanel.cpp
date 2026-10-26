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
#include "FixtureTable.h"
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
  } else if ((hit == 1 || hit == 2) && hazerPage() != PG_RECEIVER && now >= nextRepeat) {
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

  // combine the status of the selected receivers
  uint8_t     mask = radioTargets();
  RadioStatus rs[RECEIVER_COUNT];
  int  nSel = 0, firstSel = -1, nLinked = 0, missing = -1;
  bool anyConfig = false, anyFailsafe = false;
  for (int i = 0; i < RECEIVER_COUNT; i++) {
    rs[i] = radioGetStatus(i);
    if (!(mask & (1 << i))) continue;
    nSel++;
    if (firstSel < 0) firstSel = i;
    if (rs[i].linkUp) {
      nLinked++;
      if (rs[i].flags & HZ_FLAG_CONFIG)   anyConfig = true;
      if (rs[i].flags & HZ_FLAG_FAILSAFE) anyFailsafe = true;
    } else {
      missing = i;
    }
  }
  bool linkUp = nSel > 0 && nLinked == nSel;

  // numeric RSSI is sampled once a second so the display doesn't flicker
  static uint32_t lastSample = 0;
  static int shownRssi[RECEIVER_COUNT];
  if (now - lastSample >= 1000) {
    lastSample = now;
    for (int i = 0; i < RECEIVER_COUNT; i++) shownRssi[i] = rs[i].linkUp ? rs[i].rssi : -127;
  }
  int weakest = 0;                         // weakest linked receiver drives the bars
  for (int i = 0; i < RECEIVER_COUNT; i++)
    if ((mask & (1 << i)) && rs[i].linkUp && (weakest == 0 || shownRssi[i] < weakest)) weakest = shownRssi[i];

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
    } else if (nLinked == 0) {             // a partial link is shown on the info line instead
      snprintf(l1, sizeof(l1), "No Link");
      snprintf(l2, sizeof(l2), nSel > 1 ? "Searching for receivers" : "Searching for receiver");
    } else if (anyConfig) {
      snprintf(l1, sizeof(l1), "Rx Config Mode");
      snprintf(l2, sizeof(l2), "Haze disabled");
    } else if (anyFailsafe) {
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
      case PG_FAN:      snprintf(l1, sizeof(l1), "FanSpeed Set"); snprintf(l2, sizeof(l2), "%d%%", hazerFanPct()); break;
      default: {
        static const char* TARGET_NAMES[] = {"", "RX 1", "RX 2", "RX 1 + 2"};
        snprintf(l1, sizeof(l1), "Receiver Set");
        snprintf(l2, sizeof(l2), "%s", TARGET_NAMES[hazerTargetMask() & 3]);
        break;
      }
    }
  }

  // info line: receiver version, signal, value echo, fail-safe flag
  if (nSel == 1) {
    const RadioStatus& r = rs[firstSel];
    const char* name = RECEIVERS[firstSel].name;
    if (!r.linkUp) {
      snprintf(info, sizeof(info), "%s: no link", name);
      infoRed = true;
    } else if (r.flags & HZ_FLAG_FAILSAFE) {
      snprintf(info, sizeof(info), "FAILSAFE  %s v%u.%u  %ddBm", name, r.fwMajor, r.fwMinor, shownRssi[firstSel]);
      infoRed = true;
    } else {
      int echoE = r.echo[HZV_EMIT] >= 128 ? 1 : 0;
      int echoF = (r.echo[HZV_FAN] * 100 + 127) / 255;
      int echoV = (r.echo[HZV_VOL] * 100 + 127) / 255;
      snprintf(info, sizeof(info), "%s v%u.%u %ddBm  Echo E%d F%d V%d",
               name, r.fwMajor, r.fwMinor, shownRssi[firstSel], echoE, echoF, echoV);
    }
  } else {                                 // several receivers: one short entry each
    size_t n = 0;
    bool partial = nLinked > 0 && !linkUp;
    if (partial) n += snprintf(info, sizeof(info), "No Link - searching %s  ", RECEIVERS[missing].name);
    for (int i = 0; i < RECEIVER_COUNT && n < sizeof(info); i++) {
      if (!(mask & (1 << i))) continue;
      const char* name = RECEIVERS[i].name;
      if (!rs[i].linkUp) { if (!partial)       n += snprintf(info + n, sizeof(info) - n, "%s no link  ", name); }
      else if (rs[i].flags & HZ_FLAG_FAILSAFE) n += snprintf(info + n, sizeof(info) - n, "%s FS %ddBm  ", name, shownRssi[i]);
      else                                     n += snprintf(info + n, sizeof(info) - n, "%s %ddBm  ", name, shownRssi[i]);
    }
    infoRed = !linkUp || anyFailsafe;
  }

  bool out = hazerEmit() && nLinked > 0;
  int bars = nLinked > 0 ? rssiToBars(weakest) : 0;

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
