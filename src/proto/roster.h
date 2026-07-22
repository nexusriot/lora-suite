#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// A durable contact. `name` is learned from NODEINFO; `alias` is user-set and
// wins for display. Blocked contacts are hidden and (at the relay) not forwarded.
struct Contact {
  uint16_t addr    = 0;
  char     name[12]  = {0};
  char     alias[12] = {0};
  uint8_t  group   = 0;      // group bitmask
  bool     blocked = false;
  bool     favorite = false;
  bool     used    = false;
};

// Persistent name<->address roster (the addressing layer NodeTable never was).
// Fixed capacity, no heap. Storage loads/saves it; this class is pure/testable.
class Roster {
public:
  static const size_t CAP = 48;

  Contact* find(uint16_t addr);
  const Contact* find(uint16_t addr) const;
  Contact& upsert(uint16_t addr);

  void setName(uint16_t addr, const char* name);
  void setAlias(uint16_t addr, const char* alias);
  void setBlocked(uint16_t addr, bool b);
  void setFavorite(uint16_t addr, bool b);
  bool isBlocked(uint16_t addr) const;

  // Display label: alias, else learned name, else "hhhh" hex written to buf.
  const char* label(uint16_t addr, char* buf, size_t cap) const;

  // Resolve a typed alias or name (case-insensitive, exact) to an address.
  bool lookup(const char* text, uint16_t& addr) const;

  size_t size() const { return count_; }
  const Contact& at(size_t i) const { return items_[i]; }
  Contact& at(size_t i) { return items_[i]; }

  // Flat serialization for NVS/SD persistence (Contact is POD).
  size_t serialize(uint8_t* out, size_t cap) const;
  bool   deserialize(const uint8_t* in, size_t n);

private:
  Contact items_[CAP];
  size_t count_ = 0;
};

} // namespace ls
