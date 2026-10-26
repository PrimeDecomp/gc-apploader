#ifndef _DOLPHIN_OSLOWMEM
#define _DOLPHIN_OSLOWMEM

#include "dolphin/os.h"
#include "dolphin/os/OSModule.h"

typedef struct {
  OSContext* currentContext;
  OSThread* floatingPointThread;
  OSThreadQueue activeThreads;
  OSThread* currentThread;
} OSLowMemContext;

typedef struct {
  __OSExceptionHandler exceptions[__OS_EXCEPTION_MAX];
  u32 reserved;
  __OSInterruptHandler interrupts[__OS_INTERRUPT_MAX];
} OSHandlerTables;

typedef struct {
  OSModuleQueue modules;
  const void* stringTable;
} OSModuleState;

typedef struct {
  u16 wirelessPadFixMode;
  u8 reboot;
  u8 resetButtonTimer;
  u16 pad3Buttons;
  u16 deviceCode;
} OSLowMemPad;

OSBootInfo __OSBootInfo AT_ADDRESS(0x80000000);
u8 __OSDebuggerWork[0x8c] AT_ADDRESS(0x80000040);
OSLowMemContext __OSLowMemContext AT_ADDRESS(0x800000d4);
void* __OSBootInfo2 AT_ADDRESS(0x800000f4);
OSHandlerTables __OSHandlerTables AT_ADDRESS(0x80003000);
OSModuleState __OSModuleState AT_ADDRESS(0x800030c8);
u32 __OSLoadedImageSize AT_ADDRESS(0x800030d4);
OSLowMemPad __OSLowMemPad AT_ADDRESS(0x800030e0);
u16 __OSPad3Buttons AT_ADDRESS(0x800030e4);
u32 __OSReserved30ec AT_ADDRESS(0x800030ec);

#endif
