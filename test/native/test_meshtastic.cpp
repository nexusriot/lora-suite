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
    CHECK(meshtastic_decode(f.data(), f.size(), MESH_DEFAULT_KEY, 16, mp));
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
    CHECK(meshtastic_decode(f.data(), f.size(), MESH_DEFAULT_KEY, 16, mp));
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
    CHECK(meshtastic_decode(f.data(), f.size(), MESH_DEFAULT_KEY, 16, mp));
    CHECK(mp.portnum == MESH_PORT_NODEINFO && mp.hasUser);
    CHECK(std::strcmp(mp.longName, "cat") == 0);
    CHECK(std::strcmp(mp.shortName, "MC") == 0);
    CHECK(mp.hwModel == 4 && mp.role == 2);
  }

  // --- negative cases fail closed ---
  {
    auto f = buildFrame(to, from, id, 0, positionData(1, 2, 3));
    MeshPacket mp;
    CHECK(!meshtastic_decode(f.data(), 10, MESH_DEFAULT_KEY, 16, mp));   // truncated header

    // unknown portnum -> not decoded
    std::vector<uint8_t> data;
    putTag(data, 1, 0); putVarint(data, 99);
    putBytes(data, 2, (const uint8_t*)"xx", 2);
    auto f2 = buildFrame(to, from, id, 0, data);
    CHECK(!meshtastic_decode(f2.data(), f2.size(), MESH_DEFAULT_KEY, 16, mp));

    // wrong key -> garbage protobuf, must not reproduce the real position
    uint8_t badKey[16];
    std::memcpy(badKey, MESH_DEFAULT_KEY, 16);
    badKey[0] ^= 0xFF;
    bool ok = meshtastic_decode(f.data(), f.size(), badKey, 16, mp);
    CHECK(!ok || !mp.hasPos || (mp.lat != 0.0000001 && mp.lon != 0.0000002));
  }

  // --- default config = the public EU_868 LongFast channel at 869.525 MHz ---
  // Reproducing the previously hardcoded constant from the region/preset tables
  // is the check that the whole frequency derivation is right.
  {
    MeshtasticCfg cfg;
    CHECK(cfg.region == 0 && std::strcmp(meshtasticRegionName(cfg.region), "EU_868") == 0);
    CHECK(cfg.preset == MPRESET_LONG_FAST);
    CHECK(cfg.keyLen == 16 && std::memcmp(cfg.key, MESH_DEFAULT_KEY, 16) == 0);

    RadioCfg c = meshtasticRadioCfg(cfg, cfg.preset);
    CHECK(c.freqHz == 869525000);
    CHECK(c.bwHz == 250000);
    CHECK(c.sf == 11);
    CHECK(c.cr == 5);
    CHECK(c.preamble == 16);
    CHECK(c.syncWord == 0x2b);
    CHECK(c.power == 22);                    // region allows 27, radio tops out at 22
    CHECK(meshtasticChannelCount(cfg, cfg.preset) == 1);   // one 250 kHz slot in 869.4-869.65
    CHECK(meshtasticChannelNum(cfg, cfg.preset) == 0);

    // MediumFast — the old hardcoded preset — sits on the same single slot.
    CHECK(meshtasticFrequencyHz(cfg, MPRESET_MEDIUM_FAST) == 869525000);
  }

  // --- the preset table: bandwidth is NOT constant, which is why scanning one
  //     bandwidth misses whole networks ---
  {
    CHECK(meshtasticPresetInfo(MPRESET_LONG_FAST).sf == 11);
    CHECK(meshtasticPresetInfo(MPRESET_LONG_FAST).bwHz == 250000);
    CHECK(meshtasticPresetInfo(MPRESET_MEDIUM_FAST).sf == 9);
    CHECK(meshtasticPresetInfo(MPRESET_SHORT_FAST).sf == 7);
    CHECK(meshtasticPresetInfo(MPRESET_LONG_MODERATE).bwHz == 125000);
    CHECK(meshtasticPresetInfo(MPRESET_LONG_SLOW).bwHz == 125000);
    CHECK(meshtasticPresetInfo(MPRESET_LONG_SLOW).sf == 12);
    CHECK(meshtasticPresetInfo(MPRESET_VERY_LONG_SLOW).bwHz == 62500);
    CHECK(meshtasticPresetInfo(MPRESET_SHORT_TURBO).bwHz == 500000);
    CHECK(meshtasticPresetInfo(MPRESET_LONG_MODERATE).cr == 8);
    CHECK(std::strcmp(meshtasticPresetName(MPRESET_LONG_FAST), "LongFast") == 0);
    CHECK(std::strcmp(meshtasticPresetName(MPRESET_LONG_MODERATE), "LongMod") == 0);
    // out-of-range indices clamp to the default rather than reading past the table
    CHECK(meshtasticPresetInfo(200).sf == meshtasticPresetInfo(MPRESET_LONG_FAST).sf);
    CHECK(meshtasticRegionInfo(200).freqStartHz == meshtasticRegionInfo(0).freqStartHz);
  }

  // --- narrowband presets subdivide EU_868 into more than one slot ---
  {
    MeshtasticCfg cfg;
    CHECK(meshtasticChannelCount(cfg, MPRESET_LONG_SLOW) == 2);     // 250 kHz / 125 kHz
    CHECK(meshtasticChannelCount(cfg, MPRESET_VERY_LONG_SLOW) == 4);
    uint32_t f = meshtasticFrequencyHz(cfg, MPRESET_LONG_SLOW);
    CHECK(f == 869462500 || f == 869587500);                        // slot 0 or 1
  }

  // --- a wideband region gives many slots and the name picks one ---
  {
    MeshtasticCfg us;
    us.region = 1;                                                  // US 902-928
    CHECK(std::strcmp(meshtasticRegionName(us.region), "US") == 0);
    CHECK(meshtasticChannelCount(us, MPRESET_LONG_FAST) == 104);
    uint32_t f = meshtasticFrequencyHz(us, MPRESET_LONG_FAST);
    CHECK(f >= 902000000 && f < 928000000);
    CHECK((f - 902125000) % 250000 == 0);                           // on the slot grid

    // A different channel name lands on a different slot.
    MeshtasticCfg us2 = us;
    std::snprintf(us2.chanName, sizeof(us2.chanName), "privatemesh");
    CHECK(meshtasticChannelNum(us2, MPRESET_LONG_FAST) !=
          meshtasticChannelNum(us, MPRESET_LONG_FAST) ||
          meshtasticChannelHash(us2, MPRESET_LONG_FAST) !=
          meshtasticChannelHash(us, MPRESET_LONG_FAST));
  }

  // --- PSK parsing follows Meshtastic's shorthand ---
  {
    uint8_t key[32];
    uint8_t len = 0;

    // blank = the public default
    CHECK(meshtasticParsePsk("", key, len));
    CHECK(len == 16 && std::memcmp(key, MESH_DEFAULT_KEY, 16) == 0);

    // "AQ==" is single byte 1 = the default key itself
    CHECK(meshtasticParsePsk("AQ==", key, len));
    CHECK(len == 16 && std::memcmp(key, MESH_DEFAULT_KEY, 16) == 0);

    // "Ag==" is index 2 = default key with the last byte bumped
    CHECK(meshtasticParsePsk("Ag==", key, len));
    CHECK(len == 16 && key[15] == (uint8_t)(MESH_DEFAULT_KEY[15] + 1));
    CHECK(std::memcmp(key, MESH_DEFAULT_KEY, 15) == 0);

    // "AA==" is index 0 = no encryption
    CHECK(meshtasticParsePsk("AA==", key, len));
    CHECK(len == 0);

    // a real 16-byte key, verbatim
    CHECK(meshtasticParsePsk("AAECAwQFBgcICQoLDA0ODw==", key, len));
    CHECK(len == 16 && key[0] == 0 && key[15] == 15);

    // a 32-byte key -> AES-256
    CHECK(meshtasticParsePsk("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=", key, len));
    CHECK(len == 32 && key[31] == 31);

    // wrong lengths and bad characters are refused outright
    CHECK(!meshtasticParsePsk("AAEC", key, len));            // 3 bytes
    CHECK(!meshtasticParsePsk("AQ=", key, len));             // not a multiple of 4
    CHECK(!meshtasticParsePsk("!!!!", key, len));            // outside the alphabet

    // round-trip through the display form
    char txt[48];
    MeshtasticCfg cfg;
    CHECK(meshtasticFormatPsk(cfg.key, cfg.keyLen, txt, sizeof(txt)) == 4);
    CHECK(std::strcmp(txt, "AQ==") == 0);
    uint8_t rk[32]; uint8_t rl = 0;
    CHECK(meshtasticParsePsk(txt, rk, rl));
    CHECK(rl == 16 && std::memcmp(rk, MESH_DEFAULT_KEY, 16) == 0);
  }

  // --- our node id, and the role table ---
  {
    MeshtasticCfg cfg;
    CHECK(meshtasticNodeId(cfg, 0x1234) == 0x4C531234u);
    cfg.nodeId = 0xDEADBEEF;
    CHECK(meshtasticNodeId(cfg, 0x1234) == 0xDEADBEEFu);
    CHECK(meshtasticRoleAt(meshtasticRoleIndex(MESH_ROLE_SENSOR)) == MESH_ROLE_SENSOR);
    CHECK(std::strcmp(meshtasticRoleName(MESH_ROLE_CLIENT_MUTE), "MUTE") == 0);
  }

  // --- TX: encode a text frame, then decode it back (round-trip) ---
  {
    MeshtasticCfg tcfg;
    uint8_t frame[128];
    uint8_t hash = meshtasticChannelHash(tcfg, tcfg.preset);
    size_t nn = meshtastic_encode_text(0xAABBCCDD, 0x11223344, hash, "hello mesh",
                                       MESH_DEFAULT_KEY, 16, frame, sizeof(frame));
    CHECK(nn > MESH_HEADER_LEN);
    MeshPacket mp;
    CHECK(meshtastic_decode(frame, nn, MESH_DEFAULT_KEY, 16, mp));
    CHECK(mp.from == 0xAABBCCDD && mp.id == 0x11223344 && mp.channelHash == hash);
    CHECK(mp.portnum == MESH_PORT_TEXT && mp.hasText);
    CHECK(std::strcmp(mp.text, "hello mesh") == 0);
    CHECK(meshtastic_encode_text(1, 2, 0, "", MESH_DEFAULT_KEY, 16, frame, sizeof(frame)) == 0); // empty rejected
  }

  // --- decode a received text message (portnum 1, raw payload) ---
  {
    std::vector<uint8_t> data;
    putTag(data, 1, 0); putVarint(data, MESH_PORT_TEXT);
    const char* msg = "hi there";
    putBytes(data, 2, (const uint8_t*)msg, std::strlen(msg));
    auto f = buildFrame(to, from, id, 0, data);
    MeshPacket mp;
    CHECK(meshtastic_decode(f.data(), f.size(), MESH_DEFAULT_KEY, 16, mp));
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
    CHECK(meshtastic_decode(f.data(), f.size(), MESH_DEFAULT_KEY, 16, mp));
    CHECK(mp.portnum == MESH_PORT_TELEMETRY && mp.hasMetrics);
    CHECK(mp.battery == 90);
    CHECK(mp.voltCv == 385);
  }

  // --- channel hash: name-derived, so it moves with the preset when the channel
  //     is unnamed, and stops moving once a name is set ---
  {
    MeshtasticCfg cfg;
    CHECK(meshtasticChannelHash(cfg, MPRESET_LONG_FAST) !=
          meshtasticChannelHash(cfg, MPRESET_MEDIUM_FAST));
    CHECK(std::strcmp(meshtasticChannelName(cfg, MPRESET_LONG_FAST), "LongFast") == 0);

    std::snprintf(cfg.chanName, sizeof(cfg.chanName), "squad");
    CHECK(std::strcmp(meshtasticChannelName(cfg, MPRESET_LONG_FAST), "squad") == 0);
    CHECK(meshtasticChannelHash(cfg, MPRESET_LONG_FAST) ==
          meshtasticChannelHash(cfg, MPRESET_MEDIUM_FAST));

    // The key is part of the hash, so a different PSK is a different channel.
    MeshtasticCfg other = cfg;
    other.key[0] ^= 0xFF;
    CHECK(meshtasticChannelHash(other, MPRESET_LONG_FAST) !=
          meshtasticChannelHash(cfg, MPRESET_LONG_FAST));
  }

  // --- TX: NodeInfo, the announce that gives us a name instead of a bare id ---
  {
    MeshtasticCfg cfg;
    uint8_t frame[192];
    uint32_t from = meshtasticNodeId(cfg, 0x0042);
    uint8_t hash = meshtasticChannelHash(cfg, cfg.preset);
    size_t nn = meshtastic_encode_nodeinfo(from, 0x99, hash, "Cardputer One", "CP1",
                                           MESH_HW_PRIVATE, MESH_ROLE_CLIENT_MUTE,
                                           cfg.key, cfg.keyLen, frame, sizeof(frame));
    CHECK(nn > MESH_HEADER_LEN);
    MeshPacket mp;
    CHECK(meshtastic_decode(frame, nn, cfg.key, cfg.keyLen, mp));
    CHECK(mp.portnum == MESH_PORT_NODEINFO && mp.hasUser);
    CHECK(mp.from == from && mp.channelHash == hash);
    CHECK(std::strcmp(mp.longName, "Cardputer One") == 0);
    CHECK(std::strcmp(mp.shortName, "CP1") == 0);
    CHECK(mp.hwModel == MESH_HW_PRIVATE && mp.role == MESH_ROLE_CLIENT_MUTE);

    // A short name over four bytes, or an empty name, is refused rather than truncated.
    CHECK(meshtastic_encode_nodeinfo(from, 1, 0, "Node", "TOOLONG", MESH_HW_PRIVATE, 0,
                                     cfg.key, cfg.keyLen, frame, sizeof(frame)) == 0);
    CHECK(meshtastic_encode_nodeinfo(from, 1, 0, "", "AB", MESH_HW_PRIVATE, 0,
                                     cfg.key, cfg.keyLen, frame, sizeof(frame)) == 0);
  }

  // --- a private 32-byte channel: AES-256 end to end ---
  {
    MeshtasticCfg cfg;
    CHECK(meshtasticParsePsk("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=",
                             cfg.key, cfg.keyLen));
    CHECK(cfg.keyLen == 32);
    std::snprintf(cfg.chanName, sizeof(cfg.chanName), "private");

    uint8_t frame[160];
    size_t nn = meshtastic_encode_text(0x4C530001, 0x77,
                                       meshtasticChannelHash(cfg, cfg.preset),
                                       "aes256 hello", cfg.key, cfg.keyLen,
                                       frame, sizeof(frame));
    CHECK(nn > MESH_HEADER_LEN);
    MeshPacket mp;
    CHECK(meshtastic_decode(frame, nn, cfg.key, cfg.keyLen, mp));
    CHECK(mp.hasText && std::strcmp(mp.text, "aes256 hello") == 0);

    // The public key must not open it, and a 16-byte read of a 32-byte key must not either.
    CHECK(!meshtastic_decode(frame, nn, MESH_DEFAULT_KEY, 16, mp) || !mp.hasText ||
          std::strcmp(mp.text, "aes256 hello") != 0);
    CHECK(!meshtastic_decode(frame, nn, cfg.key, 16, mp) || !mp.hasText ||
          std::strcmp(mp.text, "aes256 hello") != 0);

    // An unsupported key length is refused outright rather than silently skipped.
    CHECK(meshtastic_encode_text(1, 2, 0, "x", cfg.key, 24, frame, sizeof(frame)) == 0);
  }

  // --- an unencrypted channel (PSK index 0) still round-trips ---
  {
    MeshtasticCfg cfg;
    CHECK(meshtasticParsePsk("AA==", cfg.key, cfg.keyLen));
    CHECK(cfg.keyLen == 0);
    uint8_t frame[128];
    size_t nn = meshtastic_encode_text(0x4C530002, 0x11, 0, "cleartext",
                                       cfg.key, cfg.keyLen, frame, sizeof(frame));
    CHECK(nn > MESH_HEADER_LEN);
    MeshPacket mp;
    CHECK(meshtastic_decode(frame, nn, cfg.key, cfg.keyLen, mp));
    CHECK(mp.hasText && std::strcmp(mp.text, "cleartext") == 0);
  }

  // --- TX: encode a Position frame, decode it back ---
  {
    MeshtasticCfg dcfg;
    uint8_t frame[128];
    uint8_t hash = meshtasticChannelHash(dcfg, dcfg.preset);
    size_t nn = meshtastic_encode_position(0xAABBCCDD, 0x55, hash,
                                           364249088, 445150000, 991,
                                           MESH_DEFAULT_KEY, 16, frame, sizeof(frame));
    CHECK(nn > MESH_HEADER_LEN);
    MeshPacket mp;
    CHECK(meshtastic_decode(frame, nn, MESH_DEFAULT_KEY, 16, mp));
    CHECK(mp.portnum == MESH_PORT_POSITION && mp.hasPos && mp.channelHash == hash);
    CHECK_NEAR(mp.lat, 36.4249088, 1e-6);
    CHECK_NEAR(mp.lon, 44.5150000, 1e-6);
    CHECK(mp.altitude == 991);
  }
}
