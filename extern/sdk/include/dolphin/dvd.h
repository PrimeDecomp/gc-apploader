#ifndef _DOLPHIN_DVD
#define _DOLPHIN_DVD

#include "dolphin/types.h"

typedef struct DVDDiskID {
  char gameName[4];
  char company[2];
  u8 diskNumber;
  u8 gameVersion;
  u8 streaming;
  u8 streamingBufSize;
  u8 padding[22];
} DVDDiskID;

typedef struct DVDCommandBlock DVDCommandBlock;
typedef void (*DVDCBCallback)(s32 result, DVDCommandBlock* block);
struct DVDCommandBlock {
  DVDCommandBlock* next;
  DVDCommandBlock* prev;
  u32 command;
  s32 state;
  u32 offset;
  u32 length;
  void* addr;
  u32 currTransferSize;
  u32 transferredSize;
  DVDDiskID* id;
  DVDCBCallback callback;
  void* userData;
};
typedef struct DVDDriveInfo {
  u16 revisionLevel;
  u16 deviceCode;
  u32 releaseDate;
  u8 padding[24];
} DVDDriveInfo;

void DVDInit(void);
struct OSAlarm;
BOOL __DVDTestAlarm(const struct OSAlarm* alarm);
BOOL DVDCheckDisk(void);
s32 DVDGetCommandBlockStatus(DVDCommandBlock* block);
BOOL DVDSetAutoInvalidation(BOOL autoInval);
int DVDSetAutoFatalMessaging(BOOL enable);
BOOL DVDLowWaitCoverClose(void (*callback)(u32 intType));
BOOL DVDInquiryAsync(DVDCommandBlock* block, DVDDriveInfo* info, DVDCBCallback callback);
BOOL DVDReadAbsAsyncForBS(DVDCommandBlock* block, void* addr, s32 length, s32 offset,
                          DVDCBCallback callback);

#define DVD_STATE_FATAL_ERROR -1
#define DVD_STATE_END 0
#define DVD_STATE_BUSY 1
#define DVD_STATE_WAITING 2
#define DVD_STATE_COVER_CLOSED 3
#define DVD_STATE_NO_DISK 4
#define DVD_STATE_COVER_OPEN 5
#define DVD_STATE_WRONG_DISK 6
#define DVD_STATE_MOTOR_STOPPED 7
#define DVD_STATE_PAUSING 8
#define DVD_STATE_IGNORED 9
#define DVD_STATE_CANCELED 10
#define DVD_STATE_RETRY 11

#endif
