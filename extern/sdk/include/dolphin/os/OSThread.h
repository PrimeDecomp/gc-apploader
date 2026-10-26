#ifndef _DOLPHIN_OSTHREAD
#define _DOLPHIN_OSTHREAD

#include "dolphin/os/OSContext.h"
#include "dolphin/types.h"

typedef s32 OSPriority;

typedef struct OSThread OSThread;
typedef struct OSMutex OSMutex;
typedef struct OSThreadQueue OSThreadQueue;
typedef struct OSMutexQueue OSMutexQueue;
typedef struct OSThreadLink OSThreadLink;
typedef struct OSMutexLink OSMutexLink;

struct OSThreadQueue {
  OSThread* head;
  OSThread* tail;
};

struct OSThreadLink {
  OSThread* next;
  OSThread* prev;
};

struct OSMutexQueue {
  OSMutex* head;
  OSMutex* tail;
};

struct OSMutexLink {
  OSMutex* next;
  OSMutex* prev;
};

struct OSThread {
  /* 0x000 */ OSContext context;
  /* 0x2C8 */ u16 state;
  /* 0x2CA */ u16 attr;
  /* 0x2CC */ s32 suspend;
  /* 0x2D0 */ OSPriority priority;
  /* 0x2D4 */ OSPriority base;
  /* 0x2D8 */ void* val;
  /* 0x2DC */ OSThreadQueue* queue;
  /* 0x2E0 */ OSThreadLink link;
  /* 0x2E8 */ OSThreadQueue queueJoin;
  /* 0x2F0 */ OSMutex* mutex;
  /* 0x2F4 */ OSMutexQueue queueMutex;
  /* 0x2FC */ OSThreadLink linkActive;
  /* 0x304 */ u8* stackBase;
  /* 0x308 */ u32* stackEnd;
  /* 0x30C */ s32 error;
  /* 0x310 */ void* specific[2];
};

void OSCancelThread(OSThread* thread);

s32 OSEnableScheduler(void);
s32 OSDisableScheduler(void);
void __OSReschedule(void);

#endif
