#include "nodetable.h"
#include <cstring>

namespace ls {

Node* NodeTable::find(uint16_t addr) {
  for (size_t i = 0; i < count_; i++)
    if (nodes_[i].addr == addr) return &nodes_[i];
  return nullptr;
}

Node& NodeTable::slotFor(uint16_t addr) {
  if (Node* n = find(addr)) return *n;
  if (count_ < CAP) {
    Node& n = nodes_[count_++];
    n = Node{};
    n.addr = addr;
    return n;
  }
  // full: evict the least-recently-heard slot
  size_t oldest = 0;
  for (size_t i = 1; i < count_; i++)
    if (nodes_[i].lastHeard < nodes_[oldest].lastHeard) oldest = i;
  nodes_[oldest] = Node{};
  nodes_[oldest].addr = addr;
  return nodes_[oldest];
}

Node& NodeTable::heard(uint16_t addr, int16_t rssi, int8_t snr, uint32_t now) {
  Node& n = slotFor(addr);
  n.rssi = rssi;
  n.snr = snr;
  n.lastHeard = now;
  n.packets++;
  return n;
}

void NodeTable::setPos(uint16_t addr, double lat, double lon, uint32_t now) {
  Node& n = slotFor(addr);
  n.hasPos = true;
  n.lat = lat;
  n.lon = lon;
  if (now > n.lastHeard) n.lastHeard = now;
}

void NodeTable::setName(uint16_t addr, const char* name, uint32_t now) {
  Node& n = slotFor(addr);
  std::strncpy(n.name, name, sizeof(n.name) - 1);
  n.name[sizeof(n.name) - 1] = 0;
  if (now > n.lastHeard) n.lastHeard = now;
}

void NodeTable::setHealth(uint16_t addr, uint8_t battPct, uint8_t uptimeHr,
                          uint8_t dutyPct, int8_t tempC, bool lowpwr, uint8_t presence, uint32_t now) {
  Node& n = slotFor(addr);
  n.hasHealth = true;
  n.battPct = battPct;
  n.uptimeHr = uptimeHr;
  n.dutyPct = dutyPct;
  n.tempC = tempC;
  n.lowpwr = lowpwr;
  n.presence = presence;
  if (now > n.lastHeard) n.lastHeard = now;
}

size_t NodeTable::prune(uint32_t now, uint32_t ttlMs) {
  size_t removed = 0;
  size_t w = 0;
  for (size_t i = 0; i < count_; i++) {
    if (now - nodes_[i].lastHeard <= ttlMs) {
      if (w != i) nodes_[w] = nodes_[i];
      w++;
    } else {
      removed++;
    }
  }
  count_ = w;
  return removed;
}

} // namespace ls
