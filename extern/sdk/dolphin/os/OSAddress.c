#include "dolphin/os.h"

#undef OSPhysicalToCached

void* OSPhysicalToCached(u32 paddr) {
  ASSERTMSGLINE(0x2C, paddr < 0x10000000U, "OSPhysicalToCached(): illegal address.");
  return (void*)(paddr + 0x80000000);
}
