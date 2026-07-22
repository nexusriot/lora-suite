#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/proto/roster.h"

using namespace ls;

void run_roster_tests() {
  std::printf("[roster]\n");

  Roster r;
  r.setName(0x0010, "alpha");
  r.setAlias(0x0010, "A1");
  r.setName(0x0020, "bravo");

  char buf[16];
  CHECK(std::strcmp(r.label(0x0010, buf, sizeof(buf)), "A1") == 0);     // alias wins
  CHECK(std::strcmp(r.label(0x0020, buf, sizeof(buf)), "bravo") == 0);  // learned name
  CHECK(std::strcmp(r.label(0x0030, buf, sizeof(buf)), "0030") == 0);   // hex fallback
  CHECK(r.size() == 2);   // label() on unknown must not create an entry

  uint16_t a = 0;
  CHECK(r.lookup("a1", a) && a == 0x0010);      // case-insensitive alias
  CHECK(r.lookup("BRAVO", a) && a == 0x0020);   // case-insensitive name
  CHECK(!r.lookup("nobody", a));

  r.setBlocked(0x0020, true);
  CHECK(r.isBlocked(0x0020));
  CHECK(!r.isBlocked(0x0010));
  CHECK(!r.isBlocked(0x0030));   // unknown addr not blocked
  CHECK(r.size() == 2);

  r.setFavorite(0x0010, true);
  CHECK(r.find(0x0010)->favorite);

  // serialize -> deserialize round-trip preserves entries
  uint8_t blob[2048];
  size_t n = r.serialize(blob, sizeof(blob));
  CHECK(n > 0);
  Roster r2;
  CHECK(r2.deserialize(blob, n));
  CHECK(r2.size() == r.size());
  CHECK(std::strcmp(r2.label(0x0010, buf, sizeof(buf)), "A1") == 0);
  CHECK(r2.isBlocked(0x0020));
  CHECK(r2.find(0x0010)->favorite);
}
