#include "channel.h"
#include "chacha20.h"
#include <cstring>

namespace ls {

// NOTE: this is a lightweight key-stretch adequate for a skeleton. On device,
// replace with a real KDF (HKDF/SHA-256 via mbedtls) before relying on it.
void Channel::setPSK(const char* psk) {
  size_t n = psk ? std::strlen(psk) : 0;
  if (n == 0) { clear(); return; }

  for (int i = 0; i < 32; i++) key_[i] = (uint8_t)(psk[i % n] + i * 31);
  // one diffusion pass so related PSKs don't yield related keys
  uint8_t zero[32] = {0};
  uint8_t nonce[12] = {0};
  chacha20_xor(key_, 32, key_, nonce, 0);
  (void)zero;

  uint8_t h = 0;
  for (int i = 0; i < 32; i++) h ^= key_[i];
  id_ = h ? h : 1;         // reserve 0 for the public channel
  encrypted_ = true;
}

void Channel::clear() {
  std::memset(key_, 0, sizeof(key_));
  id_ = 0;
  encrypted_ = false;
}

void Channel::apply(Frame& f) const {
  if (!encrypted_ || f.len == 0) return;
  uint8_t nonce[12] = {0};
  nonce[0] = f.src & 0xFF;   nonce[1] = f.src >> 8;
  nonce[2] = f.msgid & 0xFF; nonce[3] = f.msgid >> 8;
  chacha20_xor(f.payload, f.len, key_, nonce, 0);
}

} // namespace ls
