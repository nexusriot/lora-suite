#include <cstdio>
#include "check.h"
#include "../../src/proto/dedup.h"

using namespace ls;

void run_dedup_tests() {
  std::printf("[dedup]\n");

  Dedup d(120000UL); // 2 min TTL

  CHECK(!d.seen(0x10, 1, 1000));   // first sight: new
  CHECK(d.seen(0x10, 1, 1500));    // duplicate within TTL
  CHECK(!d.seen(0x10, 2, 1500));   // different msgid: new
  CHECK(!d.seen(0x20, 1, 1500));   // different src: new

  // A hit refreshes the entry's timestamp (keeps suppressing while neighbours
  // keep rebroadcasting), so expiry is measured from the last sighting (t=2000).
  CHECK(d.seen(0x10, 1, 2000));            // still remembered, refreshes to t=2000
  CHECK(!d.seen(0x10, 1, 2000 + 120001));  // TTL past last sighting -> new again
}
