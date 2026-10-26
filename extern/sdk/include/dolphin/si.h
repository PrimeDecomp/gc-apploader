#ifndef _DOLPHIN_SI
#define _DOLPHIN_SI

#include "dolphin/types.h"

BOOL SIBusy(void);
u32 SISetXY(u32 x, u32 y);
void SISetSamplingRate(u32 msec);
void SIRefreshSamplingRate(void);

#endif
