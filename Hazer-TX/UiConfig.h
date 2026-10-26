// UiConfig.h  (Hazer-TX) - PIN entry, confirmation and "enter config mode" flow
#pragma once
#include <Arduino.h>

void uiConfigStart();      // open the PIN screen
void uiTakeoverStart();    // open the "take control" confirm for receivers another remote controls
bool uiConfigActive();     // true while any config screen is showing
bool uiConfigSending();    // true while the enter-config command is being sent (normal data paused)
void uiConfigUpdate(bool touched, int32_t tx, int32_t ty, uint32_t now);
void uiConfigRender(uint32_t now);
