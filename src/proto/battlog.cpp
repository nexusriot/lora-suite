#include "battlog.h"

namespace ls {

void BattLog::add(uint32_t minutes, uint8_t pct) {
  if (count_ < CAP) {
    min_[(head_ + count_) % CAP] = minutes;
    pct_[(head_ + count_) % CAP] = pct;
    count_++;
  } else {
    min_[head_] = minutes;          // overwrite the oldest, advance the window
    pct_[head_] = pct;
    head_ = (head_ + 1) % CAP;
  }
}

float BattLog::slopePctPerMin() const {
  if (count_ < 3) return 0.0f;

  // Shift x by the first timestamp so the sums stay small and well-conditioned.
  uint32_t t0 = minAt(0);
  double n = (double)count_, sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (size_t i = 0; i < count_; i++) {
    double x = (double)(minAt(i) - t0);
    double y = (double)pctAt(i);
    sx += x; sy += y; sxx += x * x; sxy += x * y;
  }
  double denom = n * sxx - sx * sx;
  if (denom <= 0.0) return 0.0f;      // all samples share one timestamp
  return (float)((n * sxy - sx * sy) / denom);
}

int32_t BattLog::minutesToEmpty() const {
  float s = slopePctPerMin();
  if (s >= -CHARGE_EPS) return -1;                  // charging or flat
  float pct = (float)latest();
  if (pct <= 0.0f) return 0;
  return (int32_t)(pct / -s);
}

int32_t BattLog::minutesToFull() const {
  float s = slopePctPerMin();
  if (s <= CHARGE_EPS) return -1;                   // discharging or flat
  float remain = 100.0f - (float)latest();
  if (remain <= 0.0f) return 0;
  return (int32_t)(remain / s);
}

} // namespace ls
