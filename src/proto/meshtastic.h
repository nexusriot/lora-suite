#pragma once
#include <cstdint>
#include <cstddef>
#include "airtime.h"   // RadioCfg

namespace ls {

// Codec for over-the-air Meshtastic frames — receive (the "scanner", Direction B)
// and transmit (MeshTX/MeshChat/NodeInfo). This is a foreign protocol sharing the
// same band; it is NOT our wire format — see proto/frame.h for ours.
//
// Frame layout: a 16-byte cleartext PacketHeader, then the Data protobuf encrypted
// with AES-CTR under the channel key (AES-128 for a 16-byte PSK, AES-256 for 32).

constexpr size_t MESH_HEADER_LEN = 16;

enum MeshPort : uint8_t {
  MESH_PORT_TEXT      = 1,   // TEXT_MESSAGE_APP — payload is raw UTF-8
  MESH_PORT_POSITION  = 3,
  MESH_PORT_NODEINFO  = 4,   // User: id / long_name / short_name / hw_model / role
  MESH_PORT_TELEMETRY = 67,  // DeviceMetrics (battery/voltage)
};

// Meshtastic HardwareModel. PRIVATE_HW is the honest value for a device that is
// not one of their boards, and is what we advertise.
constexpr uint8_t MESH_HW_PRIVATE = 255;

// Meshtastic device Role, numbered as in their config protobuf. We default to
// CLIENT_MUTE because we do not rebroadcast Meshtastic traffic — claiming plain
// CLIENT would invite neighbours to route through a node that never forwards.
enum MeshDeviceRole : uint8_t {
  MESH_ROLE_CLIENT      = 0,
  MESH_ROLE_CLIENT_MUTE = 1,
  MESH_ROLE_ROUTER      = 2,
  MESH_ROLE_REPEATER    = 4,
  MESH_ROLE_TRACKER     = 5,
  MESH_ROLE_SENSOR      = 6,
};
enum { MESH_ROLE_COUNT = 6 };
uint8_t     meshtasticRoleAt(uint8_t idx);        // idx -> role value
uint8_t     meshtasticRoleIndex(uint8_t role);    // role value -> idx (0 if unknown)
const char* meshtasticRoleName(uint8_t role);

// Well-known default public-channel key (the "AQ==" PSK expansion).
// VERIFY against the Meshtastic firmware during on-hardware bring-up.
extern const uint8_t MESH_DEFAULT_KEY[16];

// Modem presets, numbered as in the firmware's ModemPreset enum so a stored value
// means the same thing on both sides.
//
// Bandwidth is NOT constant across presets: the Long/VeryLong entries are
// narrowband. A scanner pinned to one bandwidth is deaf to the others, which is
// why the whole table matters and not just the spreading factor.
// VERIFY the SF/BW/CR per preset against the firmware on bring-up.
enum MeshPreset : uint8_t {
  MPRESET_LONG_FAST      = 0,
  MPRESET_LONG_SLOW      = 1,
  MPRESET_VERY_LONG_SLOW = 2,   // deprecated upstream; kept so older nodes decode
  MPRESET_MEDIUM_SLOW    = 3,
  MPRESET_MEDIUM_FAST    = 4,
  MPRESET_SHORT_SLOW     = 5,
  MPRESET_SHORT_FAST     = 6,
  MPRESET_LONG_MODERATE  = 7,
  MPRESET_SHORT_TURBO    = 8,
};
enum { MESH_PRESET_COUNT = 9 };

struct MeshPresetInfo {
  const char* name;     // name Meshtastic substitutes for an empty primary channel
  uint8_t     sf;
  uint32_t    bwHz;
  uint8_t     cr;       // coding-rate denominator (5 => 4/5)
};

// Regional band plan. Indices are ours (0 = EU_868, the project's home region),
// not Meshtastic's RegionCode — the region is never transmitted, so it only has
// to be stable locally. VERIFY the limits against local regulations before
// transmitting; maxPowerDbm is the regional ceiling, further clamped to what the
// SX1262 can actually produce.
struct MeshRegionInfo {
  const char* name;
  uint32_t    freqStartHz;
  uint32_t    freqEndHz;
  uint32_t    spacingHz;
  int8_t      maxPowerDbm;
};
enum { MESH_REGION_COUNT = 13 };

const MeshPresetInfo& meshtasticPresetInfo(uint8_t preset);
const MeshRegionInfo& meshtasticRegionInfo(uint8_t regionIdx);
const char*           meshtasticPresetName(uint8_t preset);
const char*           meshtasticRegionName(uint8_t regionIdx);

// Everything needed to speak to one Meshtastic network. One instance lives in the
// Context and is edited by the MeshCfg app; every Meshtastic path reads it rather
// than hardcoding the public channel.
struct MeshtasticCfg {
  uint8_t  region = 0;                        // index into the region table (0 = EU_868)
  uint8_t  preset = MPRESET_LONG_FAST;        // Meshtastic's global default preset
  char     chanName[13] = "";                 // "" = the preset's default primary name
  uint8_t  key[32] = {0};                     // filled with MESH_DEFAULT_KEY by the ctor
  uint8_t  keyLen = 16;                       // 0 = unencrypted, 16 = AES-128, 32 = AES-256
  char     longName[20] = "";                 // "" = fall back to our callsign
  char     shortName[5] = "";                 // "" = derived from longName/callsign
  uint8_t  role = MESH_ROLE_CLIENT_MUTE;
  uint32_t nodeId = 0;                        // 0 = derive from our short address
  uint16_t announceMin = 0;                   // NodeInfo auto-announce period, 0 = off

  MeshtasticCfg();
};

// Effective channel name: the configured name, or the preset's default when blank
// (Meshtastic substitutes the preset name for an unnamed primary channel).
const char* meshtasticChannelName(const MeshtasticCfg& c, uint8_t preset);

// Centre frequency for a preset, derived the way the firmware does it:
//   numChannels = floor((freqEnd - freqStart) / (spacing + bandwidth))
//   channel     = hash(channelName) % numChannels
//   freq        = freqStart + bandwidth/2 + channel * bandwidth
// For EU_868 the wide presets yield exactly one channel, so this reproduces the
// familiar 869.525 MHz. VERIFY the channel-number hash for multi-channel regions.
uint32_t meshtasticFrequencyHz(const MeshtasticCfg& c, uint8_t preset);
uint32_t meshtasticChannelNum(const MeshtasticCfg& c, uint8_t preset);
uint32_t meshtasticChannelCount(const MeshtasticCfg& c, uint8_t preset);

// Header channel-hash byte: xorHash(effective name) ^ xorHash(key). VERIFY.
uint8_t meshtasticChannelHash(const MeshtasticCfg& c, uint8_t preset);

// Radio settings for a preset under this config (frequency, SF/BW/CR, sync word,
// preamble, region-limited power).
RadioCfg meshtasticRadioCfg(const MeshtasticCfg& c, uint8_t preset);

// Our node number on the Meshtastic side: the configured override, or a stable
// value derived from our short address. VERIFY that this does not collide on a
// busy mesh.
uint32_t meshtasticNodeId(const MeshtasticCfg& c, uint16_t ourAddr);

// PSK text (base64, as Meshtastic apps show it) -> key bytes, with their
// shorthand: a single byte 0 means "no encryption", and 1..255 selects the
// default key with its last byte offset by (value - 1), so "AQ==" is the public
// channel. Accepts 16- and 32-byte keys verbatim; rejects everything else.
// An empty string yields the default public key.
bool meshtasticParsePsk(const char* b64, uint8_t key[32], uint8_t& keyLen);

// Inverse for display: emits "AQ==" when the key is exactly the default one,
// otherwise the full base64. Returns the length written.
size_t meshtasticFormatPsk(const uint8_t key[32], uint8_t keyLen, char* out, size_t cap);

struct MeshPacket {
  uint32_t from = 0;
  uint32_t to = 0;
  uint32_t id = 0;
  uint8_t  channelHash = 0;
  uint8_t  portnum = 0;

  bool     hasPos = false;
  double   lat = 0, lon = 0;
  int32_t  altitude = 0;

  bool     hasUser = false;
  char     longName[20] = {0};
  char     shortName[5] = {0};
  uint8_t  hwModel = 0;   // raw Meshtastic HardwareModel enum
  uint8_t  role = 0;      // raw Meshtastic device Role enum

  bool     hasText = false;
  char     text[64] = {0};

  bool     hasMetrics = false;
  uint8_t  battery = 0;   // percent
  uint16_t voltCv = 0;    // centivolts (voltage * 100)
};

// Build the 16-byte AES-CTR nonce from (packetId, fromNode).
// VERIFY against the Meshtastic firmware during on-hardware bring-up.
void meshtastic_nonce(uint32_t packetId, uint32_t fromNode, uint8_t nonce[16]);

// Decode one received frame (header + encrypted payload). `key`/`keyLen` are the
// channel key. Returns true only when the header parses, the payload decrypts to
// a valid Data protobuf, and the port is one we understand (Text/Position/
// NodeInfo/Telemetry) — a wrong key fails closed.
bool meshtastic_decode(const uint8_t* buf, size_t n, const uint8_t* key, uint8_t keyLen,
                       MeshPacket& out);

// Encoders. Each builds a complete broadcast frame (16-byte header + AES-CTR
// Data) and returns its length, or 0 on error. VERIFY header flags/hash on
// bring-up.
size_t meshtastic_encode_text(uint32_t from, uint32_t packetId, uint8_t channelHash,
                              const char* text, const uint8_t* key, uint8_t keyLen,
                              uint8_t* out, size_t cap);

// Position in degrees*1e7, altitude in metres.
size_t meshtastic_encode_position(uint32_t from, uint32_t packetId, uint8_t channelHash,
                                  int32_t latI, int32_t lonI, int32_t altM,
                                  const uint8_t* key, uint8_t keyLen,
                                  uint8_t* out, size_t cap);

// NodeInfo (User): what makes us appear as a named node in everyone's node list
// and on the map instead of a bare "!xxxxxxxx".
size_t meshtastic_encode_nodeinfo(uint32_t from, uint32_t packetId, uint8_t channelHash,
                                  const char* longName, const char* shortName,
                                  uint8_t hwModel, uint8_t role,
                                  const uint8_t* key, uint8_t keyLen,
                                  uint8_t* out, size_t cap);

} // namespace ls
