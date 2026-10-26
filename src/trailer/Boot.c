#include "BS2.h"
#include "dolphin/os.h"
#include <string.h>

u32 ResetCode;
u32 RebootCode AT_ADDRESS(0x817ffffc);
u32 RebootArgument AT_ADDRESS(0x817ffff8);
u32 RebootParameter;
static double FpuZero;

static void BootResetMemory(void);

void main(void) {
  ResetCode = RebootCode;
  RebootParameter = RebootArgument;
  BootResetMemory();
  OSInit();
  DVDInit();
  OSReport("\n--- OSReboot ---\n");
  for (;;) {
    switch (BS2Main()) {
    case BS2_STATE_READY:
      BS2Run();
      break;
    case BS2_STATE_COVER_CLOSED:
      break;
    case 13:
      break;
    case 14:
      break;
    case BS2_STATE_MOTOR_STOPPED:
      break;
    case BS2_STATE_FATAL_ERROR:
      break;
    }
  }
}

/* clang-format off */
static asm void BootClearFPRs(void) {
  nofralloc
  lfd f0, FpuZero
  fmr f1, f0
  fmr f2, f0
  fmr f3, f0
  fmr f4, f0
  fmr f5, f0
  fmr f6, f0
  fmr f7, f0
  fmr f8, f0
  fmr f9, f0
  fmr f10, f0
  fmr f11, f0
  fmr f12, f0
  fmr f13, f0
  fmr f14, f0
  fmr f15, f0
  fmr f16, f0
  fmr f17, f0
  fmr f18, f0
  fmr f19, f0
  fmr f20, f0
  fmr f21, f0
  fmr f22, f0
  fmr f23, f0
  fmr f24, f0
  fmr f25, f0
  fmr f26, f0
  fmr f27, f0
  fmr f28, f0
  fmr f29, f0
  fmr f30, f0
  fmr f31, f0
  blr
}
/* clang-format on */

static void BootResetMemory(void) {
  OSBootInfo* bootInfo = (OSBootInfo*)0x80000000;
  bootInfo->arenaHi = 0;
  bootInfo->arenaLo = 0;
  memset((void*)0x80000040, 0, 0x8c);
  memset((void*)0x800000d4, 0, 0x14);
  memset((void*)0x800000f4, 0, 4);
  memset((void*)0x80003000, 0, 0xc0);
  memset((void*)0x800030c8, 0, 0xc);
  BootClearFPRs();
}
