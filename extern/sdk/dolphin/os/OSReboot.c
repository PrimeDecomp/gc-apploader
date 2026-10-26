#include "__os.h"
#include "dolphin/os/OSExec.h"

void OSGetSavedRegion(void** start, void** end) {
  *start = __OSRebootParams.regionStart;
  *end = __OSRebootParams.regionEnd;
}
