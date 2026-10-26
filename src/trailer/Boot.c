#include "BS2.h"
#include "__os.h"
#include "dolphin/base/PPCArch.h"
#include "dolphin/os.h"
#include "dolphin/os/OSLowMem.h"
#include "dolphin/os/OSReset.h"
#include "dolphin/os/OSTime.h"
#include <stdlib.h>
#include <string.h>

#if SDK_VERSION < 20040521
u32 ResetCode;
u32 RebootCode AT_ADDRESS(0x817ffffc);
u32 RebootArgument AT_ADDRESS(0x817ffff8);
u32 RebootParameter;
static double FpuZero;

#endif

static void BootResetMemory(void);

#if SDK_VERSION >= 20040521
static BOOL BootIsDebugDevice(void) {
  DVDCommandBlock block;
  u8 buffer[64];
  DVDDriveInfo* info;
  OSTime start;

  if ((__OSLowMemPad.pad3Buttons & 0xeef) != 0xeef) {
    return FALSE;
  }
  info = (DVDDriveInfo*)(((u32)buffer + 31) & ~31);
  DVDInquiryAsync(&block, info, NULL);
  start = OSGetTime();
  while (DVDGetCommandBlockStatus(&block) != DVD_STATE_END) {
    if (!DVDCheckDisk() || OS_TIMER_CLOCK < OSGetTime() - start) {
      __OSDoHotReset(0);
    }
  }
  __OSLowMemPad.deviceCode = info->deviceCode | 0x8000;
  return info->deviceCode == 1 ? TRUE : FALSE;
}

void main(int argc, char** argv) {
  int i;
  u32 dolOffset;
  void* regionStart;
  void* regionEnd;
  BOOL argsUseDefault;

  BootResetMemory();
  OSInit();
  DVDInit();
  DVDSetAutoInvalidation(TRUE);
  if (BootIsDebugDevice()) {
    CRCMain(0, NULL);
  }
  OSReport("Received %d arguments\n", argc);
  for (i = 0; i < argc; i++) {
    OSReport("argv[%d] is %s\n", i, argv[i]);
  }
  dolOffset = atoi(argv[0]);
  OSGetSavedRegion(&regionStart, &regionEnd);
  if (OSGetResetCode() & OS_RESETCODE_SYSTEM) {
    OSReport("This is for exec\n");
    argsUseDefault = FALSE;
  } else {
    OSReport("This is for reboot\n");
    argsUseDefault = TRUE;
  }
  OSSetArenaLo((void*)0x81280000);
  OSSetArenaHi((void*)0x812f0000);
  __OSShutdownDevices(TRUE);
  __OSBootDolSimple(dolOffset, OSGetResetCode(), regionStart, regionEnd, argsUseDefault, argc - 1,
                    argv + 1);
}

// The trailer overrides the SDK's reporting hooks.
void OSReport(const char* msg, ...) {}

void OSPanic(const char* file, int line, const char* msg, ...) { PPCHalt(); }

#else
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

#endif

static void BootResetMemory(void) {
  OSBootInfo* bootInfo = &__OSBootInfo;
  bootInfo->arenaHi = 0;
  bootInfo->arenaLo = 0;
  memset(__OSDebuggerWork, 0, sizeof(__OSDebuggerWork));
  memset(&__OSLowMemContext, 0, sizeof(__OSLowMemContext));
  memset(&__OSBootInfo2, 0, sizeof(__OSBootInfo2));
  memset(&__OSHandlerTables, 0, sizeof(__OSHandlerTables));
  memset(&__OSModuleState, 0, sizeof(__OSModuleState));
#if SDK_VERSION < 20040521
  BootClearFPRs();
#endif
}
