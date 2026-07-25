#include "ble_bridge.h"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <M5Cardputer.h>
#include <cstring>
#include <cstdio>
#include "context.h"
#include "net.h"
#include "../services/lora_service.h"
#include "../services/gps_service.h"
#include "../services/storage.h"
#include "../services/audio.h"
#include "../services/clock.h"
#include "../services/ir.h"

namespace ls {
namespace ble {

// Nordic UART Service UUIDs — works with our app and any generic BLE-UART tool.
static const char* NUS_SVC = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* NUS_RX  = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // phone -> device (write)
static const char* NUS_TX  = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";  // device -> phone (notify)

static bool s_enabled = false;
static volatile bool s_connected = false;
static NimBLECharacteristic* s_tx = nullptr;
static uint32_t s_lastStatus = 0;

// Single-slot mailbox: the BLE host task drops a command here; the main loop drains it.
static char s_cmd[220];
static volatile bool s_cmdPending = false;

static void enqueueCmd(const char* p, size_t n) {
  if (s_cmdPending) return;                 // busy — drop (phone can retry)
  if (n >= sizeof(s_cmd)) n = sizeof(s_cmd) - 1;
  std::memcpy(s_cmd, p, n);
  s_cmd[n] = 0;
  s_cmdPending = true;                       // set last: hands off to the main loop
}

static void pushChunks(const char* buf, size_t n) {
  if (!s_connected || !s_tx) return;
  const size_t CH = 180;                     // stay under the negotiated MTU
  for (size_t off = 0; off < n; off += CH) {
    size_t len = (n - off < CH) ? (n - off) : CH;
    s_tx->setValue((const uint8_t*)(buf + off), len);
    s_tx->notify();
  }
}

// Serialize a JSON doc, append '\n' as the line delimiter, and notify. Sized for
// the largest event: a full TEXT_MAX message plus its JSON envelope.
static void pushDoc(JsonDocument& d) {
  char buf[TEXT_MAX + 128];
  size_t n = serializeJson(d, buf, sizeof(buf) - 2);
  buf[n++] = '\n';
  pushChunks(buf, n);
}

static void hex16(char* out, uint16_t v) { std::snprintf(out, 5, "%04X", v); }

void onRadioText(uint16_t from, const char* text, int16_t rssi) {
  if (!s_connected) return;
  JsonDocument d;
  char h[5]; hex16(h, from);
  d["e"] = "msg";
  d["from"] = h;
  d["rssi"] = rssi;
  d["t"] = text;
  pushDoc(d);
}

static void pushStatus() {
  JsonDocument d;
  d["e"] = "st";
  GpsService* gp = ctx.gps;
  d["fix"] = gp && gp->hasFix() ? 1 : 0;
  if (gp && gp->hasFix()) { d["lat"] = gp->lat(); d["lon"] = gp->lon(); d["sats"] = gp->sats(); }
  d["batt"] = M5.Power.getBatteryLevel();
  d["rx"] = ctx.rxCount;
  d["fwd"] = ctx.relayForwarded;
  d["ch"] = ctx.channel.id();
  d["q"] = ctx.lora ? (uint32_t)ctx.lora->queueDepth() : 0;
  d["name"] = ctx.callName;
  d["pres"] = ctx.presence;
  d["pwr"] = (uint8_t)ctx.power;
  d["gw"] = ctx.gatewayOn ? 1 : 0;
  d["unread"] = ctx.unread;
  d["duty"] = (uint8_t)(ctx.lora ? ctx.lora->duty().usedFraction(millis()) * 100.0 : 0.0);
  pushDoc(d);
}

// Push the user's IR code table as a series of `ir` events.
static void pushIrCodes() {
  for (size_t i = 0; i < ctx.irCodes.size(); i++) {
    const IrCode& c = ctx.irCodes.at(i);
    JsonDocument d;
    d["e"] = "ir";
    d["i"] = (uint8_t)i;
    d["label"] = c.label;
    d["addr"] = c.addr;
    d["cmd"] = c.cmd;
    pushDoc(d);
  }
}

// Push the durable contact roster as a series of `ct` events.
static void pushRoster() {
  char h[5], nm[13];
  for (size_t i = 0; i < ctx.roster.size(); i++) {
    const Contact& c = ctx.roster.at(i);
    if (!c.used) continue;
    JsonDocument d;
    hex16(h, c.addr);
    d["e"] = "ct";
    d["addr"] = h;
    d["name"] = ctx.roster.label(c.addr, nm, sizeof(nm));
    d["blk"] = c.blocked ? 1 : 0;
    d["fav"] = c.favorite ? 1 : 0;
    pushDoc(d);
  }
}

static void pushNodes() {
  uint32_t now = millis();
  char h[5], nm[13];
  for (size_t i = 0; i < ctx.nodes.size(); i++) {
    const Node& n = ctx.nodes.at(i);
    JsonDocument d;
    hex16(h, n.addr);
    d["e"] = "nd";
    d["addr"] = h;
    d["name"] = ctx.roster.label(n.addr, nm, sizeof(nm));
    d["batt"] = n.hasHealth ? n.battPct : 0;
    d["rssi"] = n.rssi;
    d["age"] = (now - n.lastHeard) / 1000;
    pushDoc(d);
  }
}

static void pushMesh() {
  for (size_t i = 0; i < ctx.mesh.size(); i++) {
    const MeshNode& m = ctx.mesh.at(i);
    JsonDocument d;
    char id[9]; std::snprintf(id, sizeof(id), "%08lx", (unsigned long)m.id);
    d["e"] = "mn";
    d["id"] = id;
    d["name"] = m.longName[0] ? m.longName : m.shortName;
    if (m.hasPos) { d["lat"] = m.lat; d["lon"] = m.lon; }
    d["batt"] = m.battPct;
    pushDoc(d);
  }
}

static void applyCfg(JsonDocument& d) {
  bool ident = false;
  if (d["name"].is<const char*>()) {
    std::strncpy(ctx.callName, d["name"].as<const char*>(), sizeof(ctx.callName) - 1);
    ctx.callName[sizeof(ctx.callName) - 1] = 0;
    ident = true;
  }
  if (d["addr"].is<const char*>()) {
    ctx.myAddr = (uint16_t)strtol(d["addr"].as<const char*>(), nullptr, 16);
    ident = true;
  }
  if (ident && ctx.store) ctx.store->saveIdentity(ctx.myAddr, ctx.callName);
  if (d["wssid"].is<const char*>() && ctx.store) {
    ctx.store->saveWifi(d["wssid"].as<const char*>(), d["wpass"] | "");
  }

  // Brightness + volume: apply live and persist together (NVS stores them as a pair).
  if ((d["bright"].is<int>() || d["vol"].is<int>()) && ctx.store) {
    uint8_t b, v;
    ctx.store->loadSettings(b, v);
    if (d["bright"].is<int>()) { b = (uint8_t)d["bright"].as<int>(); M5Cardputer.Display.setBrightness(b); }
    if (d["vol"].is<int>())    { v = (uint8_t)d["vol"].as<int>();    audio::setVolume(v); }
    ctx.store->saveSettings(b, v);
  }

  // Channel PSK: "" clears back to the public channel; otherwise key + persist.
  if (d["psk"].is<const char*>()) {
    const char* psk = d["psk"].as<const char*>();
    ctx.channel.setPSK(psk);
    if (ctx.store) ctx.store->saveProfile(ctx.store->activeSlot(), ctx.cfg, psk);
  }

  if (d["region"].is<int>()) {
    switch (d["region"].as<int>()) {
      case 0: ctx.cfg.freqHz = 868000000; ctx.cfg.power = 14; break;
      case 1: ctx.cfg.freqHz = 915000000; ctx.cfg.power = 20; break;
      default: ctx.cfg.freqHz = 923000000; ctx.cfg.power = 16; break;
    }
    if (ctx.lora) ctx.lora->applyConfig(ctx.cfg);
  }
  // echo the current identity back as an ack
  JsonDocument a;
  char h[5]; hex16(h, ctx.myAddr);
  a["e"] = "cfg";
  a["name"] = ctx.callName;
  a["addr"] = h;
  pushDoc(a);
}

static void handleCmd(const char* json) {
  JsonDocument d;
  if (deserializeJson(d, json)) return;
  const char* c = d["c"] | "";
  if (!std::strcmp(c, "tx")) {
    const char* to = d["to"] | "FFFF";
    const char* t = d["t"] | "";
    if (t[0]) {
      uint16_t dst = (uint16_t)strtol(to, nullptr, 16);
      netSendText(dst, t, dst != ADDR_BROADCAST);   // compresses + fragments
    }
  } else if (!std::strcmp(c, "get")) {
    const char* w = d["w"] | "";
    if (!std::strcmp(w, "status")) pushStatus();
    else if (!std::strcmp(w, "nodes")) pushNodes();
    else if (!std::strcmp(w, "mesh")) pushMesh();
    else if (!std::strcmp(w, "roster")) pushRoster();
    else if (!std::strcmp(w, "ir")) pushIrCodes();
  } else if (!std::strcmp(c, "cfg")) {
    applyCfg(d);
  } else if (!std::strcmp(c, "ntp")) {
    bool ok = ntpSyncViaWifi();
    JsonDocument a;
    a["e"] = "ntp"; a["ok"] = ok ? 1 : 0;
    pushDoc(a);
  } else if (!std::strcmp(c, "pres")) {
    ctx.presence = (uint8_t)(d["p"] | 0) & 0x03;
    pushStatus();
  } else if (!std::strcmp(c, "gw")) {
    ctx.gatewayOn = (d["on"] | 0) != 0;
    pushStatus();
  } else if (!std::strcmp(c, "beacon")) {
    if (ctx.gps && ctx.gps->hasFix()) {
      Position p;
      p.lat = ctx.gps->lat(); p.lon = ctx.gps->lon(); p.altM = (int16_t)ctx.gps->altM();
      Frame f = makeBeacon(p);
      netSend(f);
    }
  } else if (!std::strcmp(c, "ping")) {
    const char* to = d["to"] | "FFFF";
    uint16_t dst = (uint16_t)strtol(to, nullptr, 16);
    static uint16_t seq = 0;
    Frame f = makePing(dst, ++seq);
    netSend(f);
  } else if (!std::strcmp(c, "alert")) {
    Frame f = makeAlert((uint8_t)(d["code"] | 0), d["label"] | "ALERT");
    netSend(f, true);
  } else if (!std::strcmp(c, "distress")) {
    Position p{};
    if (ctx.gps && ctx.gps->hasFix()) {
      p.lat = ctx.gps->lat(); p.lon = ctx.gps->lon(); p.altM = (int16_t)ctx.gps->altM();
    }
    Frame f = makeDistress(p, (uint8_t)M5.Power.getBatteryLevel());
    netSend(f, true);   // urgent: bypass the duty hold
  } else if (!std::strcmp(c, "countdown")) {
    int secs = d["secs"] | 0;
    if (ctx.clock && ctx.clock->hasUtc() && secs > 0) {
      uint32_t unix = ctx.clock->utc() + (uint32_t)secs;
      uint8_t code = (uint8_t)(d["code"] | 0);
      Frame f = makeCountdown(unix, code);
      netSend(f);
      ctx.cdTarget = unix; ctx.cdCode = code; ctx.cdFrom = ctx.myAddr; ctx.cdFired = false;
    }
  } else if (!std::strcmp(c, "irset")) {
    // Add or overwrite one code; index past the end appends.
    int i = d["i"] | (int)ctx.irCodes.size();
    ctx.irCodes.set((size_t)i, d["label"] | "code",
                    (uint8_t)(d["addr"] | 0), (uint8_t)(d["cmd"] | 0));
    if (ctx.store) ctx.store->saveIrCodes(ctx.irCodes);
    pushIrCodes();
  } else if (!std::strcmp(c, "irdel")) {
    ctx.irCodes.remove((size_t)(d["i"] | -1));
    if (ctx.store) ctx.store->saveIrCodes(ctx.irCodes);
    pushIrCodes();
  } else if (!std::strcmp(c, "irsend")) {
    int i = d["i"] | -1;
    if (i >= 0 && i < (int)ctx.irCodes.size()) {
      const IrCode& c2 = ctx.irCodes.at((size_t)i);
      ir::init();
      ir::sendNEC(c2.addr, c2.cmd);
    }
  } else if (!std::strcmp(c, "meshtx")) {
    bool ok = (d["pos"] | 0) ? meshtasticSendPosition() : meshtasticSendText(d["t"] | "");
    JsonDocument a;
    a["e"] = "meshtx"; a["ok"] = ok ? 1 : 0;
    pushDoc(a);
  }
}

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    std::string v = c->getValue();
    enqueueCmd(v.c_str(), v.size());
  }
};

class SrvCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*) override { s_connected = true; }
  void onDisconnect(NimBLEServer*) override {
    s_connected = false;
    NimBLEDevice::startAdvertising();
  }
};

static RxCallbacks s_rxCb;
static SrvCallbacks s_srvCb;

void begin(const char* name) {
  if (s_enabled) return;
  NimBLEDevice::init(name);
  NimBLEDevice::setMTU(247);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(&s_srvCb, false);   // static instance — NimBLE must NOT delete it on deinit
  NimBLEService* svc = server->createService(NUS_SVC);
  NimBLECharacteristic* rx =
      svc->createCharacteristic(NUS_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(&s_rxCb);
  s_tx = svc->createCharacteristic(NUS_TX, NIMBLE_PROPERTY::NOTIFY);
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SVC);
  adv->setScanResponse(true);
  adv->start();
  s_enabled = true;
}

void end() {
  if (!s_enabled) return;
  NimBLEDevice::deinit(true);
  s_enabled = false;
  s_connected = false;
  s_tx = nullptr;
  s_cmdPending = false;
}

bool enabled() { return s_enabled; }
bool connected() { return s_connected; }

void loop() {
  if (!s_enabled) return;
  if (s_cmdPending) {
    handleCmd(s_cmd);            // executed on the main loop, not the BLE task
    s_cmdPending = false;
  }
  if (s_connected && millis() - s_lastStatus > 3000) {
    s_lastStatus = millis();
    pushStatus();
  }
}

} // namespace ble
} // namespace ls
