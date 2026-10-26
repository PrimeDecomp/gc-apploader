#ifndef _DOLPHIN_OSTHREAD
#define _DOLPHIN_OSTHREAD

#include "dolphin/types.h"

s32 OSEnableScheduler(void);
s32 OSDisableScheduler(void);
void __OSReschedule(void);

#endif
