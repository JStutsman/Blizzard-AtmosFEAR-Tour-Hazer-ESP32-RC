// FixtureTable.h  (Hazer-TX)
// Receivers this remote can talk to, and the devices it controls. The hazer is the
// only fixture for now; more fixtures (and their UI pages) can be added here when the
// remote becomes a hub.
#pragma once
#include <stdint.h>
#include "Config.h"

struct Receiver {
  const char* name;          // short label shown on screen
  uint8_t     mac[6];
};

static const Receiver RECEIVERS[] = {
  { "RX1", RX_MAC_RECEIVER_1 },
  { "RX2", RX_MAC_RECEIVER_2 },
};

#define RECEIVER_COUNT  (int)(sizeof(RECEIVERS) / sizeof(RECEIVERS[0]))

struct Fixture {
  const char* name;
  uint8_t     valueCount;    // number of control values sent to it
};

static const Fixture FIXTURES[] = {
  { "Hazer", 3 },
};

#define FIXTURE_COUNT   (sizeof(FIXTURES) / sizeof(FIXTURES[0]))
#define ACTIVE_FIXTURE  0
