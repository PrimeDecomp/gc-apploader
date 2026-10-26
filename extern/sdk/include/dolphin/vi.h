#ifndef _DOLPHIN_VI
#define _DOLPHIN_VI

#include "dolphin/types.h"

#define VI_NTSC 0
#define VI_PAL 1
#define VI_MPAL 2
#define VI_DEBUG 3
#define VI_DEBUG_PAL 4
#define VI_EURGB60 5

u32 VIGetTvFormat(void);
u32 VIGetCurrentLine(void);

#endif
