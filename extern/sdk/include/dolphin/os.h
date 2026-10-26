#ifndef _DOLPHIN_OS
#define _DOLPHIN_OS

#include "dolphin/dvd.h"
#include "dolphin/os/OSArena.h"
#include "dolphin/os/OSContext.h"
#include "dolphin/os/OSError.h"
#include "dolphin/os/OSInterrupt.h"
#include "dolphin/os/OSThread.h"
#include "dolphin/types.h"

typedef struct OSBootInfo_s {
  DVDDiskID DVDDiskID;
  u32 magic;
  u32 version;
  u32 memorySize;
  u32 consoleType;
  void* arenaLo;
  void* arenaHi;
  void* FSTLocation;
  u32 FSTMaxLength;
} OSBootInfo;

#define OFFSET(addr, align) (((u32)(addr) & ((align) - 1)))

u32 __OSBusClock AT_ADDRESS(0x800000F8);
#define OS_BUS_CLOCK __OSBusClock
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)
#define OSTicksToMilliseconds(ticks) ((ticks) / (OS_TIMER_CLOCK / 1000))

#define OS_CONSOLE_ARTHUR 0x10000002

BOOL OSDisableInterrupts(void);
BOOL OSEnableInterrupts(void);
BOOL OSRestoreInterrupts(BOOL level);
u32 OSGetConsoleType(void);
void* OSPhysicalToCached(u32 paddr);
#if !DEBUG
#define OSPhysicalToCached(paddr) ((void*)(0x80000000u + (u32)(paddr)))
#endif

#endif
