#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/proto/wavfmt.h"
#include "../../src/proto/bmp.h"

using namespace ls;

static uint32_t rd32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

void run_media_tests() {
  std::printf("[media]\n");

  // WAV: a 8 kHz mono 16-bit header must have the canonical field values.
  {
    uint8_t h[WAV_HEADER_LEN];
    const uint32_t data = 16000;                 // 1 s at 8 kHz mono 16-bit
    CHECK(wavHeader(h, data, 8000) == WAV_HEADER_LEN);
    CHECK(std::memcmp(h, "RIFF", 4) == 0);
    CHECK(rd32(h + 4) == 36 + data);
    CHECK(std::memcmp(h + 8, "WAVE", 4) == 0);
    CHECK(std::memcmp(h + 12, "fmt ", 4) == 0);
    CHECK(rd32(h + 16) == 16);                   // PCM chunk size
    CHECK(rd16(h + 20) == 1);                    // format = PCM
    CHECK(rd16(h + 22) == 1);                    // mono
    CHECK(rd32(h + 24) == 8000);                 // sample rate
    CHECK(rd32(h + 28) == 8000 * 2);             // byte rate
    CHECK(rd16(h + 32) == 2);                    // block align
    CHECK(rd16(h + 34) == 16);                   // bits
    CHECK(std::memcmp(h + 36, "data", 4) == 0);
    CHECK(rd32(h + 40) == data);
    CHECK(wavDataBytes(h, sizeof(h)) == data);
  }

  // Stereo / 8-bit derived fields stay consistent.
  {
    uint8_t h[WAV_HEADER_LEN];
    wavHeader(h, 1000, 44100, 2, 8);
    CHECK(rd16(h + 22) == 2);
    CHECK(rd32(h + 28) == 44100 * 2 * 1);        // byteRate = rate*ch*bytes
    CHECK(rd16(h + 32) == 2);                    // blockAlign
  }

  // A zero-length recording is still a valid (empty) file.
  {
    uint8_t h[WAV_HEADER_LEN];
    wavHeader(h, 0, 8000);
    CHECK(rd32(h + 4) == 36);
    CHECK(wavDataBytes(h, sizeof(h)) == 0);
  }

  // Garbage is not mistaken for a WAV.
  {
    uint8_t junk[WAV_HEADER_LEN] = {0};
    CHECK(wavDataBytes(junk, sizeof(junk)) == 0);
    uint8_t shortBuf[10] = {0};
    CHECK(wavDataBytes(shortBuf, sizeof(shortBuf)) == 0);
  }

  // BMP: row padding rounds up to 4 bytes.
  {
    CHECK(bmpRowBytes(240) == 720);      // already a multiple of 4
    CHECK(bmpRowBytes(1) == 4);          // 3 -> 4
    CHECK(bmpRowBytes(2) == 8);          // 6 -> 8
    CHECK(bmpRowBytes(3) == 12);         // 9 -> 12
    CHECK(bmpRowBytes(4) == 12);         // 12 stays
  }

  // BMP header fields for the real screen size.
  {
    uint8_t h[BMP_HEADER_LEN];
    CHECK(bmpHeader(h, 240, 135) == BMP_HEADER_LEN);
    CHECK(h[0] == 'B' && h[1] == 'M');
    uint32_t img = 720u * 135u;
    CHECK(rd32(h + 2) == BMP_HEADER_LEN + img);
    CHECK(rd32(h + 10) == BMP_HEADER_LEN);
    CHECK(rd32(h + 14) == 40);
    CHECK(rd32(h + 18) == 240);
    CHECK(rd32(h + 22) == 135);
    CHECK(rd16(h + 26) == 1);
    CHECK(rd16(h + 28) == 24);
    CHECK(rd32(h + 30) == 0);            // BI_RGB
    CHECK(rd32(h + 34) == img);
  }

  // Pixel conversion: RGB565 -> BGR24, full scale must reach 0xFF.
  {
    uint16_t px[4] = {
      0xFFFF,   // white
      0x0000,   // black
      0xF800,   // pure red
      0x001F,   // pure blue
    };
    uint8_t row[bmpRowBytes(4)];
    size_t n = bmpRow565(px, 4, row);
    CHECK(n == bmpRowBytes(4));

    CHECK(row[0] == 0xFF && row[1] == 0xFF && row[2] == 0xFF);   // white
    CHECK(row[3] == 0x00 && row[4] == 0x00 && row[5] == 0x00);   // black
    CHECK(row[6] == 0x00 && row[7] == 0x00 && row[8] == 0xFF);   // red -> B,G,R
    CHECK(row[9] == 0xFF && row[10] == 0x00 && row[11] == 0x00); // blue -> B,G,R
  }

  // Green channel (6 bits) also reaches full scale.
  {
    uint16_t px[1] = {0x07E0};           // pure green
    uint8_t row[bmpRowBytes(1)];
    bmpRow565(px, 1, row);
    CHECK(row[0] == 0x00 && row[1] == 0xFF && row[2] == 0x00);
  }

  // Padding bytes are zeroed, not left as stack garbage.
  {
    uint16_t px[1] = {0xFFFF};
    uint8_t row[bmpRowBytes(1)];
    std::memset(row, 0xAA, sizeof(row));
    bmpRow565(px, 1, row);
    CHECK(row[3] == 0);
  }
}
