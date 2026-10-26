#ifndef __OS_H__
#define __OS_H__

#include "dolphin/os.h"

OSThreadQueue __OSActiveThreadQueue AT_ADDRESS(0x800000dc);

extern BOOL __OSInIPL;

u32 __OSGetDIConfig(void);
BOOL __OSSyncSram(void);
int __OSCallResetFunctions(BOOL final);
void __OSShutdownDevices(BOOL doRecal);
void __OSDoHotReset(u32 resetCode);
void __OSStopAudioSystem(void);
void __OSInitAudioSystem(void);
void __OSResetSWInterruptHandler(__OSInterrupt interrupt, OSContext* context);
void __OSUnlockAllMutex(OSThread* thread);
void OSGetSavedRegion(void** start, void** end);
void __OSBootDolSimple(u32 doloffset, u32 restartCode, void* regionStart, void* regionEnd,
                       BOOL argsUseDefault, s32 argc, char** argv);

#endif
