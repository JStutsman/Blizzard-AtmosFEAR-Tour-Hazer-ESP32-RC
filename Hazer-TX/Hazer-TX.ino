// Hazer-TX.ino
// Touchscreen remote (ESP32-2432S028R "CYD") for the Blizzard AtmosFEAR Tour HZ.
// The screen mimics the machine's front panel; control goes over ESP-NOW to Hazer-RX,
// which outputs DMX to the hazer.
//
// Files: Config.h (defines), FixtureTable.h (devices), Display, Radio, HazerState,
//        UiPanel (panel screen), UiConfig (PIN + config-mode flow), HazerProtocol.h
//
// Requires the LovyanGFX library.

#include "Config.h"
#include "HazerProtocol.h"
#include "Display.h"
#include "Radio.h"
#include "HazerState.h"
#include "UiPanel.h"
#include "UiConfig.h"
#include "FixtureTable.h"
#include "Theme.h"

static uint8_t pctTo255(uint8_t pct) { return (uint8_t)(((uint16_t)pct * 255) / 100); }

// DMX address pages: show the receiver's current address until one is set here, and
// keep every receiver that is answering on the address set for it. A receiver that
// missed the change (off or out of range) gets it as soon as it reports again.
static void updateDmxBase(uint32_t now) {
  static uint32_t lastSend = 0;

  int rx = hazerDmxPageRx();
  if (rx >= 0 && hazerDmxEdit() < 0) {
    RadioStatus rs = radioGetStatus(rx);
    if (rs.linkUp && !(rs.flags & HZ_FLAG_BUSY)) hazerLoadDmxBase(rs.base);
  }

  if (now - lastSend < 200) return;
  lastSend = now;
  for (int i = 0; i < RECEIVER_COUNT && i < DMX_PAGE_COUNT; i++) {
    int want = hazerDmxWanted(i);
    RadioStatus rs = radioGetStatus(i);
    if (want && rs.linkUp && !(rs.flags & HZ_FLAG_BUSY) && rs.base != want) {   // only receivers we control
      radioSendCommandTo(i, HZ_CMD_SET_BASE, want);
      Serial.printf("[TX] %s DMX base %u -> %d\n", RECEIVERS[i].name, rs.base, want);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\n[TX] Hazer-TX v%d.%d\n", FW_VERSION_MAJOR, FW_VERSION_MINOR);
  displayBegin();
  hazerBegin();
  themeSetDark(hazerDarkMode());
  themeSetBacklight(hazerBacklightPct());
  radioBegin();
  radioSetTargets(hazerTargetMask());
}

void loop() {
  uint32_t now = millis();

  int32_t tx, ty;
  bool touched = lcd.getTouch(&tx, &ty);

  if (uiConfigActive()) {
    uiConfigUpdate(touched, tx, ty, now);
  } else {
    UiResult r = uiPanelUpdate(touched, tx, ty, now);
    if (r == UI_REQUEST_CONFIG)   uiConfigStart();
    if (r == UI_REQUEST_TAKEOVER) uiTakeoverStart();
  }

  hazerUpdate(now);
  radioSetTargets(hazerTargetMask());
  themeSetDark(hazerDarkMode());
  themeSetBacklight(hazerBacklightPct());
  updateDmxBase(now);

  // Send the current control values at a fixed rate (paused while the config command is sent)
  static uint32_t nextSend = 0;
  if (!uiConfigSending() && (int32_t)(now - nextSend) >= 0) {
    nextSend = now + SEND_INTERVAL_MS;
    uint8_t v[HZ_HAZER_VALUES];
    v[HZV_EMIT] = hazerEmit() ? 255 : 0;
    v[HZV_FAN]  = pctTo255(hazerFanPct());
    v[HZV_VOL]  = pctTo255(hazerVolPct());
    radioSendData(v, HZ_HAZER_VALUES);
  }

  if (uiConfigActive()) uiConfigRender(now);
  else                  uiPanelRender(now);

  delay(5);
}
