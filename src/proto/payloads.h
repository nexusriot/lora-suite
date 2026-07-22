#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// Compact BEACON body (12 bytes): lat/lon as int32 * 1e7 (uBlox/APRS style),
// altitude int16 m, speed uint8 km/h, course uint8 in 2-degree steps.
struct Position {
  double  lat        = 0;
  double  lon        = 0;
  int16_t altM       = 0;
  uint8_t speedKmh   = 0;
  uint8_t course2    = 0;   // degrees / 2 (0..179 => 0..358)
};

constexpr size_t POSITION_LEN = 12;
size_t packPosition(const Position& p, uint8_t* out, size_t cap);
bool   unpackPosition(const uint8_t* in, size_t len, Position& p);

// TELEMETRY body (8 bytes): accel milli-g x/y/z, battery %, temperature C.
struct Telemetry {
  int16_t ax = 0, ay = 0, az = 0;   // milli-g
  uint8_t battPct = 0;
  int8_t  tempC = 0;
};

constexpr size_t TELEMETRY_LEN = 8;
size_t packTelemetry(const Telemetry& t, uint8_t* out, size_t cap);
bool   unpackTelemetry(const uint8_t* in, size_t len, Telemetry& t);

// Pulse health TLV (7 bytes), appended to a frame's tail behind FLAG_HEALTH.
struct Health {
  uint8_t battPct  = 0;
  uint8_t uptimeHr = 0;     // capped uptime in hours
  uint8_t heapKb   = 0;     // free heap, KB (capped 255)
  uint8_t dutyPct  = 0;     // rolling duty-cycle use %
  int8_t  tempC    = 0;
  uint8_t reboot   = 0;     // esp_reset_reason() enum
  uint8_t presence = 0;     // Presence state (0 AVAIL, 1 BUSY, 2 EN-ROUTE, 3 RESTING)
};

constexpr size_t HEALTH_LEN = 7;
size_t packHealth(const Health& h, uint8_t* out, size_t cap);
bool   unpackHealth(const uint8_t* in, size_t len, Health& h);

// Waypoint (Pathfinder): a named target position (16 bytes).
struct Waypoint {
  double  lat = 0;
  double  lon = 0;
  int16_t altM = 0;
  char    label[8] = {0};
};

constexpr size_t WAYPOINT_LEN = 18;
size_t packWaypoint(const Waypoint& w, uint8_t* out, size_t cap);
bool   unpackWaypoint(const uint8_t* in, size_t len, Waypoint& w);

// TimeSync (Chronos): UTC seconds + a source-quality byte (0=none,1=mesh,2=GPS,3=NTP).
struct TimeSync {
  uint32_t unix = 0;
  uint8_t  source = 0;
};

constexpr size_t TIMESYNC_LEN = 5;
size_t packTimeSync(const TimeSync& t, uint8_t* out, size_t cap);
bool   unpackTimeSync(const uint8_t* in, size_t len, TimeSync& t);

// Countdown: an absolute UTC fire-time + a label/action code, so every node
// counts down to the same instant regardless of receive delay.
struct Countdown {
  uint32_t unix = 0;
  uint8_t  code = 0;
};

constexpr size_t COUNTDOWN_LEN = 5;
size_t packCountdown(const Countdown& c, uint8_t* out, size_t cap);
bool   unpackCountdown(const uint8_t* in, size_t len, Countdown& c);

} // namespace ls
