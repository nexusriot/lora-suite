#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/crypto/aes.h"

using namespace ls;

static bool eq(const uint8_t* a, const uint8_t* b, size_t n) { return std::memcmp(a, b, n) == 0; }

void run_aes_tests() {
  std::printf("[aes]\n");

  // FIPS-197 C.1 known-answer vector
  {
    uint8_t key[16] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
    uint8_t in[16]  = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    uint8_t exp[16] = {0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
    uint8_t rk[176], out[16];
    aes128_key_expand(key, rk);
    aes128_encrypt_block(rk, in, out);
    CHECK(eq(out, exp, 16));
  }

  // FIPS-197 Appendix B worked example
  {
    uint8_t key[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
    uint8_t in[16]  = {0x32,0x43,0xf6,0xa8,0x88,0x5a,0x30,0x8d,0x31,0x31,0x98,0xa2,0xe0,0x37,0x07,0x34};
    uint8_t exp[16] = {0x39,0x25,0x84,0x1d,0x02,0xdc,0x09,0xfb,0xdc,0x11,0x85,0x97,0x19,0x6a,0x0b,0x32};
    uint8_t rk[176], out[16];
    aes128_key_expand(key, rk);
    aes128_encrypt_block(rk, in, out);
    CHECK(eq(out, exp, 16));
  }

  // CTR is involutive: encrypting twice with the same key/iv restores the input.
  {
    uint8_t key[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
    uint8_t iv[16]  = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    uint8_t data[35], orig[35];
    for (int i = 0; i < 35; i++) data[i] = orig[i] = (uint8_t)(i * 7 + 1);
    aes128_ctr_xor(key, iv, data, sizeof(data));
    CHECK(!eq(data, orig, 35));                      // actually transformed
    aes128_ctr_xor(key, iv, data, sizeof(data));
    CHECK(eq(data, orig, 35));                        // round-trips
  }

  // CTR keystream = AES(counter blocks); spans a block boundary correctly.
  {
    uint8_t key[16] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
    uint8_t iv[16]  = {0}; // counter starts at 0
    uint8_t buf[20] = {0}; // all-zero -> output is the raw keystream
    aes128_ctr_xor(key, iv, buf, sizeof(buf));

    uint8_t rk[176], ctr[16], ks0[16], ks1[16];
    aes128_key_expand(key, rk);
    std::memset(ctr, 0, 16);
    aes128_encrypt_block(rk, ctr, ks0);
    for (int i = 15; i >= 0; i--) if (++ctr[i]) break;
    aes128_encrypt_block(rk, ctr, ks1);

    CHECK(eq(buf, ks0, 16));           // first block
    CHECK(eq(buf + 16, ks1, 4));       // partial second block
  }

  // FIPS-197 C.3 known-answer test for AES-256 (needed for 32-byte Meshtastic PSKs).
  {
    uint8_t key[32];
    for (int i = 0; i < 32; i++) key[i] = (uint8_t)i;
    const uint8_t pt[16] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                            0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    const uint8_t want[16] = {0x8e,0xa2,0xb7,0xca,0x51,0x67,0x45,0xbf,
                              0xea,0xfc,0x49,0x90,0x4b,0x49,0x60,0x89};
    uint8_t rk[240], out[16];
    aes256_key_expand(key, rk);
    aes256_encrypt_block(rk, pt, out);
    CHECK(eq(out, want, 16));
  }

  // The generic CTR entry point must agree with the sized one, and refuse any
  // other key length rather than quietly leaving the data in the clear.
  {
    uint8_t key[32];
    for (int i = 0; i < 32; i++) key[i] = (uint8_t)(i * 7 + 1);
    uint8_t iv[16];
    for (int i = 0; i < 16; i++) iv[i] = (uint8_t)(i + 3);

    uint8_t a[40], b[40];
    for (int i = 0; i < 40; i++) a[i] = b[i] = (uint8_t)i;
    aes128_ctr_xor(key, iv, a, sizeof(a));
    CHECK(aes_ctr_xor(key, 16, iv, b, sizeof(b)));
    CHECK(eq(a, b, sizeof(a)));

    // AES-256 must produce a different keystream from AES-128 on the same bytes.
    uint8_t c[40];
    for (int i = 0; i < 40; i++) c[i] = (uint8_t)i;
    CHECK(aes_ctr_xor(key, 32, iv, c, sizeof(c)));
    CHECK(!eq(b, c, sizeof(c)));

    // CTR is its own inverse.
    CHECK(aes_ctr_xor(key, 32, iv, c, sizeof(c)));
    for (int i = 0; i < 40; i++) CHECK(c[i] == (uint8_t)i);

    // Unsupported lengths: refused, data untouched.
    uint8_t d[8] = {1,2,3,4,5,6,7,8};
    CHECK(!aes_ctr_xor(key, 24, iv, d, sizeof(d)));
    CHECK(!aes_ctr_xor(key, 0, iv, d, sizeof(d)));
    for (int i = 0; i < 8; i++) CHECK(d[i] == (uint8_t)(i + 1));
  }
}
