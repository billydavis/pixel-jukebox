#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// New firmware over Wi-Fi (POST /api/v1/update), with rollback.
//
// The flash has two firmware slots. An upload is written into the one not
// running while the panel shows its progress; only a complete image that
// passes its checks is chosen for the next start-up. That start-up is on
// probation: the new firmware must get its API up and keep running for a
// while before it is kept. If it crashes, hangs or never gets that far, the
// bootloader goes back to the previous firmware on the next restart.

void firmwareUpdateBegin();

// Call from the display loop. Keeps a new firmware once the API has been up
// for a while; gives up on it (and restarts into the old one) if that takes
// too long.
void firmwareUpdateMaintain(bool apiUp);

// The upload, called by the API task as the body arrives. `expectedBytes` may
// be 0 when unknown; `md5` may be empty.
bool firmwareUpdateStart(size_t expectedBytes, const String &md5, String &error);
bool firmwareUpdateWrite(uint8_t *data, size_t length, String &error);
// Checks the whole image and chooses it for the next start-up.
bool firmwareUpdateFinish(String &error);
void firmwareUpdateAbort();

// True while an upload is being written.
bool firmwareUpdateActive();
// 0-100 while active.
int firmwareUpdatePercent();

// For /diagnostics: which slot is running, and whether it is on probation.
void firmwareUpdateReport(JsonObject out);
