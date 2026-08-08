#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// Compact AES-128 (FIPS-197), encrypt path only — CTR mode uses it for both
// directions. Dependency-free so the Meshtastic decoder is host-testable; on
// device it can be swapped for the ESP32 hardware AES engine if throughput matters.

// Expand a 16-byte key into 11 round keys (176 bytes).
void aes128_key_expand(const uint8_t key[16], uint8_t roundKeys[176]);

// AES-256 (Nk=8, Nr=14). Meshtastic channels carry either a 16-byte or a 32-byte
// PSK, so both key sizes are needed to join an arbitrary channel.
void aes256_key_expand(const uint8_t key[32], uint8_t roundKeys[240]);

// Encrypt one 16-byte block (in -> out) with pre-expanded round keys.
void aes128_encrypt_block(const uint8_t roundKeys[176], const uint8_t in[16], uint8_t out[16]);
void aes256_encrypt_block(const uint8_t roundKeys[240], const uint8_t in[16], uint8_t out[16]);

// AES-128-CTR: XOR `data` (len bytes) with the keystream produced by encrypting
// the 16-byte counter block `iv`, incremented as a 128-bit big-endian integer
// per block. Encryption and decryption are the same operation.
void aes128_ctr_xor(const uint8_t key[16], const uint8_t iv[16], uint8_t* data, size_t len);

// AES-CTR over either key size. `keyLen` must be 16 or 32; any other length is
// refused and `data` is left untouched (returns false) rather than XORed with a
// keystream from a wrong-sized key — a silent failure here would put plaintext
// on the air.
bool aes_ctr_xor(const uint8_t* key, uint8_t keyLen, const uint8_t iv[16],
                 uint8_t* data, size_t len);

} // namespace ls
