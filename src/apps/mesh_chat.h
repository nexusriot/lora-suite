#pragma once
#include <M5Unified.h>
#include <cstdio>
#include <cstring>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/lora_service.h"
#include "../proto/meshtastic.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// A real two-way conversation on the Meshtastic public channel — the culmination
// of the interop work: it owns the radio while open (retunes to a preset, receive-
// only, raw-RX tap), decodes incoming TEXT into a feed, and transmits typed text
// back. Tab cycles the preset (LongFast = the global default). Same one-radio
// caveat as MeshScan: while this is open you can't hear your own mesh.
class MeshChat : public App {
public:
  const char* name() const override { return "MeshChat"; }
  const char* callsign() const override { return "MCHT"; }
  Cat category() const override { return Cat::RF; }
  bool consumesText() const override { return true; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // bubble + antenna
    g.drawRoundRect(x + 1, y + 4, 15, 10, 2, c);
    g.fillTriangle(x + 4, y + 13, x + 4, y + 17, x + 8, y + 13, c);
    g.drawLine(x + 16, y + 6, x + 18, y + 2, c);
    g.fillCircle(x + 18, y + 2, 1, c);
  }

  void onEnter() override {
    active_ = this;
    preset_ = ctx.meshCfg.preset;
    if (ctx.lora) {
      saved_ = ctx.cfg;
      ctx.lora->setRxOnly(true);
      ctx.lora->onRawReceive(&MeshChat::rawTrampoline);
      applyPreset();
    }
  }
  void onExit() override {
    if (ctx.lora) {
      ctx.lora->onRawReceive(nullptr);
      ctx.lora->setRxOnly(false);
      ctx.lora->applyConfig(saved_);
      ctx.lora->startReceive();
    }
    active_ = nullptr;
  }

  void onKey(const KeyEvent& k) override {
    if (k.enter) { send(); return; }
    if (k.del) { if (len_) input_[--len_] = 0; return; }
    if (k.tab) { preset_ = (preset_ + 1) % MESH_PRESET_COUNT; applyPreset(); return; }
    if (k.ch >= 0x20 && k.ch < 0x7f && len_ < (int)sizeof(input_) - 1) {
      input_[len_++] = k.ch;
      input_[len_] = 0;
    }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    g.setTextColor(theme::RF, theme::BG);
    char hd[40];
    std::snprintf(hd, sizeof(hd), "MSH %s (Tab)  sent %lu", meshtasticPresetName(preset_), (unsigned long)sent_);
    g.drawString(hd, 6, ui::BODY_Y + 1);

    int y = ui::BODY_Y + 13;
    int start = count_ > ROWS ? count_ - ROWS : 0;
    for (int i = start; i < count_; i++) {
      const Msg& mm = log_[i % RING];
      char line[48];
      if (mm.mine) std::snprintf(line, sizeof(line), "me> %s", mm.text);
      else std::snprintf(line, sizeof(line), "%04x> %s", (unsigned)(mm.from & 0xffff), mm.text);
      g.setTextColor(mm.mine ? theme::ACCENT : theme::TEXT, theme::BG);
      g.drawString(line, 6, y);
      y += 10;
    }

    int iy = ui::SCREEN_H - ui::FOOTER_H - 12;
    g.drawFastHLine(0, iy - 2, ui::SCREEN_W, theme::LINE);
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString(">", 6, iy);
    g.setTextColor(theme::TEXT, theme::BG);
    g.drawString(input_, 18, iy);
    ui::footer(g);
  }

private:
  struct Msg { uint32_t from; bool mine; char text[48]; };
  static const int RING = 20;
  static const int ROWS = 8;
  static inline MeshChat* active_ = nullptr;
  RadioCfg saved_;
  Msg log_[RING] = {};
  int count_ = 0;
  char input_[64] = {0};
  int len_ = 0;
  uint8_t preset_ = MPRESET_LONG_FAST;   // seeded from ctx.meshCfg on enter
  uint32_t sent_ = 0;

  void applyPreset() {
    if (ctx.lora) {
      ctx.lora->applyConfig(meshtasticRadioCfg(ctx.meshCfg, preset_));
      ctx.lora->startReceive();
    }
  }

  void add(uint32_t from, bool mine, const char* text) {
    Msg& m = log_[count_ % RING];
    m.from = from;
    m.mine = mine;
    std::strncpy(m.text, text, sizeof(m.text) - 1);
    m.text[sizeof(m.text) - 1] = 0;
    count_++;
  }

  void send() {
    if (!ctx.lora || len_ == 0) return;
    uint8_t frame[128];
    uint32_t pid = ((uint32_t)millis() << 8) ^ (++seq_);
    uint32_t from = meshtasticNodeId(ctx.meshCfg, ctx.myAddr);
    size_t n = meshtastic_encode_text(from, pid, meshtasticChannelHash(ctx.meshCfg, preset_),
                                      input_, ctx.meshCfg.key, ctx.meshCfg.keyLen,
                                      frame, sizeof(frame));
    if (n && ctx.lora->transmitRaw(frame, n)) {   // direct TX on the current preset
      add(from, true, input_);
      sent_++;
    }
    input_[0] = 0;
    len_ = 0;
  }

  uint32_t seq_ = 0;

  static void rawTrampoline(const uint8_t* buf, size_t n, const RxMeta& m) {
    if (active_) active_->handleRaw(buf, n, m);
  }
  void handleRaw(const uint8_t* buf, size_t n, const RxMeta&) {
    MeshPacket p;
    if (meshtastic_decode(buf, n, ctx.meshCfg.key, ctx.meshCfg.keyLen, p) && p.hasText)
      add(p.from, false, p.text);
  }
};

} // namespace ls
