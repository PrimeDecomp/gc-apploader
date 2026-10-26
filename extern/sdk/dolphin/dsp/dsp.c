#include <dolphin/dsp.h>
#include <dolphin/hw_regs.h>
#include <stddef.h>

#include "__dsp.h"

#if SDK_VERSION >= 20040521
#define BUILD_DATE "May 21 2004"
#define BUILD_TIME "09:29:15"
const char* __DSPVersion =
    "<< Dolphin SDK - DSP\trelease build: " BUILD_DATE " " BUILD_TIME " (0x2301) >>";
#else
#define BUILD_DATE "Mar 28 2002"
#define BUILD_TIME "02:11:16"
#endif

static BOOL __DSP_init_flag;

u32 DSPCheckMailToDSP(void) { return (__DSPRegs[0] & (1 << 15)) >> 15; }

u32 DSPCheckMailFromDSP(void) { return (__DSPRegs[2] & (1 << 15)) >> 15; }

u32 DSPReadCPUToDSPMbox(void) { return (__DSPRegs[0] << 16) | __DSPRegs[1]; }

u32 DSPReadMailFromDSP(void) { return (__DSPRegs[2] << 16) | __DSPRegs[3]; }

void DSPSendMailToDSP(u32 mail) {
  __DSPRegs[0] = mail >> 16;
  __DSPRegs[1] = mail & 0xFFFF;
}

void DSPAssertInt(void) {
  BOOL old;
  u16 tmp;

  old = OSDisableInterrupts();
  tmp = __DSPRegs[5];
  tmp = (tmp & ~0xA8) | 2;
  __DSPRegs[5] = tmp;
  OSRestoreInterrupts(old);
}

void DSPInit(void) {
  BOOL old;
  u16 tmp;

  __DSP_debug_printf("DSPInit(): Build Date: %s %s\n", BUILD_DATE, BUILD_TIME);

  if (__DSP_init_flag == TRUE)
    return;

#if SDK_VERSION >= 20040521
  OSRegisterVersion(__DSPVersion);
#endif

  old = OSDisableInterrupts();
  __OSSetInterruptHandler(__OS_INTERRUPT_DSP_DSP, __DSPHandler);
  __OSUnmaskInterrupts(OS_INTERRUPTMASK_DSP_DSP);

  tmp = __DSPRegs[5];
  tmp = (tmp & ~0xA8) | 0x800;
  __DSPRegs[5] = tmp;

  tmp = __DSPRegs[5];
  __DSPRegs[5] = tmp = tmp & ~0xAC;

  __DSP_first_task = __DSP_last_task = __DSP_curr_task = __DSP_tmp_task = NULL;
  __DSP_init_flag = TRUE;

  OSRestoreInterrupts(old);
}

BOOL DSPCheckInit(void) { return __DSP_init_flag; }

void DSPReset(void) {
  BOOL old;
  u16 tmp;

  old = OSDisableInterrupts();
  tmp = __DSPRegs[5];
  tmp = (tmp & ~0xA8) | 0x800 | 1;
  __DSPRegs[5] = tmp;
  __DSP_init_flag = FALSE;
  OSRestoreInterrupts(old);
}

void DSPHalt(void) {
  BOOL old;
  u16 tmp;

  old = OSDisableInterrupts();
  tmp = __DSPRegs[5];
  tmp = (tmp & ~0xA8) | 4;
  __DSPRegs[5] = tmp;
  OSRestoreInterrupts(old);
}

void DSPUnhalt(void) {
  BOOL old;
  u16 tmp;

  old = OSDisableInterrupts();
  tmp = __DSPRegs[5];
  tmp = (tmp & ~0xAC);
  __DSPRegs[5] = tmp;
  OSRestoreInterrupts(old);
}

u32 DSPGetDMAStatus(void) { return (__DSPRegs[5] & (1 << 9)); }

DSPTaskInfo* DSPAddTask(DSPTaskInfo* task) {
  BOOL old;

  ASSERTMSGLINE(556, __DSP_init_flag == TRUE, "DSPAddTask(): DSP driver not initialized!\n");

  old = OSDisableInterrupts();

  __DSP_insert_task(task);
  task->state = DSP_TASK_STATE_INIT;
  task->flags = DSP_TASK_FLAG_ATTACHED;

  OSRestoreInterrupts(old);
  if (task == __DSP_first_task)
    __DSP_boot_task(task);
  return task;
}

DSPTaskInfo* DSPCancelTask(DSPTaskInfo* task) {
  BOOL old;

  ASSERTMSGLINE(592, __DSP_init_flag == TRUE, "DSPCancelTask(): DSP driver not initialized!\n");

  old = OSDisableInterrupts();

  task->flags |= DSP_TASK_FLAG_CANCEL;

  OSRestoreInterrupts(old);
  return task;
}

DSPTaskInfo* DSPAssertTask(DSPTaskInfo* task) {
  s32 old;

  ASSERTMSGLINE(623, __DSP_init_flag == TRUE, "DSPAssertTask(): DSP driver not initialized!\n");
  ASSERTMSGLINE(624, task->flags & DSP_TASK_FLAG_ATTACHED,
                "DSPAssertTask(): Specified task not in active task list!\n");

  old = OSDisableInterrupts();

  if (__DSP_curr_task == task) {
    __DSP_rude_task = task;
    __DSP_rude_task_pending = TRUE;
    OSRestoreInterrupts(old);
    return task;
  }

  if (task->priority < __DSP_curr_task->priority) {
    __DSP_rude_task = task;
    __DSP_rude_task_pending = TRUE;
    if (__DSP_curr_task->state == DSP_TASK_STATE_RUN) {
      DSPAssertInt();
    }
    OSRestoreInterrupts(old);
    return task;
  }

  OSRestoreInterrupts(old);
  return NULL;
}
