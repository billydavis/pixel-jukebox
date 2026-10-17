#pragma once

// Internal RAM at a named point, on the serial port. Only in a build made
// with -DMEMORY_TRACE, for working out what uses internal RAM:
//
//   PLATFORMIO_BUILD_FLAGS=-DMEMORY_TRACE pio run -t upload
//
// Internal RAM is the scarce kind: Wi-Fi, task stacks, the display's DMA
// and every TLS session need it, while PSRAM has megabytes to spare.

#ifdef MEMORY_TRACE
#include <Arduino.h>
#include <esp_heap_caps.h>
#define MEMORY_AT(step)                                                                                      \
  Serial.printf("mem %-22s free %6u  largest %6u  lowest %6u\n", step,                                      \
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),                                      \
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),                             \
                (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL))
#else
#define MEMORY_AT(step)
#endif
