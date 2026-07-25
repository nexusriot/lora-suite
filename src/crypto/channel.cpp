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
  static const char MINFO[] = "hmac-key";
  hkdf_sha256((const uint8_t*)SALT, sizeof(SALT) - 1, (const uint8_t*)psk, n,
              (const uint8_t*)INFO, sizeof(INFO) - 1, key_, 32);
  // Separate info string => an independent MAC key, so the cipher key is never
  // used for authentication (and a tag leaks nothing about the keystream).
  hkdf_sha256((const uint8_t*)SALT, sizeof(SALT) - 1, (const uint8_t*)psk, n,
              (const uint8_t*)MINFO, sizeof(MINFO) - 1, macKey_, 32);

  // Channel id byte = first byte of SHA256(key), reserving 0 for the public channel.
  uint8_t h[32];
  sha256(key_, 32, h);
  id_ = h[0] ? h[0] : 1;
  encrypted_ = true;
}

void Channel::clear() {
  std::memset(key_, 0, sizeof(key_));
  std::memset(macKey_, 0, sizeof(macKey_));
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

void Channel::tag(const Frame& f, const uint8_t* body, uint8_t bodyLen, uint8_t out[MAC_LEN]) const {
  // Canonical, hop-independent header image. FLAG_MAC is forced on so both sides
  // agree regardless of when the flag is set relative to tagging.
  uint8_t hdr[9] = {
    f.type,
    (uint8_t)(f.flags | FLAG_MAC),
    f.chan,
    (uint8_t)(f.src & 0xFF), (uint8_t)(f.src >> 8),
    (uint8_t)(f.dst & 0xFF), (uint8_t)(f.dst >> 8),
    (uint8_t)(f.msgid & 0xFF), (uint8_t)(f.msgid >> 8),
  };
  Sha256Ctx c;
  uint8_t k[64];
  std::memcpy(k, macKey_, 32);
  std::memset(k + 32, 0, 32);
  uint8_t ipad[64], opad[64];
  for (int i = 0; i < 64; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }

  uint8_t inner[32];
  sha256_init(c);
  sha256_update(c, ipad, 64);
  sha256_update(c, hdr, sizeof(hdr));
  sha256_update(c, &bodyLen, 1);          // length-prefix the body: no splicing
  sha256_update(c, body, bodyLen);
  sha256_final(c, inner);

  uint8_t full[32];
  sha256_init(c);
  sha256_update(c, opad, 64);
  sha256_update(c, inner, 32);
  sha256_final(c, full);

  std::memcpy(out, full, MAC_LEN);        // truncated tag
}

bool Channel::seal(Frame& f) const {
  if (!encrypted_ || f.len == 0) return true;   // public channel: nothing to do
  if ((size_t)f.len + MAC_LEN > MAX_PAYLOAD) return false;

  f.flags |= FLAG_ENCRYPTED;
  apply(f);                                     // encrypt in place
  uint8_t t[MAC_LEN];
  tag(f, f.payload, f.len, t);                  // ...then MAC the ciphertext
  std::memcpy(f.payload + f.len, t, MAC_LEN);
  f.len += MAC_LEN;
  f.flags |= FLAG_MAC;
  return true;
}

bool Channel::open(Frame& f) const {
  if (!(f.flags & FLAG_ENCRYPTED)) return true;     // cleartext passes through
  if (!encrypted_) return false;                    // keyed frame, but we hold no key
  if (!(f.flags & FLAG_MAC)) return false;          // v3 requires authentication
  if (f.len < MAC_LEN) return false;

  uint8_t bodyLen = (uint8_t)(f.len - MAC_LEN);
  uint8_t want[MAC_LEN];
  tag(f, f.payload, bodyLen, want);

  uint8_t diff = 0;                                  // constant-time compare
  for (size_t i = 0; i < MAC_LEN; i++) diff |= (uint8_t)(want[i] ^ f.payload[bodyLen + i]);
  if (diff) return false;

  f.len = bodyLen;
  f.flags &= (uint8_t)~FLAG_MAC;
  apply(f);                                          // decrypt only after verifying
  return true;
}

} // namespace ls
