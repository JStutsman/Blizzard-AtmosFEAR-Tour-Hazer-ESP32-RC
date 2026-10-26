// HazerState.h  (Hazer-TX) - logic of the hazer "front panel" (menus, timer, output state)
#pragma once
#include <Arduino.h>

// Menu pages, in the order FUNCTION cycles through them (same names as the machine)
enum { PG_MAIN, PG_INTERVAL, PG_DURATION, PG_VOLUME, PG_FAN, PG_COUNT };

void     hazerBegin();                    // load saved settings
void     hazerButtonDown(int b);          // 0 FUNCTION, 1 UP, 2 DOWN, 3 TIMER, 4 START/STOP, 5 MANUAL
void     hazerButtonUp(int b);
void     hazerAdjust(int delta);          // UP / DOWN on the current page
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
