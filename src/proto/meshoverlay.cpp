#include "meshoverlay.h"
#include <cstring>
#include <cstdlib>

namespace ls {

const char* meshRoleLabel(uint8_t role) {
  switch (role) {
    case MROLE_CLIENT:      return "CLI";
    case MROLE_CLIENT_MUTE: return "MUTE";
    case MROLE_ROUTER:      return "RTR";
    case MROLE_REPEATER:    return "RPT";
    case MROLE_TRACKER:     return "TRK";
    case MROLE_SENSOR:      return "SEN";
    case MROLE_BASE:        return "BASE";
    default:                return "?";
  }
}

MeshNode* MeshOverlay::find(uint32_t id) {
  for (size_t i = 0; i < count_; i++)
    if (items_[i].id == id) return &items_[i];
  return nullptr;
}

const MeshNode* MeshOverlay::find(uint32_t id) const {
  for (size_t i = 0; i < count_; i++)
    if (items_[i].id == id) return &items_[i];
  return nullptr;
}

MeshNode& MeshOverlay::slotFor(uint32_t id, uint8_t source, uint32_t now) {
  if (MeshNode* m = find(id)) return *m;
  if (count_ < CAP) {
    MeshNode& m = items_[count_++];
    m = MeshNode{};
    m.id = id;
    m.source = source;
    m.lastHeard = now;
    return m;
  }
  size_t oldest = 0;
  for (size_t i = 1; i < count_; i++)
    if (items_[i].lastHeard < items_[oldest].lastHeard) oldest = i;
  items_[oldest] = MeshNode{};
  items_[oldest].id = id;
  items_[oldest].source = source;
  items_[oldest].lastHeard = now;
  return items_[oldest];
}

void MeshOverlay::setPos(uint32_t id, double lat, double lon, uint32_t now, uint8_t source) {
  MeshNode& m = slotFor(id, source, now);
  m.lat = lat;
  m.lon = lon;
  m.hasPos = !(lat == 0.0 && lon == 0.0);
  m.source = source;
  if (now > m.lastHeard) m.lastHeard = now;
}

void MeshOverlay::setUser(uint32_t id, const char* longName, const char* shortName,
                          uint8_t role, uint32_t now, uint8_t source) {
  MeshNode& m = slotFor(id, source, now);
  if (longName) {
    std::strncpy(m.longName, longName, sizeof(m.longName) - 1);
    m.longName[sizeof(m.longName) - 1] = 0;
  }
  if (shortName) {
    std::strncpy(m.shortName, shortName, sizeof(m.shortName) - 1);
    m.shortName[sizeof(m.shortName) - 1] = 0;
  }
  m.role = role;
  m.source = source;
  if (now > m.lastHeard) m.lastHeard = now;
}

void MeshOverlay::setBattery(uint32_t id, uint8_t battPct, uint32_t now, uint8_t source) {
  MeshNode& m = slotFor(id, source, now);
  m.battPct = battPct;
  m.source = source;
  if (now > m.lastHeard) m.lastHeard = now;
}

void MeshOverlay::setRssi(uint32_t id, int16_t rssi, uint32_t now, uint8_t source) {
  MeshNode& m = slotFor(id, source, now);
  m.rssi = rssi;
  m.source = source;
  if (now > m.lastHeard) m.lastHeard = now;
}

void MeshOverlay::clearSource(uint8_t source) {
  size_t w = 0;
  for (size_t i = 0; i < count_; i++)
    if (items_[i].source != source) {
      if (w != i) items_[w] = items_[i];
      w++;
    }
  count_ = w;
}

size_t MeshOverlay::prune(uint32_t now, uint32_t ttlMs) {
  size_t removed = 0, w = 0;
  for (size_t i = 0; i < count_; i++) {
    if (now - items_[i].lastHeard <= ttlMs) {
      if (w != i) items_[w] = items_[i];
      w++;
    } else {
      removed++;
    }
  }
  count_ = w;
  return removed;
}

// Copy at most (cap-1) chars from src, stopping early at a line terminator.
static void copyField(char* dst, size_t cap, const char* src, size_t srcLen) {
  size_t w = 0;
  for (size_t i = 0; i < srcLen && w < cap - 1; i++) {
    char c = src[i];
    if (c == '\r' || c == '\n') break;
    dst[w++] = c;
  }
  dst[w] = 0;
}

bool MeshOverlay::ingestCsvLine(const char* line, uint32_t now) {
  while (*line == ' ' || *line == '\t') line++;
  if (*line == 0 || *line == '\r' || *line == '\n') return false;
  if (*line == '#') {
    const char* g = std::strstr(line, "generated");
    if (g) generatedUnix_ = (uint32_t)std::strtoul(g + 9, nullptr, 10);
    return false;
  }

  const char* p = line;
  char* e = nullptr;
  unsigned long id = std::strtoul(p, &e, 10);
  if (e == p || *e != ',') return false;
  p = e + 1;
  double lat = std::strtod(p, &e);
  if (e == p || *e != ',') return false;
  p = e + 1;
  double lon = std::strtod(p, &e);
  if (e == p || *e != ',') return false;
  p = e + 1;
  long batt = std::strtol(p, &e, 10);
  if (e == p || *e != ',') return false;
  p = e + 1;
  long volt = std::strtol(p, &e, 10);
  if (e == p || *e != ',') return false;
  p = e + 1;
  long role = std::strtol(p, &e, 10);
  if (e == p || *e != ',') return false;
  p = e + 1;
  unsigned long seen = std::strtoul(p, &e, 10);
  if (e == p || *e != ',') return false;
  p = e + 1;

  const char* hwStart = p;                  // hw, short: two comma-delimited fields
  const char* c1 = std::strchr(p, ',');
  if (!c1) return false;
  const char* shortStart = c1 + 1;
  const char* c2 = std::strchr(shortStart, ',');
  if (!c2) return false;
  const char* longStart = c2 + 1;           // rest of line, may contain commas

  if (id == 0) return false;
  MeshNode* m = find((uint32_t)id);
  if (!m) {
    if (count_ >= CAP) return false;        // import never evicts a live node
    m = &items_[count_++];
    *m = MeshNode{};
    m->id = (uint32_t)id;
  }

  m->source = SRC_IMPORT;
  m->lastHeard = now;
  m->lat = lat;
  m->lon = lon;
  m->hasPos = !(lat == 0.0 && lon == 0.0);
  m->battPct = batt < 0 ? 0 : (batt > 255 ? 255 : (uint8_t)batt);
  m->voltCv = volt < 0 ? 0 : (volt > 65535 ? 65535 : (uint16_t)volt);
  m->role = (role < 0 || role > 255) ? (uint8_t)MROLE_OTHER : (uint8_t)role;
  m->seenEpoch = (uint32_t)seen;
  copyField(m->hw, sizeof(m->hw), hwStart, (size_t)(c1 - hwStart));
  copyField(m->shortName, sizeof(m->shortName), shortStart, (size_t)(c2 - shortStart));
  copyField(m->longName, sizeof(m->longName), longStart, std::strlen(longStart));
  return true;
}

size_t MeshOverlay::ingestCsv(const char* text, size_t n, uint32_t now) {
  clearSource(SRC_IMPORT);
  generatedUnix_ = 0;
  size_t rows = 0;
  const char* p = text;
  const char* end = text + n;
  while (p < end) {
    const char* nl = (const char*)std::memchr(p, '\n', (size_t)(end - p));
    size_t len = nl ? (size_t)(nl - p) : (size_t)(end - p);
    char buf[256];
    size_t c = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    std::memcpy(buf, p, c);
    buf[c] = 0;
    if (ingestCsvLine(buf, now)) rows++;
    if (!nl) break;
    p = nl + 1;
  }
  return rows;
}

} // namespace ls
