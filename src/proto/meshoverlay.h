#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// Foreign nodes discovered *outside* our own protocol: either imported from a
// meshmap.net snapshot (SRC_IMPORT) or heard over the air by the Meshtastic
// scanner (SRC_SCAN). Kept apart from NodeTable on purpose — these are never
// peers we relay, ACK or address; they are situational-awareness dots only, and
// their ids are Meshtastic 32-bit node numbers, not our 16-bit addresses.
enum MeshSource : uint8_t { SRC_IMPORT = 0, SRC_SCAN = 1 };

// Compact role codes owned by this project (the meshpull tool maps Meshtastic's
// role strings onto these; keep the two in sync). Not Meshtastic's enum values.
enum MeshRole : uint8_t {
  MROLE_UNKNOWN     = 0,
  MROLE_CLIENT      = 1,
  MROLE_CLIENT_MUTE = 2,
  MROLE_ROUTER      = 3,
  MROLE_REPEATER    = 4,
  MROLE_TRACKER     = 5,
  MROLE_SENSOR      = 6,
  MROLE_BASE        = 7,
  MROLE_OTHER       = 255,
};

// Short display tag for a role code ("CLI","MUTE","RTR","RPT","TRK","SEN","BASE","?").
const char* meshRoleLabel(uint8_t role);

struct MeshNode {
  uint32_t id        = 0;      // Meshtastic node number
  bool     hasPos    = false;
  double   lat       = 0;
  double   lon       = 0;
  char     longName[20] = {0}; // Meshtastic long names truncated to fit the display
  char     shortName[5] = {0}; // Meshtastic short names are <=4 bytes
  char     hw[10]    = {0};    // hardware-model tag (import only, e.g. "TBEAM")
  uint8_t  battPct   = 0;      // 0 = unknown; >100 = externally powered
  uint16_t voltCv    = 0;      // centivolts (e.g. 384 = 3.84 V); 0 = unknown
  uint8_t  role      = MROLE_UNKNOWN;
  int16_t  rssi      = 0;      // only meaningful for SRC_SCAN
  uint32_t seenEpoch = 0;      // freshest meshmap "seenBy" unix time; 0 = unknown
  uint32_t lastHeard = 0;      // device ms at import/hearing
  uint8_t  source    = SRC_IMPORT;
};

// Fixed-capacity, heap-free store of foreign nodes.
//
// Import CSV line format (one node per line, produced by tools/meshpull):
//   id,lat,lon,batt,volt,role,seen,hw,short,long
// Nine fixed comma-separated fields, then the long name as the rest of the line
// (which MAY contain commas): id (uint32), lat/lon (decimal degrees), batt
// (0..255), volt (centivolts), role (a MeshRole code), seen (unix time last
// seen via MQTT), hw (hardware tag, <=9 chars, no comma), short (<=4 chars, no
// comma). Blank lines and '#' comments are skipped; a "# generated <unix>"
// comment is captured as the snapshot time.
class MeshOverlay {
public:
  static const size_t CAP = 96;

  MeshNode* find(uint32_t id);
  const MeshNode* find(uint32_t id) const;

  // Live setters (used by the OTA scanner). slotFor evicts the oldest on overflow.
  void setPos(uint32_t id, double lat, double lon, uint32_t now, uint8_t source);
  void setUser(uint32_t id, const char* longName, const char* shortName,
               uint8_t role, uint32_t now, uint8_t source);
  void setBattery(uint32_t id, uint8_t battPct, uint32_t now, uint8_t source);
  void setMetrics(uint32_t id, uint8_t battPct, uint16_t voltCv, uint32_t now, uint8_t source);
  void setRssi(uint32_t id, int16_t rssi, uint32_t now, uint8_t source);

  // Import path: drop existing SRC_IMPORT nodes, then parse a CSV buffer.
  // Returns the number of data rows ingested. Does NOT evict or overwrite live
  // (SRC_SCAN) nodes — a row whose id matches a live node is skipped so the
  // scanned fix wins, and rows past capacity are dropped (the tool sorts
  // nearest-first, so the near ones win).
  size_t ingestCsv(const char* text, size_t n, uint32_t now);
  bool   ingestCsvLine(const char* line, uint32_t now);   // one row; false if not a data row

  void   clearSource(uint8_t source);
  size_t prune(uint32_t now, uint32_t ttlMs);             // drop stale, any source

  uint32_t generatedUnix() const { return generatedUnix_; }

  size_t size() const { return count_; }
  const MeshNode& at(size_t i) const { return items_[i]; }
  MeshNode& at(size_t i) { return items_[i]; }

private:
  MeshNode& slotFor(uint32_t id, uint8_t source, uint32_t now);

  MeshNode items_[CAP] = {};
  size_t   count_ = 0;
  uint32_t generatedUnix_ = 0;
};

} // namespace ls
