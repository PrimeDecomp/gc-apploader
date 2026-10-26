#ifndef _DOLPHIN_OS
#define _DOLPHIN_OS

#include "dolphin/types.h"

void* OSPhysicalToCached(u32 paddr);
#if !DEBUG
#define OSPhysicalToCached(paddr) ((void*)(0x80000000u + (u32)(paddr)))
#endif

#endif
