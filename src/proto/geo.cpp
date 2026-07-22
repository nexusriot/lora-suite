#include "geo.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ls {

static const double R = 6371000.0;      // mean earth radius, metres
static const double D2R = M_PI / 180.0;

double haversineMeters(double lat1, double lon1, double lat2, double lon2) {
  double dlat = (lat2 - lat1) * D2R;
  double dlon = (lon2 - lon1) * D2R;
  double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
             std::cos(lat1 * D2R) * std::cos(lat2 * D2R) *
             std::sin(dlon / 2) * std::sin(dlon / 2);
  return 2.0 * R * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

double bearingDeg(double lat1, double lon1, double lat2, double lon2) {
  double dlon = (lon2 - lon1) * D2R;
  double y = std::sin(dlon) * std::cos(lat2 * D2R);
  double x = std::cos(lat1 * D2R) * std::sin(lat2 * D2R) -
             std::sin(lat1 * D2R) * std::cos(lat2 * D2R) * std::cos(dlon);
  double b = std::atan2(y, x) / D2R;
  if (b < 0) b += 360.0;
  return b;
}

} // namespace ls
