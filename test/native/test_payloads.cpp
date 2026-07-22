#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/proto/payloads.h"

using namespace ls;

void run_payloads_tests() {
  std::printf("[payloads]\n");

  Position p;
  p.lat = 51.5074; p.lon = -0.1278;   // London
  p.altM = 35; p.speedKmh = 12; p.course2 = 45; // 90 deg

  uint8_t buf[16];
  size_t n = packPosition(p, buf, sizeof(buf));
  CHECK(n == POSITION_LEN);

  Position q;
  CHECK(unpackPosition(buf, n, q));
  CHECK_NEAR(q.lat, p.lat, 1e-6);
  CHECK_NEAR(q.lon, p.lon, 1e-6);
  CHECK(q.altM == 35);
  CHECK(q.speedKmh == 12);
  CHECK(q.course2 == 45);

  // Southern/western hemisphere (negative fixed-point) round-trips.
  Position s; s.lat = -33.8688; s.lon = 151.2093; // Sydney
  CHECK(packPosition(s, buf, sizeof(buf)) == POSITION_LEN);
  Position s2;
  CHECK(unpackPosition(buf, POSITION_LEN, s2));
  CHECK_NEAR(s2.lat, s.lat, 1e-6);
  CHECK_NEAR(s2.lon, s.lon, 1e-6);

  // Too-small buffers are refused both ways.
  CHECK(packPosition(p, buf, 4) == 0);
  Position bad;
  CHECK(!unpackPosition(buf, 4, bad));

  Telemetry t; t.ax = -900; t.ay = 120; t.az = 1010; t.battPct = 87; t.tempC = -5;
  uint8_t tb[16];
  CHECK(packTelemetry(t, tb, sizeof(tb)) == TELEMETRY_LEN);
  Telemetry u;
  CHECK(unpackTelemetry(tb, TELEMETRY_LEN, u));
  CHECK(u.ax == -900 && u.ay == 120 && u.az == 1010);
  CHECK(u.battPct == 87 && u.tempC == -5);

  Health h; h.battPct = 62; h.uptimeHr = 5; h.heapKb = 180; h.dutyPct = 3; h.tempC = -2; h.reboot = 7; h.presence = 2;
  uint8_t hb[16];
  CHECK(packHealth(h, hb, sizeof(hb)) == HEALTH_LEN);
  CHECK(HEALTH_LEN == 7);
  Health h2;
  CHECK(unpackHealth(hb, HEALTH_LEN, h2));
  CHECK(h2.battPct == 62 && h2.uptimeHr == 5 && h2.heapKb == 180);
  CHECK(h2.dutyPct == 3 && h2.tempC == -2 && h2.reboot == 7);
  CHECK(h2.presence == 2);
  CHECK(!unpackHealth(hb, 3, h2));

  Waypoint w; w.lat = 48.8584; w.lon = 2.2945; w.altM = 330; // Eiffel Tower
  std::snprintf(w.label, sizeof(w.label), "%s", "TOWER");
  uint8_t wb[24];
  CHECK(packWaypoint(w, wb, sizeof(wb)) == WAYPOINT_LEN);
  Waypoint w2;
  CHECK(unpackWaypoint(wb, WAYPOINT_LEN, w2));
  CHECK_NEAR(w2.lat, w.lat, 1e-6);
  CHECK_NEAR(w2.lon, w.lon, 1e-6);
  CHECK(w2.altM == 330);
  CHECK(std::strcmp(w2.label, "TOWER") == 0);

  TimeSync ts; ts.unix = 1770000000u; ts.source = 2;
  uint8_t sb[8];
  CHECK(packTimeSync(ts, sb, sizeof(sb)) == TIMESYNC_LEN);
  TimeSync ts2;
  CHECK(unpackTimeSync(sb, TIMESYNC_LEN, ts2));
  CHECK(ts2.unix == 1770000000u && ts2.source == 2);

  Countdown cd; cd.unix = 1770000123u; cd.code = 3;
  uint8_t cb[8];
  CHECK(packCountdown(cd, cb, sizeof(cb)) == COUNTDOWN_LEN);
  Countdown cd2;
  CHECK(unpackCountdown(cb, COUNTDOWN_LEN, cd2));
  CHECK(cd2.unix == 1770000123u && cd2.code == 3);
  CHECK(!unpackCountdown(cb, 3, cd2));
}
