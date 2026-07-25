#include "sha256.h"
#include <cstring>

namespace ls {

static const uint32_t K[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void transform(uint32_t st[8], const uint8_t block[64]) {
  uint32_t w[64];
  for (int i = 0; i < 16; i++)
    w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
           ((uint32_t)block[i*4+2] << 8) | (uint32_t)block[i*4+3];
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = ror(w[i-15], 7) ^ ror(w[i-15], 18) ^ (w[i-15] >> 3);
    uint32_t s1 = ror(w[i-2], 17) ^ ror(w[i-2], 19) ^ (w[i-2] >> 10);
    w[i] = w[i-16] + s0 + w[i-7] + s1;
  }
  uint32_t a=st[0],b=st[1],c=st[2],d=st[3],e=st[4],f=st[5],g=st[6],h=st[7];
  for (int i = 0; i < 64; i++) {
    uint32_t S1 = ror(e,6) ^ ror(e,11) ^ ror(e,25);
    uint32_t ch = (e & f) ^ (~e & g);
    uint32_t t1 = h + S1 + ch + K[i] + w[i];
    uint32_t S0 = ror(a,2) ^ ror(a,13) ^ ror(a,22);
    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t t2 = S0 + maj;
    h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
  }
  st[0]+=a; st[1]+=b; st[2]+=c; st[3]+=d; st[4]+=e; st[5]+=f; st[6]+=g; st[7]+=h;
}

void sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
  uint32_t st[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  uint8_t block[64];
  size_t i = 0;
  while (len - i >= 64) { transform(st, data + i); i += 64; }

  size_t rem = len - i;
  std::memcpy(block, data + i, rem);
  block[rem] = 0x80;
  if (rem >= 56) {
    std::memset(block + rem + 1, 0, 64 - rem - 1);
    transform(st, block);
    std::memset(block, 0, 56);
  } else {
    std::memset(block + rem + 1, 0, 56 - rem - 1);
  }
  uint64_t bits = (uint64_t)len * 8;
  for (int j = 0; j < 8; j++) block[56 + j] = (uint8_t)(bits >> (56 - 8*j));
  transform(st, block);

  for (int j = 0; j < 8; j++) {
    out[j*4]   = (uint8_t)(st[j] >> 24);
    out[j*4+1] = (uint8_t)(st[j] >> 16);
    out[j*4+2] = (uint8_t)(st[j] >> 8);
    out[j*4+3] = (uint8_t)(st[j]);
  }
}

void hmac_sha256(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen, uint8_t out[32]) {
  uint8_t k[64];
  if (keyLen > 64) { sha256(key, keyLen, k); std::memset(k + 32, 0, 32); }
  else { std::memcpy(k, key, keyLen); std::memset(k + keyLen, 0, 64 - keyLen); }

  uint8_t ipad[64], opad[64];
  for (int i = 0; i < 64; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }

  // inner = SHA256(ipad || msg)
  uint8_t inner[32];
  {
    // stream: concatenate ipad + msg into a temp is wasteful; hash incrementally via a scratch.
    // Simpler: build a buffer. Messages here are small (<= a few hundred bytes).
    uint8_t buf[64 + 256];
    size_t n = msgLen > 256 ? 256 : msgLen;     // callers keep msg short (HKDF blocks)
    std::memcpy(buf, ipad, 64);
    std::memcpy(buf + 64, msg, n);
    sha256(buf, 64 + n, inner);
  }
  // out = SHA256(opad || inner)
  uint8_t buf[64 + 32];
  std::memcpy(buf, opad, 64);
  std::memcpy(buf + 64, inner, 32);
  sha256(buf, 96, out);
}

void hkdf_sha256(const uint8_t* salt, size_t saltLen,
                 const uint8_t* ikm, size_t ikmLen,
                 const uint8_t* info, size_t infoLen,
                 uint8_t* out, size_t outLen) {
  // Extract: PRK = HMAC(salt, IKM)   (salt defaults to 32 zero bytes if empty)
  uint8_t zeroSalt[32] = {0};
  uint8_t prk[32];
  hmac_sha256(salt && saltLen ? salt : zeroSalt, salt && saltLen ? saltLen : 32, ikm, ikmLen, prk);

  // Expand
  uint8_t t[32];
  size_t tLen = 0;
  uint8_t counter = 1;
  size_t done = 0;
  while (done < outLen) {
    uint8_t buf[32 + 128 + 1];
    size_t n = 0;
    std::memcpy(buf, t, tLen); n += tLen;
    size_t ilen = infoLen > 128 ? 128 : infoLen;
    if (info && ilen) { std::memcpy(buf + n, info, ilen); n += ilen; }
    buf[n++] = counter;
    hmac_sha256(prk, 32, buf, n, t);
    tLen = 32;
    size_t take = outLen - done < 32 ? outLen - done : 32;
    std::memcpy(out + done, t, take);
    done += take;
    counter++;
  }
}

} // namespace ls
