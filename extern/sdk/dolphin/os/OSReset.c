#include "dolphin/os/OSReset.h"
#include "__os.h"
#include "dolphin/base/PPCArch.h"
#include "dolphin/hw_regs.h"
#include "dolphin/os/OSCache.h"
#include "dolphin/os/OSExec.h"
#include "dolphin/pad.h"

#define ENQUEUE_INFO(info, queue)                                                                  \
  do {                                                                                             \
    OSResetFunctionInfo* __prev = (queue)->tail;                                                   \
    if (__prev == 0) {                                                                             \
      (queue)->head = (info);                                                                      \
    } else {                                                                                       \
      __prev->next = (info);                                                                       \
    }                                                                                              \
    (info)->prev = __prev;                                                                         \
    (info)->next = 0;                                                                              \
    (queue)->tail = (info);                                                                        \
  } while (0);

#define ENQUEUE_INFO_PRIO(info, queue)                                                             \
  do {                                                                                             \
    OSResetFunctionInfo* __prev;                                                                   \
    OSResetFunctionInfo* __next;                                                                   \
    for (__next = (queue)->head; __next && (__next->priority <= (info)->priority);                 \
         __next = __next->next)                                                                    \
      ;                                                                                            \
                                                                                                   \
    if (__next == 0) {                                                                             \
      ENQUEUE_INFO(info, queue);                                                                   \
    } else {                                                                                       \
      (info)->next = __next;                                                                       \
      __prev = __next->prev;                                                                       \
      __next->prev = (info);                                                                       \
      (info)->prev = __prev;                                                                       \
      if (__prev == 0) {                                                                           \
        (queue)->head = (info);                                                                    \
      } else {                                                                                     \
        __prev->next = (info);                                                                     \
      }                                                                                            \
    }                                                                                              \
  } while (0);

static OSResetFunctionQueue ResetFunctionQueue;

void OSRegisterResetFunction(OSResetFunctionInfo* info) {
  ENQUEUE_INFO_PRIO(info, &ResetFunctionQueue);
}

#if SDK_VERSION >= 20040521
int __OSCallResetFunctions(BOOL final) {
  OSResetFunctionInfo* info;
  int err;
  u32 priority;

  priority = 0;
  err = 0;

  for (info = ResetFunctionQueue.head; info != 0;) {
    if (err != 0 && priority != info->priority)
      break;
    err |= !info->func(final);
    priority = info->priority;
    info = info->next;
  }

  err |= !__OSSyncSram();
  if (err) {
    return 0;
  }
  return 1;
}
#endif

/* clang-format off */
static asm void Reset(u32 resetCode) {
    nofralloc
    b L_000001BC
L_000001A0:
    mfspr r8, HID0
    ori r8, r8, 0x8
    mtspr HID0, r8
    isync
    sync
    nop
    b L_000001C0
L_000001BC:
    b L_000001DC
L_000001C0:
    mftb r5, 268
L_000001C4:
    mftb r6, 268
    subf r7, r5, r6
    cmplwi r7, 0x1124
    blt L_000001C4
    nop
    b L_000001E0
L_000001DC:
    b L_000001FC
L_000001E0:
    lis r8, 0xcc00
    ori r8, r8, 0x3000
    li r4, 0x3
    stw r4, 0x24(r8)
    stw r3, 0x24(r8)
    nop
    b L_00000200
L_000001FC:
    b L_00000208
L_00000200:
    nop
    b L_00000200
L_00000208:
    b L_000001A0
}
/* clang-format on */

#if SDK_VERSION >= 20040521
static void KillThreads(void) {
  OSThread* thread;
  OSThread* next;

  for (thread = __OSActiveThreadQueue.head; thread; thread = next) {
    next = thread->linkActive.next;
    switch (thread->state) {
    case 1:
    case 4:
      OSCancelThread(thread);
      continue;
    default:
      continue;
    }
  }
}
#endif

void __OSDoHotReset(u32 resetCode) {
  OSDisableInterrupts();
  __VIRegs[1] = 0;
  ICFlashInvalidate();
  Reset(resetCode * 8);
}

#if SDK_VERSION >= 20040521
void __OSShutdownDevices(BOOL doRecal) {
  int rc;
  BOOL disableRecalibration;

  __OSStopAudioSystem();

  if (!doRecal) {
    disableRecalibration = __PADDisableRecalibration(TRUE);
  }

  do {
  } while (!__OSCallResetFunctions(FALSE));
  do {
  } while (!__OSSyncSram());

  OSDisableInterrupts();

  rc = __OSCallResetFunctions(TRUE);

  LCDisable();
  if (!doRecal) {
    __PADDisableRecalibration(disableRecalibration);
  }

  KillThreads();
}
u32 OSGetResetCode(void) {
  u32 resetCode;
  if (__OSRebootParams.valid)
    resetCode = 0x80000000 | __OSRebootParams.restartCode;
  else
    resetCode = (__PIRegs[9] & 0xFFFFFFF8) / 8;

  return resetCode;
}

#else
u32 OSGetResetCode(void) {
  if (*(volatile u8*)OSPhysicalToCached(0x30e2) != 0) {
    return OS_RESETCODE_RESTART;
  }
  return (__PIRegs[9] & 0xfffffff8) / 8;
}
#endif
