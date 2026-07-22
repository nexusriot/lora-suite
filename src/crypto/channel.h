#pragma once
#include <cstdint>
#include "../proto/frame.h"

namespace ls {

// A named channel: apps only see frames whose CHAN byte matches, and payloads
// are optionally ChaCha20-encrypted with a key derived from the pre-shared key.
// Channel 0 with no PSK is the cleartext "public" channel.
class Channel {
public:
  void setPSK(const char* psk);   // derive key + channel id byte
  void clear();                   // public/plaintext channel 0

  uint8_t id() const { return id_; }
  bool encrypted() const { return encrypted_; }

  // Encrypt/decrypt f.payload in place (symmetric). Nonce is bound to
  // (src, msgid) so identical plaintext never yields identical ciphertext,
  // provided a node never reuses a msgid under the same key.
  void apply(Frame& f) const;

private:
  uint8_t key_[32] = {0};
  uint8_t id_ = 0;
  bool encrypted_ = false;
};

} // namespace ls
