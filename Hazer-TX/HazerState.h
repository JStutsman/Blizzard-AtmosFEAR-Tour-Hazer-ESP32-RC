// HazerState.h  (Hazer-TX) - logic of the hazer "front panel" (menus, timer, output state)
#pragma once
#include <Arduino.h>

// Menu pages, in the order FUNCTION cycles through them (same names as the machine)
enum { PG_MAIN, PG_INTERVAL, PG_DURATION, PG_VOLUME, PG_FAN, PG_RECEIVER, PG_DMX, PG_COUNT };

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
uint8_t  hazerTargetMask();               // receivers to control: 1 = RX1, 2 = RX2, 3 = both

// DMX base address page: the value lives on the receiver, so it is loaded from its
// status when the page opens and sent back when FUNCTION leaves the page.
int      hazerDmxBase();                  // -1 until loaded from the receiver
void     hazerLoadDmxBase(int base);      // fill in the receiver's current address
bool     hazerTakeDmxCommit(int* base);   // true once after an edited address is confirmed
