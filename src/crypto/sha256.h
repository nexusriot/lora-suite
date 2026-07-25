#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// SHA-256 (FIPS 180-4) + HMAC-SHA256 + HKDF-SHA256 (RFC 5869). Dependency-free so
// the channel key-derivation is host-testable; on device it could be swapped for
// the ESP32 hardware SHA engine.
void sha256(const uint8_t* data, size_t len, uint8_t out[32]);

// Incremental SHA-256, so HMAC can absorb a message of any length without
// buffering it whole (the earlier fixed-buffer HMAC silently truncated).
struct Sha256Ctx {
  uint32_t st[8];
  uint8_t  buf[64];
  size_t   buffered;
  uint64_t total;
};

void sha256_init(Sha256Ctx& c);
void sha256_update(Sha256Ctx& c, const uint8_t* data, size_t len);
void sha256_final(Sha256Ctx& c, uint8_t out[32]);

void hmac_sha256(const uint8_t* key, size_t keyLen,
                 const uint8_t* msg, size_t msgLen, uint8_t out[32]);

// HKDF-SHA256: derive outLen bytes of key material from ikm (with optional salt/info).
void hkdf_sha256(const uint8_t* salt, size_t saltLen,
                 const uint8_t* ikm, size_t ikmLen,
                 const uint8_t* info, size_t infoLen,
                 uint8_t* out, size_t outLen);

} // namespace ls
