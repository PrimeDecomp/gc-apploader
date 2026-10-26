#include <string.h>

#include <dolphin/card.h>
#include <dolphin/os/OSCache.h>

#include "__card.h"

static void WriteCallback(s32 chan, s32 result);
static void EraseCallback(s32 chan, s32 result);

static void WriteCallback(s32 chan, s32 result) {
  CARDControl* card;
  CARDCallback callback;
  u16* fat;
  CARDDir* dir;
  CARDDir* ent;
  CARDFileInfo* fileInfo;

  card = &__CARDBlock[chan];
  if (result < 0) {
    goto error;
  }

  fileInfo = card->fileInfo;
  if (fileInfo->length < 0) {
    result = CARD_RESULT_CANCELED;
    goto error;
  }

  fileInfo->length -= card->sectorSize;
  if (fileInfo->length <= 0) {
    dir = __CARDGetDirBlock(card);
    ent = &dir[fileInfo->fileNo];
    ent->time = (u32)OSTicksToSeconds(OSGetTime());
    callback = card->apiCallback;
    card->apiCallback = 0;
    result = __CARDUpdateDir(chan, callback);
  } else {
    fat = __CARDGetFatBlock(card);
    fileInfo->offset += card->sectorSize;
    fileInfo->iBlock = fat[fileInfo->iBlock];
    if (!CARDIsValidBlockNo(card, fileInfo->iBlock)) {
      result = CARD_RESULT_BROKEN;
      goto error;
    }
    result = __CARDEraseSector(chan, card->sectorSize * (u32)fileInfo->iBlock, EraseCallback);
  }

  if (result < 0) {
    goto error;
  }
  return;

error:
  callback = card->apiCallback;
  card->apiCallback = 0;
  __CARDPutControlBlock(card, result);
  ASSERTLINE(134, callback);
  callback(chan, result);
}

static void EraseCallback(s32 chan, s32 result) {
  CARDControl* card;
  CARDCallback callback;
  CARDFileInfo* fileInfo;

  card = &__CARDBlock[chan];
  if (result < 0) {
    goto error;
  }

  fileInfo = card->fileInfo;
  ASSERTLINE(161, OFFSET(fileInfo->offset, card->sectorSize) == 0);
  result = __CARDWrite(chan, card->sectorSize * (u32)fileInfo->iBlock, card->sectorSize,
                       card->buffer, WriteCallback);
  if (result < 0) {
    goto error;
  }
  return;

error:
  callback = card->apiCallback;
  card->apiCallback = 0;
  __CARDPutControlBlock(card, result);
  ASSERTLINE(175, callback);
  callback(chan, result);
}

s32 CARDWriteAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset,
                   CARDCallback callback) {
  CARDControl* card;
  s32 result;
  CARDDir* dir;
  CARDDir* ent;

  ASSERTLINE(210, buf && ((u32)buf % 32) == 0);
  ASSERTLINE(211, 0 < length);

  result = __CARDSeek(fileInfo, length, offset, &card);
  if (result < 0) {
    return result;
  }

  ASSERTLINE(217, OFFSET(offset, card->sectorSize) == 0);
  ASSERTLINE(218, OFFSET(length, card->sectorSize) == 0);

  if (OFFSET(offset, card->sectorSize) != 0 || OFFSET(length, card->sectorSize) != 0)
    return __CARDPutControlBlock(card, CARD_RESULT_FATAL_ERROR);

  dir = __CARDGetDirBlock(card);
  ent = &dir[fileInfo->fileNo];
#if SDK_VERSION >= 20040521
  result = __CARDIsWritable(card, ent);
#else
  result = __CARDAccess(card, ent);
#endif
  if (result < 0)
    return __CARDPutControlBlock(card, result);

  DCStoreRange((void*)buf, (u32)length);
  card->apiCallback = callback ? callback : __CARDDefaultApiCallback;
  card->buffer = (void*)buf;

  result =
      __CARDEraseSector(fileInfo->chan, card->sectorSize * (u32)fileInfo->iBlock, EraseCallback);
  if (result < 0)
    __CARDPutControlBlock(card, result);
  return result;
}

s32 CARDWrite(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset) {
  s32 result = CARDWriteAsync(fileInfo, buf, length, offset, __CARDSyncCallback);
  if (result < 0) {
    return result;
  }

  return __CARDSync(fileInfo->chan);
}
