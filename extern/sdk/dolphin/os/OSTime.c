#include "dolphin/types.h"

/* clang-format off */
asm s64 OSGetTime(void) {
jump:
  nofralloc
  mftbu r3
  mftb r4
  mftbu r5
  cmpw r3, r5
  bne jump
  blr
}
/* clang-format on */
