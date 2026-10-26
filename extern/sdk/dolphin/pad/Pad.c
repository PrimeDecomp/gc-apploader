#include "dolphin/pad.h"
#include "dolphin/si.h"

static s32 ResettingChan = 32;
static u32 ResettingBits;
u32 __PADSpec;

BOOL PADSync(void) { return ResettingBits == 0 && (s32)ResettingChan == 32 && !SIBusy(); }
