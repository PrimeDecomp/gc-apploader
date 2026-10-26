#include "dolphin/os/OSTime.h"
#include "dolphin/os.h"

/* clang-format off */
asm OSTime OSGetTime(void) {
jump:
  nofralloc
  mftbu r3
  mftb r4
  mftbu r5
  cmpw r3, r5
  bne jump
  blr
}

asm OSTick OSGetTick(void) {
  nofralloc
  mftb r3
  blr
} /* clang-format on */

OSTime __OSGetSystemTime(void) {
  BOOL enabled;
  OSTime* timeAdjustAddr;
  OSTime result;

  timeAdjustAddr = __OSSystemTime;
  enabled = OSDisableInterrupts();
  result = OSGetTime() + *timeAdjustAddr;
  OSRestoreInterrupts(enabled);
  return result;
}
