#ifndef _DOLPHIN_OSMEMORY
#define _DOLPHIN_OSMEMORY

#include "dolphin/types.h"

#define OS_PROTECT_CONTROL_RDWR 3

u32 OSGetPhysicalMemSize(void);
u32 OSGetConsoleSimulatedMemSize(void);
void OSProtectRange(u32 chan, void* addr, u32 nBytes, u32 control);
void __OSInitMemoryProtection(void);

#endif
