#ifndef _DOLPHIN_OS
#define _DOLPHIN_OS

#include "dolphin/dvd.h"
#include "dolphin/os/OSError.h"
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

#define OS_CONSOLE_ARTHUR 0x10000002

u32 OSGetConsoleType(void);
void* OSPhysicalToCached(u32 paddr);
#if !DEBUG
#define OSPhysicalToCached(paddr) ((void*)(0x80000000u + (u32)(paddr)))
#endif

#endif
