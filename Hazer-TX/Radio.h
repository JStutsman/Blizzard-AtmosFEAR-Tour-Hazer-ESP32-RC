// Radio.h  (Hazer-TX) - ESP-NOW link to the receivers
#pragma once
#include <Arduino.h>

struct RadioStatus {
  bool     linkUp;       // status reply received recently
  int8_t   rssi;         // dBm of the last status reply as seen by this remote
  int8_t   rxRssi;       // dBm of our last packet as seen by the receiver
  uint8_t  fwMajor, fwMinor;
  uint8_t  flags;        // HzStatusFlags from the receiver
  uint8_t  echo[3];      // values the receiver is actually outputting
  uint16_t base;         // receiver's DMX base address
  uint32_t ageMs;        // ms since last status reply
};

void        radioBegin();
const char* radioError();                       // nullptr if OK, else a short message
void        radioSetTargets(uint8_t mask);      // bit i = RECEIVERS[i]; dropped receivers get SAFE
uint8_t     radioTargets();
void        radioSendData(const uint8_t* values, uint8_t count);   // to all targets
void        radioSendCommand(uint8_t cmd, uint16_t arg);           // to all targets
RadioStatus radioGetStatus(int rx);             // one receiver (index into RECEIVERS)
