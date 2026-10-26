// HazerState.h  (Hazer-TX) - logic of the hazer "front panel" (menus, timer, output state)
// plus the remote's own settings (edited on the gear / Settings screen)
#pragma once
#include <Arduino.h>

// Menu pages, in the order FUNCTION cycles through them (same names as the machine)
enum { PG_MAIN, PG_INTERVAL, PG_DURATION, PG_VOLUME, PG_FAN, PG_COUNT };
#define DMX_RX_COUNT 2                 // DMX addresses kept for RECEIVERS[0..1]

void     hazerBegin();                    // load saved settings
void     hazerButtonDown(int b);          // 0 FUNCTION, 1 UP, 2 DOWN, 3 TIMER, 4 START/STOP, 5 MANUAL
void     hazerButtonUp(int b);
void     hazerAdjust(int delta);          // UP / DOWN on the current page
void     hazerSetPageValue(int page, int v);   // typed on the number pad (clamped, saved)
void     hazerUpdate(uint32_t now);       // run the timer
void     hazerStopAll();                  // stop haze, cancel timer

bool     hazerEmit();                     // should the machine be emitting right now?
uint8_t  hazerFanPct();
uint8_t  hazerVolPct();

int      hazerPage();
int      hazerInterval();                 // seconds
int      hazerDuration();                 // seconds
bool     hazerTimerActive();
bool     hazerOnActive();                 // START/STOP latched on
bool     hazerManualHeld();
int      hazerTimerPhase();               // 0 = waiting, 1 = hazing
long     hazerTimerRemaining(uint32_t now); // seconds left in current phase

// ---- remote settings (Settings screen); setters take effect at once, hazerSaveSettings() stores them
uint8_t  hazerTargetMask();               // receivers to control: 1 = RX1, 2 = RX2, 3 = both
void     hazerSetTargetMask(uint8_t mask);
void     hazerAddTargets(uint8_t mask);   // also control these receivers (saved)
bool     hazerDarkMode();                 // dark screen colors
void     hazerSetDarkMode(bool dark);
int      hazerBacklightPct();             // 5-100 %, kept separately for normal and dark mode
void     hazerSetBacklightPct(int pct);   // sets the current mode's level
// The DMX address set for a receiver is pushed to it whenever it reports a different
// one, so a receiver that was offline catches up later.
int      hazerDmxWanted(int rx);          // 0 = none set from this remote
void     hazerSetDmxWanted(int rx, int base);
void     hazerSaveSettings();
