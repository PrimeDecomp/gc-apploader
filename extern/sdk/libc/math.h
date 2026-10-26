#ifndef _MATH_H_
#define _MATH_H_

#ifdef __cplusplus
extern "C" {
#endif

#if __option(little_endian)
#define __HI(x) (*(1 + (int*)&(x)))
#define __LO(x) (*(int*)&(x))
#else
#define __HI(x) (*(int*)&(x))
#define __LO(x) (*(1 + (int*)&(x)))
#endif

#define FP_NAN 1
#define FP_INFINITE 2
#define FP_ZERO 3
#define FP_NORMAL 4
#define FP_SUBNORMAL 5

static inline int __fpclassifyf(float x) {
  switch ((*(int*)&x) & 0x7f800000) {
  case 0x7f800000: {
    if ((*(int*)&x) & 0x007fffff)
      return FP_NAN;
    else
      return FP_INFINITE;
    break;
  }
  case 0: {
    if ((*(int*)&x) & 0x007fffff)
      return FP_SUBNORMAL;
    else
      return FP_ZERO;
    break;
  }
  }
  return FP_NORMAL;
}
static inline int __fpclassifyd(double x) {
  switch (__HI(x) & 0x7ff00000) {
  case 0x7ff00000: {
    if ((__HI(x) & 0x000fffff) || (__LO(x) & 0xffffffff))
      return FP_NAN;
    else
      return FP_INFINITE;
    break;
  }
  case 0: {
    if ((__HI(x) & 0x000fffff) || (__LO(x) & 0xffffffff))
      return FP_SUBNORMAL;
    else
      return FP_ZERO;
    break;
  }
  }
  return FP_NORMAL;
}

#define fpclassify(x)                                                                              \
  (sizeof(x) == sizeof(float) ? __fpclassifyf((float)(x)) : __fpclassifyd((double)(x)))
#define isnan(x) (fpclassify(x) == FP_NAN)
#define isinf(x) (fpclassify(x) == FP_INFINITE)
#define isnormal(x) (fpclassify(x) == FP_NORMAL)
#define isfinite(x) (fpclassify(x) > FP_INFINITE)

extern int __double_huge[];
extern int __double_nan[];

#define HUGE_VAL (*(double*)__double_huge)
#define DBL_NAN (*(double*)__double_nan)

double frexp(double x, int* eptr);
double pow(double x, double y);

#ifdef __cplusplus
}
#endif

#endif
