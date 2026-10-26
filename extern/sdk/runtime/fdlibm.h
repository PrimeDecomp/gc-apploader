/* @(#)fdlibm.h 1.2 95/01/04 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
#ifndef _FDLIBM_H
#define _FDLIBM_H

#if __option(little_endian)
#define __HI(x) (*(1 + (int*)&(x)))
#define __LO(x) (*(int*)&(x))
#else
#define __HI(x) (*(int*)&(x))
#define __LO(x) (*(1 + (int*)&(x)))
#endif

double frexp(double x, int* eptr);

#endif
