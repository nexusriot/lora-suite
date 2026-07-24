#include "protobuf_lite.h"

namespace ls {

bool PbReader::readVarint(uint64_t& out) {
  out = 0;
  int shift = 0;
  while (p < end && shift <= 63) {
    uint8_t b = *p++;
    out |= (uint64_t)(b & 0x7f) << shift;
    if (!(b & 0x80)) return true;
    shift += 7;
  }
  return false;
}

bool PbReader::readTag(uint32_t& field, uint8_t& wire) {
  uint64_t t;
  if (!readVarint(t)) return false;
  field = (uint32_t)(t >> 3);
  wire = (uint8_t)(t & 7);
  return true;
}

bool PbReader::readLengthDelimited(const uint8_t*& data, size_t& len) {
  uint64_t l;
  if (!readVarint(l)) return false;
  if ((uint64_t)(end - p) < l) return false;
  data = p;
  len = (size_t)l;
  p += l;
  return true;
}

bool PbReader::readFixed32(uint32_t& out) {
  if (end - p < 4) return false;
  out = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
  p += 4;
  return true;
}

bool PbReader::readFixed64(uint64_t& out) {
  if (end - p < 8) return false;
  out = 0;
  for (int i = 0; i < 8; i++) out |= (uint64_t)p[i] << (8 * i);
  p += 8;
  return true;
}

bool PbReader::skip(uint8_t wire) {
  switch (wire) {
    case 0: { uint64_t v; return readVarint(v); }
    case 1: { if (end - p < 8) return false; p += 8; return true; }
    case 2: { const uint8_t* d; size_t l; return readLengthDelimited(d, l); }
    case 5: { if (end - p < 4) return false; p += 4; return true; }
    default: return false;
  }
}

} // namespace ls
