#pragma once

// Internal RAM checks, only in a build made with -DMEMORY_TRACE, for working
// out what uses internal RAM and for catching memory corruption close to
// where it happens:
//
//   PLATFORMIO_BUILD_FLAGS=-DMEMORY_TRACE pio run -t upload
//
// Internal RAM is the scarce kind: Wi-Fi, task stacks, the display's DMA
// and every TLS session need it, while PSRAM has megabytes to spare.
//
// MEMORY_AT(step) prints free, largest block and lowest-ever free.
// MEMORY_CHECK(where) checks the allocator's bookkeeping, stopping the board
// at once (so the crash record shows where) if it is damaged, and prints a
// line whenever free internal RAM reaches a new low, naming where.

#ifdef MEMORY_TRACE
#include <Arduino.h>
#include <esp_heap_caps.h>
#define MEMORY_AT(step)                                                                                      \
  Serial.printf("mem %-22s free %6u  largest %6u  lowest %6u\n", step,                                      \
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),                                      \
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),                             \
                (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL))
void memoryCheck(const char *where);
#define MEMORY_CHECK(where) memoryCheck(where)
#else
#define MEMORY_AT(step)
#define MEMORY_CHECK(where)
#endif
