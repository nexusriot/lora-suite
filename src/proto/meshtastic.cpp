#include "meshtastic.h"
#include "protobuf_lite.h"
#include "../crypto/aes.h"
#include <cstring>

namespace ls {

const uint8_t MESH_DEFAULT_KEY[16] = {
  0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
  0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01
};

RadioCfg meshtasticPresetEU868() {
  RadioCfg c;
  c.freqHz = 869525000;   // EU_868 single default channel
  c.bwHz = 250000;        // MEDIUM_FAST
  c.sf = 9;
  c.cr = 5;               // 4/5
  c.preamble = 16;
  c.explicitHeader = true;
  c.crc = true;
  c.power = 14;           // EU868 legal default (we only RX, but keep it legal)
  c.syncWord = 0x2b;      // Meshtastic public sync word
  return c;
}

static uint32_t u32le(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void meshtastic_nonce(uint32_t packetId, uint32_t fromNode, uint8_t nonce[16]) {
  std::memset(nonce, 0, 16);
  nonce[0] = (uint8_t)packetId;
  nonce[1] = (uint8_t)(packetId >> 8);
  nonce[2] = (uint8_t)(packetId >> 16);
  nonce[3] = (uint8_t)(packetId >> 24);
  nonce[8] = (uint8_t)fromNode;
  nonce[9] = (uint8_t)(fromNode >> 8);
  nonce[10] = (uint8_t)(fromNode >> 16);
  nonce[11] = (uint8_t)(fromNode >> 24);
}

// Copy a protobuf string field, keeping only printable ASCII the display can render.
static void copyStr(char* dst, size_t cap, const uint8_t* s, size_t n) {
  size_t w = 0;
  for (size_t i = 0; i < n && w < cap - 1; i++) {
    char c = (char)s[i];
    if (c < 0x20 || c > 0x7e) continue;
    dst[w++] = c;
  }
  dst[w] = 0;
}

static bool decodePosition(const uint8_t* d, size_t n, MeshPacket& out) {
  PbReader r(d, n);
  bool haveLat = false, haveLon = false;
  int32_t lat = 0, lon = 0;
  uint32_t field;
  uint8_t wire;
  while (!r.eof()) {
    if (!r.readTag(field, wire)) return false;
    if (field == 1 && wire == 5) {
      uint32_t v; if (!r.readFixed32(v)) return false; lat = (int32_t)v; haveLat = true;
    } else if (field == 2 && wire == 5) {
      uint32_t v; if (!r.readFixed32(v)) return false; lon = (int32_t)v; haveLon = true;
    } else if (field == 3 && wire == 0) {
      uint64_t v; if (!r.readVarint(v)) return false; out.altitude = (int32_t)(int64_t)v;
    } else if (!r.skip(wire)) {
      return false;
    }
  }
  if (!haveLat || !haveLon) return false;
  out.lat = lat / 1e7;
  out.lon = lon / 1e7;
  out.hasPos = true;
  return true;
}

static bool decodeUser(const uint8_t* d, size_t n, MeshPacket& out) {
  PbReader r(d, n);
  uint32_t field;
  uint8_t wire;
  while (!r.eof()) {
    if (!r.readTag(field, wire)) return false;
    if (field == 2 && wire == 2) {
      const uint8_t* s; size_t l; if (!r.readLengthDelimited(s, l)) return false;
      copyStr(out.longName, sizeof(out.longName), s, l);
    } else if (field == 3 && wire == 2) {
      const uint8_t* s; size_t l; if (!r.readLengthDelimited(s, l)) return false;
      copyStr(out.shortName, sizeof(out.shortName), s, l);
    } else if (field == 5 && wire == 0) {
      uint64_t v; if (!r.readVarint(v)) return false; out.hwModel = (uint8_t)v;
    } else if (field == 7 && wire == 0) {
      uint64_t v; if (!r.readVarint(v)) return false; out.role = (uint8_t)v;
    } else if (!r.skip(wire)) {
      return false;
    }
  }
  out.hasUser = true;
  return true;
}

bool meshtastic_decode(const uint8_t* buf, size_t n, const uint8_t key[16], MeshPacket& out) {
  if (n <= MESH_HEADER_LEN) return false;
  out = MeshPacket{};
  out.to = u32le(buf);
  out.from = u32le(buf + 4);
  out.id = u32le(buf + 8);
  out.channelHash = buf[13];

  size_t plen = n - MESH_HEADER_LEN;
  uint8_t tmp[240];
  if (plen > sizeof(tmp)) return false;
  std::memcpy(tmp, buf + MESH_HEADER_LEN, plen);

  uint8_t nonce[16];
  meshtastic_nonce(out.id, out.from, nonce);
  aes128_ctr_xor(key, nonce, tmp, plen);

  // Parse the decrypted Data message: portnum (field 1) + payload (field 2).
  PbReader r(tmp, plen);
  uint8_t portnum = 0;
  const uint8_t* dp = nullptr;
  size_t dl = 0;
  uint32_t field;
  uint8_t wire;
  while (!r.eof()) {
    if (!r.readTag(field, wire)) return false;
    if (field == 1 && wire == 0) {
      uint64_t v; if (!r.readVarint(v)) return false; portnum = (uint8_t)v;
    } else if (field == 2 && wire == 2) {
      if (!r.readLengthDelimited(dp, dl)) return false;
    } else if (!r.skip(wire)) {
      return false;
    }
  }
  out.portnum = portnum;
  if (portnum == MESH_PORT_POSITION && dp) return decodePosition(dp, dl, out);
  if (portnum == MESH_PORT_NODEINFO && dp) return decodeUser(dp, dl, out);
  return false;
}

} // namespace ls
