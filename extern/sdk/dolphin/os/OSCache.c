#include "dolphin/types.h"

/* clang-format off */
asm void DCInvalidateRange(register void* addr, register u32 nBytes) {
  nofralloc
  cmplwi nBytes, 0
  blelr
  clrlwi. r5, addr, 27
  beq @aligned
  addi nBytes, nBytes, 32
@aligned
  addi   nBytes, nBytes, 31
  srwi   nBytes, nBytes, 5
  mtctr  nBytes

@1
  dcbi r0, addr
  addi addr, addr, 32
  bdnz @1
  blr
}


asm void DCFlushRange(register void* addr, register u32 nBytes) {
  nofralloc
  cmplwi nBytes, 0
  blelr
  clrlwi. r5, addr, 27
  beq @aligned
  addi nBytes, nBytes, 32
@aligned
  addi nBytes, nBytes, 31
  srwi nBytes, nBytes, 5
  mtctr nBytes

@1
  dcbf r0, addr
  addi addr, addr, 32
  bdnz @1
  sc
  blr
}

/* clang-format on */
