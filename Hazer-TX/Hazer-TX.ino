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

static uint8_t pctTo255(uint8_t pct) { return (uint8_t)(((uint16_t)pct * 255) / 100); }

void setup() {
  Serial.begin(115200);
  Serial.printf("\n[TX] Hazer-TX v%d.%d\n", FW_VERSION_MAJOR, FW_VERSION_MINOR);
  displayBegin();
  hazerBegin();
  radioBegin();
}

void loop() {
  uint32_t now = millis();

  int32_t tx, ty;
  bool touched = lcd.getTouch(&tx, &ty);

  if (uiConfigActive()) {
    uiConfigUpdate(touched, tx, ty, now);
  } else if (uiPanelUpdate(touched, tx, ty, now) == UI_REQUEST_CONFIG) {
    uiConfigStart();
  }

  hazerUpdate(now);

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
