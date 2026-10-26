// SerialCli.h  (Hazer-TX) - serial command line: everything the touchscreen can do
// Type "help" in a serial terminal (115200 baud, line ending LF or CR).
#pragma once
#include <Arduino.h>

void cliBegin();
void cliLoop(uint32_t now);
bool cliSending();     // true while "config" sends ENTER_CONFIG (normal data paused)
