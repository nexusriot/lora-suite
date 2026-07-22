#include "gps_service.h"
#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "../hal/pins.h"

namespace ls {

static TinyGPSPlus s_gps;

static uint32_t toUnix(int y, int mo, int d, int h, int mi, int s) {
  static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  long days = (y - 1970) * 365L + (y - 1969) / 4;   // leap days since 1970
  days += cum[mo - 1] + (d - 1);
  if (mo > 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) days += 1;
  return (uint32_t)(((days * 24L + h) * 60L + mi) * 60L + s);
}

void GpsService::begin() {
  // Host RX must sample the GPS TX line; see pins.h note.
  Serial1.begin(pins::GPS_BAUD, SERIAL_8N1, pins::GPS_MODULE_TX, pins::GPS_MODULE_RX);
}

void GpsService::loop() {
  while (Serial1.available()) s_gps.encode(Serial1.read());

  if (s_gps.location.isValid()) {
    fix_ = true;
    lat_ = s_gps.location.lat();
    lon_ = s_gps.location.lng();
  }
  if (s_gps.altitude.isValid())  alt_ = s_gps.altitude.meters();
  if (s_gps.speed.isValid())     speed_ = s_gps.speed.kmph();
  if (s_gps.course.isValid())    course_ = s_gps.course.deg();
  if (s_gps.satellites.isValid()) sats_ = (uint8_t)s_gps.satellites.value();
  if (s_gps.hdop.isValid())      hdop_ = s_gps.hdop.hdop();

  if (s_gps.date.isValid() && s_gps.time.isValid() && s_gps.date.year() >= 2020) {
    unix_ = toUnix(s_gps.date.year(), s_gps.date.month(), s_gps.date.day(),
                   s_gps.time.hour(), s_gps.time.minute(), s_gps.time.second());
    timeValid_ = true;
  }
}

} // namespace ls
