#include "dolphin/os/OSExec.h"
#include "string.h"

void __OSGetExecParams(OSExecParams* params) {
  if (0x80000000 <= (u32)__OSExecParams) {
    memcpy(params, __OSExecParams, sizeof(OSExecParams));
  } else {
    params->valid = FALSE;
  }
}
