#pragma once
#include <cstdint>
#include <cstddef>
#include "frame.h"

namespace ls {

// Reassembles multi-fragment messages (FLAG_FRAGMENT). A long text is split into
// up to MAX_FRAGMENTS pieces sharing a group id; each fragment carries a 3-byte
// header (group, index, total) ahead of its slice of the body.
//
// Fragments can arrive out of order, duplicated, or interleaved with another
// sender's, so slots are keyed by (src, group). A partial message whose peer went
// away is reclaimed after TIMEOUT_MS rather than wedging a slot forever.
struct FragHeader {
  uint8_t group = 0;
  uint8_t index = 0;
  uint8_t total = 0;
};

// Parse the 3-byte fragment header. Rejects nonsense (total 0, index >= total,
// total > MAX_FRAGMENTS) so a malformed frame can't drive the reassembler.
bool parseFragHeader(const uint8_t* p, uint8_t len, FragHeader& out);

// Max bytes of body a single fragment can carry, given the worst-case overhead
// (fragment header + MAC tag) on a keyed channel.
constexpr size_t FRAG_BODY_MAX = MAX_PAYLOAD - FRAG_HDR_LEN - MAC_LEN;

// Largest message that can be reassembled.
constexpr size_t DEFRAG_MAX = FRAG_BODY_MAX * MAX_FRAGMENTS;

class Defrag {
public:
  static const size_t SLOTS = 4;               // concurrent senders mid-message
  static const uint32_t TIMEOUT_MS = 60000;    // abandon a stalled reassembly

  // Feed one fragment body (already decrypted and with the 3-byte header split
  // off). When this completes a message, returns true and exposes it via
  // data()/size() until the next call.
  bool offer(uint16_t src, const FragHeader& h, const uint8_t* body, uint8_t bodyLen, uint32_t now);

  const uint8_t* data() const { return out_; }
  size_t size() const { return outLen_; }

  // Drop reassemblies older than TIMEOUT_MS (called from a background tick).
  void sweep(uint32_t now);

  size_t activeSlots() const;

private:
  struct Slot {
    bool     used = false;
    uint16_t src = 0;
    uint8_t  group = 0;
    uint8_t  total = 0;
    uint8_t  have = 0;                  // bitmask of received indices
    uint32_t started = 0;
    uint8_t  len[MAX_FRAGMENTS] = {0};
    uint8_t  buf[MAX_FRAGMENTS][FRAG_BODY_MAX] = {};
  };

  Slot* findSlot(uint16_t src, uint8_t group, uint32_t now);

  Slot   slots_[SLOTS];
  uint8_t out_[DEFRAG_MAX] = {0};
  size_t  outLen_ = 0;
};

} // namespace ls
