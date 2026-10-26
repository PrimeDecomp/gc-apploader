#ifndef _DOLPHIN_OSFONT
#define _DOLPHIN_OSFONT

#include "dolphin/types.h"

#define OS_FONT_ENCODE_ANSI 0u
#define OS_FONT_ENCODE_SJIS 1u
#define OS_FONT_ENCODE_MAX 5u

u16 OSGetFontEncode(void);

#endif
