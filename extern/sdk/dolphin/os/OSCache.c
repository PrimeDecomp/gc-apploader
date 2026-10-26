#include "dolphin/types.h"

/* clang-format off */
asm void DCInvalidateRange(register void* addr, register u32 nBytes) {
  nofralloc
  cmplwi nBytes, 0
  blelr
#if SDK_VERSION >= 20040521
  clrlwi r5, addr, 27
  add nBytes, nBytes, r5
#else
  clrlwi. r5, addr, 27
  beq @aligned
  addi nBytes, nBytes, 32
@aligned
#endif
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
#if SDK_VERSION >= 20040521
  clrlwi r5, addr, 27
  add nBytes, nBytes, r5
#else
  clrlwi. r5, addr, 27
  beq @aligned
  addi nBytes, nBytes, 32
@aligned
#endif
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

#if SDK_VERSION >= 20040521
asm void DCStoreRange(register void* addr, register u32 nBytes) {
  nofralloc
  cmplwi nBytes, 0
  blelr
  clrlwi r5, addr, 27
  add nBytes, nBytes, r5
  addi nBytes, nBytes, 31
  srwi nBytes, nBytes, 5
  mtctr nBytes

@1
  dcbst r0, addr
  addi addr, addr, 32
  bdnz @1
  sc

  blr
}

asm void ICInvalidateRange(register void* addr, register u32 nBytes) {
  nofralloc
  cmplwi nBytes, 0
  blelr
  clrlwi r5, addr, 27
  add nBytes, nBytes, r5
  addi nBytes, nBytes, 31
  srwi nBytes, nBytes, 5
  mtctr nBytes

@1
  icbi r0, addr
  addi addr, addr, 32
  bdnz @1
  sync
  isync

  blr
}
#endif

/* clang-format on */
