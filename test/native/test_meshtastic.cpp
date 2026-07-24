#include <cstdio>
#include <cstring>
#include <vector>
#include "check.h"
#include "../../src/proto/meshtastic.h"
#include "../../src/crypto/aes.h"

using namespace ls;

// --- tiny protobuf writer, mirroring the Meshtastic messages we decode ---
static void putVarint(std::vector<uint8_t>& v, uint64_t x) {
  while (x >= 0x80) { v.push_back((uint8_t)(x | 0x80)); x >>= 7; }
  v.push_back((uint8_t)x);
}
static void putTag(std::vector<uint8_t>& v, uint32_t field, uint8_t wire) {
  putVarint(v, ((uint64_t)field << 3) | wire);
}
static void putFixed32(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(x & 0xff); v.push_back((x >> 8) & 0xff);
  v.push_back((x >> 16) & 0xff); v.push_back((x >> 24) & 0xff);
}
static void putBytes(std::vector<uint8_t>& v, uint32_t field, const uint8_t* d, size_t n) {
  putTag(v, field, 2); putVarint(v, n); v.insert(v.end(), d, d + n);
}
static void putString(std::vector<uint8_t>& v, uint32_t field, const char* s) {
  putBytes(v, field, (const uint8_t*)s, std::strlen(s));
}
static void put32(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(x & 0xff); v.push_back((x >> 8) & 0xff);
  v.push_back((x >> 16) & 0xff); v.push_back((x >> 24) & 0xff);
}

// Assemble a full received frame: 16-byte header + AES-CTR(Data protobuf).
static std::vector<uint8_t> buildFrame(uint32_t to, uint32_t from, uint32_t id,
                                       uint8_t chan, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> enc = data;
  uint8_t nonce[16];
  meshtastic_nonce(id, from, nonce);
  aes128_ctr_xor(MESH_DEFAULT_KEY, nonce, enc.data(), enc.size());

  std::vector<uint8_t> f;
  put32(f, to); put32(f, from); put32(f, id);
  f.push_back(0x00);   // flags
  f.push_back(chan);   // channel hash
  f.push_back(0x00);   // next_hop
  f.push_back(0x00);   // relay_node
  f.insert(f.end(), enc.begin(), enc.end());
  return f;
}

static std::vector<uint8_t> positionData(int32_t latI, int32_t lonI, int32_t alt) {
  std::vector<uint8_t> pos;
  putTag(pos, 1, 5); putFixed32(pos, (uint32_t)latI);   // latitude_i sfixed32
  putTag(pos, 2, 5); putFixed32(pos, (uint32_t)lonI);   // longitude_i sfixed32
  putTag(pos, 3, 0); putVarint(pos, (uint64_t)alt);     // altitude
  std::vector<uint8_t> data;
  putTag(data, 1, 0); putVarint(data, MESH_PORT_POSITION);
  putBytes(data, 2, pos.data(), pos.size());
  return data;
}

void run_meshtastic_tests() {
  std::printf("[meshtastic]\n");

  const uint32_t to = 0xFFFFFFFF, from = 0xAABBCCDD, id = 0x11223344;

  // --- Position round-trip (mooncat's coords) ---
  {
    auto data = positionData(364249088, 445150000, 991);
    auto f = buildFrame(to, from, id, 0x08, data);
    MeshPacket mp;
    CHECK(meshtastic_decode(f.data(), f.size(), MESH_DEFAULT_KEY, mp));
    CHECK(mp.from == from && mp.to == to && mp.id == id);
    CHECK(mp.channelHash == 0x08);
    CHECK(mp.portnum == MESH_PORT_POSITION && mp.hasPos);
    CHECK_NEAR(mp.lat, 36.4249088, 1e-6);
    CHECK_NEAR(mp.lon, 44.5150000, 1e-6);
    CHECK(mp.altitude == 991);
  }

  // --- signed sfixed32 (southern/western hemisphere) ---
  {
    auto data = positionData(-338600000, -151209000, 5);   // lat ~ -33.86, lon ~ -15.12
    auto f = buildFrame(to, from, id, 0, data);
    MeshPacket mp;
    CHECK(meshtastic_decode(f.data(), f.size(), MESH_DEFAULT_KEY, mp));
    CHECK_NEAR(mp.lat, -33.86, 1e-4);
    CHECK_NEAR(mp.lon, -15.1209, 1e-4);
  }

  // --- NodeInfo (User) round-trip, with emoji stripped from the name ---
  {
    std::vector<uint8_t> user;
    putString(user, 2, "cat\xF0\x9F\x90\xB1");   // long_name "cat🐱" -> "cat"
    putString(user, 3, "MC");                     // short_name
    putTag(user, 5, 0); putVarint(user, 4);        // hw_model
    putTag(user, 7, 0); putVarint(user, 2);        // role (CLIENT_MUTE)
    std::vector<uint8_t> data;
    putTag(data, 1, 0); putVarint(data, MESH_PORT_NODEINFO);
    putBytes(data, 2, user.data(), user.size());

    auto f = buildFrame(to, from, id, 0, data);
    MeshPacket mp;
    CHECK(meshtastic_decode(f.data(), f.size(), MESH_DEFAULT_KEY, mp));
    CHECK(mp.portnum == MESH_PORT_NODEINFO && mp.hasUser);
    CHECK(std::strcmp(mp.longName, "cat") == 0);
    CHECK(std::strcmp(mp.shortName, "MC") == 0);
    CHECK(mp.hwModel == 4 && mp.role == 2);
  }

  // --- negative cases fail closed ---
  {
    auto f = buildFrame(to, from, id, 0, positionData(1, 2, 3));
    MeshPacket mp;
    CHECK(!meshtastic_decode(f.data(), 10, MESH_DEFAULT_KEY, mp));   // truncated header

    // unknown portnum -> not decoded
    std::vector<uint8_t> data;
    putTag(data, 1, 0); putVarint(data, 99);
    putBytes(data, 2, (const uint8_t*)"xx", 2);
    auto f2 = buildFrame(to, from, id, 0, data);
    CHECK(!meshtastic_decode(f2.data(), f2.size(), MESH_DEFAULT_KEY, mp));

    // wrong key -> garbage protobuf, must not reproduce the real position
    uint8_t badKey[16];
    std::memcpy(badKey, MESH_DEFAULT_KEY, 16);
    badKey[0] ^= 0xFF;
    bool ok = meshtastic_decode(f.data(), f.size(), badKey, mp);
    CHECK(!ok || !mp.hasPos || (mp.lat != 0.0000001 && mp.lon != 0.0000002));
  }

  // --- EU_868 / MEDIUM_FAST modem preset ---
  {
    RadioCfg c = meshtasticPresetEU868();
    CHECK(c.freqHz == 869525000);
    CHECK(c.bwHz == 250000);
    CHECK(c.sf == 9);
    CHECK(c.cr == 5);
    CHECK(c.preamble == 16);
    CHECK(c.syncWord == 0x2b);
  }

  // --- scanner presets: LongFast/MediumFast/ShortFast differ only in SF ---
  {
    CHECK(meshtasticPreset(0).sf == 11 && std::strcmp(meshtasticPresetName(0), "LongFast") == 0);
    CHECK(meshtasticPreset(1).sf == 9  && std::strcmp(meshtasticPresetName(1), "MediumFast") == 0);
    CHECK(meshtasticPreset(2).sf == 7  && std::strcmp(meshtasticPresetName(2), "ShortFast") == 0);
    CHECK(meshtasticPreset(0).freqHz == 869525000 && meshtasticPreset(2).bwHz == 250000);
  }

  // --- TX: encode a text frame, then decode it back (round-trip) ---
  {
    uint8_t frame[128];
    uint8_t hash = meshtasticDefaultChannelHash();
    size_t nn = meshtastic_encode_text(0xAABBCCDD, 0x11223344, hash, "hello mesh",
                                       MESH_DEFAULT_KEY, frame, sizeof(frame));
    CHECK(nn > MESH_HEADER_LEN);
    MeshPacket mp;
    CHECK(meshtastic_decode(frame, nn, MESH_DEFAULT_KEY, mp));
    CHECK(mp.from == 0xAABBCCDD && mp.id == 0x11223344 && mp.channelHash == hash);
    CHECK(mp.portnum == MESH_PORT_TEXT && mp.hasText);
    CHECK(std::strcmp(mp.text, "hello mesh") == 0);
    CHECK(meshtastic_encode_text(1, 2, 0, "", MESH_DEFAULT_KEY, frame, sizeof(frame)) == 0); // empty rejected
  }

  // --- decode a received text message (portnum 1, raw payload) ---
  {
    std::vector<uint8_t> data;
    putTag(data, 1, 0); putVarint(data, MESH_PORT_TEXT);
    const char* msg = "hi there";
    putBytes(data, 2, (const uint8_t*)msg, std::strlen(msg));
    auto f = buildFrame(to, from, id, 0, data);
    MeshPacket mp;
    CHECK(meshtastic_decode(f.data(), f.size(), MESH_DEFAULT_KEY, mp));
    CHECK(mp.portnum == MESH_PORT_TEXT && mp.hasText);
    CHECK(std::strcmp(mp.text, "hi there") == 0);
  }

  // --- decode Telemetry -> DeviceMetrics (battery + voltage) ---
  {
    std::vector<uint8_t> dm;                       // DeviceMetrics
    putTag(dm, 1, 0); putVarint(dm, 90);            // battery_level = 90
    float volts = 3.85f; uint32_t vbits;
    std::memcpy(&vbits, &volts, 4);
    putTag(dm, 2, 5); putFixed32(dm, vbits);        // voltage = 3.85 (float)
    std::vector<uint8_t> tel;                       // Telemetry
    putBytes(tel, 2, dm.data(), dm.size());         // device_metrics = dm
    std::vector<uint8_t> data;                      // Data
    putTag(data, 1, 0); putVarint(data, MESH_PORT_TELEMETRY);
    putBytes(data, 2, tel.data(), tel.size());
    auto f = buildFrame(to, from, id, 0, data);
    MeshPacket mp;
    CHECK(meshtastic_decode(f.data(), f.size(), MESH_DEFAULT_KEY, mp));
    CHECK(mp.portnum == MESH_PORT_TELEMETRY && mp.hasMetrics);
    CHECK(mp.battery == 90);
    CHECK(mp.voltCv == 385);
  }
}
