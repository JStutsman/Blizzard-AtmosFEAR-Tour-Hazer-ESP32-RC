// HazerState.h  (Hazer-TX) - logic of the hazer "front panel" (menus, timer, output state)
#pragma once
#include <Arduino.h>

// Menu pages, in the order FUNCTION cycles through them (same names as the machine)
enum { PG_MAIN, PG_INTERVAL, PG_DURATION, PG_VOLUME, PG_FAN, PG_RECEIVER, PG_DMX1, PG_DMX2, PG_DISPLAY, PG_BACKLIGHT, PG_COUNT };
#define DMX_PAGE_COUNT 2               // one address page per receiver (RECEIVERS[0..1])

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
void     hazerAddTargets(uint8_t mask);   // also control these receivers (saved)
bool     hazerDarkMode();                 // "Display Set" page: dark screen colors
int      hazerBacklightPct();             // "Backlight Set" page: 5-100 %, kept separately for normal and dark

// DMX address pages, one per receiver (only selected receivers' pages are shown).
// The address set here is saved on the remote and pushed to the receiver whenever
// it reports a different one, so a receiver that was offline catches up later.
int      hazerDmxPageRx();                // receiver index of the current page, -1 if not a DMX page
int      hazerDmxEdit();                  // value on the page, -1 until known
void     hazerLoadDmxBase(int base);      // fill in the receiver's current address if unknown
int      hazerDmxWanted(int rx);          // address the remote keeps that receiver on, 0 = none set
