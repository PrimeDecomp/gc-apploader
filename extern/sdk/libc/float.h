#ifndef _FLOAT_H_
#define _FLOAT_H_

#ifdef __cplusplus
extern "C" {
#endif

extern int __double_min[];
extern int __double_max[];

#define DBL_MIN (*(double*)__double_min)
#define DBL_MAX (*(double*)__double_max)
#define DBL_DIG 15
#define DBL_MANT_DIG 53

#ifdef __cplusplus
}
#endif

#endif
