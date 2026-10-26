// HazerState.cpp  (Hazer-TX)
// Mimics the AtmosFEAR Tour HZ front panel: FUNCTION cycles menu pages, UP/DOWN change
// values, TIMER runs interval/duration cycles, START/STOP latches haze, MANUAL = hold.
// The timer runs here (on the remote) because the hazer locks its own buttons while
// it receives DMX. The remote's own settings (receivers, DMX addresses, display) live
// here too but are edited on the Settings screen, not on FUNCTION pages.
#include <Arduino.h>
#include <Preferences.h>
#include "HazerState.h"

enum { HM_IDLE, HM_ON, HM_TIMER };

static Preferences prefs;
static int      mode = HM_IDLE;
static int      page = PG_MAIN;
static int      interval = 30, duration = 5;     // seconds, 0-200
static int      volPct = 50, fanPct = 50;        // 1-100
static int      target = 1;                      // receiver mask 1-3 (RX1, RX2, both)
static bool     darkMode = false;
static int      backlight[2] = {100, 30};        // % for normal / dark mode
static int      dmxWant[DMX_RX_COUNT];           // per receiver 1-510, 0 = not set from here
static bool     manualHeld = false;
static bool     dirty = false;
static int      timerPhase = 0;                  // 0 = interval wait, 1 = hazing
static uint32_t phaseEnd = 0;

static void saveSettings() {
  prefs.putInt("interval", interval);
  prefs.putInt("duration", duration);
  prefs.putInt("vol", volPct);
  prefs.putInt("fan", fanPct);
  prefs.putInt("target", target);
  prefs.putBool("dark", darkMode);
  prefs.putInt("bl", backlight[0]);
  prefs.putInt("bldark", backlight[1]);
  for (int i = 0; i < DMX_RX_COUNT; i++) prefs.putInt(i ? "base1" : "base0", dmxWant[i]);
  dirty = false;
}

void hazerBegin() {
  prefs.begin("hazertx", false);
  interval = prefs.getInt("interval", 30);
  duration = prefs.getInt("duration", 5);
  volPct   = prefs.getInt("vol", 50);
  fanPct   = prefs.getInt("fan", 50);
  target   = prefs.getInt("target", 1);
  darkMode = prefs.getBool("dark", false);
  backlight[0] = constrain(prefs.getInt("bl", 100), 5, 100);
  backlight[1] = constrain(prefs.getInt("bldark", 30), 5, 100);
  interval = constrain(interval, 0, 200);
  duration = constrain(duration, 0, 200);
  volPct   = constrain(volPct, 1, 100);
  fanPct   = constrain(fanPct, 1, 100);
  target   = constrain(target, 1, 3);
  for (int i = 0; i < DMX_RX_COUNT; i++) {
    dmxWant[i] = prefs.getInt(i ? "base1" : "base0", 0);
    if (dmxWant[i] < 1 || dmxWant[i] > 510) dmxWant[i] = 0;
  }
}

void hazerAdjust(int d) {
  switch (page) {
    case PG_INTERVAL: interval = constrain(interval + d, 0, 200); break;
    case PG_DURATION: duration = constrain(duration + d, 0, 200); break;
    case PG_VOLUME:   volPct   = constrain(volPct + d, 1, 100);   break;
    case PG_FAN:      fanPct   = constrain(fanPct + d, 1, 100);   break;
    default: return;
  }
  dirty = true;
}

void hazerSetPageValue(int pg, int v) {
  switch (pg) {
    case PG_INTERVAL: interval = constrain(v, 0, 200); break;
    case PG_DURATION: duration = constrain(v, 0, 200); break;
    case PG_VOLUME:   volPct   = constrain(v, 1, 100); break;
    case PG_FAN:      fanPct   = constrain(v, 1, 100); break;
    default: return;
  }
  saveSettings();
}

void hazerButtonDown(int b) {
  switch (b) {
    case 0:  // FUNCTION
      if (dirty) saveSettings();
      page = (page + 1) % PG_COUNT;
      break;
    case 1: hazerAdjust(+1); break;
    case 2: hazerAdjust(-1); break;
    case 3:  // TIMER
      if (mode == HM_TIMER) {
        mode = HM_IDLE;
      } else {
        mode = HM_TIMER;
        timerPhase = 0;
        phaseEnd = millis() + (uint32_t)interval * 1000UL;
      }
      break;
    case 4:  // START/STOP
      mode = (mode == HM_IDLE) ? HM_ON : HM_IDLE;
      break;
    case 5: manualHeld = true; break;
  }
}

void hazerButtonUp(int b) {
  if (b == 5) manualHeld = false;
}

void hazerUpdate(uint32_t now) {
  if (mode != HM_TIMER) return;
  if ((int32_t)(now - phaseEnd) >= 0) {
    timerPhase ^= 1;
    phaseEnd = now + (uint32_t)(timerPhase ? duration : interval) * 1000UL;
  }
}

void hazerStopAll() {
  mode = HM_IDLE;
  manualHeld = false;
}

void hazerStart() { mode = HM_ON; }

void hazerSetTimer(bool on) {
  if (on == (mode == HM_TIMER)) return;
  if (on) {
    mode = HM_TIMER;
    timerPhase = 0;
    phaseEnd = millis() + (uint32_t)interval * 1000UL;
  } else {
    mode = HM_IDLE;
  }
}

void hazerSetManual(bool on) { manualHeld = on; }

void hazerSetPage(int pg) {
  if (pg < 0 || pg >= PG_COUNT) return;
  if (dirty) saveSettings();
  page = pg;
}

bool hazerEmit() {
  return mode == HM_ON || manualHeld || (mode == HM_TIMER && timerPhase == 1);
}
uint8_t hazerFanPct()        { return (uint8_t)fanPct; }
uint8_t hazerVolPct()        { return (uint8_t)volPct; }
int     hazerPage()          { return page; }
int     hazerInterval()      { return interval; }
int     hazerDuration()      { return duration; }
bool    hazerTimerActive()   { return mode == HM_TIMER; }
bool    hazerOnActive()      { return mode == HM_ON; }
bool    hazerManualHeld()    { return manualHeld; }
int     hazerTimerPhase()    { return timerPhase; }
uint8_t hazerTargetMask()    { return (uint8_t)target; }
bool    hazerDarkMode()      { return darkMode; }
int     hazerBacklightPct()  { return backlight[darkMode]; }
void hazerAddTargets(uint8_t mask) {
  int t = constrain(target | mask, 1, 3);
  if (t == target) return;
  target = t;
  saveSettings();
}
void    hazerSetTargetMask(uint8_t mask) { target = constrain((int)mask, 1, 3); }
void    hazerSetDarkMode(bool dark)      { darkMode = dark; }
void    hazerSetBacklightPct(int pct)    { backlight[darkMode] = constrain(pct, 5, 100); }
int     hazerDmxWanted(int rx) { return (rx >= 0 && rx < DMX_RX_COUNT) ? dmxWant[rx] : 0; }
void    hazerSetDmxWanted(int rx, int base) {
  if (rx >= 0 && rx < DMX_RX_COUNT && base >= 1 && base <= 510) dmxWant[rx] = base;
}
void    hazerSaveSettings()  { saveSettings(); }
long hazerTimerRemaining(uint32_t now) {
  long rem = ((long)(phaseEnd - now) + 999) / 1000;
  return rem < 0 ? 0 : rem;
}
