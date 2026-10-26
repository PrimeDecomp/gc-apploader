#ifndef _DOLPHIN_CARD_H_
#define _DOLPHIN_CARD_H_

#include <dolphin/dsp.h>
#include <dolphin/dvd.h>
#include <dolphin/os.h>
#include <dolphin/os/OSAlarm.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CARD_FILENAME_MAX 32
#define CARD_MAX_FILE 127
#define CARD_ICON_MAX 8

typedef void (*CARDCallback)(s32 chan, s32 result);

typedef struct CARDFileInfo {
  s32 chan;
  s32 fileNo;
  s32 offset;
  s32 length;
  u16 iBlock;
} CARDFileInfo;

typedef struct CARDDir {
  u8 gameName[4];
  u8 company[2];
  u8 _padding0;
  u8 bannerFormat;
  u8 fileName[CARD_FILENAME_MAX];
  u32 time;     // seconds since 01/01/2000 midnight
  u32 iconAddr; // 0xffffffff if not used
  u16 iconFormat;
  u16 iconSpeed;
  u8 permission;
  u8 copyTimes;
  u16 startBlock;
  u16 length;
  u8 _padding1[2];
  u32 commentAddr; // 0xffffffff if not used
} CARDDir;

typedef struct CARDControl {
  BOOL attached;
  s32 result;
  u16 size;
  u16 pageSize;
  s32 sectorSize;
  u16 cBlock;
  u16 vendorID;
  s32 latency;
  u8 id[12];
  int mountStep;
  int formatStep;
  u32 scramble;
  DSPTaskInfo task;
  void* workArea;
  CARDDir* currentDir;
  u16* currentFat;
  OSThreadQueue threadQueue;
  u8 cmd[9];
  s32 cmdlen;
  volatile u32 mode;
  int retry;
  int repeat;
  u32 addr;
  void* buffer;
  s32 xferred;
  u16 freeNo;
  u16 startBlock;
  CARDFileInfo* fileInfo;
  CARDCallback extCallback;
  CARDCallback txCallback;
  CARDCallback exiCallback;
  CARDCallback apiCallback;
  CARDCallback xferCallback;
  CARDCallback eraseCallback;
  CARDCallback unlockCallback;
  OSAlarm alarm;
#if SDK_VERSION >= 20020328
  u32 cid;
  const DVDDiskID* diskID;
#endif
} CARDControl;

typedef struct CARDDecParam {
  u8* inputAddr;
  u32 inputLength;
  u32 aramAddr;
  u8* outputAddr;
} CARDDecParam;

typedef struct CARDID {
  u8 serial[32];
  u16 deviceID;
  u16 size;
  u16 encode;
  u8 padding[470];
  u16 checkSum;
  u16 checkSumInv;
} CARDID;

typedef struct CARDDirCheck {
  u8 padding0[56];
  u16 padding1;
  s16 checkCode;
  u16 checkSum;
  u16 checkSumInv;
} CARDDirCheck;

typedef struct CARDStat {
  char fileName[CARD_FILENAME_MAX];
  u32 length;
  u32 time;
  u8 gameName[4];
  u8 company[2];
  u8 bannerFormat;
  u32 iconAddr;
  u16 iconFormat;
  u16 iconSpeed;
  u32 commentAddr;
  u32 offsetBanner;
  u32 offsetBannerTlut;
  u32 offsetIcon[CARD_ICON_MAX];
  u32 offsetIconTlut;
  u32 offsetData;
} CARDStat;

#define CARD_ATTR_PUBLIC 0x04u
#define CARD_ATTR_NO_COPY 0x08u
#define CARD_ATTR_NO_MOVE 0x10u
#define CARD_ATTR_GLOBAL 0x20u
#define CARD_ATTR_COMPANY 0x40u

#define CARD_FAT_AVAIL 0x0000u
#define CARD_FAT_CHECKSUM 0x0000u
#define CARD_FAT_CHECKSUMINV 0x0001u
#define CARD_FAT_CHECKCODE 0x0002u
#define CARD_FAT_FREEBLOCKS 0x0003u
#define CARD_FAT_LASTSLOT 0x0004u

#define CARD_WORKAREA_SIZE (5 * 8 * 1024)

#define CARD_SEG_SIZE 0x200u
#define CARD_PAGE_SIZE 0x80u
#define CARD_MAX_SIZE 0x01000000U

#define CARD_NUM_SYSTEM_BLOCK 5
#define CARD_SYSTEM_BLOCK_SIZE (8 * 1024u)

#define CARD_MAX_MOUNT_STEP (CARD_NUM_SYSTEM_BLOCK + 2)

#define CARD_STAT_SPEED_END 0
#define CARD_STAT_SPEED_FAST 1
#define CARD_STAT_SPEED_MIDDLE 2
#define CARD_STAT_SPEED_SLOW 3
#define CARD_STAT_SPEED_MASK 3

#define CARD_RESULT_UNLOCKED 1
#define CARD_RESULT_READY 0
#define CARD_RESULT_BUSY -1
#define CARD_RESULT_WRONGDEVICE -2
#define CARD_RESULT_NOCARD -3
#define CARD_RESULT_NOFILE -4
#define CARD_RESULT_IOERROR -5
#define CARD_RESULT_BROKEN -6
#define CARD_RESULT_EXIST -7
#define CARD_RESULT_NOENT -8
#define CARD_RESULT_INSSPACE -9
#define CARD_RESULT_NOPERM -10
#define CARD_RESULT_LIMIT -11
#define CARD_RESULT_NAMETOOLONG -12
#define CARD_RESULT_ENCODING -13
#define CARD_RESULT_CANCELED -14
#define CARD_RESULT_FATAL_ERROR -128

#define CARDIsValidBlockNo(card, blockNo)                                                          \
  ((blockNo) >= CARD_NUM_SYSTEM_BLOCK && (blockNo) < (card)->cBlock)

#define CARD_READ_SIZE 512
#define CARD_COMMENT_SIZE 64

#define CARD_ICON_WIDTH 32
#define CARD_ICON_HEIGHT 32

#define CARD_BANNER_WIDTH 96
#define CARD_BANNER_HEIGHT 32

#define CARD_STAT_ICON_NONE 0
#define CARD_STAT_ICON_C8 1
#define CARD_STAT_ICON_RGB5A3 2
#define CARD_STAT_ICON_MASK 3

#define CARD_STAT_BANNER_NONE 0
#define CARD_STAT_BANNER_C8 1
#define CARD_STAT_BANNER_RGB5A3 2
#define CARD_STAT_BANNER_MASK 3

#define CARD_ENCODE_ANSI 0
#define CARD_ENCODE_SJIS 1

#define CARDGetDirCheck(dir) ((CARDDirCheck*)&(dir)[CARD_MAX_FILE])
#define CARDGetBannerFormat(stat) (((stat)->bannerFormat) & CARD_STAT_BANNER_MASK)
#define CARDGetIconAnim(stat) (((stat)->bannerFormat) & CARD_STAT_ANIM_MASK)
#define CARDGetIconFormat(stat, n) (((stat)->iconFormat >> (2 * (n))) & CARD_STAT_ICON_MASK)
#define CARDGetIconSpeed(stat, n) (((stat)->iconSpeed >> (2 * (n))) & CARD_STAT_SPEED_MASK)
#define CARDSetBannerFormat(stat, f)                                                               \
  ((stat)->bannerFormat = (u8)(((stat)->bannerFormat & ~CARD_STAT_BANNER_MASK) | (f)))
#define CARDSetIconAnim(stat, f)                                                                   \
  ((stat)->bannerFormat = (u8)(((stat)->bannerFormat & ~CARD_STAT_ANIM_MASK) | (f)))
#define CARDSetIconFormat(stat, n, f)                                                              \
  ((stat)->iconFormat =                                                                            \
       (u16)(((stat)->iconFormat & ~(CARD_STAT_ICON_MASK << (2 * (n)))) | ((f) << (2 * (n)))))
#define CARDSetIconSpeed(stat, n, f)                                                               \
  ((stat)->iconSpeed =                                                                             \
       (u16)(((stat)->iconSpeed & ~(CARD_STAT_SPEED_MASK << (2 * (n)))) | ((f) << (2 * (n)))))
#define CARDSetIconAddress(stat, addr) ((stat)->iconAddr = (u32)(addr))
#define CARDSetCommentAddress(stat, addr) ((stat)->commentAddr = (u32)(addr))
#define CARDGetFileNo(fileInfo) ((fileInfo)->fileNo)

extern u32 __CARDFreq;

#if DEBUG
#define CARDFreq __CARDFreq
#else
#define CARDFreq EXI_FREQ_16M
#endif

// CARDBios
void CARDInit(void);
s32 CARDGetResultCode(s32 chan);
s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed);
s32 CARDGetEncoding(s32 chan, u16* encode);
s32 CARDGetMemSize(s32 chan, u16* size);
s32 CARDGetSectorSize(s32 chan, u32* size);
const DVDDiskID* CARDGetDiskID(s32 chan);
s32 CARDSetDiskID(s32 chan, const DVDDiskID* diskID);
BOOL CARDSetFastMode(BOOL enable);
BOOL CARDGetFastMode(void);
s32 CARDGetCurrentMode(s32 chan, u32* mode);

// CARDCheck
s32 CARDCheckExAsync(s32 chan, s32* xferBytes, CARDCallback callback);
s32 CARDCheckAsync(s32 chan, CARDCallback callback);
s32 CARDCheckEx(s32 chan, s32* xferBytes);
s32 CARDCheck(s32 chan);

// CARDCreate
s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo,
                    CARDCallback callback);
s32 CARDCreate(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo);

// CARDDelete
s32 CARDFastDeleteAsync(s32 chan, s32 fileNo, CARDCallback callback);
s32 CARDFastDelete(s32 chan, s32 fileNo);
s32 CARDDeleteAsync(s32 chan, const char* fileName, CARDCallback callback);
s32 CARDDelete(s32 chan, const char* fileName);

// CARDErase
s32 CARDEraseAsync(CARDFileInfo* fileInfo, s32 length, s32 offset, CARDCallback callback);
s32 CARDErase(CARDFileInfo* fileInfo, s32 length, s32 offset);

// CARDFormat
s32 CARDFormat(s32 chan);

// CARDMount
int CARDProbe(s32 chan);
s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize);
s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback,
                   CARDCallback attachCallback);
s32 CARDMount(s32 chan, void* workArea, CARDCallback detachCallback);
s32 CARDUnmount(s32 chan);

// CARDNet
u16 CARDSetVendorID(u16 vendorID);
u16 CARDGetVendorID(void);
s32 CARDGetSerialNo(s32 chan, u64* serialNo);
s32 CARDGetUniqueCode(s32 chan, u64* uniqueCode);
s32 CARDGetAttributes(s32 chan, s32 fileNo, u8* attr);
s32 CARDSetAttributesAsync(s32 chan, s32 fileNo, u8 attr, CARDCallback callback);
s32 CARDSetAttributes(s32 chan, s32 fileNo, u8 attr);

// CARDOpen
s32 CARDFastOpen(s32 chan, s32 fileNo, CARDFileInfo* fileInfo);
s32 CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo);
s32 CARDClose(CARDFileInfo* fileInfo);

// CARDProgram
s32 CARDProgramAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset,
                     CARDCallback callback);
s32 CARDProgram(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset);

// CARDRdwr
s32 CARDGetXferredBytes(s32 chan);

// CARDRead
s32 CARDReadAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset, CARDCallback callback);
s32 CARDRead(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset);
s32 CARDCancel(CARDFileInfo* fileInfo);

// CARDRename
s32 CARDRenameAsync(s32 chan, const char* old, const char* new, CARDCallback callback);
s32 CARDRename(s32 chan, const char* old, const char* new);

// CARDStat
s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat);
s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat* stat, CARDCallback callback);
s32 CARDSetStatus(s32 chan, s32 fileNo, CARDStat* stat);

// CARDWrite
s32 CARDWriteAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset,
                   CARDCallback callback);
s32 CARDWrite(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset);

#ifdef __cplusplus
}
#endif

#endif
