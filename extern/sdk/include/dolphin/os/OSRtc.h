#ifndef _DOLPHIN_OSRTC
#define _DOLPHIN_OSRTC

#include "dolphin/types.h"

typedef struct OSSram {
  u16 checkSum;
  u16 checkSumInv;
  u32 ead0;
  u32 ead1;
  u32 counterBias;
  s8 displayOffsetH;
  u8 ntd;
  u8 language;
  u8 flags;
} OSSram;

typedef struct OSSramEx {
  u8 flashID[2][12];
  u32 wirelessKeyboardID;
  u16 wirelessPadID[4];
  u8 dvdErrorCode;
  u8 _padding0;
  u8 flashIDCheckSum[2];
#if SDK_VERSION >= 20040521
  u16 gbs;
  u8 _padding1[2];
#else
  u8 _padding1[4];
#endif
} OSSramEx;

#define SRAM_SIZE (sizeof(OSSram) + sizeof(OSSramEx))

typedef struct SramControl {
  u8 sram[SRAM_SIZE];
  u32 offset;
  BOOL enabled;
  BOOL locked;
  int sync;
  void (*callback)(void);
} SramControl;

void __OSInitSram(void);
OSSram* __OSLockSram(void);
OSSramEx* __OSLockSramEx(void);
int __OSUnlockSram(BOOL commit);
int __OSUnlockSramEx(BOOL commit);
int __OSSyncSram(void);
int __OSReadROM(void* buffer, s32 length, s32 offset);
u8 OSGetLanguage(void);
u16 OSGetWirelessID(s32 chan);
void OSSetWirelessID(s32 chan, u16 id);
#if SDK_VERSION >= 20040521
u16 OSGetGbsMode(void);
void OSSetGbsMode(u16 mode);
#endif

#endif
