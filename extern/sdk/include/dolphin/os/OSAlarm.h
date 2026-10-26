#ifndef _DOLPHIN_OSALARM
#define _DOLPHIN_OSALARM

#include "dolphin/os/OSContext.h"
#include "dolphin/os/OSTime.h"

typedef struct OSAlarm OSAlarm;
typedef void (*OSAlarmHandler)(OSAlarm* alarm, OSContext* context);

struct OSAlarm {
  OSAlarmHandler handler;
#if SDK_VERSION >= 20040521
  u32 tag;
#endif
  OSTime fire;
  OSAlarm* prev;
  OSAlarm* next;
  OSTime period;
  OSTime start;
};

BOOL OSCheckAlarmQueue(void);
void OSInitAlarm(void);
void OSCreateAlarm(OSAlarm* alarm);
void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler);
void OSSetAbsAlarm(OSAlarm* alarm, OSTime time, OSAlarmHandler handler);
void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler);
void OSCancelAlarm(OSAlarm* alarm);
#if SDK_VERSION >= 20040521
void OSSetAlarmTag(OSAlarm* alarm, u32 tag);
void OSCancelAlarms(u32 tag);
#endif

#endif
