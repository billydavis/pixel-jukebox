#include "app/memory_trace.h"

#ifdef MEMORY_TRACE

static volatile size_t lowest = SIZE_MAX;

void memoryCheck(const char *where) {
  if (!heap_caps_check_integrity(MALLOC_CAP_INTERNAL, true) || !heap_caps_check_integrity(MALLOC_CAP_SPIRAM, true)) {
    Serial.printf("HEAP CORRUPT: found at %s in task %s\n", where, pcTaskGetName(nullptr));
    Serial.flush();
    abort(); // the crash record then shows this moment
  }
  size_t free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  if (free < lowest) {
    lowest = free;
    Serial.printf("mem new low %6u at %s (task %s, largest %u)\n", (unsigned)free, where, pcTaskGetName(nullptr),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  }
}

#endif
