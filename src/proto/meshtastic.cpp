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

RadioCfg meshtasticPreset(int idx) {
  static const uint8_t SF[MESH_PRESET_COUNT] = {11, 9, 7};   // LongFast / MediumFast / ShortFast
  RadioCfg c = meshtasticPresetEU868();
  c.sf = SF[(idx < 0 || idx >= MESH_PRESET_COUNT) ? 1 : idx];
  return c;
}

const char* meshtasticPresetName(int idx) {
  static const char* NAMES[MESH_PRESET_COUNT] = {"LongFast", "MediumFast", "ShortFast"};
  return (idx >= 0 && idx < MESH_PRESET_COUNT) ? NAMES[idx] : "?";
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

// Telemetry { uint32 time=1; DeviceMetrics device_metrics=2; ... }
// DeviceMetrics { uint32 battery_level=1; float voltage=2; ... }
static bool decodeTelemetry(const uint8_t* d, size_t n, MeshPacket& out) {
  PbReader r(d, n);
  uint32_t field;
  uint8_t wire;
  while (!r.eof()) {
    if (!r.readTag(field, wire)) return false;
    if (field == 2 && wire == 2) {
      const uint8_t* dm; size_t dml;
      if (!r.readLengthDelimited(dm, dml)) return false;
      PbReader m(dm, dml);
      uint32_t f2; uint8_t w2;
      while (!m.eof()) {
        if (!m.readTag(f2, w2)) return false;
        if (f2 == 1 && w2 == 0) {
          uint64_t v; if (!m.readVarint(v)) return false; out.battery = (uint8_t)v;
        } else if (f2 == 2 && w2 == 5) {
          uint32_t bits; if (!m.readFixed32(bits)) return false;
          float volts; std::memcpy(&volts, &bits, 4);
          if (volts > 0 && volts < 20) out.voltCv = (uint16_t)(volts * 100 + 0.5f);
        } else if (!m.skip(w2)) {
          return false;
        }
      }
      out.hasMetrics = true;
    } else if (!r.skip(wire)) {
      return false;
    }
  }
  return out.hasMetrics;
}

static uint8_t xorHash(const uint8_t* p, size_t n) {
  uint8_t h = 0;
  for (size_t i = 0; i < n; i++) h ^= p[i];
  return h;
}

uint8_t meshtasticDefaultChannelHash() {
  // The default primary channel's empty name resolves to the modem-preset name.
  static const char NAME[] = "MediumFast";
  return (uint8_t)(xorHash((const uint8_t*)NAME, sizeof(NAME) - 1) ^ xorHash(MESH_DEFAULT_KEY, 16));
}

size_t meshtastic_encode_text(uint32_t from, uint32_t packetId, uint8_t channelHash,
                              const char* text, const uint8_t key[16],
                              uint8_t* out, size_t cap) {
  if (!text || !out) return 0;
  size_t tlen = std::strlen(text);
  if (tlen == 0 || tlen > 200) return 0;

  uint8_t data[256];
  PbWriter w(data, sizeof(data));
  if (!w.putVarintField(1, MESH_PORT_TEXT)) return 0;               // portnum
  if (!w.putBytesField(2, (const uint8_t*)text, tlen)) return 0;    // payload

  uint8_t nonce[16];
  meshtastic_nonce(packetId, from, nonce);
  aes128_ctr_xor(key, nonce, data, w.len);

  size_t total = MESH_HEADER_LEN + w.len;
  if (cap < total) return 0;
  out[0] = 0xFF; out[1] = 0xFF; out[2] = 0xFF; out[3] = 0xFF;       // to = broadcast
  out[4] = (uint8_t)from;  out[5] = (uint8_t)(from >> 8);
  out[6] = (uint8_t)(from >> 16); out[7] = (uint8_t)(from >> 24);
  out[8] = (uint8_t)packetId; out[9] = (uint8_t)(packetId >> 8);
  out[10] = (uint8_t)(packetId >> 16); out[11] = (uint8_t)(packetId >> 24);
  out[12] = 0x63;              // flags: hop_limit=3 | hop_start=3<<5  (VERIFY)
  out[13] = channelHash;
  out[14] = 0;                 // next_hop
  out[15] = 0;                 // relay_node
  std::memcpy(out + MESH_HEADER_LEN, data, w.len);
  return total;
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
  if (portnum == MESH_PORT_TEXT && dp) {
    copyStr(out.text, sizeof(out.text), dp, dl);
    out.hasText = out.text[0] != 0;
    return out.hasText;
  }
  if (portnum == MESH_PORT_POSITION && dp) return decodePosition(dp, dl, out);
  if (portnum == MESH_PORT_NODEINFO && dp) return decodeUser(dp, dl, out);
  if (portnum == MESH_PORT_TELEMETRY && dp) return decodeTelemetry(dp, dl, out);
  return false;
}

} // namespace ls
