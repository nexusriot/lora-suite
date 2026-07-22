#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/proto/nodetable.h"
#include "../../src/proto/geo.h"

using namespace ls;

void run_nodetable_tests() {
  std::printf("[nodetable]\n");

  NodeTable t;
  t.heard(0xAA, -80, 9, 1000);
  CHECK(t.size() == 1);
  Node* a = t.find(0xAA);
  CHECK(a && a->rssi == -80 && a->packets == 1);

  // Same node updates in place.
  t.heard(0xAA, -70, 10, 2000);
  CHECK(t.size() == 1);
  a = t.find(0xAA);
  CHECK(a->rssi == -70 && a->packets == 2 && a->lastHeard == 2000);

  t.heard(0xBB, -95, 3, 2500);
  t.setName(0xBB, "delta", 2500);
  t.setPos(0xBB, 51.5, -0.12, 2500);
  Node* b = t.find(0xBB);
  CHECK(b && b->hasPos);
  CHECK(std::strcmp(b->name, "delta") == 0);
  CHECK(t.size() == 2);

  // Prune drops the stale node only.
  size_t removed = t.prune(3000000UL, 100000UL);
  CHECK(removed == 2); // both older than TTL at this now
  CHECK(t.size() == 0);

  // Geo sanity: ~1 degree of latitude is ~111 km.
  double dLat = haversineMeters(0.0, 0.0, 1.0, 0.0);
  CHECK_NEAR(dLat, 111195.0, 500.0);

  // Bearing due north ~ 0/360, due east ~ 90.
  double north = bearingDeg(0.0, 0.0, 1.0, 0.0);
  CHECK(north < 1.0 || north > 359.0);
  double east = bearingDeg(0.0, 0.0, 0.0, 1.0);
  CHECK_NEAR(east, 90.0, 1.0);
}
