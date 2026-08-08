#include "meshtastic.h"
#include "protobuf_lite.h"
#include "base64.h"
#include "../crypto/aes.h"
#include <cstdio>
#include <cstring>

namespace ls {

const uint8_t MESH_DEFAULT_KEY[16] = {
  0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
  0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01
};

// The SX1262's ceiling; a regional limit above this cannot be honoured anyway.
static const int8_t RADIO_MAX_DBM = 22;

static const MeshPresetInfo PRESETS[MESH_PRESET_COUNT] = {
  {"LongFast",   11, 250000, 5},
  {"LongSlow",   12, 125000, 8},
  {"VLongSlow",  12,  62500, 8},
  {"MediumSlow", 10, 250000, 5},
  {"MediumFast",  9, 250000, 5},
  {"ShortSlow",   8, 250000, 5},
  {"ShortFast",   7, 250000, 5},
  {"LongMod",    11, 125000, 8},
  {"ShortTurbo",  7, 500000, 5},
};

static const MeshRegionInfo REGIONS[MESH_REGION_COUNT] = {
  {"EU_868", 869400000, 869650000, 0, 27},
  {"US",     902000000, 928000000, 0, 30},
  {"EU_433", 433000000, 434000000, 0, 12},
  {"CN",     470000000, 510000000, 0, 19},
  {"JP",     920800000, 927800000, 0, 16},
  {"ANZ",    915000000, 928000000, 0, 30},
  {"KR",     920000000, 923000000, 0, 22},
  {"TW",     920000000, 925000000, 0, 27},
  {"RU",     868700000, 869200000, 0, 20},
  {"IN",     865000000, 867000000, 0, 30},
  {"NZ_865", 864000000, 868000000, 0, 30},
  {"TH",     920000000, 925000000, 0, 16},
  {"UA_868", 868000000, 868600000, 0, 14},
};

static const uint8_t ROLES[MESH_ROLE_COUNT] = {
  MESH_ROLE_CLIENT, MESH_ROLE_CLIENT_MUTE, MESH_ROLE_ROUTER,
  MESH_ROLE_REPEATER, MESH_ROLE_TRACKER, MESH_ROLE_SENSOR
};

uint8_t meshtasticRoleAt(uint8_t idx) { return ROLES[idx < MESH_ROLE_COUNT ? idx : 1]; }

uint8_t meshtasticRoleIndex(uint8_t role) {
  for (uint8_t i = 0; i < MESH_ROLE_COUNT; i++) if (ROLES[i] == role) return i;
  return 0;
}

const char* meshtasticRoleName(uint8_t role) {
  switch (role) {
    case MESH_ROLE_CLIENT:      return "CLIENT";
    case MESH_ROLE_CLIENT_MUTE: return "MUTE";
    case MESH_ROLE_ROUTER:      return "ROUTER";
    case MESH_ROLE_REPEATER:    return "REPEAT";
    case MESH_ROLE_TRACKER:     return "TRACKER";
    case MESH_ROLE_SENSOR:      return "SENSOR";
    default:                    return "?";
  }
}

const MeshPresetInfo& meshtasticPresetInfo(uint8_t preset) {
  return PRESETS[preset < MESH_PRESET_COUNT ? preset : (uint8_t)MPRESET_LONG_FAST];
}

const MeshRegionInfo& meshtasticRegionInfo(uint8_t regionIdx) {
  return REGIONS[regionIdx < MESH_REGION_COUNT ? regionIdx : 0];
}

const char* meshtasticPresetName(uint8_t preset) { return meshtasticPresetInfo(preset).name; }
const char* meshtasticRegionName(uint8_t regionIdx) { return meshtasticRegionInfo(regionIdx).name; }

MeshtasticCfg::MeshtasticCfg() {
  std::memcpy(key, MESH_DEFAULT_KEY, sizeof(MESH_DEFAULT_KEY));
}

const char* meshtasticChannelName(const MeshtasticCfg& c, uint8_t preset) {
  return c.chanName[0] ? c.chanName : meshtasticPresetName(preset);
}

static uint8_t xorHash(const uint8_t* p, size_t n) {
  uint8_t h = 0;
  for (size_t i = 0; i < n; i++) h ^= p[i];
  return h;
}

// djb2 — what the firmware uses to spread channel names across a region's slots.
// VERIFY: only observable in multi-channel regions (EU_868 has a single slot for
// the 250 kHz presets, so the result there is 0 regardless).
static uint32_t nameHash(const char* s) {
  uint32_t h = 5381;
  while (*s) h = ((h << 5) + h) + (uint8_t)*s++;
  return h;
}

uint32_t meshtasticChannelCount(const MeshtasticCfg& c, uint8_t preset) {
  const MeshRegionInfo& r = meshtasticRegionInfo(c.region);
  uint32_t step = r.spacingHz + meshtasticPresetInfo(preset).bwHz;
  if (step == 0 || r.freqEndHz <= r.freqStartHz) return 1;
  uint32_t n = (r.freqEndHz - r.freqStartHz) / step;
  return n ? n : 1;
}

uint32_t meshtasticChannelNum(const MeshtasticCfg& c, uint8_t preset) {
  return nameHash(meshtasticChannelName(c, preset)) % meshtasticChannelCount(c, preset);
}

uint32_t meshtasticFrequencyHz(const MeshtasticCfg& c, uint8_t preset) {
  const MeshRegionInfo& r = meshtasticRegionInfo(c.region);
  uint32_t bw = meshtasticPresetInfo(preset).bwHz;
  return r.freqStartHz + bw / 2 + meshtasticChannelNum(c, preset) * bw;
}

uint8_t meshtasticChannelHash(const MeshtasticCfg& c, uint8_t preset) {
  const char* name = meshtasticChannelName(c, preset);
  return (uint8_t)(xorHash((const uint8_t*)name, std::strlen(name)) ^ xorHash(c.key, c.keyLen));
}

RadioCfg meshtasticRadioCfg(const MeshtasticCfg& c, uint8_t preset) {
  const MeshPresetInfo& p = meshtasticPresetInfo(preset);
  const MeshRegionInfo& r = meshtasticRegionInfo(c.region);
  RadioCfg out;
  out.freqHz = meshtasticFrequencyHz(c, preset);
  out.bwHz = p.bwHz;
  out.sf = p.sf;
  out.cr = p.cr;
  out.preamble = 16;
  out.explicitHeader = true;
  out.crc = true;
  out.power = r.maxPowerDbm < RADIO_MAX_DBM ? r.maxPowerDbm : RADIO_MAX_DBM;
  out.syncWord = 0x2b;   // Meshtastic public sync word
  return out;
}

uint32_t meshtasticNodeId(const MeshtasticCfg& c, uint16_t ourAddr) {
  return c.nodeId ? c.nodeId : (0x4C530000u | ourAddr);
}

bool meshtasticParsePsk(const char* b64, uint8_t key[32], uint8_t& keyLen) {
  if (!key) return false;
  if (!b64 || !b64[0]) {                       // blank = the default public channel
    std::memcpy(key, MESH_DEFAULT_KEY, 16);
    keyLen = 16;
    return true;
  }

  uint8_t raw[33];
  size_t n = base64Decode(b64, raw, sizeof(raw));
  if (n == 0) return false;

  if (n == 1) {
    // Shorthand: 0 disables encryption, otherwise index into the default key by
    // offsetting its last byte ("AQ==" = 1 = the public channel itself).
    if (raw[0] == 0) { keyLen = 0; std::memset(key, 0, 32); return true; }
    std::memcpy(key, MESH_DEFAULT_KEY, 16);
    key[15] = (uint8_t)(key[15] + (raw[0] - 1));
    keyLen = 16;
    return true;
  }
  if (n == 16 || n == 32) {
    std::memset(key, 0, 32);
    std::memcpy(key, raw, n);
    keyLen = (uint8_t)n;
    return true;
  }
  return false;
}

size_t meshtasticFormatPsk(const uint8_t key[32], uint8_t keyLen, char* out, size_t cap) {
  if (!out || cap == 0) return 0;
  if (keyLen == 0) { if (cap < 5) return 0; std::memcpy(out, "AA==", 5); return 4; }
  if (keyLen == 16 && std::memcmp(key, MESH_DEFAULT_KEY, 16) == 0) {
    if (cap < 5) return 0;
    std::memcpy(out, "AQ==", 5);
    return 4;
  }
  return base64Encode(key, keyLen, out, cap);
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

// Wrap an already-built port payload as Data{portnum,payload}, encrypt it and
// prepend the 16-byte cleartext header. Shared by all three encoders.
static size_t buildFrame(uint32_t from, uint32_t packetId, uint8_t channelHash,
                         uint8_t portnum, const uint8_t* payload, size_t plen,
                         const uint8_t* key, uint8_t keyLen, uint8_t* out, size_t cap) {
  if (!payload || !out) return 0;

  uint8_t data[256];
  PbWriter w(data, sizeof(data));
  if (!w.putVarintField(1, portnum)) return 0;
  if (!w.putBytesField(2, payload, plen)) return 0;

  if (keyLen) {
    uint8_t nonce[16];
    meshtastic_nonce(packetId, from, nonce);
    if (!aes_ctr_xor(key, keyLen, nonce, data, w.len)) return 0;
  }

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

size_t meshtastic_encode_text(uint32_t from, uint32_t packetId, uint8_t channelHash,
                              const char* text, const uint8_t* key, uint8_t keyLen,
                              uint8_t* out, size_t cap) {
  if (!text) return 0;
  size_t tlen = std::strlen(text);
  if (tlen == 0 || tlen > 200) return 0;
  return buildFrame(from, packetId, channelHash, MESH_PORT_TEXT,
                    (const uint8_t*)text, tlen, key, keyLen, out, cap);
}

size_t meshtastic_encode_position(uint32_t from, uint32_t packetId, uint8_t channelHash,
                                  int32_t latI, int32_t lonI, int32_t altM,
                                  const uint8_t* key, uint8_t keyLen,
                                  uint8_t* out, size_t cap) {
  uint8_t pos[48];
  PbWriter p(pos, sizeof(pos));
  if (!p.putFixed32Field(1, (uint32_t)latI)) return 0;          // latitude_i (sfixed32)
  if (!p.putFixed32Field(2, (uint32_t)lonI)) return 0;          // longitude_i (sfixed32)
  if (!p.putVarintField(3, (uint64_t)(int64_t)altM)) return 0;  // altitude (int32)
  return buildFrame(from, packetId, channelHash, MESH_PORT_POSITION,
                    pos, p.len, key, keyLen, out, cap);
}

size_t meshtastic_encode_nodeinfo(uint32_t from, uint32_t packetId, uint8_t channelHash,
                                  const char* longName, const char* shortName,
                                  uint8_t hwModel, uint8_t role,
                                  const uint8_t* key, uint8_t keyLen,
                                  uint8_t* out, size_t cap) {
  if (!longName || !shortName || !longName[0] || !shortName[0]) return 0;
  size_t ll = std::strlen(longName), sl = std::strlen(shortName);
  if (ll > 39 || sl > 4) return 0;

  char id[12];
  std::snprintf(id, sizeof(id), "!%08x", (unsigned)from);

  uint8_t user[96];
  PbWriter u(user, sizeof(user));
  if (!u.putBytesField(1, (const uint8_t*)id, std::strlen(id))) return 0;   // id
  if (!u.putBytesField(2, (const uint8_t*)longName, ll)) return 0;          // long_name
  if (!u.putBytesField(3, (const uint8_t*)shortName, sl)) return 0;         // short_name
  if (!u.putVarintField(5, hwModel)) return 0;                              // hw_model
  if (!u.putVarintField(7, role)) return 0;                                 // role
  return buildFrame(from, packetId, channelHash, MESH_PORT_NODEINFO,
                    user, u.len, key, keyLen, out, cap);
}

bool meshtastic_decode(const uint8_t* buf, size_t n, const uint8_t* key, uint8_t keyLen,
                       MeshPacket& out) {
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

  if (keyLen) {
    uint8_t nonce[16];
    meshtastic_nonce(out.id, out.from, nonce);
    if (!aes_ctr_xor(key, keyLen, nonce, tmp, plen)) return false;
  }

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
