#pragma once
#include <cstdint>
#include <cstddef>
#include "airtime.h"   // RadioCfg

namespace ls {

// Decoder for over-the-air Meshtastic frames (the "scanner", Direction B). This
// is read-only reception of a foreign protocol on the shared 868 band; results
// feed MeshOverlay(SRC_SCAN). NOT our wire format — see proto/frame.h for ours.
//
// Frame layout: a 16-byte cleartext PacketHeader, then the Data protobuf encrypted
// with AES-128-CTR. On the public channel the key is the well-known "AQ==" default.

constexpr size_t MESH_HEADER_LEN = 16;

enum MeshPort : uint8_t { MESH_PORT_POSITION = 3, MESH_PORT_NODEINFO = 4 };

// Well-known default public-channel key (the "AQ==" PSK expansion).
// VERIFY against the Meshtastic firmware during on-hardware bring-up.
extern const uint8_t MESH_DEFAULT_KEY[16];

// EU_868 / MEDIUM_FAST modem preset — the single default channel at 869.525 MHz.
// This is what the scanner tunes to and what Console applies with 'm'.
// VERIFY the exact SF/BW/CR/sync against the Meshtastic firmware on bring-up.
RadioCfg meshtasticPresetEU868();

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
};

// Build the 16-byte AES-CTR nonce from (packetId, fromNode).
// VERIFY against the Meshtastic firmware during on-hardware bring-up.
void meshtastic_nonce(uint32_t packetId, uint32_t fromNode, uint8_t nonce[16]);

// Decode one received frame (header + encrypted payload). `key` is the 16-byte
// channel key (MESH_DEFAULT_KEY for the public channel). Returns true only when
// the header parses, the payload decrypts to a valid Data protobuf, and the port
// is one we understand (Position or NodeInfo) — so a wrong key fails closed.
bool meshtastic_decode(const uint8_t* buf, size_t n, const uint8_t key[16], MeshPacket& out);

} // namespace ls
