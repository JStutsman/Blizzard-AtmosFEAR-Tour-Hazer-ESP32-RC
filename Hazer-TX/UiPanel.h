// UiPanel.h  (Hazer-TX) - the hazer front-panel screen (blue LCD + six buttons)
#pragma once
#include <Arduino.h>

enum UiResult { UI_NONE, UI_REQUEST_CONFIG, UI_REQUEST_TAKEOVER };

// Process touch input. Returns UI_REQUEST_CONFIG when the hidden long-press is detected,
// UI_REQUEST_TAKEOVER on a tap of the info line while another remote has a selected receiver.
UiResult uiPanelUpdate(bool touched, int32_t tx, int32_t ty, uint32_t now);
void     uiPanelRender(uint32_t now);
void     uiPanelInvalidate();   // force a full redraw (and ignore the touch until released)
