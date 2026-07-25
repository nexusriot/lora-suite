#include "defrag.h"
#include <cstring>

namespace ls {

bool parseFragHeader(const uint8_t* p, uint8_t len, FragHeader& out) {
  if (len < FRAG_HDR_LEN) return false;
  out.group = p[0];
  out.index = p[1];
  out.total = p[2];
  if (out.total == 0 || out.total > MAX_FRAGMENTS) return false;
  if (out.index >= out.total) return false;
  return true;
}

Defrag::Slot* Defrag::findSlot(uint16_t src, uint8_t group, uint32_t now) {
  for (Slot& s : slots_)
    if (s.used && s.src == src && s.group == group) return &s;

  for (Slot& s : slots_)
    if (!s.used) return &s;

  // All slots busy: evict the oldest. A stalled reassembly must never block a
  // live one (the timeout sweep usually gets there first).
  Slot* oldest = &slots_[0];
  for (Slot& s : slots_)
    if ((uint32_t)(now - s.started) > (uint32_t)(now - oldest->started)) oldest = &s;
  oldest->used = false;
  return oldest;
}

bool Defrag::offer(uint16_t src, const FragHeader& h, const uint8_t* body, uint8_t bodyLen, uint32_t now) {
  if (h.total == 0 || h.total > MAX_FRAGMENTS || h.index >= h.total) return false;
  if (bodyLen > FRAG_BODY_MAX) return false;

  Slot* s = findSlot(src, h.group, now);
  if (!s->used || s->src != src || s->group != h.group) {
    s->used = true;
    s->src = src;
    s->group = h.group;
    s->total = h.total;
    s->have = 0;
    s->started = now;
    std::memset(s->len, 0, sizeof(s->len));
  }
  // A sender changing its mind about the fragment count mid-message means we are
  // looking at a stale group id being reused; restart rather than mix the two.
  if (s->total != h.total) {
    s->total = h.total;
    s->have = 0;
    s->started = now;
    std::memset(s->len, 0, sizeof(s->len));
  }

  std::memcpy(s->buf[h.index], body, bodyLen);
  s->len[h.index] = bodyLen;
  s->have |= (uint8_t)(1u << h.index);

  uint8_t want = (uint8_t)((h.total >= 8) ? 0xFF : ((1u << h.total) - 1));
  if (s->have != want) return false;

  size_t o = 0;
  for (uint8_t i = 0; i < s->total; i++) {
    if (o + s->len[i] > sizeof(out_)) { s->used = false; return false; }
    std::memcpy(out_ + o, s->buf[i], s->len[i]);
    o += s->len[i];
  }
  outLen_ = o;
  s->used = false;                     // slot is free for the next message
  return true;
}

void Defrag::sweep(uint32_t now) {
  for (Slot& s : slots_)
    if (s.used && (uint32_t)(now - s.started) > TIMEOUT_MS) s.used = false;
}

size_t Defrag::activeSlots() const {
  size_t n = 0;
  for (const Slot& s : slots_) if (s.used) n++;
  return n;
}

} // namespace ls
