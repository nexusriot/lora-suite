#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// ChaCha20 stream cipher (RFC 8439 layout, 20 rounds). XOR-based, so the same
// call both encrypts and decrypts. Chosen over AES for the skeleton because it
// is compact, constant-time-ish, and dependency-free for native unit tests; on
// device it can be swapped for mbedtls/ESP32 hardware AES-CTR if preferred.
void chacha20_xor(uint8_t* data, size_t len,
                  const uint8_t key[32], const uint8_t nonce[12],
                  uint32_t counter);

} // namespace ls
