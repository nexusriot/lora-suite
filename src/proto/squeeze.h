#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// Squeeze — a smaz-style dictionary compressor for short ASCII messages.
//
// General-purpose compressors (deflate/LZ4) lose on 20-80 byte strings: their
// headers and adaptive tables cost more than they save. Squeeze instead uses a
// fixed dictionary of the most common English fragments, so a typical field
// message packs to roughly half its size — which on a 1%-duty 868 MHz link means
// twice the words per airtime budget.
//
// Encoding: a byte < SQZ_DICT_SIZE is a dictionary index; SQZ_LIT1 escapes a
// single literal byte; SQZ_LITN introduces a run as (marker, count, bytes...).
// Decoding is bounds-checked and fails closed on a malformed stream (it may
// arrive from the air).
constexpr uint8_t SQZ_DICT_SIZE = 254;   // usable index space: 0..253
constexpr uint8_t SQZ_LIT1      = 254;   // next 1 byte is literal
constexpr uint8_t SQZ_LITN      = 255;   // next byte = count, then that many literals
constexpr uint8_t SQZ_MAX_RUN   = 255;

// Compress `in` into `out`. Returns the compressed length, or 0 if the result
// would not fit in outCap (the caller then sends the text uncompressed).
size_t squeeze(const uint8_t* in, size_t inLen, uint8_t* out, size_t outCap);

// Inverse. Returns the decompressed length, or 0 on a malformed/oversized stream.
size_t unsqueeze(const uint8_t* in, size_t inLen, uint8_t* out, size_t outCap);

// Convenience: compress only if it actually helps. Returns true and fills
// out/outLen when the compressed form is strictly smaller than the input.
bool squeezeIfSmaller(const uint8_t* in, size_t inLen, uint8_t* out, size_t outCap, size_t& outLen);

} // namespace ls
