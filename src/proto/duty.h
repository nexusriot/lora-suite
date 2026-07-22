#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// Rolling-window airtime accountant. 868 MHz sub-bands are typically capped at
// 1% duty cycle; the governor meters every transmission's time-on-air against a
// sliding window and refuses non-urgent traffic once the budget is spent.
// Timestamps are passed in explicitly (millis() on device) so it is pure and testable.
class DutyGovernor {
public:
  explicit DutyGovernor(uint32_t windowMs = 3600000UL, double limit = 0.01)
      : win_(windowMs), limit_(limit) {}

  void setLimit(double fraction) { limit_ = fraction; }
  void setWindow(uint32_t windowMs) { win_ = windowMs; }
  uint32_t budgetMs() const { return (uint32_t)(win_ * limit_); }

  uint32_t airtimeMs(uint32_t now) const {
    uint32_t sum = 0;
    for (size_t i = 0; i < count_; i++) {
      const Ev& e = ev_[i];
      if (now - e.t <= win_) sum += e.toa;
    }
    return sum;
  }

  double usedFraction(uint32_t now) const {
    uint32_t b = budgetMs();
    return b ? (double)airtimeMs(now) / (double)b : 1.0;
  }

  bool canSend(uint32_t now, uint32_t toaMs) const {
    return airtimeMs(now) + toaMs <= budgetMs();
  }

  // Milliseconds until a frame of toaMs would fit under the budget: 0 if it fits
  // now, or the wait until enough past airtime ages out of the window. Returns
  // NEVER if toaMs alone exceeds the whole budget.
  static const uint32_t NEVER = 0xFFFFFFFFu;
  uint32_t timeToNextTxMs(uint32_t now, uint32_t toaMs) const {
    uint32_t budget = budgetMs();
    if (budget == 0 || toaMs > budget) return NEVER;
    uint32_t air = airtimeMs(now);
    if (air + toaMs <= budget) return 0;
    uint32_t need = air + toaMs - budget;   // airtime that must age out

    Ev tmp[CAP];
    size_t m = 0;
    for (size_t i = 0; i < count_; i++)
      if (now - ev_[i].t <= win_) tmp[m++] = ev_[i];

    uint32_t freed = 0;
    for (size_t k = 0; k < m; k++) {
      size_t oldest = m;
      uint32_t ot = 0;
      for (size_t j = 0; j < m; j++)
        if (tmp[j].toa != 0 && (oldest == m || tmp[j].t < ot)) { oldest = j; ot = tmp[j].t; }
      if (oldest == m) break;
      freed += tmp[oldest].toa;
      uint32_t exitAt = tmp[oldest].t + win_;
      tmp[oldest].toa = 0;
      if (freed >= need) return exitAt > now ? exitAt - now : 0;
    }
    return NEVER;
  }

  void record(uint32_t now, uint32_t toaMs) {
    ev_[head_] = {now, toaMs};
    head_ = (head_ + 1) % CAP;
    if (count_ < CAP) count_++;
  }

private:
  struct Ev { uint32_t t; uint32_t toa; };
  static const size_t CAP = 64;
  Ev ev_[CAP] = {};
  size_t head_ = 0, count_ = 0;
  uint32_t win_;
  double limit_;
};

} // namespace ls
