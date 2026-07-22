#include "roster.h"
#include <cstring>
#include <cstdio>
#include <cctype>

namespace ls {

Contact* Roster::find(uint16_t addr) {
  for (size_t i = 0; i < count_; i++)
    if (items_[i].used && items_[i].addr == addr) return &items_[i];
  return nullptr;
}

const Contact* Roster::find(uint16_t addr) const {
  for (size_t i = 0; i < count_; i++)
    if (items_[i].used && items_[i].addr == addr) return &items_[i];
  return nullptr;
}

Contact& Roster::upsert(uint16_t addr) {
  if (Contact* c = find(addr)) return *c;
  if (count_ < CAP) {
    Contact& c = items_[count_++];
    c = Contact{};
    c.addr = addr;
    c.used = true;
    return c;
  }
  // full: reuse the first non-favorite slot, else slot 0
  size_t victim = 0;
  for (size_t i = 0; i < count_; i++)
    if (!items_[i].favorite) { victim = i; break; }
  items_[victim] = Contact{};
  items_[victim].addr = addr;
  items_[victim].used = true;
  return items_[victim];
}

void Roster::setName(uint16_t addr, const char* name) {
  Contact& c = upsert(addr);
  std::strncpy(c.name, name, sizeof(c.name) - 1);
  c.name[sizeof(c.name) - 1] = 0;
}

void Roster::setAlias(uint16_t addr, const char* alias) {
  Contact& c = upsert(addr);
  std::strncpy(c.alias, alias, sizeof(c.alias) - 1);
  c.alias[sizeof(c.alias) - 1] = 0;
}

void Roster::setBlocked(uint16_t addr, bool b) { upsert(addr).blocked = b; }
void Roster::setFavorite(uint16_t addr, bool b) { upsert(addr).favorite = b; }

bool Roster::isBlocked(uint16_t addr) const {
  const Contact* c = find(addr);
  return c && c->blocked;
}

const char* Roster::label(uint16_t addr, char* buf, size_t cap) const {
  const Contact* c = find(addr);
  if (c && c->alias[0]) return c->alias;
  if (c && c->name[0]) return c->name;
  std::snprintf(buf, cap, "%04X", addr);
  return buf;
}

static bool ieq(const char* a, const char* b) {
  while (*a && *b) {
    if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return false;
    a++; b++;
  }
  return *a == 0 && *b == 0;
}

size_t Roster::serialize(uint8_t* out, size_t cap) const {
  size_t need = 2 + count_ * sizeof(Contact);
  if (cap < need) return 0;
  out[0] = (uint8_t)(count_ & 0xFF);
  out[1] = (uint8_t)((count_ >> 8) & 0xFF);
  std::memcpy(out + 2, items_, count_ * sizeof(Contact));
  return need;
}

bool Roster::deserialize(const uint8_t* in, size_t n) {
  if (n < 2) return false;
  size_t c = (size_t)in[0] | ((size_t)in[1] << 8);
  if (c > CAP || n < 2 + c * sizeof(Contact)) return false;
  std::memcpy(items_, in + 2, c * sizeof(Contact));
  count_ = c;
  return true;
}

bool Roster::lookup(const char* text, uint16_t& addr) const {
  for (size_t i = 0; i < count_; i++) {
    const Contact& c = items_[i];
    if (!c.used) continue;
    if ((c.alias[0] && ieq(c.alias, text)) || (c.name[0] && ieq(c.name, text))) {
      addr = c.addr;
      return true;
    }
  }
  return false;
}

} // namespace ls
