#pragma once
#include <cstdint>
#include <cstddef>
#include "frame.h"
#include "qos.h"

namespace ls {

// Bounded priority transmit queue with age-based promotion. Marshal pumps it:
// pop() returns the highest effective-priority frame, where effective priority =
// class + one level per AGING_MS waited, so a bulk transfer never starves an
// alert yet a long-queued low frame eventually gets out. Pure/testable.
class TxQueue {
public:
  static const size_t   CAP = 12;
  static const uint32_t AGING_MS = 5000;

  bool empty() const { return count_ == 0; }
  bool full() const { return count_ == CAP; }
  size_t size() const { return count_; }

  // Enqueue f. When full, evict the lowest-CLASS slot (aging governs drain order,
  // not admission — otherwise an aged bulk frame could outrank a fresh alert).
  // Among equal classes the youngest is evicted so long-waiting frames survive.
  // Admits only if the newcomer's class strictly outranks that victim.
  bool push(const Frame& f, uint32_t now, bool urgent = false) {
    uint8_t np = priorityOf(f.type, f.flags);
    uint8_t nr = urgent ? 255 : np;
    if (count_ == CAP) {
      size_t lo = 0;
      for (size_t i = 1; i < count_; i++) {
        uint8_t ri = slots_[i].urgent ? 255 : slots_[i].prio;
        uint8_t rl = slots_[lo].urgent ? 255 : slots_[lo].prio;
        if (ri < rl || (ri == rl && slots_[i].enq > slots_[lo].enq)) lo = i;
      }
      uint8_t rl = slots_[lo].urgent ? 255 : slots_[lo].prio;
      if (rl >= nr) return false;
      slots_[lo] = {f, np, now, urgent};
      return true;
    }
    slots_[count_++] = {f, np, now, urgent};
    return true;
  }

  // Inspect the highest effective-priority slot without removing it.
  bool peek(uint32_t now, Frame& out, bool& urgent) const {
    int b = bestIndex(now);
    if (b < 0) return false;
    out = slots_[b].frame;
    urgent = slots_[b].urgent;
    return true;
  }

  // Remove the highest effective-priority slot (call after a successful TX).
  void removeBest(uint32_t now) {
    int b = bestIndex(now);
    if (b >= 0) slots_[b] = slots_[--count_];
  }

  // Remove and return the highest effective-priority slot (oldest breaks ties).
  bool pop(uint32_t now, Frame& out, bool& urgent) {
    if (!peek(now, out, urgent)) return false;
    removeBest(now);
    return true;
  }

  // Recall: drop a still-queued frame by (src, msgid). Returns true if removed.
  bool cancel(uint16_t src, uint16_t msgid) {
    for (size_t i = 0; i < count_; i++)
      if (slots_[i].frame.src == src && slots_[i].frame.msgid == msgid) {
        slots_[i] = slots_[--count_];
        return true;
      }
    return false;
  }

  // Read-only access for the Recall UI (order is unspecified).
  const Frame& frameAt(size_t i) const { return slots_[i].frame; }
  uint32_t enqAt(size_t i) const { return slots_[i].enq; }

private:
  struct Slot { Frame frame; uint8_t prio; uint32_t enq; bool urgent; };
  Slot slots_[CAP];
  size_t count_ = 0;

  int bestIndex(uint32_t now) const {
    if (count_ == 0) return -1;
    size_t best = 0;
    uint8_t be = 0;
    for (size_t i = 0; i < count_; i++) {
      uint8_t e = eff(slots_[i].prio, slots_[i].enq, now, slots_[i].urgent);
      if (i == 0 || e > be || (e == be && slots_[i].enq < slots_[best].enq)) { best = i; be = e; }
    }
    return (int)best;
  }

  static uint8_t eff(uint8_t prio, uint32_t enq, uint32_t now, bool urgent) {
    if (urgent) return 255;
    uint32_t e = (uint32_t)prio + (now - enq) / AGING_MS;
    // Aging promotes to prevent starvation, but non-alert traffic is capped just
    // below PRIO_ALERT so an aged bulk/telemetry frame can never preempt a queued
    // alert; alert-class frames age among themselves.
    uint32_t cap = (prio >= PRIO_ALERT) ? 254 : (uint32_t)(PRIO_ALERT - 1);
    if (e > cap) e = cap;
    return (uint8_t)e;
  }
};

} // namespace ls
