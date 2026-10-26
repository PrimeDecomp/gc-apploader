#ifndef _DOLPHIN_OSTIME
#define _DOLPHIN_OSTIME

#include "dolphin/types.h"

typedef s64 OSTime;
typedef u32 OSTick;

OSTime OSGetTime(void);
OSTick OSGetTick(void);

#endif
