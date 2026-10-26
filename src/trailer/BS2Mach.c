#include "Apploader.h"
#include "BS2.h"
#include "__os.h"
#include "dolphin/hw_regs.h"
#include "dolphin/os/OSCache.h"
#include "dolphin/pad.h"

static BS2State BootState;
static ApploaderInitCallback AppLoaderInit;
static ApploaderMainCallback AppLoaderMain;
static ApploaderFinalCallback AppLoaderFinal;
static DVDDiskID DiskID ATTRIBUTE_ALIGN(32);
static ApploaderHeader AppLoaderHeader ATTRIBUTE_ALIGN(32);
static DVDDriveInfo DriveInfo ATTRIBUTE_ALIGN(32);
static DVDCommandBlock Block ATTRIBUTE_ALIGN(32);

static void BS2Report(const char* format, ...) {}

static void BS2CoverClosed(u32 intType) {
  __DIRegs[1] = 0;
  BootState = BS2_STATE_COVER_CLOSED;
}

static void BS2DVDCallback(s32 result, DVDCommandBlock* block) {}

/* clang-format off */
static asm void BS2StartProgram(register void* entry, register void* addr, register u32 lines) {
  fralloc
  bl ICFlashInvalidate
  sync
  isync
  mtctr lines
  li r0, 0
  // Fetch the final branch before clearing this code from memory.
  b clearMemory
clearLine:
  dcbz addr, r0
  dcbf addr, r0
  addi addr, addr, 32
  bdnz clearLine
  b startProgram
startProgram:
  mtlr entry
  blr
clearMemory:
  b clearLine
  frfree
  blr
}
/* clang-format on */

void BS2Run(void) {
  __PIRegs[9] = 7;
  while (!PADSync()) {
  }
  OSDisableInterrupts();
  __OSStopAudioSystem();
  BS2StartProgram(AppLoaderFinal(), (void*)0x81300000, 0x20000);
}

static BOOL BS2CheckDVD(DVDCommandBlock* block) {
  switch (block->state) {
  case DVD_STATE_END:
    return TRUE;
  case DVD_STATE_COVER_CLOSED:
    BootState = BS2_STATE_COVER_CLOSED;
    break;
  case DVD_STATE_NO_DISK:
    BootState = BS2_STATE_NO_DISK;
    break;
  case DVD_STATE_COVER_OPEN:
    BootState = BS2_STATE_COVER_OPEN;
    break;
  case DVD_STATE_FATAL_ERROR:
    BootState = BS2_STATE_FATAL_ERROR;
    break;
  case DVD_STATE_RETRY:
    BootState = BS2_STATE_RETRY;
    break;
  case DVD_STATE_MOTOR_STOPPED:
    BootState = BS2_STATE_MOTOR_STOPPED;
    break;
  }
  return FALSE;
}

BS2State BS2Main(void) {
  BOOL enabled;
  void* addr;
  u32 length;
  u32 offset;

  enabled = OSDisableInterrupts();
  switch (BootState) {
  case BS2_STATE_CHECK_DEVICE:
    if ((*(u16*)0x800030e4 & 0xeef) != 0xeef) {
      BootState = BS2_STATE_READ_HEADER;
      break;
    }
  case BS2_STATE_INQUIRE:
    BootState = BS2_STATE_WAIT_INQUIRY;
    DVDInquiryAsync(&Block, &DriveInfo, BS2DVDCallback);
    break;
  case BS2_STATE_WAIT_INQUIRY:
    if (!BS2CheckDVD(&Block)) {
      break;
    }
    if (DriveInfo.deviceCode == 1) {
      OSRestoreInterrupts(enabled);
      CRCMain(0, NULL);
    }
    *(u16*)0x800030e6 = DriveInfo.deviceCode | 0x8000;
    BootState = BS2_STATE_READ_HEADER;
  case BS2_STATE_READ_HEADER:
    DVDReadAbsAsyncForBS(&Block, &AppLoaderHeader, 32, 0x2440, BS2DVDCallback);
    BootState = BS2_STATE_WAIT_HEADER;
    break;
  case BS2_STATE_WAIT_HEADER:
    if (!BS2CheckDVD(&Block)) {
      break;
    }
    BS2Report("  appLoaderLength ...... 0x%x\n", AppLoaderHeader.size);
    BS2Report("  appLoaderFunc1  ...... 0x%x\n", AppLoaderHeader.entry);
    BootState = BS2_STATE_READ_APPLOADER;
  case BS2_STATE_READ_APPLOADER:
    DVDReadAbsAsyncForBS(&Block, (void*)0x81200000, (AppLoaderHeader.size + 31) & ~31, 0x2460,
                         BS2DVDCallback);
    BootState = BS2_STATE_WAIT_APPLOADER;
    break;
  case BS2_STATE_WAIT_APPLOADER:
    if (!BS2CheckDVD(&Block)) {
      break;
    }
    AppLoaderHeader.entry(&AppLoaderInit, &AppLoaderMain, &AppLoaderFinal);
    AppLoaderInit(OSReport);
    BS2Report("\nApploader2 Initialized\n");
    BootState = BS2_STATE_LOAD_SECTION;
  case BS2_STATE_LOAD_SECTION:
    if (AppLoaderMain(&addr, &length, &offset)) {
      BS2Report("Addr [0x%x] length [0x%x] offset [0x%x]\n", addr, length, offset);
      DVDReadAbsAsyncForBS(&Block, addr, length, offset, BS2DVDCallback);
      BootState = BS2_STATE_WAIT_SECTION;
    } else {
      BootState = BS2_STATE_READY;
    }
    break;
  case BS2_STATE_WAIT_SECTION:
    if (BS2CheckDVD(&Block)) {
      BootState = BS2_STATE_LOAD_SECTION;
    }
    break;
  case BS2_STATE_READY:
    DVDLowWaitCoverClose(BS2CoverClosed);
    break;
  case BS2_STATE_COVER_CLOSED:
  case BS2_STATE_NO_DISK:
  case BS2_STATE_COVER_OPEN:
  case 13:
  case 14:
  case BS2_STATE_MOTOR_STOPPED:
  case BS2_STATE_FATAL_ERROR:
  case BS2_STATE_RETRY:
    __OSDoHotReset(ResetCode);
    break;
  case BS2_STATE_IDLE:
    break;
  default:
    OSPanic("BS2Mach.c", 0x2d2, "OSReboot(): ERROR >>> UNKNOWN STATE");
  }
  OSRestoreInterrupts(enabled);
  return BootState;
}
