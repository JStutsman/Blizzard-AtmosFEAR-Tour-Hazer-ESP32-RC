// UiConfig.cpp  (Hazer-TX)
// Flow: hidden long-press -> 4-digit PIN (lockout after 3 wrong tries) -> confirm
// -> stop haze and send ENTER_CONFIG until the receiver reports config mode.
#include <Arduino.h>
#include "Config.h"
#include "HazerProtocol.h"
#include "Display.h"
#include "HazerState.h"
#include "Radio.h"
#include "FixtureTable.h"
#include "UiPanel.h"
#include "UiConfig.h"

enum UcState { UC_OFF, UC_PIN, UC_CONFIRM, UC_SENDING, UC_RESULT };

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

void uiConfigRender(uint32_t now) {
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
