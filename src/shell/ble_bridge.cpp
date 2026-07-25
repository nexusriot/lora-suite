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

// Serialize a JSON doc, append '\n' as the line delimiter, and notify.
static void pushDoc(JsonDocument& d) {
  char buf[256];
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
  pushDoc(d);
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
  if (d["bright"].is<int>()) M5Cardputer.Display.setBrightness((uint8_t)d["bright"].as<int>());
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
      Frame f = makeText(dst, t, dst != ADDR_BROADCAST);
      netSend(f);
    }
  } else if (!std::strcmp(c, "get")) {
    const char* w = d["w"] | "";
    if (!std::strcmp(w, "status")) pushStatus();
    else if (!std::strcmp(w, "nodes")) pushNodes();
    else if (!std::strcmp(w, "mesh")) pushMesh();
  } else if (!std::strcmp(c, "cfg")) {
    applyCfg(d);
  } else if (!std::strcmp(c, "ntp")) {
    bool ok = ntpSyncViaWifi();
    JsonDocument a;
    a["e"] = "ntp";
    a["ok"] = ok ? 1 : 0;
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
