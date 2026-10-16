#include "crash_cause.h"

namespace crash {

// From the Xtensa ISA reference, in the order of their EXCCAUSE values.
static const char *const NAMES[] = {
    "IllegalInstruction",      // 0
    "Syscall",                 // 1
    "InstructionFetchError",   // 2
    "LoadStoreError",          // 3
    "Level1Interrupt",         // 4
    "Alloca",                  // 5
    "IntegerDivideByZero",     // 6
    "PCValue",                 // 7
    "Privileged",              // 8
    "LoadStoreAlignment",      // 9
    "Unknown",                 // 10
    "Unknown",                 // 11
    "InstrPIFDataError",       // 12
    "LoadStorePIFDataError",   // 13
    "InstrPIFAddrError",       // 14
    "LoadStorePIFAddrError",   // 15
    "InstTLBMiss",             // 16
    "InstTLBMultiHit",         // 17
    "InstFetchPrivilege",      // 18
    "Unknown",                 // 19
    "InstFetchProhibited",     // 20
    "Unknown",                 // 21
    "Unknown",                 // 22
    "Unknown",                 // 23
    "LoadStoreTLBMiss",        // 24
    "LoadStoreTLBMultiHit",    // 25
    "LoadStorePrivilege",      // 26
    "Unknown",                 // 27
    "LoadProhibited",          // 28
    "StoreProhibited",         // 29
};

const char *causeName(uint32_t cause) {
  return cause < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[cause] : "Unknown";
}

const char *causeHint(uint32_t cause) {
  switch (cause) {
    case 0:
      return "jumped to something that is not code, often a corrupted stack or a bad function pointer";
    case 6:
      return "a whole number was divided by zero";
    case 9:
      return "read or wrote memory at an address not aligned for its size";
    case 20:
      return "jumped to an address with no code, often a null or freed function pointer";
    case 28:
      return "read from an address that is not memory, often a null or freed pointer";
    case 29:
      return "wrote to an address that is not memory, often a null or freed pointer";
    default:
      return nullptr;
  }
}

} // namespace crash
