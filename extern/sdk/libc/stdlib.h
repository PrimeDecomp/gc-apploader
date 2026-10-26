#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int mbtowc(wchar_t* pwc, const char* s, size_t n);
size_t wcstombs(char* s, const wchar_t* pwcs, size_t n);

#ifdef __cplusplus
}
#endif

#endif
