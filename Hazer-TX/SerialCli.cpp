// SerialCli.cpp  (Hazer-TX)
// Line-based commands on the USB serial port, mirroring the touchscreen: hazer control,
// FUNCTION pages, Settings, take control, receiver setup mode and the debug screen.
// Typed characters are echoed (most serial monitors don't echo locally).
#include <Arduino.h>
#include <esp_wifi.h>
#include "Config.h"
#include "HazerProtocol.h"
#include "HazerState.h"
#include "Radio.h"
#include "FixtureTable.h"
#include "UiPanel.h"
#include "UiConfig.h"
#include "UiSettings.h"
#include "UiNumpad.h"
#include "SerialCli.h"

extern const char* const BUILD_STAMP;

static char     line[64];
static uint8_t  len = 0;
static bool     lastWasCR = false;

// "take" and "config" run for a moment after the command; their state lives here
static uint8_t  takeMask = 0;
static bool     cfgActive = false;
static uint32_t opStart = 0, opLastSend = 0;
static uint32_t watchMs = 0, lastWatch = 0;     // "watch": periodic status
static uint32_t manualUntil = 0;                // "manual <seconds>": release time, 0 = none

static const char* PAGE_NAMES[PG_COUNT] = {"main", "interval", "duration", "volume", "fan"};

// ---------------- helpers ----------------
static bool eq(const char* a, const char* b) { return a && strcasecmp(a, b) == 0; }

static bool toInt(const char* s, int* v) {
  if (!s || !*s) return false;
  char* end;
  long n = strtol(s, &end, 10);
  if (*end) return false;
  *v = (int)n;
  return true;
}

static bool inRange(const char* what, const char* arg, int lo, int hi, int* v) {
  if (toInt(arg, v) && *v >= lo && *v <= hi) return true;
  Serial.printf("! %s must be %d-%d\n", what, lo, hi);
  return false;
}

// "1", "2", "both" / "rx1", "rx2" -> receiver mask, 0 if not valid
static uint8_t rxMask(const char* s) {
  if (eq(s, "1") || eq(s, "rx1")) return 1;
  if (eq(s, "2") || eq(s, "rx2")) return 2;
  if (eq(s, "both") || eq(s, "all") || eq(s, "1+2")) return 3;
  return 0;
}

static const char* targetName(uint8_t m) {
  static const char* N[] = {"none", "RX 1", "RX 2", "RX 1 + 2"};
  return N[m & 3];
}

static const char* stateName() {
  if (hazerManualHeld())  return "Manual Haze";
  if (hazerOnActive())    return "Haze On";
  if (hazerTimerActive()) return hazerTimerPhase() ? "Timer Haze" : "Timer Wait";
  return "Idle";
}

static const char* activeScreen() {
  if (uiNumpadActive())   return "number pad";
  if (uiConfigActive())   return "PIN / config / take control / debug";
  if (uiSettingsActive()) return "settings";
  return "panel";
}

// ---------------- output ----------------
static void printReceiver(int i, bool verbose) {
  RadioStatus r = radioGetStatus(i);
  const uint8_t* m = RECEIVERS[i].mac;
  bool sel  = radioTargets() & (1 << i);
  bool busy = r.linkUp && (r.flags & HZ_FLAG_BUSY);
  Serial.printf("  %s %02X:%02X:%02X:%02X:%02X:%02X %s  ", RECEIVERS[i].name,
                m[0], m[1], m[2], m[3], m[4], m[5], sel ? "[selected]" : "          ");
  if (r.ageMs == 0xFFFFFFFFUL) { Serial.println("never heard from"); return; }
  Serial.printf("%s  fw v%u.%u  DMX %u", !r.linkUp ? "NO LINK" : busy ? "IN USE " : "LINKED ",
                r.fwMajor, r.fwMinor, r.base);
  if (hazerDmxWanted(i) && hazerDmxWanted(i) != r.base) Serial.printf(" (set %d)", hazerDmxWanted(i));
  Serial.printf("  %d dBm", r.rssi);
  if (verbose) {
    Serial.printf(" / %d at RX  last %lums  echo E%d F%d V%d", r.rxRssi, r.ageMs,
                  r.echo[HZV_EMIT] >= 128 ? 1 : 0,
                  (r.echo[HZV_FAN] * 100 + 127) / 255, (r.echo[HZV_VOL] * 100 + 127) / 255);
  }
  Serial.printf("%s%s%s%s\n", (r.flags & HZ_FLAG_FAILSAFE) ? "  FAILSAFE" : "",
                (r.flags & HZ_FLAG_CONFIG) ? "  CONFIG" : "", (r.flags & HZ_FLAG_IDENTIFY) ? "  IDENTIFY" : "",
                busy ? "  (another remote has control)" : "");
}

static void printStatus(bool verbose) {
  uint32_t now = millis();
  Serial.printf("State: %s", stateName());
  if (hazerTimerActive()) Serial.printf(" (%lds left)", hazerTimerRemaining(now));
  Serial.printf("   output %s   page: %s\n", hazerEmit() ? "HAZE" : "off", PAGE_NAMES[hazerPage()]);
  Serial.printf("Volume %d%%  Fan %d%%  Interval %ds  Duration %ds\n",
                hazerVolPct(), hazerFanPct(), hazerInterval(), hazerDuration());
  Serial.printf("Receivers: %s   Display: %s   Backlight %d%%\n", targetName(hazerTargetMask()),
                hazerDarkMode() ? "dark" : "normal", hazerBacklightPct());
  if (radioError()) Serial.printf("Radio error: %s\n", radioError());
  for (int i = 0; i < RECEIVER_COUNT; i++) printReceiver(i, verbose);
}

static void printScreen() {
  const char *l1, *l2, *info;
  uiPanelText(&l1, &l2, &info);
  Serial.printf("Screen: %s\n", activeScreen());
  Serial.printf("  | %s\n  | %s\n  | ", l1, l2);
  for (const char* p = info; *p; p++) {          // decode the info line's signal markers
    if (*p == '|') Serial.print("  |  ");
    else if (*p == '#' && p[1]) {
      p++;
      if (*p == 'X')       Serial.print(" [X]");
      else if (*p >= 'a')  Serial.printf(" [%d/4]", *p - 'a');
      else                 Serial.printf(" [%d/4]", *p - '0');
    } else Serial.print(*p);
  }
  Serial.println();
}

static void printDebug() {
  uint8_t own[6];
  esp_wifi_get_mac(WIFI_IF_STA, own);
  uint32_t up = millis() / 1000;
  Serial.printf("TX firmware v%d.%d  built %s\n", FW_VERSION_MAJOR, FW_VERSION_MINOR, BUILD_STAMP);
  Serial.printf("MAC %02X:%02X:%02X:%02X:%02X:%02X  channel %d  up %lu:%02lu:%02lu  heap %uk\n",
                own[0], own[1], own[2], own[3], own[4], own[5], RADIO_CHANNEL,
                up / 3600, (up / 60) % 60, up % 60, ESP.getFreeHeap() / 1024);
  printStatus(true);
}

static void printHelp() {
  Serial.println(
    "Hazer-TX commands (case-insensitive):\n"
    " Hazer\n"
    "  start | stop              haze on (latched) / stop everything\n"
    "  manual on|off|<1-60>      hold MANUAL; a number releases after that many seconds\n"
    "  timer on|off              interval/duration cycles\n"
    "  volume|fan <1-100>        set %          interval|duration <0-200>   set seconds\n"
    "  function | up | down      press a panel button     page <main|interval|duration|volume|fan>\n"
    " Settings (gear)\n"
    "  rx <1|2|both>             receivers to control\n"
    "  dmx <1|2> <1-510>         DMX address of a receiver\n"
    "  display normal|dark       backlight <5-100>\n"
    " Receivers\n"
    "  take <1|2|both>           take control from another remote\n"
    "  config <PIN>              send selected receivers into setup mode\n"
    "  identify <1|2>            blink a receiver's LED\n"
    " Info\n"
    "  status | screen | debug   state / text on the display / debug screen\n"
    "  watch <seconds>|off       print status repeatedly\n"
    "  reboot");
}

// ---------------- long-running operations ----------------
static void opLoop(uint32_t now) {
  if (takeMask) {
    if (now - opLastSend >= 100) {
      opLastSend = now;
      for (int i = 0; i < RECEIVER_COUNT; i++)
        if (takeMask & (1 << i)) radioSendCommandTo(i, HZ_CMD_TAKE_CONTROL, 0);
    }
    bool done = true;
    for (int i = 0; i < RECEIVER_COUNT; i++) {
      if (!(takeMask & (1 << i))) continue;
      RadioStatus rs = radioGetStatus(i);
      if (!rs.linkUp || (rs.flags & HZ_FLAG_BUSY)) done = false;
    }
    if (done || now - opStart > 2000) {
      Serial.printf("%s %s\n", targetName(takeMask), done ? "control taken" : "- no response");
      takeMask = 0;
    }
  }

  if (cfgActive) {
    if (now - opLastSend >= 100) { opLastSend = now; radioSendCommand(HZ_CMD_ENTER_CONFIG, 0); }
    bool all = true;
    for (int i = 0; i < RECEIVER_COUNT; i++) {
      if (!(radioTargets() & (1 << i))) continue;
      RadioStatus rs = radioGetStatus(i);
      if (!rs.linkUp || !(rs.flags & HZ_FLAG_CONFIG)) all = false;
    }
    if (all || now - opStart > 4000) {
      cfgActive = false;
      if (all) Serial.println("Receiver(s) in setup mode: join WiFi Hazer-RX-Setup-..., open 192.168.4.1");
      else     Serial.println("! The receiver did not confirm setup mode");
    }
  }

  if (manualUntil && (int32_t)(now - manualUntil) >= 0) {
    manualUntil = 0;
    hazerSetManual(false);
    Serial.println("manual released");
  }
}

// ---------------- commands ----------------
static void run(char* cmdline) {
  char* argv[4] = {nullptr, nullptr, nullptr, nullptr};
  int argc = 0;
  for (char* t = strtok(cmdline, " \t"); t && argc < 4; t = strtok(nullptr, " \t")) argv[argc++] = t;
  if (!argc) return;
  const char *c = argv[0], *a = argv[1], *b = argv[2];
  int v;

  if (eq(c, "help") || eq(c, "?")) { printHelp(); return; }

  // ---- hazer
  if (eq(c, "start"))  { hazerStart();   Serial.println("Haze on"); return; }
  if (eq(c, "stop"))   { hazerStopAll(); Serial.println("Stopped"); return; }
  if (eq(c, "manual")) {
    if (eq(a, "on"))  { manualUntil = 0; hazerSetManual(true);  Serial.println("Manual haze on (send 'manual off')"); return; }
    if (eq(a, "off")) { manualUntil = 0; hazerSetManual(false); Serial.println("Manual haze off"); return; }
    if (inRange("seconds", a, 1, 60, &v)) {
      hazerSetManual(true);
      manualUntil = millis() + (uint32_t)v * 1000UL;
      Serial.printf("Manual haze for %ds\n", v);
    }
    return;
  }
  if (eq(c, "timer")) {
    if (eq(a, "on") || eq(a, "off")) {
      hazerSetTimer(eq(a, "on"));
      Serial.printf("Timer %s (interval %ds, duration %ds)\n", eq(a, "on") ? "on" : "off",
                    hazerInterval(), hazerDuration());
    } else Serial.println("! timer on|off");
    return;
  }
  if (eq(c, "volume"))   { if (inRange("volume", a, 1, 100, &v))   { hazerSetPageValue(PG_VOLUME, v);   Serial.printf("Volume %d%%\n", v); } return; }
  if (eq(c, "fan"))      { if (inRange("fan", a, 1, 100, &v))      { hazerSetPageValue(PG_FAN, v);      Serial.printf("Fan %d%%\n", v); } return; }
  if (eq(c, "interval")) { if (inRange("interval", a, 0, 200, &v)) { hazerSetPageValue(PG_INTERVAL, v); Serial.printf("Interval %ds\n", v); } return; }
  if (eq(c, "duration")) { if (inRange("duration", a, 0, 200, &v)) { hazerSetPageValue(PG_DURATION, v); Serial.printf("Duration %ds\n", v); } return; }
  if (eq(c, "function")) { hazerButtonDown(0); Serial.printf("Page: %s\n", PAGE_NAMES[hazerPage()]); return; }
  if (eq(c, "up") || eq(c, "down")) {
    hazerAdjust(eq(c, "up") ? +1 : -1);
    printScreen();
    return;
  }
  if (eq(c, "page")) {
    for (int p = 0; p < PG_COUNT; p++)
      if (eq(a, PAGE_NAMES[p])) { hazerSetPage(p); Serial.printf("Page: %s\n", PAGE_NAMES[p]); return; }
    Serial.println("! page main|interval|duration|volume|fan");
    return;
  }

  // ---- settings
  if (eq(c, "rx")) {
    uint8_t m = rxMask(a);
    if (!m) { Serial.println("! rx 1|2|both"); return; }
    hazerSetTargetMask(m);
    hazerSaveSettings();
    Serial.printf("Receivers: %s\n", targetName(m));
    return;
  }
  if (eq(c, "dmx")) {
    uint8_t m = rxMask(a);
    if (m != 1 && m != 2) { Serial.println("! dmx 1|2 <1-510>"); return; }
    if (!inRange("DMX address", b, 1, 510, &v)) return;
    int i = m - 1;
    hazerSetDmxWanted(i, v);
    hazerSaveSettings();
    Serial.printf("%s DMX %d (ch %d-%d) - sent while %s is linked and controlled by this remote\n",
                  RECEIVERS[i].name, v, v, v + HZ_HAZER_VALUES - 1, RECEIVERS[i].name);
    return;
  }
  if (eq(c, "display")) {
    if (eq(a, "normal") || eq(a, "dark")) {
      hazerSetDarkMode(eq(a, "dark"));
      hazerSaveSettings();
      Serial.printf("Display %s (backlight %d%%)\n", a, hazerBacklightPct());
    } else Serial.println("! display normal|dark");
    return;
  }
  if (eq(c, "backlight")) {
    if (inRange("backlight", a, 5, 100, &v)) {
      hazerSetBacklightPct(v);
      hazerSaveSettings();
      Serial.printf("Backlight %d%% (%s mode)\n", v, hazerDarkMode() ? "dark" : "normal");
    }
    return;
  }

  // ---- receivers
  if (eq(c, "take")) {
    uint8_t m = rxMask(a);
    if (!m) { Serial.println("! take 1|2|both"); return; }
    hazerAddTargets(m);                         // keep sending to it, or control lapses
    radioSetTargets(hazerTargetMask());
    takeMask = m; opStart = millis(); opLastSend = 0;
    Serial.printf("Taking control of %s...\n", targetName(m));
    return;
  }
  if (eq(c, "config")) {
    if (!eq(a, HZ_CONFIG_PIN)) { Serial.println("! config <PIN>"); return; }
    hazerStopAll();
    cfgActive = true; opStart = millis(); opLastSend = 0;
    Serial.printf("Sending %s into setup mode (haze stopped)...\n", targetName(radioTargets()));
    return;
  }
  if (eq(c, "identify")) {
    uint8_t m = rxMask(a);
    if (m != 1 && m != 2) { Serial.println("! identify 1|2"); return; }
    radioSendCommandTo(m - 1, HZ_CMD_IDENTIFY, 0);
    Serial.printf("%s identify (LED blinks for 3 s)\n", RECEIVERS[m - 1].name);
    return;
  }

  // ---- info
  if (eq(c, "status")) { printStatus(false); return; }
  if (eq(c, "screen")) { printScreen(); return; }
  if (eq(c, "debug"))  { printDebug(); return; }
  if (eq(c, "watch")) {
    if (eq(a, "off")) { watchMs = 0; Serial.println("watch off"); return; }
    if (inRange("seconds", a, 1, 3600, &v)) { watchMs = (uint32_t)v * 1000UL; lastWatch = 0; }
    return;
  }
  if (eq(c, "reboot")) { Serial.println("Rebooting..."); Serial.flush(); ESP.restart(); }

  Serial.printf("! unknown command '%s' - type help\n", c);
}

// ---------------- public ----------------
void cliBegin() {
  Serial.println("[CLI] type 'help' for serial commands");
}

bool cliSending() { return cfgActive; }

void cliLoop(uint32_t now) {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r' || ch == '\n') {
      if (ch == '\n' && lastWasCR) { lastWasCR = false; continue; }   // CR LF = one line
      lastWasCR = (ch == '\r');
      Serial.println();
      line[len] = 0;
      len = 0;
      run(line);
      Serial.print("> ");
    } else if (ch == '\b' || ch == 0x7F) {
      lastWasCR = false;
      if (len) { len--; Serial.print("\b \b"); }
    } else if (ch >= 32 && len < sizeof(line) - 1) {
      lastWasCR = false;
      line[len++] = ch;
      Serial.print(ch);                          // echo
    }
  }

  opLoop(now);

  if (watchMs && now - lastWatch >= watchMs) {
    lastWatch = now;
    Serial.println("---");
    printStatus(false);
  }
}
