#pragma once
#include <cstdint>
#include "frame.h"

namespace ls {

// Airtime accountant with per-category attribution — the historical companion to
// the live DutyGovernor. LoRaService charges every transmission here (tagged by
// MsgType, or AIRTAG_RELAY for forwarded frames) so the Ledger app can show which
// traffic is eating the 1% EU868 budget, and log a daily total to SD. Pure/testable.
class AirLedger {
public:
  static const int SLOTS = 14;   // 0 unused, 1..11 MsgType, 12 relay, 13 other

  static int slotFor(uint8_t tag) {
    if (tag == AIRTAG_RELAY) return 12;
    if (tag >= 1 && tag <= 11) return tag;
    return 13;
  }

  void record(uint8_t tag, uint32_t toaMs) {
    buckets_[slotFor(tag)] += toaMs;
    total_ += toaMs;
    frames_++;
  }

  uint32_t total() const { return total_; }
  uint32_t frames() const { return frames_; }
  uint32_t bucket(int i) const { return (i >= 0 && i < SLOTS) ? buckets_[i] : 0; }

  void reset() {
    for (int i = 0; i < SLOTS; i++) buckets_[i] = 0;
    total_ = 0;
    frames_ = 0;
  }

private:
  uint32_t buckets_[SLOTS] = {0};
  uint32_t total_ = 0;
  uint32_t frames_ = 0;
};

} // namespace ls
