#include "net.h"
#include <cstring>
#include <Arduino.h>
#include <M5Unified.h>
#include <esp_system.h>
#include "../services/lora_service.h"
#include "../services/gps_service.h"
#include "../services/storage.h"
#include "../services/audio.h"
#include "../services/clock.h"
#include "../proto/meshtastic.h"
#include "../proto/squeeze.h"
#include "../proto/defrag.h"
#include <WiFi.h>
#include <time.h>

namespace ls {

// Only fixed-length binary bodies are safe to piggyback health onto — a health
// tail on a TEXT/NODEINFO string would render as garbage on the receiver.
static bool safeHealthCarrier(uint8_t type) {
  return type == MSG_BEACON || type == MSG_TELEMETRY || type == MSG_PONG;
}

static Health buildHealth() {
  Health h;
  h.battPct = (uint8_t)M5.Power.getBatteryLevel();
  h.uptimeHr = (uint8_t)((millis() / 3600000UL) & 0xFF);
  uint32_t heapKb = ESP.getFreeHeap() / 1024;
  h.heapKb = heapKb > 255 ? 255 : (uint8_t)heapKb;
  double duty = ctx.lora ? ctx.lora->duty().usedFraction(millis()) : 0.0;
  h.dutyPct = (uint8_t)(duty * 100.0);
  float tmp = 0;
  M5.Imu.getTemp(&tmp);
  h.tempC = (int8_t)tmp;
  h.reboot = (uint8_t)esp_reset_reason();
  h.presence = ctx.presence;
  return h;
}

static void maybeAppendHealth(Frame& f) {
  if (!ctx.pulseEnabled || !safeHealthCarrier(f.type)) return;
  if (f.len + HEALTH_LEN > MAX_PAYLOAD) return;
  uint32_t now = millis();
  if (ctx.lastHealthMs != 0 && now - ctx.lastHealthMs < ctx.healthPeriodMs) return;

  uint8_t tlv[HEALTH_LEN];
  packHealth(buildHealth(), tlv, sizeof(tlv));
  memcpy(f.payload + f.len, tlv, HEALTH_LEN);
  f.len += HEALTH_LEN;
  f.flags |= FLAG_HEALTH;
  ctx.lastHealthMs = now;
}

bool netSend(Frame& f, bool urgent) {
  f.src = ctx.myAddr;
  f.msgid = ctx.allocMsgId();
  f.chan = ctx.channel.id();
  f.airTag = f.type;                     // Ledger: attribute this airtime to its type
  if (f.hop == 0) f.hop = ctx.relayHops;
  if (ctx.power >= PWR_SURVIVAL) f.flags |= FLAG_LOWPWR;
  maybeAppendHealth(f);
  // Encrypt-then-MAC (nonce + tag both bind src+msgid, set above). A body too
  // large to carry its tag is refused rather than sent unauthenticated.
  if (!ctx.channel.seal(f)) return false;
  return ctx.lora && ctx.lora->sendFrame(f, urgent);
}

bool netSendText(uint16_t dst, const char* text, bool wantAck) {
  size_t n = strnlen(text, TEXT_MAX);
  if (n == 0) return false;

  // Compress first: it shrinks what has to be encrypted, tagged and fragmented.
  uint8_t body[TEXT_MAX];
  size_t bodyLen = n;
  bool squeezed = false;
  {
    uint8_t comp[TEXT_MAX];
    size_t cn = 0;
    if (squeezeIfSmaller((const uint8_t*)text, n, comp, sizeof(comp), cn)) {
      memcpy(body, comp, cn);
      bodyLen = cn;
      squeezed = true;
    } else {
      memcpy(body, text, n);
    }
  }

  size_t budget = MAX_PAYLOAD;
  if (ctx.channel.encrypted()) budget -= MAC_LEN;   // leave room for the tag
  uint8_t base = FLAG_MESH | (squeezed ? FLAG_SQUEEZE : 0);

  if (bodyLen <= budget) {
    Frame f;
    f.type = MSG_TEXT;
    f.dst = dst;
    f.flags = base | (wantAck ? FLAG_ACK_REQ : 0);
    f.setPayload(body, (uint8_t)bodyLen);
    return netSend(f);
  }

  // The receiver's per-fragment slot is the real ceiling, and it is sized for the
  // worst case (tag present). On a cleartext channel the frame would allow more,
  // but sending more would overrun the reassembler and the message would vanish.
  size_t per = budget - FRAG_HDR_LEN;
  if (per > FRAG_BODY_MAX) per = FRAG_BODY_MAX;
  size_t total = (bodyLen + per - 1) / per;
  if (total > MAX_FRAGMENTS) return false;

  static uint8_t group = 0;
  group++;
  bool ok = true;
  for (size_t i = 0; i < total; i++) {
    size_t off = i * per;
    size_t chunk = (bodyLen - off < per) ? (bodyLen - off) : per;
    uint8_t p[MAX_PAYLOAD];
    p[0] = group;
    p[1] = (uint8_t)i;
    p[2] = (uint8_t)total;
    memcpy(p + FRAG_HDR_LEN, body + off, chunk);

    Frame f;
    f.type = MSG_TEXT;
    f.dst = dst;
    // Only the final fragment asks for an ACK — one ACK per message, and it
    // implicitly confirms the whole thing reassembled.
    f.flags = base | FLAG_FRAGMENT | ((wantAck && i + 1 == total) ? FLAG_ACK_REQ : 0);
    f.setPayload(p, (uint8_t)(FRAG_HDR_LEN + chunk));
    if (!netSend(f)) ok = false;
  }
  return ok;
}

Frame makeText(uint16_t dst, const char* text, bool wantAck) {
  Frame f;
  f.type = MSG_TEXT;
  f.dst = dst;
  f.flags = FLAG_MESH | (wantAck ? FLAG_ACK_REQ : 0);
  f.setPayload(text, (uint8_t)strnlen(text, MAX_PAYLOAD));
  return f;
}

Frame makeAck(uint16_t dst, uint16_t ackId) {
  Frame f;
  f.type = MSG_ACK;
  f.dst = dst;
  uint8_t p[2] = {(uint8_t)(ackId & 0xFF), (uint8_t)(ackId >> 8)};
  f.setPayload(p, 2);
  return f;
}

Frame makeBeacon(const Position& p) {
  Frame f;
  f.type = MSG_BEACON;
  f.flags = FLAG_MESH;
  uint8_t p12[POSITION_LEN];
  packPosition(p, p12, sizeof(p12));
  f.setPayload(p12, POSITION_LEN);
  return f;
}

Frame makeAlert(uint8_t code, const char* label) {
  Frame f;
  f.type = MSG_ALERT;
  f.flags = FLAG_MESH;
  uint8_t p[1 + 11];
  p[0] = code;
  uint8_t n = (uint8_t)strnlen(label, 11);
  memcpy(p + 1, label, n);
  f.setPayload(p, 1 + n);
  return f;
}

Frame makeNodeInfo() {
  Frame f;
  f.type = MSG_NODEINFO;
  f.flags = FLAG_MESH;
  f.setPayload(ctx.callName, (uint8_t)strnlen(ctx.callName, 11));
  return f;
}

Frame makePing(uint16_t dst, uint16_t seq) {
  Frame f;
  f.type = MSG_PING;
  f.dst = dst;
  uint8_t p[2] = {(uint8_t)(seq & 0xFF), (uint8_t)(seq >> 8)};
  f.setPayload(p, 2);
  return f;
}

Frame makePong(uint16_t dst, uint16_t seq, int16_t rssi, int8_t snr) {
  Frame f;
  f.type = MSG_PONG;
  f.dst = dst;
  uint8_t p[5] = {(uint8_t)(seq & 0xFF), (uint8_t)(seq >> 8),
                  (uint8_t)(rssi & 0xFF), (uint8_t)((rssi >> 8) & 0xFF),
                  (uint8_t)snr};
  f.setPayload(p, 5);
  return f;
}

Frame makeWaypoint(const Waypoint& w) {
  Frame f;
  f.type = MSG_WAYPOINT;
  f.flags = FLAG_MESH;
  uint8_t b[WAYPOINT_LEN];
  packWaypoint(w, b, sizeof(b));
  f.setPayload(b, WAYPOINT_LEN);
  return f;
}

Frame makeTimeSync(uint32_t unix, uint8_t source) {
  Frame f;
  f.type = MSG_TIMESYNC;
  f.flags = 0;   // direct only — never relayed, so adopted syncs are one hop old at most
  TimeSync ts{unix, source};
  uint8_t b[TIMESYNC_LEN];
  packTimeSync(ts, b, sizeof(b));
  f.setPayload(b, TIMESYNC_LEN);
  return f;
}

Frame makeDistress(const Position& p, uint8_t battPct) {
  Frame f;
  f.type = MSG_ALERT;
  f.flags = FLAG_MESH;
  uint8_t b[1 + POSITION_LEN + 1];
  b[0] = ALERT_DISTRESS;
  packPosition(p, b + 1, POSITION_LEN);
  b[1 + POSITION_LEN] = battPct;
  f.setPayload(b, sizeof(b));
  return f;
}

Frame makeCountdown(uint32_t unix, uint8_t code) {
  Frame f;
  f.type = MSG_COUNTDOWN;
  f.flags = FLAG_MESH;
  Countdown c{unix, code};
  uint8_t b[COUNTDOWN_LEN];
  packCountdown(c, b, sizeof(b));
  f.setPayload(b, COUNTDOWN_LEN);
  return f;
}

static const char* const CANNED[] = {"ROGER", "OK", "STANDBY", "ON MY WAY", "NEGATIVE", "RALLY"};

void runRuleAction(const RuleAction& a) {
  switch (a.type) {
    case AC_BEEP:
      audio::tone(1800, 200);
      break;
    case AC_SEND_TEXT: {
      uint8_t i = a.param % (sizeof(CANNED) / sizeof(CANNED[0]));
      Frame f = makeText(ADDR_BROADCAST, CANNED[i], false);
      netSend(f);
      break;
    }
    case AC_SEND_ALERT: {
      // Automated alerts stay duty-gated + CAD (NOT urgent) so a rule can't
      // amplify into a non-listening TX storm; human alerts (Klaxon/Mayday) stay urgent.
      Frame f = makeAlert((uint8_t)(a.param & 0x0F), "AUTO");
      netSend(f);
      break;
    }
    case AC_BEACON: {
      if (ctx.gps && ctx.gps->hasFix()) {
        Position p;
        p.lat = ctx.gps->lat();
        p.lon = ctx.gps->lon();
        p.altM = (int16_t)ctx.gps->altM();
        Frame f = makeBeacon(p);
        netSend(f);
      }
      break;
    }
    default:
      break;
  }
}

bool ntpSyncViaWifi() {
  if (!ctx.store || !ctx.clock) return false;
  char ssid[33] = {0}, pass[65] = {0};
  ctx.store->loadWifi(ssid, sizeof(ssid), pass, sizeof(pass));
  if (!ssid[0]) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass[0] ? pass : nullptr);   // nullptr key = open network
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) delay(100);

  bool ok = false;
  if (WiFi.status() == WL_CONNECTED) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");   // UTC, no DST offset
    struct tm tmv;
    if (getLocalTime(&tmv, 5000)) {
      time_t now = time(nullptr);
      if (now > 1600000000) {   // sanity: after 2020, so SNTP really landed
        ctx.clock->adopt((uint32_t)now, 3);   // 3 = NTP
        ctx.timeSource = 3;
        ok = true;
      }
    }
  }
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  return ok;
}

static uint32_t s_meshSeq = 0;

// Retune to the Meshtastic preset, air one raw encoded packet, restore our config.
static bool meshTxRaw(const uint8_t* frame, size_t n) {
  if (!frame || n == 0) return false;
  RadioCfg saved = ctx.cfg;
  ctx.lora->applyConfig(meshtasticPresetEU868());
  bool ok = ctx.lora->transmitRaw(frame, n);
  ctx.lora->applyConfig(saved);
  ctx.lora->startReceive();
  return ok;
}

bool meshtasticSendText(const char* text) {
  if (!ctx.lora || !text || !text[0]) return false;
  uint8_t frame[128];
  uint32_t pid = ((uint32_t)millis() << 8) ^ (++s_meshSeq);
  uint32_t from = 0x4C530000u | ctx.myAddr;   // our Meshtastic-style node id (VERIFY)
  size_t n = meshtastic_encode_text(from, pid, meshtasticDefaultChannelHash(),
                                    text, MESH_DEFAULT_KEY, frame, sizeof(frame));
  return meshTxRaw(frame, n);
}

bool meshtasticSendPosition() {
  if (!ctx.lora || !ctx.gps || !ctx.gps->hasFix()) return false;
  int32_t latI = (int32_t)(ctx.gps->lat() * 1e7);
  int32_t lonI = (int32_t)(ctx.gps->lon() * 1e7);
  uint8_t frame[128];
  uint32_t pid = ((uint32_t)millis() << 8) ^ (++s_meshSeq);
  uint32_t from = 0x4C530000u | ctx.myAddr;
  size_t n = meshtastic_encode_position(from, pid, meshtasticDefaultChannelHash(),
                                        latI, lonI, (int32_t)ctx.gps->altM(),
                                        MESH_DEFAULT_KEY, frame, sizeof(frame));
  return meshTxRaw(frame, n);
}

} // namespace ls
