#ifndef __OS_H__
#define __OS_H__

#include "dolphin/os.h"

OSThreadQueue __OSActiveThreadQueue AT_ADDRESS(0x800000dc);

BOOL __OSSyncSram(void);
int __OSCallResetFunctions(BOOL final);
void __OSShutdownDevices(BOOL doRecal);
void __OSDoHotReset(u32 resetCode);
void __OSStopAudioSystem(void);
void OSGetSavedRegion(void** start, void** end);
void __OSBootDolSimple(u32 doloffset, u32 restartCode, void* regionStart, void* regionEnd,
                       BOOL argsUseDefault, s32 argc, char** argv);

#endif
