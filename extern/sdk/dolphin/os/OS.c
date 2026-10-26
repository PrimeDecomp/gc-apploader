#include "dolphin/os.h"

static OSBootInfo* BootInfo;

u32 OSGetConsoleType(void) {
  if (!BootInfo || BootInfo->consoleType == 0) {
    return OS_CONSOLE_ARTHUR;
  }
  return BootInfo->consoleType;
}
