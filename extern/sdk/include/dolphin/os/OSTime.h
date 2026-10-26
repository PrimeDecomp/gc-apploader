#ifndef _DOLPHIN_OSTIME
#define _DOLPHIN_OSTIME

#include "dolphin/types.h"

typedef s64 OSTime;
typedef u32 OSTick;

#define __OSSystemTime (OSTime*)0x800030D8

OSTime OSGetTime(void);
OSTick OSGetTick(void);
OSTime __OSGetSystemTime(void);

#endif
