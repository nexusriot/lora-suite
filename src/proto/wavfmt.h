#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// Canonical 44-byte RIFF/WAVE PCM header. Kept Arduino-free so the byte layout is
// host-testable; the recorder streams samples straight to SD after writing this,
// then rewrites it at the end once the final length is known.
constexpr size_t WAV_HEADER_LEN = 44;

// Fill `out` (>= WAV_HEADER_LEN) for `dataBytes` of PCM. Returns WAV_HEADER_LEN.
size_t wavHeader(uint8_t* out, uint32_t dataBytes, uint32_t sampleRate,
                 uint16_t channels = 1, uint16_t bitsPerSample = 16);

// Read the data-chunk length back out of a header (0 if it isn't a PCM WAVE).
uint32_t wavDataBytes(const uint8_t* hdr, size_t n);

} // namespace ls
