#pragma once
#include <cstdint>

namespace ls {

// ATGM336H over UART1, parsed by TinyGPSPlus. Also the node's UTC time source
// (the Adv has no RTC), used by Clock, Breadcrumb filenames and message stamps.
class GpsService {
public:
  void begin();
  void loop();          // feed the parser from the serial buffer

  bool     hasFix() const { return fix_; }
  double   lat() const { return lat_; }
  double   lon() const { return lon_; }
  double   altM() const { return alt_; }
  double   speedKmh() const { return speed_; }
  double   courseDeg() const { return course_; }
  uint8_t  sats() const { return sats_; }
  double   hdop() const { return hdop_; }

  bool     hasTime() const { return timeValid_; }
  uint32_t unixTime() const { return unix_; }   // UTC seconds, 0 if unknown

private:
  bool     fix_ = false;
  double   lat_ = 0, lon_ = 0, alt_ = 0, speed_ = 0, course_ = 0, hdop_ = 99;
  uint8_t  sats_ = 0;
  bool     timeValid_ = false;
  uint32_t unix_ = 0;
};

} // namespace ls
