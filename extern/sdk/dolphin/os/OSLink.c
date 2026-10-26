#include "dolphin/os/OSModule.h"

OSModuleQueue __OSModuleInfoList AT_ADDRESS(0x800030C8);
const void* __OSStringTable AT_ADDRESS(0x800030D0);

void __OSModuleInit(void) {
  __OSModuleInfoList.head = __OSModuleInfoList.tail = 0;
  __OSStringTable = 0;
}
