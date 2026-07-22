#include <cstdio>
#include "check.h"
#include "../../src/proto/ledger.h"

using namespace ls;

void run_ledger_tests() {
  std::printf("[ledger]\n");

  // tag -> slot mapping
  CHECK(AirLedger::slotFor(MSG_TEXT) == 1);
  CHECK(AirLedger::slotFor(MSG_WAYPOINT) == 11);
  CHECK(AirLedger::slotFor(AIRTAG_RELAY) == 12);
  CHECK(AirLedger::slotFor(0) == 13);     // unset -> other
  CHECK(AirLedger::slotFor(99) == 13);    // unknown -> other

  AirLedger L;
  CHECK(L.total() == 0 && L.frames() == 0);
  L.record(MSG_TEXT, 100);
  L.record(MSG_BEACON, 50);
  L.record(AIRTAG_RELAY, 30);
  L.record(200, 10);        // unknown -> "other"
  CHECK(L.total() == 190);
  CHECK(L.frames() == 4);
  CHECK(L.bucket(AirLedger::slotFor(MSG_TEXT)) == 100);
  CHECK(L.bucket(AirLedger::slotFor(MSG_BEACON)) == 50);
  CHECK(L.bucket(12) == 30);
  CHECK(L.bucket(13) == 10);

  L.reset();
  CHECK(L.total() == 0 && L.frames() == 0 && L.bucket(1) == 0);
}
