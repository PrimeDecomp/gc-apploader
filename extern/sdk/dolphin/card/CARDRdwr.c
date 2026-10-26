#include <dolphin/card.h>

#include "__card.h"

static void BlockReadCallback(s32 chan, s32 result);
static void BlockWriteCallback(s32 chan, s32 result);

static void BlockReadCallback(s32 chan, s32 result) {
  CARDControl* card;
  CARDCallback callback;

  card = &__CARDBlock[chan];

  if (result >= 0) {
    card->xferred += CARD_SEG_SIZE;
    card->addr += CARD_SEG_SIZE;
    card->buffer = (u8*)card->buffer + CARD_SEG_SIZE;

    if (--card->repeat > 0) {
      result = __CARDReadSegment(chan, BlockReadCallback);
      if (result >= 0) {
        return;
      }
    }
  }

  if (!card->apiCallback) {
    __CARDPutControlBlock(card, result);
  }

  callback = card->xferCallback;
  if (callback) {
    card->xferCallback = NULL;
    callback(chan, result);
  }
}

s32 __CARDRead(s32 chan, u32 addr, s32 length, void* dst, CARDCallback callback) {
  CARDControl* card;

#if SDK_VERSION >= 20040521
  ASSERTLINE(91, 0 < length && length % CARD_SEG_SIZE == 0);
  ASSERTLINE(92, 0 <= chan && chan < 2);
#else
  ASSERTLINE(88, 0 < length && length % CARD_SEG_SIZE == 0);
  ASSERTLINE(89, 0 <= chan && chan < 2);
#endif

  card = &__CARDBlock[chan];
  if (card->attached == 0) {
    return CARD_RESULT_NOCARD;
  }
  card->xferCallback = callback;
  card->repeat = length / CARD_SEG_SIZE;
  card->addr = addr;
  card->buffer = dst;
  return __CARDReadSegment(chan, BlockReadCallback);
}

static void BlockWriteCallback(s32 chan, s32 result) {
  CARDControl* card;
  CARDCallback callback;

  card = &__CARDBlock[chan];
  if (result >= 0) {
#if SDK_VERSION >= 20040521
    card->xferred += card->pageSize;
    card->addr += card->pageSize;
    card->buffer = (u8*)card->buffer + card->pageSize;
#else
    card->xferred += CARD_PAGE_SIZE;
    card->addr += CARD_PAGE_SIZE;
    card->buffer = (u8*)card->buffer + CARD_PAGE_SIZE;
#endif

    if (--card->repeat > 0) {
      result = __CARDWritePage(chan, BlockWriteCallback);
      if (result >= 0) {
        return;
      }
    }
  }

  if (!card->apiCallback) {
    __CARDPutControlBlock(card, result);
  }

  callback = card->xferCallback;
  if (callback) {
    card->xferCallback = NULL;
    callback(chan, result);
  }
}

s32 __CARDWrite(s32 chan, u32 addr, s32 length, void* dst, CARDCallback callback) {
  CARDControl* card;
  card = &__CARDBlock[chan];

#if SDK_VERSION >= 20040521
  ASSERTLINE(153, 0 < length && length % card->pageSize == 0);
  ASSERTLINE(154, 0 <= chan && chan < 2);
#else
  ASSERTLINE(149, 0 < length && length % CARD_PAGE_SIZE == 0);
  ASSERTLINE(150, 0 <= chan && chan < 2);
#endif

  if (card->attached == 0) {
    return CARD_RESULT_NOCARD;
  }
  card->xferCallback = callback;
#if SDK_VERSION >= 20040521
  card->repeat = length / card->pageSize;
#else
  card->repeat = length / CARD_PAGE_SIZE;
#endif
  card->addr = addr;
  card->buffer = dst;
  return __CARDWritePage(chan, BlockWriteCallback);
}

s32 CARDGetXferredBytes(s32 chan) {
#if SDK_VERSION >= 20040521
  ASSERTLINE(183, 0 <= chan && chan < 2);
#else
  ASSERTLINE(180, 0 <= chan && chan < 2);
#endif
  return __CARDBlock[chan].xferred;
}
