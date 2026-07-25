#include "wavfmt.h"
#include <cstring>

namespace ls {

static void put32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}

size_t wavHeader(uint8_t* out, uint32_t dataBytes, uint32_t sampleRate,
                 uint16_t channels, uint16_t bitsPerSample) {
  uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
  uint16_t blockAlign = (uint16_t)(channels * (bitsPerSample / 8));

  std::memcpy(out, "RIFF", 4);
  put32(out + 4, 36 + dataBytes);        // size of everything after this field
  std::memcpy(out + 8, "WAVE", 4);
  std::memcpy(out + 12, "fmt ", 4);
  put32(out + 16, 16);                   // PCM fmt chunk size
  put16(out + 20, 1);                    // PCM, uncompressed
  put16(out + 22, channels);
  put32(out + 24, sampleRate);
  put32(out + 28, byteRate);
  put16(out + 32, blockAlign);
  put16(out + 34, bitsPerSample);
  std::memcpy(out + 36, "data", 4);
  put32(out + 40, dataBytes);
  return WAV_HEADER_LEN;
}

uint32_t wavDataBytes(const uint8_t* hdr, size_t n) {
  if (n < WAV_HEADER_LEN) return 0;
  if (std::memcmp(hdr, "RIFF", 4) || std::memcmp(hdr + 8, "WAVE", 4)) return 0;
  if (std::memcmp(hdr + 36, "data", 4)) return 0;
  return (uint32_t)hdr[40] | ((uint32_t)hdr[41] << 8) |
         ((uint32_t)hdr[42] << 16) | ((uint32_t)hdr[43] << 24);
}

} // namespace ls
