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

  // Encrypt-then-MAC: encrypt the body, append an 8-byte tag, set FLAG_ENCRYPTED
  // and FLAG_MAC. No-op on the public channel or an empty body. Returns false if
  // the tag would not fit (caller must keep len <= MAX_PAYLOAD - MAC_LEN).
  bool seal(Frame& f) const;

  // Inverse of seal: verify the tag (constant-time), strip it, and decrypt.
  // Returns false if the frame claims our keyed channel but fails authentication
  // — the caller MUST drop such frames. Cleartext frames pass through untouched.
  bool open(Frame& f) const;

  // Tag covers the header fields an attacker must not be able to rewrite, plus
  // the ciphertext. `hop` is excluded on purpose: relays decrement it in flight,
  // so including it would break every forwarded frame.
  void tag(const Frame& f, const uint8_t* body, uint8_t bodyLen, uint8_t out[MAC_LEN]) const;

private:
  uint8_t key_[32] = {0};
  uint8_t macKey_[32] = {0};
  uint8_t id_ = 0;
  bool encrypted_ = false;
};

} // namespace ls
