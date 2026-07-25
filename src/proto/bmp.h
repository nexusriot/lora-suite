#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// Minimal 24-bit BMP writer support for screenshots. The canvas is RGB565, but
// plain 24-bit BI_RGB is what every viewer opens without argument, so rows are
// converted on the way out. Rows are written bottom-up and padded to 4 bytes,
// which lets the screenshot stream one row at a time instead of buffering a
// whole 97 KB image.
constexpr size_t BMP_HEADER_LEN = 54;

// Bytes per padded output row for a `width`-pixel 24-bit image.
constexpr size_t bmpRowBytes(uint32_t width) { return ((size_t)width * 3 + 3) & ~(size_t)3; }

// Fill `out` (>= BMP_HEADER_LEN). Returns BMP_HEADER_LEN.
size_t bmpHeader(uint8_t* out, uint32_t width, uint32_t height);

// Convert one row of RGB565 pixels to padded 24-bit BGR. `out` must hold
// bmpRowBytes(width). Returns the bytes written.
size_t bmpRow565(const uint16_t* px, uint32_t width, uint8_t* out);

} // namespace ls
