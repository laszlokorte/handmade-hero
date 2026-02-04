
#include "handmade_types.h"
#define PI 3.14159265358979323846f

#define abs(x) ((x) >= 0 ? (x) : -(x))
#define sinf(x) handmade_sinf(x)
#define powf(x, y) powf_approx(x, y)
#define sqrtf(x) powf_approx(x, 0.5)
internal inline real32 fmodf(real32 x, real32 y) {
  if (y == 0.0f)
    return 0.0f; // undefined in C, pick something

  int q = (int)(x / y); // trunc toward zero (C cast rule)
  return x - (real32)q * y;
}
internal inline real32 handmade_sinf(real32 x) {
  double sign = 1;
  if (x < 0) {
    sign = -1.0;
    x = -x;
  }

  while (x < -2 * PI)
    x += 2 * PI;
  while (x > 2 * PI)
    x -= 2 * PI;
  double res = 0;
  double term = x;
  int k = 1;
  while (res + term != res) {
    res += term;
    k += 2;
    term *= -x * x / k / (k - 1);
  }

  return sign * res;
}
internal inline real32 expf_approx(real32 x) {
  real32 r = 1.0f;
  real32 term = 1.0f;
  for (int i = 1; i < 20; i++) {
    term *= x / i;
    r += term;
  }
  return r;
}

internal inline real32 logf_approx(real32 x) {
  // only valid for x > 0, poor accuracy
  real32 y = (x - 1.0f) / (x + 1.0f);
  real32 y2 = y * y;
  real32 r = 0.0f;
  real32 term = y;
  for (int i = 1; i < 20; i += 2) {
    r += term / i;
    term *= y2;
  }
  return 2.0f * r;
}

internal inline real32 powf_approx(real32 x, real32 y) {
  return expf_approx(y * logf_approx(x));
}

internal int RoundRealToInt(real32 Real) { return (int)(Real + 0.5); }

internal int32 min(int32 a, int32 b) {
  if (a < b) {
    return a;
  } else {
    return b;
  }
}
