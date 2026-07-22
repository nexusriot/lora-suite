#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/crypto/chacha20.h"
#include "../../src/crypto/channel.h"

using namespace ls;

void run_crypto_tests() {
  std::printf("[crypto]\n");

  uint8_t key[32], nonce[12] = {0};
  for (int i = 0; i < 32; i++) key[i] = (uint8_t)i;

  const char* plain = "the quick brown fox jumps over the lazy dog";
  size_t n = std::strlen(plain);

  uint8_t buf[64];
  std::memcpy(buf, plain, n);
  chacha20_xor(buf, n, key, nonce, 0);
  CHECK(std::memcmp(buf, plain, n) != 0);   // actually transformed

  // Symmetric: applying again with same key/nonce/counter restores plaintext.
  chacha20_xor(buf, n, key, nonce, 0);
  CHECK(std::memcmp(buf, plain, n) == 0);

  // Keystream is deterministic and block-independent of data.
  uint8_t z1[64] = {0}, z2[64] = {0};
  chacha20_xor(z1, 64, key, nonce, 0);
  chacha20_xor(z2, 64, key, nonce, 0);
  CHECK(std::memcmp(z1, z2, 64) == 0);

  // Different counter => different keystream.
  uint8_t z3[64] = {0};
  chacha20_xor(z3, 64, key, nonce, 1);
  CHECK(std::memcmp(z1, z3, 64) != 0);

  // Flipping one key bit changes the keystream.
  uint8_t key2[32];
  std::memcpy(key2, key, 32);
  key2[0] ^= 0x01;
  uint8_t z4[64] = {0};
  chacha20_xor(z4, 64, key2, nonce, 0);
  CHECK(std::memcmp(z1, z4, 64) != 0);

  // Channel round-trip over a Frame, bound to (src, msgid).
  Frame f;
  f.type = MSG_TEXT;
  f.src = 0x2222;
  f.msgid = 0x0099;
  const char* body = "rendezvous at grid 44";
  f.setPayload(body, (uint8_t)std::strlen(body));

  Channel ch;
  ch.setPSK("night-city");
  CHECK(ch.encrypted());
  CHECK(ch.id() != 0);

  uint8_t original[MAX_PAYLOAD];
  std::memcpy(original, f.payload, f.len);

  ch.apply(f);                                    // encrypt
  CHECK(std::memcmp(f.payload, original, f.len) != 0);
  ch.apply(f);                                    // decrypt
  CHECK(std::memcmp(f.payload, original, f.len) == 0);

  // A different PSK yields a different channel id.
  Channel ch2;
  ch2.setPSK("day-side");
  CHECK(ch2.id() != ch.id());

  // Same plaintext under different msgid => different ciphertext (nonce binding).
  Frame g = f;
  g.msgid = 0x0100;
  ch.apply(f);
  ch.apply(g);
  CHECK(std::memcmp(f.payload, g.payload, f.len) != 0);
}
