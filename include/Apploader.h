#ifndef _APPLOADER
#define _APPLOADER

#include "dolphin/os.h"
#include "dolphin/types.h"
#include "string.h"

typedef void (*ApploaderReport)(const char* format, ...);
typedef void (*ApploaderInitCallback)(ApploaderReport report);
typedef BOOL (*ApploaderMainCallback)(void** address, u32* length, u32* offset);
typedef void* (*ApploaderFinalCallback)(void);

typedef void (*ApploaderEntryCallback)(ApploaderInitCallback*, ApploaderMainCallback*,
                                       ApploaderFinalCallback*);
typedef struct {
  char date[16];
  ApploaderEntryCallback entry;
  u32 size;
  u32 trailerSize;
  u32 padding;
} ApploaderHeader;

typedef struct {
  u32 textOffsets[7];
  u32 dataOffsets[11];
  u32 textAddresses[7];
  u32 dataAddresses[11];
  u32 textSizes[7];
  u32 dataSizes[11];
  u32 bssAddress;
  u32 bssSize;
  void* entry;
  u32 reserved[7];
} DolHeader;

typedef struct {
  u32 dolOffset;
  u32 fstOffset;
  u32 fstLength;
  u32 fstMaxLength;
  void* fstAddress;
  u32 reserved[3];
} BootBlock2;

u32 OSGetConsoleType(void);
void PPCHalt(void);
void DCInvalidateRange(void* address, u32 length);
void DCFlushRange(void* address, u32 length);
void DCStoreRange(void* address, u32 length);
void ICInvalidateRange(void* address, u32 length);
void ApploaderEntry(ApploaderInitCallback* init, ApploaderMainCallback* main,
                    ApploaderFinalCallback* final);
void ApploaderInit(ApploaderReport report);
BOOL ApploaderMain(void** address, u32* length, u32* offset);
void* ApploaderFinal(void);
#endif
