#include "bmp.h"
#include <cstring>

namespace ls {

static void put32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}

size_t bmpHeader(uint8_t* out, uint32_t width, uint32_t height) {
  uint32_t imgBytes = (uint32_t)bmpRowBytes(width) * height;
  std::memset(out, 0, BMP_HEADER_LEN);

  out[0] = 'B'; out[1] = 'M';
  put32(out + 2, BMP_HEADER_LEN + imgBytes);   // total file size
  put32(out + 10, BMP_HEADER_LEN);             // pixel data offset

  put32(out + 14, 40);                         // BITMAPINFOHEADER
  put32(out + 18, width);
  put32(out + 22, height);                     // positive = bottom-up rows
  put16(out + 26, 1);                          // planes
  put16(out + 28, 24);                         // bits per pixel
  put32(out + 30, 0);                          // BI_RGB, no compression
  put32(out + 34, imgBytes);
  put32(out + 38, 2835);                       // ~72 dpi, x
  put32(out + 42, 2835);                       // ~72 dpi, y
  return BMP_HEADER_LEN;
}

size_t bmpRow565(const uint16_t* px, uint32_t width, uint8_t* out) {
  size_t o = 0;
  for (uint32_t x = 0; x < width; x++) {
    uint16_t c = px[x];
    uint8_t r = (uint8_t)((c >> 11) & 0x1F);
    uint8_t g = (uint8_t)((c >> 5) & 0x3F);
    uint8_t b = (uint8_t)(c & 0x1F);
    // Replicate the high bits into the low ones so full-scale stays full-scale
    // (0x1F -> 0xFF, not 0xF8) and the image doesn't come out slightly dark.
    out[o++] = (uint8_t)((b << 3) | (b >> 2));   // BMP stores BGR
    out[o++] = (uint8_t)((g << 2) | (g >> 4));
    out[o++] = (uint8_t)((r << 3) | (r >> 2));
  }
  size_t padded = bmpRowBytes(width);
  while (o < padded) out[o++] = 0;
  return o;
}

} // namespace ls
