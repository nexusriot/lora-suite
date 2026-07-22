#include "payloads.h"

namespace ls {

static inline void putI32(uint8_t* p, int32_t v) {
  p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}
static inline int32_t getI32(const uint8_t* p) {
  return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}
static inline void putI16(uint8_t* p, int16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
static inline int16_t getI16(const uint8_t* p) { return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }

size_t packPosition(const Position& p, uint8_t* out, size_t cap) {
  if (cap < POSITION_LEN) return 0;
  putI32(out + 0, (int32_t)(p.lat * 1e7));
  putI32(out + 4, (int32_t)(p.lon * 1e7));
  putI16(out + 8, p.altM);
  out[10] = p.speedKmh;
  out[11] = p.course2;
  return POSITION_LEN;
}

bool unpackPosition(const uint8_t* in, size_t len, Position& p) {
  if (len < POSITION_LEN) return false;
  p.lat = getI32(in + 0) / 1e7;
  p.lon = getI32(in + 4) / 1e7;
  p.altM = getI16(in + 8);
  p.speedKmh = in[10];
  p.course2 = in[11];
  return true;
}

size_t packTelemetry(const Telemetry& t, uint8_t* out, size_t cap) {
  if (cap < TELEMETRY_LEN) return 0;
  putI16(out + 0, t.ax);
  putI16(out + 2, t.ay);
  putI16(out + 4, t.az);
  out[6] = t.battPct;
  out[7] = (uint8_t)t.tempC;
  return TELEMETRY_LEN;
}

bool unpackTelemetry(const uint8_t* in, size_t len, Telemetry& t) {
  if (len < TELEMETRY_LEN) return false;
  t.ax = getI16(in + 0);
  t.ay = getI16(in + 2);
  t.az = getI16(in + 4);
  t.battPct = in[6];
  t.tempC = (int8_t)in[7];
  return true;
}

size_t packHealth(const Health& h, uint8_t* out, size_t cap) {
  if (cap < HEALTH_LEN) return 0;
  out[0] = h.battPct;
  out[1] = h.uptimeHr;
  out[2] = h.heapKb;
  out[3] = h.dutyPct;
  out[4] = (uint8_t)h.tempC;
  out[5] = h.reboot;
  out[6] = h.presence;
  return HEALTH_LEN;
}

bool unpackHealth(const uint8_t* in, size_t len, Health& h) {
  if (len < HEALTH_LEN) return false;
  h.battPct = in[0];
  h.uptimeHr = in[1];
  h.heapKb = in[2];
  h.dutyPct = in[3];
  h.tempC = (int8_t)in[4];
  h.reboot = in[5];
  h.presence = in[6];
  return true;
}

size_t packWaypoint(const Waypoint& w, uint8_t* out, size_t cap) {
  if (cap < WAYPOINT_LEN) return 0;
  putI32(out + 0, (int32_t)(w.lat * 1e7));
  putI32(out + 4, (int32_t)(w.lon * 1e7));
  putI16(out + 8, w.altM);
  for (int i = 0; i < 7; i++) out[10 + i] = (uint8_t)w.label[i];
  out[17] = 0;   // label is 7 chars + NUL; keep byte 17 defined (matches unpack)
  return WAYPOINT_LEN;
}

bool unpackWaypoint(const uint8_t* in, size_t len, Waypoint& w) {
  if (len < WAYPOINT_LEN) return false;
  w.lat = getI32(in + 0) / 1e7;
  w.lon = getI32(in + 4) / 1e7;
  w.altM = getI16(in + 8);
  for (int i = 0; i < 7; i++) w.label[i] = (char)in[10 + i];
  w.label[7] = 0;
  return true;
}

size_t packTimeSync(const TimeSync& t, uint8_t* out, size_t cap) {
  if (cap < TIMESYNC_LEN) return 0;
  putI32(out + 0, (int32_t)t.unix);
  out[4] = t.source;
  return TIMESYNC_LEN;
}

bool unpackTimeSync(const uint8_t* in, size_t len, TimeSync& t) {
  if (len < TIMESYNC_LEN) return false;
  t.unix = (uint32_t)getI32(in + 0);
  t.source = in[4];
  return true;
}

size_t packCountdown(const Countdown& c, uint8_t* out, size_t cap) {
  if (cap < COUNTDOWN_LEN) return 0;
  putI32(out + 0, (int32_t)c.unix);
  out[4] = c.code;
  return COUNTDOWN_LEN;
}

bool unpackCountdown(const uint8_t* in, size_t len, Countdown& c) {
  if (len < COUNTDOWN_LEN) return false;
  c.unix = (uint32_t)getI32(in + 0);
  c.code = in[4];
  return true;
}

} // namespace ls
