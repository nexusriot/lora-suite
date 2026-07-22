#pragma once
#include <cstdint>

namespace ls {

class GpsService;

// GPS-disciplined clock. The Adv has no RTC, so UTC comes from GPS once fixed;
// before that only monotonic uptime is available.
class Clock {
public:
  void attach(GpsService* gps) { gps_ = gps; }
  void loop();                         // latch GPS time against uptime

  // Adopt a network time (Chronos). Accepted only if its source outranks ours,
  // or we have no time at all. source: 1 mesh, 2 GPS, 3 NTP.
  void adopt(uint32_t unix, uint8_t source);
  uint8_t source() const { return source_; }

  bool     hasUtc() const { return base_ != 0; }
  uint32_t utc() const;                // seconds, 0 if unknown
  uint32_t uptimeMs() const;

  // "hh:mm:ss" UTC or "--:--:--"; buf must hold >= 9 bytes.
  void hms(char* buf) const;
  // "YYYYMMDD" for log filenames, or "00000000".
  void ymd(char* buf) const;

private:
  GpsService* gps_ = nullptr;
  uint32_t base_ = 0;      // unix seconds latched at baseMs_
  uint32_t baseMs_ = 0;
  uint8_t  source_ = 0;    // 0 none, 1 mesh, 2 GPS, 3 NTP
};

} // namespace ls
