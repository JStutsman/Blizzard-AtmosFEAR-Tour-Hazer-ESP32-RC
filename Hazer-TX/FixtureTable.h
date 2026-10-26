// FixtureTable.h  (Hazer-TX)
// List of devices this remote can control. The hazer is the only one for now;
// more fixtures (and their UI pages) can be added here when the remote becomes a hub.
#pragma once
#include <stdint.h>
#include "Config.h"

struct Fixture {
  const char* name;
  uint8_t     mac[6];        // receiver MAC
  uint8_t     valueCount;    // number of control values sent to it
};

static const Fixture FIXTURES[] = {
  { "Hazer", RX_MAC_BYTES, 3 },
};

#define FIXTURE_COUNT   (sizeof(FIXTURES) / sizeof(FIXTURES[0]))
#define ACTIVE_FIXTURE  0
