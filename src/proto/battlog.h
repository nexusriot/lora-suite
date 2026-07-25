#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// Rolling battery history + a discharge forecast.
//
// Reactor already reacts to the *current* battery level; this answers the other
// question — "how long have I got?" — by fitting a least-squares line through the
// recent samples. A linear fit over a window is deliberately simple: it beats
// instantaneous deltas (which are pure ADC noise) without pretending to model
// the cell's real discharge curve.
class BattLog {
public:
  static const size_t CAP = 96;          // samples retained for the fit + graph

  // Record a reading. `minutes` is monotonic uptime; callers sample on a slow
  // period (the app uses 1 min), so CAP covers about 1.5 hours.
  void add(uint32_t minutes, uint8_t pct);

  size_t size() const { return count_; }
  uint8_t pctAt(size_t i) const { return pct_[(head_ + i) % CAP]; }
  uint32_t minAt(size_t i) const { return min_[(head_ + i) % CAP]; }
  uint8_t latest() const { return count_ ? pctAt(count_ - 1) : 0; }

  // Least-squares slope in percent per minute (negative while discharging).
  // Returns 0 with fewer than 3 samples or no elapsed time.
  float slopePctPerMin() const;

  // Minutes until empty at the current trend, or -1 if not discharging
  // (charging, flat, or not enough data).
  int32_t minutesToEmpty() const;

  // Minutes until full, or -1 if not charging.
  int32_t minutesToFull() const;

  bool charging() const { return slopePctPerMin() > CHARGE_EPS; }

  void clear() { count_ = 0; head_ = 0; }

private:
  // Below this magnitude the trend is indistinguishable from ADC noise.
  static constexpr float CHARGE_EPS = 0.002f;   // %/min

  uint32_t min_[CAP] = {0};
  uint8_t  pct_[CAP] = {0};
  size_t   count_ = 0;
  size_t   head_ = 0;
};

} // namespace ls
