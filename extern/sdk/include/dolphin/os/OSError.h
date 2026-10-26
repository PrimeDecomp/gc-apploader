#ifndef _DOLPHIN_OSERROR
#define _DOLPHIN_OSERROR

void OSPanic(const char* file, int line, const char* msg, ...);

#ifdef DEBUG
#define ASSERTMSGLINE(line, cond, msg) ((cond) || (OSPanic(__FILE__, line, msg), 0))
#else
#define ASSERTMSGLINE(line, cond, msg) ((void)0)
#endif

#endif
