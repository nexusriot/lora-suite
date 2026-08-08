#include "base64.h"
#include <cstring>

namespace ls {

static const char ALPHA[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int sextet(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+' || c == '-') return 62;   // '-' = URL-safe alias
  if (c == '/' || c == '_') return 63;   // '_' = URL-safe alias ('/' is an arrow key here)
  return -1;
}

size_t base64Decode(const char* in, uint8_t* out, size_t cap) {
  if (!in || !out) return 0;
  size_t n = std::strlen(in);
  if (n == 0 || n % 4) return 0;

  size_t pad = 0;
  if (in[n - 1] == '=') pad++;
  if (in[n - 2] == '=') pad++;
  size_t outLen = n / 4 * 3 - pad;
  if (outLen == 0 || outLen > cap) return 0;

  size_t w = 0;
  for (size_t i = 0; i < n; i += 4) {
    uint32_t q = 0;
    for (int j = 0; j < 4; j++) {
      char c = in[i + j];
      if (c == '=') {
        // Padding is only legal in the last two slots of the final quantum.
        if (i + 4 != n || j < 2) return 0;
        q <<= 6;
      } else {
        // A data character following padding means the tail is malformed.
        if (j > 0 && in[i + j - 1] == '=') return 0;
        int v = sextet(c);
        if (v < 0) return 0;
        q = (q << 6) | (uint32_t)v;
      }
    }
    if (w < outLen) out[w++] = (uint8_t)(q >> 16);
    if (w < outLen) out[w++] = (uint8_t)(q >> 8);
    if (w < outLen) out[w++] = (uint8_t)q;
  }
  return w;
}

size_t base64Encode(const uint8_t* in, size_t n, char* out, size_t cap) {
  if (!in || !out || n == 0) return 0;
  size_t need = (n + 2) / 3 * 4;
  if (need + 1 > cap) return 0;

  size_t w = 0;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t t = (uint32_t)in[i] << 16;
    if (i + 1 < n) t |= (uint32_t)in[i + 1] << 8;
    if (i + 2 < n) t |= (uint32_t)in[i + 2];
    out[w++] = ALPHA[(t >> 18) & 63];
    out[w++] = ALPHA[(t >> 12) & 63];
    out[w++] = (i + 1 < n) ? ALPHA[(t >> 6) & 63] : '=';
    out[w++] = (i + 2 < n) ? ALPHA[t & 63] : '=';
  }
  out[w] = 0;
  return w;
}

} // namespace ls
