#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/proto/ircodes.h"

using namespace ls;

void run_ircodes_tests() {
  std::printf("[ircodes]\n");

  // Defaults give a usable set out of the box.
  {
    IrCodeSet s;
    s.loadDefaults();
    CHECK(s.size() == 6);
    CHECK(std::strcmp(s.at(0).label, "Power") == 0);
    CHECK(s.at(0).addr == 0x00 && s.at(0).cmd == 0x0C);
    CHECK(s.at(5).cmd == 0x21);
  }

  // add / set / remove keep the list dense.
  {
    IrCodeSet s;
    CHECK(s.add("TV On", 0x04, 0x08));
    CHECK(s.add("TV Off", 0x04, 0x09));
    CHECK(s.add("Input", 0x04, 0x0A));
    CHECK(s.size() == 3);

    CHECK(s.set(1, "Renamed", 0x05, 0x55));      // overwrite in place
    CHECK(std::strcmp(s.at(1).label, "Renamed") == 0);
    CHECK(s.at(1).addr == 0x05);
    CHECK(s.size() == 3);

    CHECK(s.remove(0));                          // shift down
    CHECK(s.size() == 2);
    CHECK(std::strcmp(s.at(0).label, "Renamed") == 0);
    CHECK(std::strcmp(s.at(1).label, "Input") == 0);

    CHECK(!s.remove(5));                         // out of range
  }

  // Setting past the end would leave a hole: refused.
  {
    IrCodeSet s;
    s.add("One", 1, 1);
    CHECK(!s.set(5, "Hole", 2, 2));
    CHECK(s.set(1, "Two", 2, 2));                // exactly at the end appends
    CHECK(s.size() == 2);
  }

  // Capacity is enforced.
  {
    IrCodeSet s;
    for (size_t i = 0; i < IrCodeSet::CAP; i++) CHECK(s.add("x", (uint8_t)i, (uint8_t)i));
    CHECK(s.size() == IrCodeSet::CAP);
    CHECK(!s.add("overflow", 9, 9));
    CHECK(s.size() == IrCodeSet::CAP);
  }

  // Long labels are truncated, never overrun.
  {
    IrCodeSet s;
    s.add("aVeryLongLabelIndeed", 1, 2);
    CHECK(std::strlen(s.at(0).label) == 11);
  }

  // Serialize -> deserialize round trip.
  {
    IrCodeSet a;
    a.add("Power", 0x10, 0x20);
    a.add("Vol +", 0x11, 0x21);
    a.add("Src",   0x12, 0x22);

    uint8_t buf[256];
    size_t n = a.serialize(buf, sizeof(buf));
    CHECK(n > 0);

    IrCodeSet b;
    CHECK(b.deserialize(buf, n));
    CHECK(b.size() == 3);
    CHECK(std::strcmp(b.at(0).label, "Power") == 0);
    CHECK(b.at(1).addr == 0x11 && b.at(1).cmd == 0x21);
    CHECK(std::strcmp(b.at(2).label, "Src") == 0);
  }

  // An empty set round-trips too.
  {
    IrCodeSet a, b;
    uint8_t buf[16];
    size_t n = a.serialize(buf, sizeof(buf));
    CHECK(n == 3);
    CHECK(b.deserialize(buf, n));
    CHECK(b.size() == 0);
  }

  // serialize refuses to overflow a small buffer.
  {
    IrCodeSet a;
    a.loadDefaults();
    uint8_t tiny[10];
    CHECK(a.serialize(tiny, sizeof(tiny)) == 0);
  }

  // A corrupt or truncated NVS blob must be rejected whole, not partly applied.
  {
    IrCodeSet a;
    a.loadDefaults();
    uint8_t buf[256];
    size_t n = a.serialize(buf, sizeof(buf));

    IrCodeSet b;
    CHECK(!b.deserialize(buf, 2));                     // too short
    uint8_t bad[256];
    std::memcpy(bad, buf, n);
    bad[0] = 0x00;                                     // wrong magic
    CHECK(!b.deserialize(bad, n));
    std::memcpy(bad, buf, n);
    bad[1] = 99;                                       // wrong version
    CHECK(!b.deserialize(bad, n));
    std::memcpy(bad, buf, n);
    bad[2] = IrCodeSet::CAP + 5;                       // impossible count
    CHECK(!b.deserialize(bad, n));
    std::memcpy(bad, buf, n);
    CHECK(!b.deserialize(bad, n - 5));                 // truncated payload
    CHECK(b.size() == 0);                              // nothing was applied
  }

  // A stored label with no NUL terminator must not run off the end.
  {
    IrCodeSet a;
    a.add("0123456789AB", 1, 2);
    uint8_t buf[64];
    size_t n = a.serialize(buf, sizeof(buf));
    std::memset(buf + 3, 'Z', 12);                     // clobber the terminator
    IrCodeSet b;
    CHECK(b.deserialize(buf, n));
    CHECK(std::strlen(b.at(0).label) == 11);
  }
}
