#pragma once

#include <ArduinoJson.h>

// The record ESP-IDF writes to the coredump partition when the board crashes
// or a watchdog fires. It stays in flash, through restarts, until erased, so
// a crash that happened while nobody was watching can still be looked at.
// tools/decode_crash.py turns its addresses into function names.

// Call once at start-up, after clockChipBegin() so the time is known.
void crashBegin();

bool crashPresent();

// The summary: task, cause, addresses and backtrace.
void crashReport(JsonObject report);
// A few words of it, for /diagnostics.
void crashBrief(JsonObject report);

// The raw record, for decoding in full on a PC with espcoredump.py.
size_t crashDumpSize();
bool crashDumpRead(size_t offset, uint8_t *buffer, size_t length);

void crashErase();
