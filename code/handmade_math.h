
#define PI 3.14159265358979323846f

#define abs(x) ((x) >= 0 ? (x) : -(x))
#define sinf(x) handmade_sinf(x)
#define powf(x, y) powf_approx(x, y)
#define sqrtf(x) powf_approx(x, 0.5)
inline float fmodf(float x, float y) {
  if (y == 0.0f)
    return 0.0f; // undefined in C, pick something

  int q = (int)(x / y); // trunc toward zero (C cast rule)
  return x - (float)q * y;
}
inline float handmade_sinf(float x) {
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
inline float expf_approx(float x) {
  float r = 1.0f;
  float term = 1.0f;
  for (int i = 1; i < 20; i++) {
    term *= x / i;
    r += term;
  }
  return r;
}

inline float logf_approx(float x) {
  // only valid for x > 0, poor accuracy
  float y = (x - 1.0f) / (x + 1.0f);
  float y2 = y * y;
  float r = 0.0f;
  float term = y;
  for (int i = 1; i < 20; i += 2) {
    r += term / i;
    term *= y2;
  }
  return 2.0f * r;
}

inline float powf_approx(float x, float y) {
  return expf_approx(y * logf_approx(x));
}
