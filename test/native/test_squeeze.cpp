#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/proto/squeeze.h"

using namespace ls;

static bool roundTrip(const char* s) {
  uint8_t comp[512], back[512];
  size_t n = squeeze((const uint8_t*)s, std::strlen(s), comp, sizeof(comp));
  if (n == 0 && std::strlen(s) != 0) return false;
  size_t m = unsqueeze(comp, n, back, sizeof(back));
  return m == std::strlen(s) && std::memcmp(back, s, m) == 0;
}

void run_squeeze_tests() {
  std::printf("[squeeze]\n");

  // Round-trip fidelity across the kinds of traffic this actually carries.
  const char* corpus[] = {
    "",
    "a",
    "ok",
    "roger that",
    "moving north to the ridge, eta 20 min",
    "status: battery 45% and holding position at the bridge",
    "need help at grid 44.51 40.18 - two of us, no water",
    "CONFIRM: we will meet you at base camp before it is dark",
    "!@#$%^&*()_+{}|:\"<>?~`",                       // punctuation, no dictionary help
    "\x01\x02\x03\x7f",                              // control bytes
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",      // degenerate repeat
    "The quick brown fox jumps over the lazy dog",
  };
  for (const char* s : corpus) CHECK(roundTrip(s));

  // Every byte value must survive a round trip (the payload is not always text).
  {
    uint8_t all[256];
    for (int i = 0; i < 256; i++) all[i] = (uint8_t)i;
    uint8_t comp[1024], back[512];
    size_t n = squeeze(all, sizeof(all), comp, sizeof(comp));
    CHECK(n > 0);
    size_t m = unsqueeze(comp, n, back, sizeof(back));
    CHECK(m == sizeof(all));
    CHECK(std::memcmp(back, all, sizeof(all)) == 0);
  }

  // The point of the feature: real messages must get materially smaller.
  {
    const char* msg = "moving north to the ridge, eta 20 min - confirm you can see us";
    uint8_t comp[512];
    size_t raw = std::strlen(msg);
    size_t n = squeeze((const uint8_t*)msg, raw, comp, sizeof(comp));
    CHECK(n > 0);
    CHECK(n < raw);
    CHECK(n <= raw * 3 / 4);   // comfortably better than 0.75x
  }

  // squeezeIfSmaller declines when compression does not help (e.g. random bytes).
  {
    uint8_t noise[64];
    for (size_t i = 0; i < sizeof(noise); i++) noise[i] = (uint8_t)(i * 137 + 11);
    uint8_t out[256];
    size_t n = 0;
    bool ok = squeezeIfSmaller(noise, sizeof(noise), out, sizeof(out), n);
    if (ok) CHECK(n < sizeof(noise));   // if it claims a win it must really be smaller
  }
  {
    const char* msg = "the position of the contact is north of the bridge";
    uint8_t out[256];
    size_t n = 0;
    CHECK(squeezeIfSmaller((const uint8_t*)msg, std::strlen(msg), out, sizeof(out), n));
    CHECK(n < std::strlen(msg));
  }

  // Output-capacity limits are respected rather than overflowing.
  {
    const char* msg = "status report from the northern ridge position";
    uint8_t tiny[4];
    CHECK(squeeze((const uint8_t*)msg, std::strlen(msg), tiny, sizeof(tiny)) == 0);
  }
  {
    uint8_t comp[512], small[8];
    const char* msg = "the quick brown fox and the lazy dog";
    size_t n = squeeze((const uint8_t*)msg, std::strlen(msg), comp, sizeof(comp));
    CHECK(n > 0);
    CHECK(unsqueeze(comp, n, small, sizeof(small)) == 0);   // won't fit: fail closed
  }

  // Malformed streams (these arrive off the air) must fail closed, never crash.
  {
    uint8_t back[512];
    uint8_t truncEsc[] = {SQZ_LIT1};                       // escape with no literal
    CHECK(unsqueeze(truncEsc, sizeof(truncEsc), back, sizeof(back)) == 0);
    uint8_t noCount[] = {SQZ_LITN};                        // run with no count
    CHECK(unsqueeze(noCount, sizeof(noCount), back, sizeof(back)) == 0);
    uint8_t shortRun[] = {SQZ_LITN, 9, 'a', 'b'};          // count exceeds input
    CHECK(unsqueeze(shortRun, sizeof(shortRun), back, sizeof(back)) == 0);
    uint8_t zeroRun[] = {SQZ_LITN, 0};                     // zero-length run is invalid
    CHECK(unsqueeze(zeroRun, sizeof(zeroRun), back, sizeof(back)) == 0);
    uint8_t badIdx[] = {SQZ_DICT_SIZE - 1};                // index past the dictionary
    CHECK(unsqueeze(badIdx, sizeof(badIdx), back, sizeof(back)) == 0);
  }

  // Fuzz: arbitrary bytes fed to the decoder must terminate without overrunning.
  {
    uint8_t back[600];
    for (int seed = 0; seed < 300; seed++) {
      uint8_t junk[48];
      uint32_t x = (uint32_t)seed * 2654435761u + 1;
      for (size_t i = 0; i < sizeof(junk); i++) { x = x * 1103515245u + 12345u; junk[i] = (uint8_t)(x >> 16); }
      size_t m = unsqueeze(junk, sizeof(junk), back, sizeof(back));
      CHECK(m <= sizeof(back));   // never reports more than the buffer holds
    }
  }
}
