#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/crypto/sha256.h"

using namespace ls;

static void hx(const char* hex, uint8_t* out, size_t n) {
  for (size_t i = 0; i < n; i++) {
    auto v = [](char c) -> int { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; };
    out[i] = (uint8_t)((v(hex[i*2]) << 4) | v(hex[i*2+1]));
  }
}

static bool eq(const uint8_t* a, const char* hex, size_t n) {
  uint8_t exp[64];
  hx(hex, exp, n);
  return std::memcmp(a, exp, n) == 0;
}

void run_sha256_tests() {
  std::printf("[sha256]\n");

  // FIPS 180-4 / standard vectors
  {
    uint8_t d[32];
    sha256((const uint8_t*)"abc", 3, d);
    CHECK(eq(d, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", 32));
    sha256((const uint8_t*)"", 0, d);
    CHECK(eq(d, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", 32));
    const char* m = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";  // 56 bytes, spans a block
    sha256((const uint8_t*)m, std::strlen(m), d);
    CHECK(eq(d, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", 32));
  }

  // HMAC-SHA256, RFC 4231 test case 2
  {
    uint8_t mac[32];
    hmac_sha256((const uint8_t*)"Jefe", 4, (const uint8_t*)"what do ya want for nothing?", 28, mac);
    CHECK(eq(mac, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843", 32));
  }

  // HKDF-SHA256, RFC 5869 test case 1
  {
    uint8_t ikm[22]; std::memset(ikm, 0x0b, 22);
    uint8_t salt[13]; hx("000102030405060708090a0b0c", salt, 13);
    uint8_t info[10]; hx("f0f1f2f3f4f5f6f7f8f9", info, 10);
    uint8_t okm[42];
    hkdf_sha256(salt, 13, ikm, 22, info, 10, okm, 42);
    CHECK(eq(okm, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865", 42));
  }
}
