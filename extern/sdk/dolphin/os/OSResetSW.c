#include "dolphin/os/OSResetSW.h"
#include "__os.h"

static OSResetCallback ResetCallback;
static BOOL Down;
static BOOL LastState;
static OSTime HoldDown;

extern volatile u32 __PIRegs[64] AT_ADDRESS(0xcc003000);

void __OSResetSWInterruptHandler(__OSInterrupt interrupt, OSContext* context) {
  OSResetCallback callback;

  HoldDown = __OSGetSystemTime();
  while (__OSGetSystemTime() - HoldDown < OSMicrosecondsToTicks(100) &&
         !(__PIRegs[0] & 0x00010000)) {
    ;
  }
  if (!(__PIRegs[0] & 0x00010000)) {
    LastState = Down = TRUE;
    __OSMaskInterrupts(OS_INTERRUPTMASK_PI_RSW);
    if (ResetCallback) {
      callback = ResetCallback;
      ResetCallback = NULL;
      callback();
    }
  }
  __PIRegs[0] = 2;
}
