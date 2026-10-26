#include "dolphin/types.h"

/* clang-format off */
asm u32 OSGetStackPointer(void) {
  nofralloc
  mr r3, r1
  blr
}
/* clang-format on */
