#include "clock.h"
#include <Arduino.h>
#include <cstdio>
#include "gps_service.h"

namespace ls {

void Clock::loop() {
  if (gps_ && gps_->hasTime()) {
    base_ = gps_->unixTime();
    baseMs_ = millis();
    source_ = 2;               // GPS is authoritative
  }
}

void Clock::adopt(uint32_t unix, uint8_t source) {
  if (unix == 0) return;
  // >= lets an equal-source sync re-latch (a mesh-only node cancels millis drift
  // on each fresh mesh TIMESYNC); a GPS node (source_=2) still rejects mesh (1).
  if (source >= source_ || base_ == 0) {
    base_ = unix;
    baseMs_ = millis();
    source_ = source;
  }
}

uint32_t Clock::uptimeMs() const { return millis(); }

uint32_t Clock::utc() const {
  if (!base_) return 0;
  return base_ + (millis() - baseMs_) / 1000;
}

void Clock::hms(char* buf) const {
  uint32_t t = utc();
  if (!t) { std::snprintf(buf, 9, "--:--:--"); return; }
  uint32_t s = t % 86400;
  std::snprintf(buf, 9, "%02u:%02u:%02u",
                (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60));
}

void Clock::ymd(char* buf) const {
  uint32_t t = utc();
  if (!t) { std::snprintf(buf, 9, "00000000"); return; }
  long days = t / 86400;
  int y = 1970;
  while (true) {
    int dy = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    if (days < dy) break;
    days -= dy; y++;
  }
  static const int md[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int mo = 0;
  bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
  while (mo < 12) {
    int dm = md[mo] + ((mo == 1 && leap) ? 1 : 0);
    if (days < dm) break;
    days -= dm; mo++;
  }
  std::snprintf(buf, 9, "%04d%02d%02d", y, mo + 1, (int)days + 1);
}

} // namespace ls
