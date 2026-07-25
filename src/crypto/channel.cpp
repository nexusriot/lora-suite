#include "channel.h"
#include "chacha20.h"
#include "sha256.h"
#include <cstring>

namespace ls {

// Derive the 32-byte ChaCha20 key from the PSK with HKDF-SHA256 (RFC 5869), with
// a domain-separation salt/info so the key is specific to this app + purpose.
void Channel::setPSK(const char* psk) {
  size_t n = psk ? std::strlen(psk) : 0;
  if (n == 0) { clear(); return; }

  static const char SALT[] = "lora-suite/channel/v1";
  static const char INFO[] = "chacha20-key";
  hkdf_sha256((const uint8_t*)SALT, sizeof(SALT) - 1, (const uint8_t*)psk, n,
              (const uint8_t*)INFO, sizeof(INFO) - 1, key_, 32);

  // Channel id byte = first byte of SHA256(key), reserving 0 for the public channel.
  uint8_t h[32];
  sha256(key_, 32, h);
  id_ = h[0] ? h[0] : 1;
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
