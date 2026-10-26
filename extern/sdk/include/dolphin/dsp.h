#ifndef _DOLPHIN_DSP_H_
#define _DOLPHIN_DSP_H_

#include <dolphin/os.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DSP_TASK_FLAG_CLEARALL 0x00000000
#define DSP_TASK_FLAG_ATTACHED 0x00000001
#define DSP_TASK_FLAG_CANCEL 0x00000002

#define DSP_TASK_STATE_INIT 0
#define DSP_TASK_STATE_RUN 1
#define DSP_TASK_STATE_YIELD 2
#define DSP_TASK_STATE_DONE 3

typedef void (*DSPCallback)(void* task);

typedef struct STRUCT_DSP_TASK {
  volatile u32 state;
  volatile u32 priority;
  volatile u32 flags;
  u16* iram_mmem_addr;
  u32 iram_length;
  u32 iram_addr;
  u16* dram_mmem_addr;
  u32 dram_length;
  u32 dram_addr;
  u16 dsp_init_vector;
  u16 dsp_resume_vector;
  DSPCallback init_cb;
  DSPCallback res_cb;
  DSPCallback done_cb;
  DSPCallback req_cb;
  struct STRUCT_DSP_TASK* next;
  struct STRUCT_DSP_TASK* prev;
  OSTime t_context;
  OSTime t_task;
} DSPTaskInfo;

u32 DSPCheckMailToDSP(void);
u32 DSPCheckMailFromDSP(void);
u32 DSPReadCPUToDSPMbox(void);
u32 DSPReadMailFromDSP(void);
void DSPSendMailToDSP(u32 mail);
void DSPAssertInt(void);
void DSPInit(void);
BOOL DSPCheckInit(void);
void DSPReset(void);
void DSPHalt(void);
void DSPUnhalt(void);
u32 DSPGetDMAStatus(void);
DSPTaskInfo* DSPAddTask(DSPTaskInfo* task);
DSPTaskInfo* DSPCancelTask(DSPTaskInfo* task);
DSPTaskInfo* DSPAssertTask(DSPTaskInfo* task);

DSPTaskInfo* __DSPGetCurrentTask(void);

#ifdef __cplusplus
}
#endif

#endif
