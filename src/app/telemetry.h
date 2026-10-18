#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Memory and task headroom, for deciding limits from measurements rather than
// guesses. Reported by GET /diagnostics. Every call is safe from any task.

// Long-lived tasks are registered once and sampled when a report is made.
void telemetryRegisterTask(const char *name, TaskHandle_t task, uint32_t stackBytes);

// Short-lived tasks report their own headroom just before they exit; the
// smallest value seen is kept.
void telemetryNoteTaskExit(const char *name, uint32_t stackBytes);

// Called for each artwork downloaded, with the bytes held for it.
void telemetryNoteImage(size_t bytes);

// Notes how far start-up has got, in ms since power-on, for /diagnostics
// ("boot"). `step` must be a string literal.
void telemetryBootMark(const char *step);

void telemetryReport(JsonObject report);
