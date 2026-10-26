// UiSettings.h  (Hazer-TX) - gear screen: the remote's own settings (not part of the hazer menu)
#pragma once
#include <Arduino.h>

void uiSettingsStart();
bool uiSettingsActive();
void uiSettingsUpdate(bool touched, int32_t tx, int32_t ty, uint32_t now);
void uiSettingsRender(uint32_t now);
