#include "frame.h"
#include <cstring>

namespace ls {

void Frame::setPayload(const void* data, uint8_t n) {
  if (n > MAX_PAYLOAD) n = MAX_PAYLOAD;
  len = n;
  if (n && data) memcpy(payload, data, n);
}

uint16_t crc16(const uint8_t* data, size_t n) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

static inline void put16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
static inline uint16_t get16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }

size_t encode(const Frame& f, uint8_t* out, size_t outCap) {
  if (f.len > MAX_PAYLOAD) return 0;
  size_t total = HEADER_LEN + f.len + CRC_LEN;
  if (outCap < total) return 0;

  out[0] = PROTO_MAGIC;
  out[1] = PROTO_VERSION;
  out[2] = f.type;
  out[3] = f.flags;
  out[4] = f.chan;
  out[5] = f.hop;
  put16(out + 6, f.src);
  put16(out + 8, f.dst);
  put16(out + 10, f.msgid);
  out[12] = f.len;
  if (f.len) memcpy(out + HEADER_LEN, f.payload, f.len);

  uint16_t crc = crc16(out, HEADER_LEN + f.len);
  put16(out + HEADER_LEN + f.len, crc);
  return total;
}

bool decode(const uint8_t* buf, size_t n, Frame& out) {
  if (n < HEADER_LEN + CRC_LEN) return false;
  if (buf[0] != PROTO_MAGIC || buf[1] != PROTO_VERSION) return false;

  uint8_t len = buf[12];
  if (len > MAX_PAYLOAD) return false;
  size_t total = HEADER_LEN + len + CRC_LEN;
  if (n < total) return false;

  uint16_t want = get16(buf + HEADER_LEN + len);
  if (crc16(buf, HEADER_LEN + len) != want) return false;

  out.type  = buf[2];
  out.flags = buf[3];
  out.chan  = buf[4];
  out.hop   = buf[5];
  out.src   = get16(buf + 6);
  out.dst   = get16(buf + 8);
  out.msgid = get16(buf + 10);
  out.len   = len;
  if (len) memcpy(out.payload, buf + HEADER_LEN, len);
  return true;
}

} // namespace ls
