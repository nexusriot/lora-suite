#pragma once
#include <M5Unified.h>
#include <cstdio>
#include <cstring>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/lora_service.h"
#include "../proto/meshtastic.h"
#include "../proto/meshoverlay.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Over-the-air Meshtastic scanner (Direction B). On enter it retunes the radio to
// the EU_868 preset, goes receive-only (so we never air our frames on their
// channel), and decrypts the public channel — decoded Position/NodeInfo land in
// MeshOverlay(SRC_SCAN), viewable in the Mesh app. On exit it restores our config.
// NOTE: one radio — while this screen is open we are deaf to our own mesh.
class MeshScan : public App {
public:
  const char* name() const override { return "MeshScan"; }
  const char* callsign() const override { return "SCAN"; }
  Cat category() const override { return Cat::RF; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // magnifier
    g.drawCircle(x + 8, y + 8, 6, c);
    g.drawLine(x + 12, y + 12, x + 18, y + 18, c);
  }

  void onEnter() override {
    active_ = this;
    heard_ = decoded_ = 0;
    lastMs_ = 0;
    lastRssi_ = 0;
    if (ctx.lora) {
      saved_ = ctx.cfg;                               // remember our own profile
      ctx.lora->setRxOnly(true);                       // receive-only: never TX here
      ctx.lora->onRawReceive(&MeshScan::rawTrampoline);
      applyPreset();                                   // tune to the current preset
    }
  }

  void update() override {   // auto-cycle presets so we sweep LongFast/MediumFast/ShortFast
    if (ctx.lora && millis() - lastCycle_ >= CYCLE_MS) {
      preset_ = (preset_ + 1) % MESH_PRESET_COUNT;
      applyPreset();
    }
  }

  void onExit() override {
    if (ctx.lora) {
      ctx.lora->onRawReceive(nullptr);
      ctx.lora->setRxOnly(false);
      ctx.lora->applyConfig(saved_);                   // put our radio back
      ctx.lora->startReceive();
    }
    active_ = nullptr;
  }

  void onKey(const KeyEvent& k) override {
    if (k.ch == 'c') { heard_ = decoded_ = texts_ = 0; lastText_[0] = 0; }
    else if (k.ch == 'n') { preset_ = (preset_ + 1) % MESH_PRESET_COUNT; applyPreset(); }  // next preset now
    else if (k.enter) ctx.navRequest = "MESH";         // jump to the scanned-node list
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    char s[40];
    int y = ui::BODY_Y + 2;
    g.setTextColor(theme::RF, theme::BG);
    std::snprintf(s, sizeof(s), "RX %s SF%d  %.3f", meshtasticPresetName(preset_),
                  meshtasticPreset(preset_).sf, meshtasticPreset(preset_).freqHz / 1e6);
    g.drawString(s, 6, y); y += 13;

    g.setTextColor(theme::TEXT, theme::BG);
    std::snprintf(s, sizeof(s), "heard %lu   decoded %lu",
                  (unsigned long)heard_, (unsigned long)decoded_);
    g.drawString(s, 6, y); y += 12;

    g.setTextColor(theme::MUTED, theme::BG);
    if (lastMs_) {
      std::snprintf(s, sizeof(s), "last %lus ago  rssi %d",
                    (unsigned long)((millis() - lastMs_) / 1000), lastRssi_);
    } else {
      std::snprintf(s, sizeof(s), "listening...");
    }
    g.drawString(s, 6, y); y += 12;

    std::snprintf(s, sizeof(s), "overlay: %u nodes", (unsigned)ctx.mesh.size());
    g.drawString(s, 6, y); y += 12;

    if (texts_) {
      std::snprintf(s, sizeof(s), "txt %u: %.20s", (unsigned)texts_, lastText_);
      g.setTextColor(theme::ACCENT, theme::BG);
      g.drawString(s, 6, y);
    }
    y += 14;

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("Enter=list  c=clear", 6, y);

    ui::footer(g);
  }

private:
  static inline MeshScan* active_ = nullptr;
  static const uint32_t CYCLE_MS = 15000;   // auto-advance the scan preset every 15 s
  RadioCfg saved_;
  uint32_t heard_ = 0, decoded_ = 0, lastMs_ = 0, texts_ = 0;
  int16_t lastRssi_ = 0;
  char lastText_[64] = {0};
  int preset_ = 0;                          // 0=LongFast (Meshtastic default), 1=MediumFast, 2=ShortFast
  uint32_t lastCycle_ = 0;

  void applyPreset() {
    if (ctx.lora) {
      ctx.lora->applyConfig(meshtasticPreset(preset_));
      ctx.lora->startReceive();
    }
    lastCycle_ = millis();
  }

  static void rawTrampoline(const uint8_t* buf, size_t n, const RxMeta& m) {
    if (active_) active_->handleRaw(buf, n, m);
  }

  void handleRaw(const uint8_t* buf, size_t n, const RxMeta& m) {
    heard_++;
    lastMs_ = m.when ? m.when : 1;
    lastRssi_ = m.rssi;
    MeshPacket p;
    if (!meshtastic_decode(buf, n, MESH_DEFAULT_KEY, p)) return;
    decoded_++;
    if (p.hasPos) ctx.mesh.setPos(p.from, p.lat, p.lon, m.when, SRC_SCAN);
    if (p.hasUser) ctx.mesh.setUser(p.from, p.longName, p.shortName, mapRole(p.role), m.when, SRC_SCAN);
    if (p.hasMetrics) ctx.mesh.setMetrics(p.from, p.battery, p.voltCv, m.when, SRC_SCAN);
    if (p.hasText) {
      texts_++;
      std::strncpy(lastText_, p.text, sizeof(lastText_) - 1);
      lastText_[sizeof(lastText_) - 1] = 0;
    }
    ctx.mesh.setRssi(p.from, m.rssi, m.when, SRC_SCAN);
  }

  // Meshtastic device-role enum -> our compact MeshRole code (verify on bring-up).
  static uint8_t mapRole(uint8_t r) {
    switch (r) {
      case 0:  return MROLE_CLIENT;
      case 1:  return MROLE_CLIENT_MUTE;
      case 2: case 3: case 11: return MROLE_ROUTER;
      case 4:  return MROLE_REPEATER;
      case 5: case 10: return MROLE_TRACKER;
      case 6:  return MROLE_SENSOR;
      case 8:  return MROLE_CLIENT;    // hidden ~ client
      default: return MROLE_OTHER;
    }
  }
};

} // namespace ls
