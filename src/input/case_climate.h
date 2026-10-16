#pragma once

#include <ArduinoJson.h>

// Temperature and humidity inside the case, from the board's SHTC3 sensor.
// The board and the panel warm the air around it, so this is how hot the
// electronics are running, not the room: a diagnostic, and nothing more.

// Looks for the sensor and starts reading it once a minute in the background.
void caseClimateBegin();

void caseClimateReport(JsonObject report);
