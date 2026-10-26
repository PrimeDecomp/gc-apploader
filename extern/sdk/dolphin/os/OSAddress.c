#include "dolphin/types.h"

void* OSPhysicalToCached(u32 address) { return (void*)(address + 0x80000000); }
