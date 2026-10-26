#include "Apploader.h"

#define ROUND_UP_32(value) (((value) + 31) & ~31)
#define ROUND_DOWN_32(value) ((value) & ~31)
#define LOW_MEMORY(type, address) (*(type*)(address))
#define DEBUG_MONITOR_SIZE LOW_MEMORY(u32, 0x800000e8)
#define DEBUG_MONITOR_ADDRESS LOW_MEMORY(u32, 0x800000ec)
#define SIMULATED_MEMORY_SIZE LOW_MEMORY(u32, 0x800000f0)
#define BOOT_INFO_2 LOW_MEMORY(void*, 0x800000f4)
#define LOADED_IMAGE_SIZE LOW_MEMORY(u32, 0x800030d4)
#define PAD_3_BUTTON LOW_MEMORY(u16, 0x800030e4)
volatile u32 __SIRegs[64] : 0xcc006400;
#define SERIAL_REGISTERS __SIRegs

typedef enum {
  kLoad_BootBlock = 0,
  kLoad_BootBlockAlias = 1,
  kLoad_ValidateBootBlock = 2,
  kLoad_MemoryLayout = 3,
  kLoad_SelectFstOrder = 4,
  kLoad_DolHeader = 5,
  kLoad_ValidateDol = 6,
  kLoad_Text = 7,
  kLoad_Data = 8,
  kLoad_FinalFst = 9,
  kLoad_BootInfo = 10,
  kLoad_Done = 11,
  kLoad_ReadRemainder = 12
} ELoadState;

static BOOL SecondTimeForThePart;
static void* SavedAddress;
static u32 SavedLength;
static u32 SavedOffset;
static ELoadState SavedState;
static BootBlock2 DiscLayout ATTRIBUTE_ALIGN(32);
static DolHeader Executable ATTRIBUTE_ALIGN(32);
static u32 NextSection;
static ELoadState LoadState;
static u32 ReadStatus;
static ApploaderReport Report;
static u32 DiscBootInfo[4] ATTRIBUTE_ALIGN(32);
static BOOL LoadFstLast;

static void ApploaderLimitRead(void** address, u32* length, u32* offset, ELoadState* state) {
  if (SecondTimeForThePart) {
    SecondTimeForThePart = FALSE;
    *address = SavedAddress;
    *length = SavedLength;
    *offset = SavedOffset;
    *state = SavedState;
  } else if (*length > 0x20000) {
    SecondTimeForThePart = TRUE;
    SavedAddress = (u8*)*address + 0x20000;
    SavedLength = *length - 0x20000;
    SavedOffset = *offset + 0x20000;
    SavedState = *state;
    *length = 0x20000;
    *state = kLoad_ReadRemainder;
  }
}

static void ApploaderReadController(s32 channel, u32* response) {
  if ((SERIAL_REGISTERS[12] & (0x80 >> channel)) &&
      (SERIAL_REGISTERS[3 * channel] & ~0x700) == 0x00400000) {
    response[0] = SERIAL_REGISTERS[3 * channel + 1];
    response[1] = SERIAL_REGISTERS[3 * channel + 2];
    return;
  }
  response[0] = response[1] = 0;
}

u32 OSGetConsoleType(void) {
  BootInfo* bootInfo = OSPhysicalToCached(0);
  return bootInfo->consoleType;
}

void ApploaderEntry(ApploaderInitCallback* init, ApploaderMainCallback* main,
                    ApploaderFinalCallback* final) {
  *init = ApploaderInit;
  *main = ApploaderMain;
  *final = ApploaderFinal;
}

void ApploaderInit(ApploaderReport report) {
  memset(&DiscLayout, 0, sizeof(DiscLayout));
  memset(&Executable, 0, sizeof(Executable));
  NextSection = 0;
  LoadState = kLoad_BootBlock;
  ReadStatus = 0;
  Report = report;
  Report("Apploader Initialized.  $Revision: 29 $.\n");
  Report("This Apploader built %s %s\n", "Mar 28 2002", "02:34:54");
}

static u32 ApploaderGetDolSize(void) {
  u32 size = 0;
  u32 i;
  for (i = 0; i < 7; ++i) {
    if (Executable.textOffsets[i]) {
      size += ROUND_UP_32(Executable.textSizes[i]);
    }
  }
  for (i = 0; i < 11; ++i) {
    if (Executable.dataOffsets[i]) {
      size += ROUND_UP_32(Executable.dataSizes[i]);
    }
  }
  return size;
}

static BOOL ApploaderCheckDolBounds(u32 limit) {
  u32 end;
  int i;
  for (i = 0; i < 7; ++i) {
    if (Executable.textOffsets[i]) {
      end = Executable.textAddresses[i] + Executable.textSizes[i];
      if (!(0x81100000 <= Executable.textAddresses[i] && end <= 0x81130000)) {
        if (limit < end) {
          return FALSE;
        }
      }
    }
  }
  for (i = 0; i < 11; ++i) {
    if (Executable.dataOffsets[i]) {
      end = Executable.dataAddresses[i] + Executable.dataSizes[i];
      if (!(0x81100000 <= Executable.dataAddresses[i] && end <= 0x81130000)) {
        if (limit < end) {
          return FALSE;
        }
      }
    }
  }
  end = Executable.bssAddress + Executable.bssSize;
  if (0x81100000 <= Executable.bssAddress && end <= 0x81130000) {
    return TRUE;
  }
  if (limit < end) {
    return FALSE;
  }
  return TRUE;
}

static void ApploaderPrepareDisc(void) {}

static void ApploaderSetBootInfo(BootInfo* bootInfo) {
  bootInfo->bootMagic = 0x0d15ea5e;
  bootInfo->bootVersion = 1;
  bootInfo->arenaLo = 0;
  bootInfo->arenaHi = DiscLayout.fstAddress;
  bootInfo->fstAddress = DiscLayout.fstAddress;
  bootInfo->fstMaxLength = DiscLayout.fstMaxLength;
}

BOOL ApploaderMain(void** address, u32* length, u32* offset) {
  BootInfo* bootInfo = OSPhysicalToCached(0);
  u32 limit;
  u32 response[2];

  switch (LoadState) {
  case kLoad_BootBlock:
  case kLoad_BootBlockAlias:
    *address = &DiscLayout;
    *length = 0x20;
    *offset = 0x420;
    LoadState = kLoad_ValidateBootBlock;
    DCInvalidateRange(*address, *length);
    break;
  case kLoad_ValidateBootBlock:
    ApploaderPrepareDisc();
    if (DiscLayout.fstLength > DiscLayout.fstMaxLength) {
      Report("APPLOADER ERROR >>> FSTLength(%d) in BB2 is greater than FSTMaxLength(%d)\n",
             DiscLayout.fstLength, DiscLayout.fstMaxLength);
      PPCHalt();
    }
    *address = DiscBootInfo;
    *length = 0x20;
    *offset = 0x440;
    LoadState = kLoad_MemoryLayout;
    DCInvalidateRange(*address, *length);
    break;
  case kLoad_MemoryLayout:
    DEBUG_MONITOR_SIZE = DiscBootInfo[0];
    DEBUG_MONITOR_ADDRESS = ROUND_DOWN_32(bootInfo->memorySize + 0x80000000 - DEBUG_MONITOR_SIZE);
    SIMULATED_MEMORY_SIZE = DiscBootInfo[1];
    if (DEBUG_MONITOR_SIZE & 31) {
      Report("APPLOADER ERROR >>> Debug monitor size (%d) should be a multiple of 32\n",
             DEBUG_MONITOR_SIZE);
      PPCHalt();
    }
    if (SIMULATED_MEMORY_SIZE & 31) {
      Report("APPLOADER ERROR >>> Simulated memory size (%d) should be a multiple of 32\n",
             SIMULATED_MEMORY_SIZE);
      PPCHalt();
    }
    if (SIMULATED_MEMORY_SIZE == 0) {
      SIMULATED_MEMORY_SIZE = bootInfo->memorySize;
    }
    if (SIMULATED_MEMORY_SIZE < bootInfo->memorySize) {
      if (DEBUG_MONITOR_SIZE >= bootInfo->memorySize - SIMULATED_MEMORY_SIZE) {
        Report("APPLOADER ERROR >>> [Physical memory size(0x%x)] - [Console simulated memory "
               "size(0x%x)]\n"
               "APPLOADER ERROR >>> must be greater than debug monitor size(0x%x)\n",
               bootInfo->memorySize, SIMULATED_MEMORY_SIZE, DEBUG_MONITOR_SIZE);
        PPCHalt();
      }
      DiscLayout.fstAddress =
          (void*)ROUND_DOWN_32(SIMULATED_MEMORY_SIZE + 0x80000000 - DiscLayout.fstMaxLength);
    } else {
      if (SIMULATED_MEMORY_SIZE > bootInfo->memorySize) {
        Report("APPLOADER WARNING >>> Physical memory size is 0x%x bytes.\n"
               "APPLOADER WARNING >>> Console simulated memory size (0x%x) must be smaller than or "
               "equal to the Physical memory size\n"
               "APPLOADER WARNING >>> Use physical memory size as the console simulated memory "
               "size.....\n",
               bootInfo->memorySize, SIMULATED_MEMORY_SIZE);
        SIMULATED_MEMORY_SIZE = bootInfo->memorySize;
      }
      DiscLayout.fstAddress = (void*)ROUND_DOWN_32(DEBUG_MONITOR_ADDRESS - DiscLayout.fstMaxLength);
    }
    BOOT_INFO_2 = (u8*)DiscLayout.fstAddress - 0x2000;
    *address = BOOT_INFO_2;
    *length = 0x2000;
    *offset = 0x440;
    LoadState = kLoad_SelectFstOrder;
    DCInvalidateRange(*address, *length);
    break;
  case kLoad_SelectFstOrder:
    LoadFstLast = DiscLayout.dolOffset < DiscLayout.fstOffset;
    if (LoadFstLast) {
      LoadState = kLoad_DolHeader;
      goto loadDolHeader;
    }
    *address = DiscLayout.fstAddress;
    *length = ROUND_UP_32(DiscLayout.fstLength);
    *offset = DiscLayout.fstOffset;
    LoadState = kLoad_DolHeader;
    if (0x81700000 > (u32)*address) {
      Report("APPLOADER ERROR >>> Illegal FST destination address! (0x%x)\n", *address);
      PPCHalt();
    }
    DCInvalidateRange(*address, *length);
    ApploaderLimitRead(address, length, offset, &LoadState);
    break;
  case kLoad_DolHeader:
  loadDolHeader:
    *address = &Executable;
    *length = sizeof(Executable);
    *offset = DiscLayout.dolOffset;
    LoadState = kLoad_ValidateDol;
    DCInvalidateRange(*address, *length);
    break;
  case kLoad_ValidateDol: {
    u32 size = ApploaderGetDolSize();
    limit = ((u32*)BOOT_INFO_2)[10];
    if (limit && size > limit) {
      Report("APPLOADER ERROR >>> Total size of text/data sections of the dol file are too big "
             "(%d(0x%08x) bytes). Currently the limit is set as %d(0x%08x) bytes\n",
             size, size, limit, limit);
      PPCHalt();
    }
    if (LoadFstLast) {
      LOADED_IMAGE_SIZE = size + ROUND_UP_32(DiscLayout.fstLength);
    } else {
      LOADED_IMAGE_SIZE = size;
    }
    if (!(OSGetConsoleType() & 0x10000000)) {
      if (!ApploaderCheckDolBounds(0x80700000)) {
        Report("APPLOADER ERROR >>> One of the sections in the dol file exceeded its boundary. All "
               "the sections should not exceed 0x%08x (production mode)\n",
               0x80700000);
        PPCHalt();
      }
    } else {
      if (!ApploaderCheckDolBounds(0x80700000)) {
        Report("\n\nAPPLOADER WARNING >>> One of the sections in the dol file exceeded its "
               "boundary. All the sections should not exceed 0x%08x in production mode.\n\n",
               0x80700000);
      }
      if (!ApploaderCheckDolBounds(0x81200000)) {
        Report("APPLOADER ERROR >>> One of the sections in the dol file exceeded its boundary. All "
               "the sections should not exceed 0x%08x (development mode).\n",
               0x81200000);
        PPCHalt();
      }
    }
    memset((void*)Executable.bssAddress, 0, Executable.bssSize);
    DCFlushRange((void*)Executable.bssAddress, Executable.bssSize);
    LoadState = kLoad_Text;
  }
    /* fallthrough */
  case kLoad_Text:
    while (!Executable.textOffsets[NextSection] && NextSection < 7) {
      ++NextSection;
    }
    if (NextSection == 7) {
      LoadState = kLoad_Data;
      NextSection = 0;
      goto loadData;
    }
    *address = (void*)Executable.textAddresses[NextSection];
    *length = ROUND_UP_32(Executable.textSizes[NextSection]);
    *offset = DiscLayout.dolOffset + Executable.textOffsets[NextSection];
    if ((u32)*address + *length > 0x81200000) {
      Report("APPLOADER ERROR >>> Too big text segment! (0x%x - 0x%x)\n", *address,
             (u8*)*address + *length);
      PPCHalt();
    }
    ++NextSection;
    DCInvalidateRange(*address, *length);
    break;
  case kLoad_Data:
  loadData:
    while (!Executable.dataOffsets[NextSection] && NextSection < 11) {
      ++NextSection;
    }
    if (NextSection == 11) {
      LoadState = kLoad_FinalFst;
      NextSection = 0;
      goto loadFinalFst;
    }
    *address = (void*)Executable.dataAddresses[NextSection];
    *length = ROUND_UP_32(Executable.dataSizes[NextSection]);
    *offset = Executable.dataOffsets[NextSection] + DiscLayout.dolOffset;
    if ((u32)*address + *length > 0x81200000) {
      Report("APPLOADER ERROR >>> Too big data segment! (0x%x - 0x%x)\n", *address,
             (u8*)*address + *length);
      PPCHalt();
    }
    ++NextSection;
    DCInvalidateRange(*address, *length);
    break;
  case kLoad_FinalFst:
  loadFinalFst:
    if (LoadFstLast) {
      *address = DiscLayout.fstAddress;
      *length = ROUND_UP_32(DiscLayout.fstLength);
      *offset = DiscLayout.fstOffset;
      LoadState = kLoad_BootInfo;
      if (0x81700000 > (u32)*address) {
        Report("APPLOADER ERROR >>> Illegal FST destination address! (0x%x)\n", *address);
        PPCHalt();
      }
      DCInvalidateRange(*address, *length);
      ApploaderLimitRead(address, length, offset, &LoadState);
      break;
    }
    LoadState = kLoad_BootInfo;
    /* fallthrough */
  case kLoad_BootInfo:
    ApploaderSetBootInfo(bootInfo);
    LoadState = kLoad_Done;
    /* fallthrough */
  case kLoad_Done:
    ApploaderReadController(3, response);
    PAD_3_BUTTON = ((u16*)response)[0];
    return FALSE;
  case kLoad_ReadRemainder:
    if (SecondTimeForThePart != TRUE) {
      Report("Failed assertion SecondTimeForThePart == TRUE");
      PPCHalt();
    }
    ApploaderLimitRead(address, length, offset, &LoadState);
    break;
  default:
    return FALSE;
  }
  return TRUE;
}

void* ApploaderFinal(void) { return Executable.entry; }
