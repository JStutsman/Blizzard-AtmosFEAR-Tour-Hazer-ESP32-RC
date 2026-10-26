// UiNumpad.h  (Hazer-TX) - number pad for typing a value (opened by tapping a value)
#pragma once
#include <Arduino.h>

typedef void (*NumpadDone)(bool ok, int value);   // ok = false when cancelled

// Opens the pad over the current screen. done() is called once when it closes; the
// caller applies the value and redraws its own screen.
void uiNumpadStart(const char* title, int value, int minV, int maxV, NumpadDone done);
bool uiNumpadActive();
void uiNumpadUpdate(bool touched, int32_t tx, int32_t ty, uint32_t now);
void uiNumpadRender(uint32_t now);
