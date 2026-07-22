#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// Mesh de-duplicator: remembers (src, msgid) pairs seen within a TTL so the
// background relay never re-forwards a frame it has already flooded.
class Dedup {
public:
  explicit Dedup(uint32_t ttlMs = 120000UL) : ttl_(ttlMs) {}

  // Returns true if this (src,msgid) was already seen (and refreshes it);
  // returns false and records it if new.
  bool seen(uint16_t src, uint16_t msgid, uint32_t now) {
    for (size_t i = 0; i < count_; i++) {
      Ev& e = ev_[i];
      if (e.src == src && e.msgid == msgid && now - e.t <= ttl_) {
        e.t = now;
        return true;
      }
    }
    ev_[head_] = {src, msgid, now};
    head_ = (head_ + 1) % CAP;
    if (count_ < CAP) count_++;
    return false;
  }

private:
  struct Ev { uint16_t src, msgid; uint32_t t; };
  static const size_t CAP = 128;
  Ev ev_[CAP] = {};
  size_t head_ = 0, count_ = 0;
  uint32_t ttl_;
};

} // namespace ls
