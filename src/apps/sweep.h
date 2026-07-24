#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/lora_service.h"
#include "../proto/meshtastic.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Channel-activity scanner. Steps a plan of frequencies, samples RSSI on each,
// and draws a live bar per channel with a running noise-floor readout. Restores
// the active radio config on exit.
class Sweep : public App {
public:
  static const int N = 16;

  const char* name() const override { return "Sweep"; }
  const char* callsign() const override { return "SWP"; }
  Cat category() const override { return Cat::RF; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // spectrum bars
    g.fillRect(x + 2, y + 12, 3, 6, c);
    g.fillRect(x + 7, y + 7, 3, 11, c);
    g.fillRect(x + 12, y + 3, 3, 15, c);
    g.fillRect(x + 16, y + 9, 3, 9, c);
  }

  void onEnter() override {
    base_ = ctx.cfg.freqHz;
    scan_ = ctx.cfg;
    for (int i = 0; i < N; i++) rssi_[i] = -128;
  }
  void onExit() override {
    if (ctx.lora) ctx.lora->applyConfig(ctx.cfg);   // put the radio back
  }

  void onKey(const KeyEvent& k) override {
    if (k.up) stepHz_ += 100000;
    else if (k.down && stepHz_ > 100000) stepHz_ -= 100000;
    else if (k.ch == 'm') {                        // jump to the EU_868 Meshtastic band
      uint32_t f = meshtasticPresetEU868().freqHz;  // 869.525 MHz, centred
      stepHz_ = 25000;
      base_ = f - (uint32_t)(N / 2) * stepHz_;
      peak_ = -128;
      for (int i = 0; i < N; i++) rssi_[i] = -128;
    }
  }

  void update() override {
    if (!ctx.lora) return;
    scan_.freqHz = base_ + (uint32_t)cur_ * stepHz_;
    ctx.lora->applyConfig(scan_);
    float r = ctx.lora->channelRssi();
    rssi_[cur_] = (int16_t)r;
    if (r > peak_) peak_ = r;
    cur_ = (cur_ + 1) % N;
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    char s[40];
    std::snprintf(s, sizeof(s), "%.1f MHz  step %dk  peak %d",
                  base_ / 1e6, (int)(stepHz_ / 1000), (int)peak_);
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString(s, 6, ui::BODY_Y + 2);

    const int x0 = 8, y0 = ui::SCREEN_H - ui::FOOTER_H - 6, bw = 13, maxh = 60;
    for (int i = 0; i < N; i++) {
      int v = rssi_[i] + 128;                 // -128..0 -> 0..128
      if (v < 0) v = 0; if (v > 128) v = 128;
      int h = v * maxh / 128;
      int x = x0 + i * bw;
      uint16_t c = rssi_[i] > -90 ? theme::RF : theme::ACCENT;
      g.fillRect(x, y0 - h, bw - 2, h, i == cur_ ? theme::TEXT : c);
    }
    ui::footer(g);
  }

private:
  uint32_t base_ = 868000000;
  uint32_t stepHz_ = 200000;
  RadioCfg scan_;
  int cur_ = 0;
  int16_t rssi_[N] = {};
  float peak_ = -128;
};

} // namespace ls
