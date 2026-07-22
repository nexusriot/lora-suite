#include <cstdio>
#include "check.h"
#include "../../src/proto/airtime.h"

using namespace ls;

void run_airtime_tests() {
  std::printf("[airtime]\n");

  RadioCfg c; // SF9 BW125 CR4/5 preamble8, explicit header + CRC

  // A 20-byte frame at SF9/BW125 sits in the low-hundreds of ms.
  double t = timeOnAirMs(c, 20);
  CHECK(t > 80.0 && t < 200.0);

  // Higher SF => strictly longer time on air for the same payload.
  double prev = -1;
  for (uint8_t sf = 7; sf <= 12; sf++) {
    RadioCfg s = c; s.sf = sf;
    double v = timeOnAirMs(s, 32);
    CHECK(v > prev);
    prev = v;
  }

  // Doubling bandwidth roughly halves time on air.
  RadioCfg a = c; a.bwHz = 125000;
  RadioCfg b = c; b.bwHz = 250000;
  double ta = timeOnAirMs(a, 40);
  double tb = timeOnAirMs(b, 40);
  CHECK(tb < ta);
  CHECK_NEAR(tb, ta / 2.0, ta * 0.15);

  // Larger payload => longer.
  CHECK(timeOnAirMs(c, 100) > timeOnAirMs(c, 10));

  // Free-space path loss grows with distance and is a sane magnitude for LoRa.
  double near = pathLossDb(c, 100.0);
  double far = pathLossDb(c, 5000.0);
  CHECK(far > near);
  CHECK(far > 90.0 && far < 130.0);
}
