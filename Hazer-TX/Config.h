// Config.h  (Hazer-TX)
// All compile-time settings for the CYD remote live here.
#pragma once

#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0

// ---------------- display / touch ----------------
#define DISPLAY_INVERT     false   // set true if colors look inverted
#define DISPLAY_RGB_ORDER  false   // set true if red/blue are swapped
#define TOUCH_CAL_ON_BOOT  false   // set true once to calibrate; values print to Serial

// ---------------- radio link ----------------
#define RADIO_CHANNEL      6       // must match the receiver (HZ_DEFAULT_CHANNEL)
#define SEND_INTERVAL_MS   50      // data packet rate (20 Hz)
#define LINK_TIMEOUT_MS    1500    // "No Link" if no status reply for this long

// MAC addresses of the Hazer-RX receivers (printed on the receiver's serial
// monitor at boot, and shown on its config page).
#define RX_MAC_RECEIVER_1  {0xE8, 0x3D, 0xC1, 0x81, 0xF2, 0xF8}  // E8:3D:C1:81:F2:F8
#define RX_MAC_RECEIVER_2  {0xE8, 0x3D, 0xC1, 0x83, 0x83, 0xF0}  // E8:3D:C1:83:83:F0
// Which of them the remote controls is picked on the "Receiver Set" menu page.

// ---------------- config-mode access ----------------
#define HZ_CONFIG_PIN       "4817"  // 4-digit PIN to send the receiver into config mode
#define PIN_LOCKOUT_TRIES   3       // wrong tries before lockout
#define PIN_LOCKOUT_MS      5000    // lockout length
#define CONFIG_HOLD_MS      2000    // hold the blue LCD area this long to open PIN entry
#define PIN_IDLE_TIMEOUT_MS 30000   // PIN / confirm screens close after this much inactivity
