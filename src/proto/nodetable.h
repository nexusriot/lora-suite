#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// One known peer, populated by any received frame and enriched by BEACON/NODEINFO.
struct Node {
  uint16_t addr      = 0;
  int16_t  rssi      = 0;      // dBm of last packet
  int8_t   snr       = 0;      // dB
  uint32_t lastHeard = 0;      // ms
  uint32_t packets   = 0;
  bool     hasPos    = false;
  double   lat       = 0;
  double   lon       = 0;
  char     name[12]  = {0};    // from NODEINFO
  bool     hasHealth = false;  // from a Pulse TLV
  uint8_t  battPct   = 0;
  uint8_t  uptimeHr  = 0;
  uint8_t  dutyPct   = 0;
  int8_t   tempC     = 0;
  bool     lowpwr    = false;
  uint8_t  presence  = 0;      // Presence state from the peer's health TLV
};

// Fixed-capacity table (no heap). Newest-heard wins on overflow.
class NodeTable {
public:
  static const size_t CAP = 32;

  // Record activity from addr. Returns the slot.
  Node& heard(uint16_t addr, int16_t rssi, int8_t snr, uint32_t now);
  void  setPos(uint16_t addr, double lat, double lon, uint32_t now);
  void  setName(uint16_t addr, const char* name, uint32_t now);
  void  setHealth(uint16_t addr, uint8_t battPct, uint8_t uptimeHr,
                  uint8_t dutyPct, int8_t tempC, bool lowpwr, uint8_t presence, uint32_t now);

  Node* find(uint16_t addr);
  size_t size() const { return count_; }
  const Node& at(size_t i) const { return nodes_[i]; }

  // Drop nodes not heard within ttlMs. Returns count removed.
  size_t prune(uint32_t now, uint32_t ttlMs);

private:
  Node nodes_[CAP] = {};
  size_t count_ = 0;
  Node& slotFor(uint16_t addr);
};

} // namespace ls
