#include <ansi_fp.h>
#include <float.h>
#include <math.h>

const double bit_values[] = {
    1e1, 1e2, 1e4, 1e8, 1e16, 1e32, 1e64, 1e128, 1e256,
};

const double digit_values[] = {
    1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8,
};

static double ten = 10.0;

void __num2dec(const decform* f, double x, decimal* d) {
  int binaryExponent;
  int exp;
  unsigned char* p;
  int digits;
  int scaleExponent;
  double scale;
  int count;
  int chunkDigits;
  int extraDigits;
  int chunkValue;
  int paddingCount;
  const double* powers;

  digits = f->digits;
  if (digits > 16) {
    digits = 16;
  }

  d->sgn = 0;
  d->exp = 0;
  d->sig.length = 1;

  if (x == 0.0) {
    d->sig.text[0] = '0';
    return;
  }

  if (!isfinite(x)) {
    d->sig.text[0] = isnan(x) ? 'N' : 'I';
    return;
  }

  d->sig.length = 0;
  if (x < 0.0) {
    x = -x;
    d->sgn = 1;
  }

  frexp(x, &binaryExponent);
  scaleExponent = (binaryExponent * 301029) / 1000000; // log_10(2)
  exp = scaleExponent;
  powers = bit_values;
  if (scaleExponent < 0) {
    scaleExponent = -scaleExponent;
    while (scaleExponent != 0) {
      if (scaleExponent & 1) {
        x *= *powers;
      }
      scaleExponent >>= 1;
      powers++;
    }
  } else if (scaleExponent > 0) {
    scale = 1.0f;
    while (scaleExponent != 0) {
      if (scaleExponent & 1) {
        scale *= *powers;
      }
      scaleExponent >>= 1;
      powers++;
    }
    x /= scale;
  }

  while (x >= 1.0) {
    x *= 0.1;
    exp += 1;
  }

  while (x < 0.1) {
    x *= 10.0;
    exp -= 1;
  }

  p = d->sig.text;
  while (digits != 0) {
    chunkDigits = digits;
    if (digits > 8) {
      chunkDigits = 8;
    }
    d->sig.length += chunkDigits;
    digits -= chunkDigits;
    exp -= chunkDigits;
    p += chunkDigits;
    x *= digit_values[chunkDigits - 1];
    chunkValue = (int)x;
    x = x - chunkValue;

    count = chunkDigits + 1;
    while (--count != 0) {
      *--p = '0' + (chunkValue % 10);
      chunkValue /= 10;
    }
    p += chunkDigits;
  }

  digits = f->digits;
  if (f->digits > 36) {
    digits = 36;
  }

  extraDigits = digits - d->sig.length;
  if (extraDigits > 0) {
    paddingCount = extraDigits + 1;
    while (--paddingCount != 0) {
      *p++ = '0';
    }
    exp -= extraDigits;
    d->sig.length += extraDigits;
  }

  d->exp = exp;
}

double __dec2num(const decimal* d) {
  double num = 0.0;
  int sgn = d->sgn;
  int exp = d->exp;
  int length = d->sig.length;
  char first_digit = d->sig.text[0];
  double divisor;
  double scale;
  const double* p;
  int ndig;
  int ival;
  int i;
  int scaleExponent;
  const unsigned char* s;

  if (length < 1 || first_digit == '0') {
    return 0.0;
  } else if (first_digit == 'I') {
    return sgn ? -HUGE_VAL : HUGE_VAL;
  } else if (first_digit == 'N') {
    return DBL_NAN;
  }

  if (length > 16) {
    exp = length + exp;
    length = 16;
    exp -= 16;
  }

  ndig = length % 8;
  s = d->sig.text;
  if (ndig == 0) {
    ndig = 8;
  }
  exp += length - 1;
  divisor = pow(ten, length - 1);

  while (ndig != 0) {
    ival = 0;
    i = ndig + 1;
    while (--i) {
      ival = ival * 10 + (*s++ - '0');
    }

    length -= ndig;
    num = num * 100000000.0 + ival;
    if (length == 0) {
      break;
    }
    ndig = 8;
  }

  num = num / divisor;
  scaleExponent = __labs(exp);
  p = bit_values;
  if (scaleExponent > 0x1FF) {
    if (exp < 0) {
      return 0.0;
    } else {
      return sgn ? -HUGE_VAL : HUGE_VAL;
    }
  }

  scale = 1.0;
  while (scaleExponent != 0) {
    if (scaleExponent & 1) {
      if (scale > DBL_MAX / *p) {
        return sgn ? -HUGE_VAL : HUGE_VAL;
      }
      scale *= *p;
    }
    scaleExponent >>= 1;
    p++;
  }

  if (exp < 0) {
    if (num < DBL_MIN * scale) {
      return 0.0;
    }
    num /= scale;
  } else if (exp > 0) {
    if (num > DBL_MAX / scale) {
      return sgn ? -HUGE_VAL : HUGE_VAL;
    }
    num *= scale;
  }

  return sgn ? -num : num;
}
