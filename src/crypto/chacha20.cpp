#include "chacha20.h"
#include <cstring>

namespace ls {

static inline uint32_t rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
static inline uint32_t load32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void quarter(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
  a += b; d ^= a; d = rotl(d, 16);
  c += d; b ^= c; b = rotl(b, 12);
  a += b; d ^= a; d = rotl(d, 8);
  c += d; b ^= c; b = rotl(b, 7);
}

static void block(const uint32_t in[16], uint8_t out[64]) {
  uint32_t x[16];
  std::memcpy(x, in, 64);
  for (int i = 0; i < 10; i++) {
    quarter(x[0], x[4], x[8],  x[12]);
    quarter(x[1], x[5], x[9],  x[13]);
    quarter(x[2], x[6], x[10], x[14]);
    quarter(x[3], x[7], x[11], x[15]);
    quarter(x[0], x[5], x[10], x[15]);
    quarter(x[1], x[6], x[11], x[12]);
    quarter(x[2], x[7], x[8],  x[13]);
    quarter(x[3], x[4], x[9],  x[14]);
  }
  for (int i = 0; i < 16; i++) {
    uint32_t v = x[i] + in[i];
    out[4 * i + 0] = v & 0xFF;
    out[4 * i + 1] = (v >> 8) & 0xFF;
    out[4 * i + 2] = (v >> 16) & 0xFF;
    out[4 * i + 3] = (v >> 24) & 0xFF;
  }
}

void chacha20_xor(uint8_t* data, size_t len,
                  const uint8_t key[32], const uint8_t nonce[12],
                  uint32_t counter) {
  uint32_t st[16];
  st[0] = 0x61707865; st[1] = 0x3320646e; st[2] = 0x79622d32; st[3] = 0x6b206574;
  for (int i = 0; i < 8; i++) st[4 + i] = load32(key + 4 * i);
  st[12] = counter;
  st[13] = load32(nonce + 0);
  st[14] = load32(nonce + 4);
  st[15] = load32(nonce + 8);

  uint8_t ks[64];
  size_t off = 0;
  while (off < len) {
    block(st, ks);
    size_t n = (len - off < 64) ? (len - off) : 64;
    for (size_t i = 0; i < n; i++) data[off + i] ^= ks[i];
    off += n;
    st[12]++;
  }
}

} // namespace ls
