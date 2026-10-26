#ifndef _DOLPHIN_OSEXEC
#define _DOLPHIN_OSEXEC

#include "dolphin/types.h"

typedef struct {
  BOOL valid;
  u32 restartCode;
  u32 bootDol;
  void* regionStart;
  void* regionEnd;
  int argsUseDefault;
  void* argsAddr;
} OSExecParams;

OSExecParams* __OSExecParams AT_ADDRESS(0x800030F0);

void __OSGetExecParams(OSExecParams* params);

#endif
