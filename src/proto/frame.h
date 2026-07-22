#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

constexpr uint8_t  PROTO_MAGIC    = 0x4C;   // 'L'
constexpr uint8_t  PROTO_VERSION  = 2;      // v2: Pulse health TLV grew to 7 bytes (+presence)
constexpr size_t   HEADER_LEN     = 13;
constexpr size_t   CRC_LEN        = 2;
constexpr size_t   MAX_PAYLOAD    = 200;
constexpr size_t   MAX_FRAME      = HEADER_LEN + MAX_PAYLOAD + CRC_LEN;
constexpr uint16_t ADDR_BROADCAST = 0xFFFF;
constexpr uint8_t  DEFAULT_HOP    = 3;

enum MsgType : uint8_t {
  MSG_TEXT      = 1,
  MSG_ACK       = 2,
  MSG_BEACON    = 3,
  MSG_PING      = 4,
  MSG_PONG      = 5,
  MSG_TELEMETRY = 6,
  MSG_ALERT     = 7,
  MSG_NODEINFO  = 8,
  MSG_FILECHUNK = 9,
  MSG_TIMESYNC  = 10,
  MSG_WAYPOINT  = 11,
  MSG_COUNTDOWN = 12,
};

enum Flags : uint8_t {
  FLAG_ACK_REQ   = 0x01,
  FLAG_ENCRYPTED = 0x02,
  FLAG_MESH      = 0x04,
  FLAG_FRAGMENT  = 0x08,
  FLAG_HEALTH    = 0x10,   // a Pulse health TLV is appended after the body
  FLAG_LOWPWR    = 0x20,   // sender is in a degraded/survival power state
};

// ALERT payload[0] codes above the user pager range are reserved for the system.
constexpr uint8_t ALERT_DISTRESS = 0xE0;  // Mayday distress last-will
constexpr uint8_t ALERT_LOWPWR   = 0xE1;  // Reactor entered Survival

// Ledger airtime-attribution tag (a local Frame annotation, never serialized).
// Originated frames are tagged with their MsgType; relayed frames use this.
constexpr uint8_t AIRTAG_RELAY   = 0xFE;

// Wire layout (little-endian):
//   [0] MAGIC [1] VER [2] TYPE [3] FLAGS [4] CHAN [5] HOP
//   [6..7] SRC  [8..9] DST  [10..11] MSGID  [12] LEN
//   [13..13+LEN-1] PAYLOAD   [.. +2] CRC16(header+payload)
struct Frame {
  uint8_t  type  = 0;
  uint8_t  flags = 0;
  uint8_t  chan  = 0;
  uint8_t  hop   = DEFAULT_HOP;
  uint16_t src   = 0;
  uint16_t dst   = ADDR_BROADCAST;
  uint16_t msgid = 0;
  uint8_t  len   = 0;
  uint8_t  payload[MAX_PAYLOAD] = {0};
  uint8_t  airTag = 0;   // local Ledger attribution (not serialized)

  void setPayload(const void* data, uint8_t n);
  bool isBroadcast() const { return dst == ADDR_BROADCAST; }
};

uint16_t crc16(const uint8_t* data, size_t n);

// Serialize f into out (capacity outCap). Returns encoded byte count, or 0 on error.
size_t encode(const Frame& f, uint8_t* out, size_t outCap);

// Parse and validate n bytes into out. Returns true only for a well-formed, CRC-valid frame.
bool decode(const uint8_t* buf, size_t n, Frame& out);

} // namespace ls
