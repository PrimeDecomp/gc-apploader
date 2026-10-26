#ifndef _DOLPHIN_OSRESET
#define _DOLPHIN_OSRESET

#include "dolphin/types.h"

#define OS_RESETCODE_RESTART 0x80000000

typedef struct OSResetFunctionInfo OSResetFunctionInfo;
typedef BOOL (*OSResetFunction)(BOOL final);

struct OSResetFunctionInfo {
  OSResetFunction func;
  u32 priority;
  OSResetFunctionInfo* next;
  OSResetFunctionInfo* prev;
};

typedef struct OSResetFunctionQueue {
  OSResetFunctionInfo* head;
  OSResetFunctionInfo* tail;
} OSResetFunctionQueue;

void OSRegisterResetFunction(OSResetFunctionInfo* info);
u32 OSGetResetCode(void);

#endif
