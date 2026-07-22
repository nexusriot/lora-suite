#include <cstdio>
#include "check.h"
#include "../../src/proto/solar.h"

using namespace ls;

void run_solar_tests() {
  std::printf("[solar]\n");

  // Equator: ~12 h of daylight year-round.
  CHECK_NEAR(daylightHours(0.0, 172), 12.0, 0.2);
  CHECK_NEAR(daylightHours(0.0, 355), 12.0, 0.2);

  // High latitude: long summer day, short winter day.
  CHECK(daylightHours(60.0, 172) > 15.0);   // ~summer solstice
  CHECK(daylightHours(60.0, 355) < 9.0);    // ~winter solstice

  // Polar day / night clamp.
  CHECK(daylightHours(80.0, 172) == 24.0);
  CHECK(daylightHours(80.0, 355) == 0.0);
}
