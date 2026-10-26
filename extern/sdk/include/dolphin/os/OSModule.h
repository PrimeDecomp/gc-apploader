#ifndef _DOLPHIN_OSMODULE
#define _DOLPHIN_OSMODULE

#include "dolphin/types.h"

typedef struct OSModuleInfo OSModuleInfo;

typedef struct OSModuleQueue {
  OSModuleInfo* head;
  OSModuleInfo* tail;
} OSModuleQueue;

void __OSModuleInit(void);

#endif
