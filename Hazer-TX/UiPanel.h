// UiPanel.h  (Hazer-TX) - the hazer front-panel screen (blue LCD + six buttons)
#pragma once
#include <Arduino.h>

enum UiResult { UI_NONE, UI_REQUEST_CONFIG, UI_REQUEST_TAKEOVER, UI_REQUEST_SETTINGS };

// Process touch input. Returns UI_REQUEST_CONFIG when the hidden long-press is detected,
// UI_REQUEST_TAKEOVER on a short tap of the info line, UI_REQUEST_SETTINGS on a tap of the gear.
UiResult uiPanelUpdate(bool touched, int32_t tx, int32_t ty, uint32_t now);
void     uiPanelRender(uint32_t now);
void     uiPanelInvalidate();   // force a full redraw (and ignore the touch until released)
// Text currently on the blue display. info uses the segment format "RX1 Ch89 -57dBm#3|..."
// ('#' + signal level 0-4, a-e = same in orange, X = no link).
void     uiPanelText(const char** l1, const char** l2, const char** info);
