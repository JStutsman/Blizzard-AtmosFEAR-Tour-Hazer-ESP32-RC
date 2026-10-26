// UiConfig.cpp  (Hazer-TX)
// Flow: hidden long-press -> 4-digit PIN (lockout after 3 wrong tries) -> confirm
// -> stop haze and send ENTER_CONFIG until the receiver reports config mode.
// Tapping the info line opens a "take control" screen: pick RX 1, RX 2 or both, then
// TAKE_CONTROL is sent until they accept; taken receivers are also selected here.
// HZ_DEBUG_PIN on the same keypad opens a live debug/info screen instead
// (RSSI shown as "here/at RX": reply strength at the remote / our packets at the receiver).
#include <Arduino.h>
#include <esp_wifi.h>
#include "Config.h"
#include "HazerProtocol.h"
#include "Display.h"
#include "HazerState.h"
#include "Radio.h"
#include "FixtureTable.h"
#include "UiPanel.h"
#include "UiConfig.h"

enum UcState { UC_OFF, UC_PIN, UC_CONFIRM, UC_SENDING, UC_RESULT, UC_DEBUG, UC_TAKE_CONFIRM, UC_TAKING, UC_TAKE_RESULT };

static UcState  st = UC_OFF;
static bool     dirty = false;
static bool     wasTouched = false;
static char     entered[5] = "";
static uint8_t  nEntered = 0;
static uint8_t  wrongTries = 0;
static bool     lockActive = false;
static uint32_t lockUntil = 0;
static int      lastLockSec = -1;
static uint32_t lastAct = 0, stateStart = 0, lastSend = 0;
static bool     resultOk = false;
static const char* pinMsg = "";
static bool     debugRefresh = false;      // redraw the debug values without clearing the screen
static uint8_t  takeMask = 0;              // receivers to take control of

// ---------------- keypad layout ----------------
static const char* KEYS[12] = {"1","2","3","4","5","6","7","8","9","CLR","0","BACK"};
static int keyAt(int x, int y) {
  for (int i = 0; i < 12; i++) {
    int kx = 10 + (i % 3) * 104, ky = 58 + (i / 3) * 44;
    if (x >= kx && x < kx + 96 && y >= ky && y < ky + 40) return i;
  }
  return -1;
}

static void finish() {
  st = UC_OFF;
  uiPanelInvalidate();
}

static void drawBtn(int x, int y, int w, int h, const char* label, uint16_t bg) {
  lcd.fillRoundRect(x, y, w, h, 8, bg);
  lcd.drawRoundRect(x, y, w, h, 8, TFT_WHITE);
  lcd.setFont(&fonts::Font4);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setTextColor(TFT_WHITE, bg);
  lcd.drawString(label, x + w / 2, y + h / 2);
}

// ---------------- entry ----------------
void uiConfigStart() {
  st = UC_PIN;
  nEntered = 0; entered[0] = 0;
  pinMsg = "";
  lastAct = millis();
  lastLockSec = -1;
  wasTouched = true;          // ignore the touch that opened this screen until released
  dirty = true;
}

// take-control choice buttons: RX 1, RX 2, BOTH (masks 1, 2, 3)
static const int TAKE_BX[3] = {10, 112, 214};
static const int TAKE_BY = 110, TAKE_BW = 96, TAKE_BH = 50;

void uiTakeoverStart() {
  takeMask = 0;
  st = UC_TAKE_CONFIRM;
  lastAct = millis();
  wasTouched = true;
  dirty = true;
}

static bool takeoverDone() {               // every receiver in takeMask now answers us as owner
  for (int i = 0; i < RECEIVER_COUNT; i++) {
    if (!(takeMask & (1 << i))) continue;
    RadioStatus rs = radioGetStatus(i);
    if (!rs.linkUp || (rs.flags & HZ_FLAG_BUSY)) return false;
  }
  return true;
}

static void takeNames(char* buf, size_t len) {   // "RX1" or "RX1 + RX2"
  buf[0] = 0;
  size_t n = 0;
  for (int i = 0; i < RECEIVER_COUNT && n < len; i++)
    if (takeMask & (1 << i)) n += snprintf(buf + n, len - n, "%s%s", n ? " + " : "", RECEIVERS[i].name);
}

bool uiConfigActive()  { return st != UC_OFF; }
bool uiConfigSending() { return st == UC_SENDING; }

static void handlePinKey(int k, uint32_t now) {
  if (k == 11) { finish(); return; }                    // BACK
  if (k == 9)  { nEntered = 0; entered[0] = 0; pinMsg = ""; dirty = true; return; }  // CLR

  char d = (k == 10) ? '0' : (char)('1' + k);
  if (nEntered < 4) {
    entered[nEntered++] = d;
    entered[nEntered] = 0;
    dirty = true;
  }
  if (nEntered == 4) {
    if (strcmp(entered, HZ_CONFIG_PIN) == 0) {
      wrongTries = 0;
      pinMsg = "";
      st = UC_CONFIRM;
    } else if (strcmp(entered, HZ_DEBUG_PIN) == 0) {
      pinMsg = "";
      st = UC_DEBUG;
    } else {
      wrongTries++;
      pinMsg = "Wrong PIN";
      if (wrongTries >= PIN_LOCKOUT_TRIES) {
        lockActive = true;
        lockUntil = now + PIN_LOCKOUT_MS;
        lastLockSec = -1;
      }
    }
    nEntered = 0; entered[0] = 0;
    dirty = true;
  }
}

// ---------------- update ----------------
void uiConfigUpdate(bool touched, int32_t tx, int32_t ty, uint32_t now) {
  bool tap = touched && !wasTouched;
  wasTouched = touched;
  if (tap) lastAct = now;

  switch (st) {
    case UC_PIN: {
      bool locked = lockActive && (int32_t)(lockUntil - now) > 0;
      if (lockActive && !locked) { lockActive = false; wrongTries = 0; pinMsg = ""; dirty = true; }
      int secs = locked ? (int)((lockUntil - now + 999) / 1000) : 0;
      if (secs != lastLockSec) { lastLockSec = secs; dirty = true; }

      if (now - lastAct > PIN_IDLE_TIMEOUT_MS) { finish(); return; }
      if (tap && !locked) {
        int k = keyAt(tx, ty);
        if (k >= 0) handlePinKey(k, now);
      }
      break;
    }
    case UC_CONFIRM:
      if (now - lastAct > PIN_IDLE_TIMEOUT_MS) { finish(); return; }
      if (tap && ty >= 170 && ty < 226) {
        if (tx >= 10 && tx < 150) {                       // CANCEL
          finish();
        } else if (tx >= 170 && tx < 310) {               // CONFIRM
          hazerStopAll();
          st = UC_SENDING;
          stateStart = now;
          lastSend = 0;
          dirty = true;
        }
      }
      break;
    case UC_SENDING: {
      if (now - lastSend >= 100) { lastSend = now; radioSendCommand(HZ_CMD_ENTER_CONFIG, 0); }
      bool allInConfig = true;                          // every selected receiver confirmed
      for (int i = 0; i < RECEIVER_COUNT; i++) {
        if (!(radioTargets() & (1 << i))) continue;
        RadioStatus rs = radioGetStatus(i);
        if (!rs.linkUp || !(rs.flags & HZ_FLAG_CONFIG)) allInConfig = false;
      }
      if (allInConfig) {
        resultOk = true; st = UC_RESULT; stateStart = now; dirty = true;
      } else if (now - stateStart > 4000) {
        resultOk = false; st = UC_RESULT; stateStart = now; dirty = true;
      }
      break;
    }
    case UC_RESULT:
      if (tap || now - stateStart > 8000) finish();
      break;
    case UC_TAKE_CONFIRM:
      if (now - lastAct > PIN_IDLE_TIMEOUT_MS) { finish(); return; }
      if (!tap) break;
      if (ty >= 176 && ty < 226) { finish(); return; }    // CANCEL
      if (ty >= TAKE_BY && ty < TAKE_BY + TAKE_BH) {
        for (int b = 0; b < 3; b++) {
          if (tx < TAKE_BX[b] || tx >= TAKE_BX[b] + TAKE_BW) continue;
          takeMask = b + 1;                               // RX 1, RX 2, both
          hazerAddTargets(takeMask);                      // keep sending to it, or control lapses
          radioSetTargets(hazerTargetMask());
          st = UC_TAKING;
          stateStart = now;
          lastSend = 0;
          dirty = true;
        }
      }
      break;
    case UC_TAKING:
      if (now - lastSend >= 100) {
        lastSend = now;
        for (int i = 0; i < RECEIVER_COUNT; i++)
          if (takeMask & (1 << i)) radioSendCommandTo(i, HZ_CMD_TAKE_CONTROL, 0);
      }
      if (takeoverDone() || now - stateStart > 2000) {
        resultOk = takeoverDone(); st = UC_TAKE_RESULT; stateStart = now; dirty = true;
      }
      break;
    case UC_TAKE_RESULT:
      if (tap || now - stateStart > (resultOk ? 1500UL : 5000UL)) finish();
      break;
    case UC_DEBUG:
      if (tap) { finish(); return; }
      if (now - lastSend >= 500) { lastSend = now; debugRefresh = true; }   // live values
      break;
    default:
      break;
  }
}

// ---------------- render ----------------
static void drawTitle(const char* t) {
  lcd.setFont(&fonts::Font4);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString(t, 160, 16);
}

// one left-aligned debug line; padding overwrites the previous text without a full clear
static void debugLine(int row, uint16_t col, const char* fmt, ...) {
  char buf[64];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  lcd.setTextColor(col, TFT_BLACK);
  lcd.drawString(buf, 6, 34 + row * 16);
}

static void renderDebug(uint32_t now) {
  const uint16_t W = TFT_WHITE, G = lcd.color565(150, 150, 160);
  const uint16_t OK = lcd.color565(120, 255, 140), BAD = lcd.color565(255, 190, 80);
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setTextPadding(308);

  uint8_t own[6];
  esp_wifi_get_mac(WIFI_IF_STA, own);
  uint32_t up = now / 1000;
  uint8_t mask = radioTargets();
  const char* err = radioError();
  debugLine(0, W, "TX v%d.%d  %02X:%02X:%02X:%02X:%02X:%02X  ch %d", FW_VERSION_MAJOR, FW_VERSION_MINOR,
            own[0], own[1], own[2], own[3], own[4], own[5], RADIO_CHANNEL);
  debugLine(1, G, "Up %lu:%02lu:%02lu  Heap %uk  Send %s%s%s", up / 3600, (up / 60) % 60, up % 60,
            ESP.getFreeHeap() / 1024, (mask & 1) ? "RX1 " : "", (mask & 2) ? "RX2 " : "", err ? err : "");
  debugLine(2, G, "Out  Emit %d  Fan %d%%  Vol %d%%  every %dms", hazerEmit() ? 1 : 0,
            hazerFanPct(), hazerVolPct(), SEND_INTERVAL_MS);

  for (int i = 0; i < RECEIVER_COUNT && i < 3; i++) {
    int row = 3 + i * 4;
    const uint8_t* m = RECEIVERS[i].mac;
    RadioStatus r = radioGetStatus(i);
    bool sel = mask & (1 << i);
    debugLine(row, W, "%s  %02X:%02X:%02X:%02X:%02X:%02X  %s", RECEIVERS[i].name,
              m[0], m[1], m[2], m[3], m[4], m[5], sel ? "[selected]" : "");
    if (r.ageMs == 0xFFFFFFFFUL) {
      debugLine(row + 1, BAD, "  never heard from");
      debugLine(row + 2, G, "");
    } else {
      debugLine(row + 1, r.linkUp ? OK : BAD, "  %s  v%u.%u  base %u  last %lums",
                r.linkUp ? "LINK UP" : "NO LINK", r.fwMajor, r.fwMinor, r.base, r.ageMs);
      debugLine(row + 2, G, "  RSSI %d/%d dBm  Echo E%d F%d V%d%s%s%s",
                r.rssi, r.rxRssi, r.echo[HZV_EMIT] >= 128 ? 1 : 0,
                (r.echo[HZV_FAN] * 100 + 127) / 255, (r.echo[HZV_VOL] * 100 + 127) / 255,
                (r.flags & HZ_FLAG_FAILSAFE) ? " FS" : "", (r.flags & HZ_FLAG_CONFIG) ? " CFG" : "",
                (r.flags & HZ_FLAG_IDENTIFY) ? " ID" : (r.flags & HZ_FLAG_BUSY) ? " BUSY" : "");
    }
  }
  lcd.setTextPadding(0);
}

void uiConfigRender(uint32_t now) {
  if (st == UC_DEBUG && debugRefresh && !dirty) {
    debugRefresh = false;
    renderDebug(now);
    return;
  }
  if (!dirty) return;
  dirty = false;
  lcd.fillScreen(TFT_BLACK);

  switch (st) {
    case UC_PIN: {
      bool locked = lockActive && (int32_t)(lockUntil - now) > 0;
      char title[32];
      if (locked)              snprintf(title, sizeof(title), "Locked %ds", lastLockSec);
      else if (pinMsg[0])      snprintf(title, sizeof(title), "%s", pinMsg);
      else                     snprintf(title, sizeof(title), "Enter PIN");
      drawTitle(title);
      for (int i = 0; i < 4; i++) {
        int cx = 124 + i * 24;
        if (i < nEntered) lcd.fillCircle(cx, 42, 6, TFT_WHITE);
        else              lcd.drawCircle(cx, 42, 6, TFT_WHITE);
      }
      for (int i = 0; i < 12; i++) {
        uint16_t bg = locked ? lcd.color565(40, 40, 45) : lcd.color565(60, 60, 70);
        drawBtn(10 + (i % 3) * 104, 58 + (i / 3) * 44, 96, 40, KEYS[i], bg);
      }
      break;
    }
    case UC_CONFIRM: {
      drawTitle("Enter config mode?");
      lcd.setFont(&fonts::Font2);
      lcd.setTextDatum(textdatum_t::middle_center);
      lcd.setTextColor(TFT_WHITE, TFT_BLACK);
      lcd.drawString("The receiver will stop the haze", 160, 70);
      lcd.drawString("machine and start its setup WiFi.", 160, 92);
      lcd.drawString("It returns to normal after 10 min", 160, 114);
      lcd.drawString("without activity.", 160, 136);
      drawBtn(10, 170, 140, 56, "CANCEL", lcd.color565(60, 60, 70));
      drawBtn(170, 170, 140, 56, "CONFIRM", lcd.color565(30, 140, 60));
      break;
    }
    case UC_SENDING: {
      drawTitle("Contacting receiver");
      lcd.setFont(&fonts::Font2);
      lcd.setTextDatum(textdatum_t::middle_center);
      lcd.setTextColor(TFT_WHITE, TFT_BLACK);
      lcd.drawString("Sending config command...", 160, 120);
      break;
    }
    case UC_TAKE_CONFIRM: {
      drawTitle("Take control");
      lcd.setFont(&fonts::Font2);
      lcd.setTextDatum(textdatum_t::middle_center);
      for (int i = 0; i < RECEIVER_COUNT && i < 2; i++) {
        RadioStatus rs = radioGetStatus(i);
        bool mine = rs.linkUp && !(rs.flags & HZ_FLAG_BUSY) && (radioTargets() & (1 << i));
        const char* state = !rs.linkUp ? "no link"
                          : (rs.flags & HZ_FLAG_BUSY) ? "in use by another remote"
                          : mine ? "controlled by this remote" : "free";
        char line[48];
        snprintf(line, sizeof(line), "%s: %s", RECEIVERS[i].name, state);
        lcd.setTextColor((rs.flags & HZ_FLAG_BUSY) && rs.linkUp ? lcd.color565(255, 190, 80) : TFT_WHITE, TFT_BLACK);
        lcd.drawString(line, 160, 44 + i * 20);
      }
      lcd.setTextColor(lcd.color565(150, 150, 160), TFT_BLACK);
      lcd.drawString("This remote's haze settings apply at once.", 160, 94);
      static const char* TAKE_LABELS[3] = {"RX 1", "RX 2", "BOTH"};
      for (int b = 0; b < 3; b++)
        drawBtn(TAKE_BX[b], TAKE_BY, TAKE_BW, TAKE_BH, TAKE_LABELS[b], lcd.color565(200, 110, 20));
      drawBtn(10, 176, 300, 50, "CANCEL", lcd.color565(60, 60, 70));
      break;
    }
    case UC_TAKING:
      drawTitle("Taking control");
      lcd.setFont(&fonts::Font2);
      lcd.setTextDatum(textdatum_t::middle_center);
      lcd.setTextColor(TFT_WHITE, TFT_BLACK);
      lcd.drawString("Waiting for the receiver...", 160, 120);
      break;
    case UC_TAKE_RESULT: {
      char names[24];
      takeNames(names, sizeof(names));
      drawTitle(resultOk ? "Control taken" : "No response");
      lcd.setFont(&fonts::Font2);
      lcd.setTextDatum(textdatum_t::middle_center);
      lcd.setTextColor(TFT_WHITE, TFT_BLACK);
      lcd.drawString(names, 160, 80);
      if (!resultOk) lcd.drawString("The receiver did not accept. Try again.", 160, 110);
      lcd.drawString("Tap to close", 160, 210);
      break;
    }
    case UC_DEBUG:
      drawTitle("Debug Info");
      renderDebug(now);
      lcd.setFont(&fonts::Font2);
      lcd.setTextDatum(textdatum_t::middle_center);
      lcd.setTextColor(lcd.color565(150, 150, 160), TFT_BLACK);
      lcd.drawString("Tap to close", 160, 230);
      break;
    case UC_RESULT: {
      drawTitle(resultOk ? "Receiver in config mode" : "No response");
      lcd.setFont(&fonts::Font2);
      lcd.setTextDatum(textdatum_t::middle_center);
      lcd.setTextColor(TFT_WHITE, TFT_BLACK);
      if (resultOk) {
        lcd.drawString("Join the Hazer-RX-Setup WiFi network", 160, 80);
        lcd.drawString("then open  192.168.4.1", 160, 104);
        lcd.drawString("Haze is disabled until you exit.", 160, 140);
      } else {
        lcd.drawString("The receiver did not confirm.", 160, 90);
        lcd.drawString("Check power and range, then retry.", 160, 114);
      }
      lcd.drawString("Tap to close", 160, 210);
      break;
    }
    default: break;
  }
}
