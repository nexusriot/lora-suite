#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/proto/base64.h"

using namespace ls;

void run_base64_tests() {
  std::printf("[base64]\n");

  uint8_t out[64];
  char txt[64];

  // RFC 4648 test vectors.
  {
    struct { const char* b64; const char* plain; } V[] = {
      {"Zg==", "f"}, {"Zm8=", "fo"}, {"Zm9v", "foo"},
      {"Zm9vYg==", "foob"}, {"Zm9vYmE=", "fooba"}, {"Zm9vYmFy", "foobar"},
    };
    for (auto& v : V) {
      size_t n = base64Decode(v.b64, out, sizeof(out));
      CHECK(n == std::strlen(v.plain));
      CHECK(std::memcmp(out, v.plain, n) == 0);

      size_t m = base64Encode((const uint8_t*)v.plain, std::strlen(v.plain), txt, sizeof(txt));
      CHECK(m == std::strlen(v.b64));
      CHECK(std::strcmp(txt, v.b64) == 0);
    }
  }

  // The Meshtastic shorthand PSK: one byte, two pad characters.
  {
    CHECK(base64Decode("AQ==", out, sizeof(out)) == 1);
    CHECK(out[0] == 1);
    CHECK(base64Decode("AA==", out, sizeof(out)) == 1);
    CHECK(out[0] == 0);
  }

  // A full 16-byte key.
  {
    size_t n = base64Decode("AAECAwQFBgcICQoLDA0ODw==", out, sizeof(out));
    CHECK(n == 16);
    for (int i = 0; i < 16; i++) CHECK(out[i] == (uint8_t)i);
  }

  // Malformed input is rejected, never partially accepted.
  {
    CHECK(base64Decode("", out, sizeof(out)) == 0);            // empty
    CHECK(base64Decode("A", out, sizeof(out)) == 0);           // length % 4
    CHECK(base64Decode("AQ=", out, sizeof(out)) == 0);         // length % 4
    CHECK(base64Decode("AQ ==", out, sizeof(out)) == 0);       // whitespace
    CHECK(base64Decode("A!==", out, sizeof(out)) == 0);        // outside the alphabet
    CHECK(base64Decode("A===", out, sizeof(out)) == 0);        // three pad characters
    CHECK(base64Decode("=AAA", out, sizeof(out)) == 0);        // leading pad
    CHECK(base64Decode("AA==AAAA", out, sizeof(out)) == 0);    // pad before the last quantum
    CHECK(base64Decode("Zm9v", out, 2) == 0);                  // output would not fit
    CHECK(base64Decode(nullptr, out, sizeof(out)) == 0);
  }

  // Both alphabets decode: the Cardputer keyboard cannot type '/' (it is the
  // right-arrow), so URL-safe spelling is the only way to enter some keys.
  {
    uint8_t a[8], b[8];
    size_t na = base64Decode("++//", a, sizeof(a));
    size_t nb = base64Decode("--__", b, sizeof(b));
    CHECK(na == 3 && nb == 3);
    CHECK(std::memcmp(a, b, 3) == 0);
    CHECK(a[0] == 0xFB && a[1] == 0xEF && a[2] == 0xFF);
  }

  // Encoding refuses to overflow, and round-trips arbitrary bytes.
  {
    uint8_t raw[32];
    for (int i = 0; i < 32; i++) raw[i] = (uint8_t)(i * 9 + 5);
    CHECK(base64Encode(raw, sizeof(raw), txt, 8) == 0);        // too small
    size_t m = base64Encode(raw, sizeof(raw), txt, sizeof(txt));
    CHECK(m == 44);
    CHECK(base64Decode(txt, out, sizeof(out)) == 32);
    CHECK(std::memcmp(out, raw, 32) == 0);
    CHECK(base64Encode(raw, 0, txt, sizeof(txt)) == 0);
  }
}
