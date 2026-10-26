#include "Apploader.h"

#if APPLOADER_VERSION >= 20040521
#include "dolphin/os/OSExec.h"

#define RETURNFUNC_MAXSIZE 0x80
#define RETURNFUNC_ADDRESS (0x81300000 - RETURNFUNC_MAXSIZE)
#define APPLOADER_CHECK(condition, message) ((condition) || (Report message, PPCHalt(), FALSE))
#elif !DEBUG
#define APPLOADER_CHECK(condition, message)                                                        \
  ((void)((condition) || (Report message, PPCHalt(), FALSE)))
#else
#define APPLOADER_CHECK(condition, message)                                                        \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      Report message;                                                                              \
      PPCHalt();                                                                                   \
    }                                                                                              \
  } while (0)
#endif

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

#if APPLOADER_VERSION >= 20020328
static BOOL SecondTimeForThePart;
static void* SavedAddress;
static u32 SavedLength;
static u32 SavedOffset;
static ELoadState SavedState;
#endif
#if APPLOADER_VERSION >= 20040521
static ApploaderReport Report;
#endif
static BootBlock2 DiscLayout ATTRIBUTE_ALIGN(32);
static DolHeader Executable ATTRIBUTE_ALIGN(32);
static u32 NextSection;
static ELoadState LoadState;
static u32 ReadStatus;
#if APPLOADER_VERSION >= 20040521
static OSExecParams ExecParams;
#else
static ApploaderReport Report;
#endif
static u32 DiscBootInfo[4] ATTRIBUTE_ALIGN(32);
#if APPLOADER_VERSION >= 20040521
static u32 DolOffset;
#else
#define DolOffset DiscLayout.dolOffset
#endif
static BOOL LoadFstLast;

#if APPLOADER_VERSION >= 20020328
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

#if APPLOADER_VERSION >= 20040521
static void ApploaderEntryImpl(ApploaderInitCallback* init, ApploaderMainCallback* main,
                               ApploaderFinalCallback* final);
static void ApploaderInitImpl(ApploaderReport report);
static BOOL ApploaderMainImpl(void** address, u32* length, u32* offset);
static void* ApploaderFinalImpl(void);
void ReturnTrampolineEnd(void);

/* clang-format off */
static asm void ReturnTrampoline(void) {
  nofralloc
  lis r4, 0x8120
  addi r4, r4, 0
  lis r5, 0x10
  addi r5, r5, 0
  rlwinm r6, r4, 0, 0, 26
  add r7, r4, r5
  subf r7, r6, r7
  addi r7, r7, 31
  srwi r7, r7, 5
  mtctr r7
invalidate:
  icbi r0, r6
  addi r6, r6, 32
  bdnz invalidate
  sync
  isync
  lwz r0, 12(r1)
  mtlr r0
  addi r1, r1, 8
  blr
entry ReturnTrampolineEnd
  nop
}

asm void ApploaderEntry(ApploaderInitCallback* init, ApploaderMainCallback* main,
                        ApploaderFinalCallback* final) {
  nofralloc
  mflr r0
  stwu r1, -8(r1)
  stw r0, 12(r1)
  bl ApploaderEntryImpl
  lis r5, RETURNFUNC_ADDRESS@ha
  addi r5, r5, RETURNFUNC_ADDRESS@l
  mtlr r5
  blr
}

asm void ApploaderInit(ApploaderReport report) {
  nofralloc
  mflr r0
  stwu r1, -8(r1)
  stw r0, 12(r1)
  bl ApploaderInitImpl
  lis r5, RETURNFUNC_ADDRESS@ha
  addi r5, r5, RETURNFUNC_ADDRESS@l
  mtlr r5
  blr
}

asm BOOL ApploaderMain(void** address, u32* length, u32* offset) {
  nofralloc
  mflr r0
  stwu r1, -8(r1)
  stw r0, 12(r1)
  bl ApploaderMainImpl
  lis r5, RETURNFUNC_ADDRESS@ha
  addi r5, r5, RETURNFUNC_ADDRESS@l
  mtlr r5
  blr
}

asm void* ApploaderFinal(void) {
  nofralloc
  mflr r0
  stwu r1, -8(r1)
  stw r0, 12(r1)
  bl ApploaderFinalImpl
  lis r5, RETURNFUNC_ADDRESS@ha
  addi r5, r5, RETURNFUNC_ADDRESS@l
  mtlr r5
  blr
}

/* clang-format on */
#endif

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

#endif

#if APPLOADER_VERSION >= 20040521
static void ApploaderEntryImpl(ApploaderInitCallback* init, ApploaderMainCallback* main,
                               ApploaderFinalCallback* final) {
  void* dest;
  void* src;
  u32 len;
#else
void ApploaderEntry(ApploaderInitCallback* init, ApploaderMainCallback* main,
                    ApploaderFinalCallback* final) {
#endif
  *init = ApploaderInit;
  *main = ApploaderMain;
  *final = ApploaderFinal;
#if APPLOADER_VERSION >= 20040521
  dest = (void*)RETURNFUNC_ADDRESS;
  src = ReturnTrampoline;
  len = (u32)ReturnTrampolineEnd - (u32)ReturnTrampoline;
  APPLOADER_CHECK(len <= RETURNFUNC_MAXSIZE, ("Failed assertion len <= RETURNFUNC_MAXSIZE"));
  memcpy(dest, src, len);
  DCStoreRange(dest, len);
  ICInvalidateRange(dest, len);
#endif
}

#if APPLOADER_VERSION >= 20040521
static void ApploaderInitImpl(ApploaderReport report) {
#else
void ApploaderInit(ApploaderReport report) {
#endif
  memset(&DiscLayout, 0, sizeof(DiscLayout));
  memset(&Executable, 0, sizeof(Executable));
  NextSection = 0;
  LoadState = kLoad_BootBlock;
  ReadStatus = 0;
  Report = report;
#if APPLOADER_VERSION >= 20040521
  __OSGetExecParams(&ExecParams);
  Report("\nApploader Initialized.\n");
  Report("This Apploader built %s %s\n", "May 21 2004", "10:12:56");
#elif APPLOADER_VERSION >= 20020328
  Report("Apploader Initialized.  $Revision: 29 $.\n");
  Report("This Apploader built %s %s\n", "Mar 28 2002", "02:34:54");
#endif
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

#if APPLOADER_VERSION >= 20020328
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

#endif

#if APPLOADER_VERSION >= 20040521
static void ApploaderCopyArgs(void* src, void* dest) {
  u32 args = ((u32*)src)[2];
  ((u32*)dest)[2] = args;
  memcpy((u8*)dest + 0x1000, (u8*)src + 0x1000, 0x1000);
}
#endif

static void ApploaderPrepareDisc(void) {}

static void ApploaderSetBootInfo(BootInfo* bootInfo) {
  bootInfo->bootMagic = 0x0d15ea5e;
  bootInfo->bootVersion = 1;
  bootInfo->arenaLo = 0;
  bootInfo->arenaHi = DiscLayout.fstAddress;
  bootInfo->fstAddress = DiscLayout.fstAddress;
  bootInfo->fstMaxLength = DiscLayout.fstMaxLength;
}

#if APPLOADER_VERSION >= 20040521
static BOOL ApploaderMainImpl(void** address, u32* length, u32* offset) {
#else
BOOL ApploaderMain(void** address, u32* length, u32* offset) {
#endif
  BootInfo* bootInfo = OSPhysicalToCached(0);
  u32 limit;
#if !DEBUG
  u32 monitorSize;
#endif
#if APPLOADER_VERSION >= 20020328
  u32 response[2];
#endif

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
    APPLOADER_CHECK(DiscLayout.fstLength <= DiscLayout.fstMaxLength,
                    ("APPLOADER ERROR >>> FSTLength(%d) in BB2 is greater than FSTMaxLength(%d)\n",
                     DiscLayout.fstLength, DiscLayout.fstMaxLength));
    *address = DiscBootInfo;
    *length = 0x20;
    *offset = 0x440;
    LoadState = kLoad_MemoryLayout;
    DCInvalidateRange(*address, *length);
    break;
  case kLoad_MemoryLayout:
    DEBUG_MONITOR_SIZE = DiscBootInfo[0];
#if !DEBUG
    monitorSize = DEBUG_MONITOR_SIZE;
    DEBUG_MONITOR_ADDRESS = ROUND_DOWN_32(bootInfo->memorySize + 0x80000000 - monitorSize);
    SIMULATED_MEMORY_SIZE = DiscBootInfo[1];
    APPLOADER_CHECK(
        (monitorSize & 31) == 0,
        ("APPLOADER ERROR >>> Debug monitor size (%d) should be a multiple of 32\n", monitorSize));
#else
    DEBUG_MONITOR_ADDRESS = ROUND_DOWN_32(bootInfo->memorySize + 0x80000000 - DEBUG_MONITOR_SIZE);
    SIMULATED_MEMORY_SIZE = DiscBootInfo[1];
    APPLOADER_CHECK((DEBUG_MONITOR_SIZE & 31) == 0,
                    ("APPLOADER ERROR >>> Debug monitor size (%d) should be a multiple of 32\n",
                     DEBUG_MONITOR_SIZE));
#endif
    APPLOADER_CHECK((SIMULATED_MEMORY_SIZE & 31) == 0,
                    ("APPLOADER ERROR >>> Simulated memory size (%d) should be a multiple of 32\n",
                     SIMULATED_MEMORY_SIZE));
    if (SIMULATED_MEMORY_SIZE == 0) {
      SIMULATED_MEMORY_SIZE = bootInfo->memorySize;
    }
    if (SIMULATED_MEMORY_SIZE < bootInfo->memorySize) {
      APPLOADER_CHECK(
          DEBUG_MONITOR_SIZE < bootInfo->memorySize - SIMULATED_MEMORY_SIZE,
          ("APPLOADER ERROR >>> [Physical memory size(0x%x)] - [Console simulated memory "
           "size(0x%x)]\n"
           "APPLOADER ERROR >>> must be greater than debug monitor size(0x%x)\n",
           bootInfo->memorySize, SIMULATED_MEMORY_SIZE, DEBUG_MONITOR_SIZE));
      DiscLayout.fstAddress =
          (void*)ROUND_DOWN_32(SIMULATED_MEMORY_SIZE + 0x80000000 - DiscLayout.fstMaxLength);
#if APPLOADER_VERSION < 20020328
    } else if (SIMULATED_MEMORY_SIZE == bootInfo->memorySize) {
      DiscLayout.fstAddress = (void*)ROUND_DOWN_32(DEBUG_MONITOR_ADDRESS - DiscLayout.fstMaxLength);
    } else {
      APPLOADER_CHECK(
          FALSE, ("APPLOADER ERROR >>> Physical memory size is 0x%x bytes.\n"
                  "APPLOADER ERROR >>> Console simulated memory size must be smaller than or equal "
                  "to the Physical memory size\n",
                  bootInfo->memorySize, SIMULATED_MEMORY_SIZE));
#else
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
#endif
    }
    BOOT_INFO_2 = (u8*)DiscLayout.fstAddress - 0x2000;
    *address = BOOT_INFO_2;
    *length = 0x2000;
    *offset = 0x440;
    LoadState = kLoad_SelectFstOrder;
    DCInvalidateRange(*address, *length);
    break;
  case kLoad_SelectFstOrder:
#if APPLOADER_VERSION >= 20040521
    if (ExecParams.valid) {
      DolOffset = ExecParams.bootDol;
      if (DolOffset == 0) {
        DolOffset = DiscLayout.dolOffset;
      }
      if (!ExecParams.argsUseDefault && ExecParams.argsAddr) {
        ApploaderCopyArgs(ExecParams.argsAddr, BOOT_INFO_2);
      }
    } else {
      DolOffset = DiscLayout.dolOffset;
      LOW_MEMORY(u32, 0x800030ec) = 0;
    }
#endif
    LoadFstLast = DolOffset < DiscLayout.fstOffset;
    if (LoadFstLast) {
      LoadState = kLoad_DolHeader;
      goto loadDolHeader;
    }
    *address = DiscLayout.fstAddress;
    *length = ROUND_UP_32(DiscLayout.fstLength);
    *offset = DiscLayout.fstOffset;
    LoadState = kLoad_DolHeader;
    APPLOADER_CHECK(0x81700000 <= (u32)*address,
                    ("APPLOADER ERROR >>> Illegal FST destination address! (0x%x)\n", *address));
    DCInvalidateRange(*address, *length);
#if APPLOADER_VERSION >= 20020328
    ApploaderLimitRead(address, length, offset, &LoadState);
#endif
    break;
  case kLoad_DolHeader:
  loadDolHeader:
    *address = &Executable;
    *length = sizeof(Executable);
    *offset = DolOffset;
    LoadState = kLoad_ValidateDol;
    DCInvalidateRange(*address, *length);
    break;
  case kLoad_ValidateDol: {
    u32 size = ApploaderGetDolSize();
#if !DEBUG
    if ((limit = ((u32*)BOOT_INFO_2)[10])
#else
    limit = ((u32*)BOOT_INFO_2)[10];
    if (limit
#endif
#if APPLOADER_VERSION >= 20040521
        && !ExecParams.valid
#endif
    ) {
      APPLOADER_CHECK(
          size <= limit,
          ("APPLOADER ERROR >>> Total size of text/data sections of the dol file are too big "
           "(%d(0x%08x) bytes). Currently the limit is set as %d(0x%08x) bytes\n",
           size, size, limit, limit));
    }
    if (LoadFstLast) {
      LOADED_IMAGE_SIZE = size + ROUND_UP_32(DiscLayout.fstLength);
    } else {
      LOADED_IMAGE_SIZE = size;
    }
#if APPLOADER_VERSION < 20020328
    APPLOADER_CHECK(Executable.bssAddress + Executable.bssSize <= 0x81200000,
                    ("APPLOADER ERROR >>> Too big BSS segment! (0x%x - 0x%x)\n",
                     Executable.bssAddress, Executable.bssAddress + Executable.bssSize));
#else
#if APPLOADER_VERSION >= 20040521
    if (!ExecParams.valid) {
#endif
      if (!(OSGetConsoleType() & 0x10000000)) {
        APPLOADER_CHECK(
            ApploaderCheckDolBounds(0x80700000),
            ("APPLOADER ERROR >>> One of the sections in the dol file exceeded its boundary. All "
             "the sections should not exceed 0x%08x (production mode)\n",
             0x80700000));
      } else {
        if (!ApploaderCheckDolBounds(0x80700000)) {
          Report("\n\nAPPLOADER WARNING >>> One of the sections in the dol file exceeded its "
                 "boundary. All the sections should not exceed 0x%08x in production mode.\n\n",
                 0x80700000);
        }
        APPLOADER_CHECK(
            ApploaderCheckDolBounds(0x81200000),
            ("APPLOADER ERROR >>> One of the sections in the dol file exceeded its boundary. All "
             "the sections should not exceed 0x%08x (development mode).\n",
             0x81200000));
      }
#if APPLOADER_VERSION >= 20040521
    }
#endif
#endif
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
    *offset = DolOffset + Executable.textOffsets[NextSection];
#if APPLOADER_VERSION >= 20040521
    if (!ExecParams.valid) {
#endif
      APPLOADER_CHECK((u32)*address + *length <= 0x81200000,
                      ("APPLOADER ERROR >>> Too big text segment! (0x%x - 0x%x)\n", *address,
                       (u8*)*address + *length));
#if APPLOADER_VERSION >= 20040521
    }
#endif
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
    *offset = Executable.dataOffsets[NextSection] + DolOffset;
#if APPLOADER_VERSION >= 20040521
    if (!ExecParams.valid) {
#endif
      APPLOADER_CHECK((u32)*address + *length <= 0x81200000,
                      ("APPLOADER ERROR >>> Too big data segment! (0x%x - 0x%x)\n", *address,
                       (u8*)*address + *length));
#if APPLOADER_VERSION >= 20040521
    }
#endif
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
      APPLOADER_CHECK(0x81700000 <= (u32)*address,
                      ("APPLOADER ERROR >>> Illegal FST destination address! (0x%x)\n", *address));
      DCInvalidateRange(*address, *length);
#if APPLOADER_VERSION >= 20020328
      ApploaderLimitRead(address, length, offset, &LoadState);
#endif
      break;
    }
    LoadState = kLoad_BootInfo;
    /* fallthrough */
  case kLoad_BootInfo:
    ApploaderSetBootInfo(bootInfo);
    LoadState = kLoad_Done;
    /* fallthrough */
  case kLoad_Done:
#if APPLOADER_VERSION >= 20020328
#if APPLOADER_VERSION >= 20040521
    if (!ExecParams.valid) {
#endif
      ApploaderReadController(3, response);
      PAD_3_BUTTON = ((u16*)response)[0];
#if APPLOADER_VERSION >= 20040521
    }
#endif
#endif
    return FALSE;
#if APPLOADER_VERSION >= 20020328
  case kLoad_ReadRemainder:
    APPLOADER_CHECK(SecondTimeForThePart == TRUE,
                    ("Failed assertion SecondTimeForThePart == TRUE"));
    ApploaderLimitRead(address, length, offset, &LoadState);
    break;
#endif
  default:
    return FALSE;
  }
  return TRUE;
}

#if APPLOADER_VERSION >= 20040521
static void* ApploaderFinalImpl(void) { return Executable.entry; }
#else
void* ApploaderFinal(void) { return Executable.entry; }
#endif
