#include "solar.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ls {

double daylightHours(double latDeg, int dayOfYear) {
  const double d2r = M_PI / 180.0;
  double decl = 23.44 * std::sin(d2r * (360.0 / 365.0) * (dayOfYear - 81));
  double x = -std::tan(latDeg * d2r) * std::tan(decl * d2r);
  if (x <= -1.0) return 24.0;
  if (x >= 1.0) return 0.0;
  double h = std::acos(x);            // half-day angle, radians
  return 2.0 * (h / d2r) / 15.0;      // 15 degrees per hour
}

} // namespace ls
