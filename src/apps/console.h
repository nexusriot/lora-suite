#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../proto/airtime.h"
#include "../services/lora_service.h"
#include "../services/storage.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Radio profile editor + link calculator. Up/down pick a field, left/right change
// it (applied live), s/l save/load the active profile slot, r cycles a region
// preset. Shows time-on-air, duty budget and free-space path loss.
class Console : public App {
public:
  const char* name() const override { return "Console"; }
  const char* callsign() const override { return "CFG"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // sliders
    g.drawFastHLine(x + 2, y + 5, 16, c);  g.fillCircle(x + 7, y + 5, 2, c);
    g.drawFastHLine(x + 2, y + 10, 16, c); g.fillCircle(x + 13, y + 10, 2, c);
    g.drawFastHLine(x + 2, y + 15, 16, c); g.fillCircle(x + 9, y + 15, 2, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.down) field_ = (field_ + 1) % NFIELDS;
    else if (k.up) field_ = (field_ + NFIELDS - 1) % NFIELDS;
    else if (k.left) adjust(-1);
    else if (k.right) adjust(+1);
    else if (k.ch == 's' && ctx.store) ctx.store->saveProfile(slot_, ctx.cfg, "");
    else if (k.ch == 'l' && ctx.store) { char psk[24]; ctx.store->loadProfile(slot_, ctx.cfg, psk, sizeof(psk)); apply(); }
    else if (k.ch == 'r') region_ = (region_ + 1) % 3, applyRegion();
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    RadioCfg& c = ctx.cfg;
    char v[24];

    const char* names[NFIELDS] = {"FREQ", "SF", "BW", "CR", "PWR", "PRE"};
    char vals[NFIELDS][16];
    std::snprintf(vals[0], 16, "%.2f MHz", c.freqHz / 1e6);
    std::snprintf(vals[1], 16, "%u", c.sf);
    std::snprintf(vals[2], 16, "%u kHz", (unsigned)(c.bwHz / 1000));
    std::snprintf(vals[3], 16, "4/%u", c.cr);
    std::snprintf(vals[4], 16, "%d dBm", c.power);
    std::snprintf(vals[5], 16, "%u", c.preamble);

    int y = ui::BODY_Y + 2;
    for (int i = 0; i < NFIELDS; i++) {
      bool sel = i == field_;
      g.setTextColor(sel ? theme::ACCENT : theme::MUTED, theme::BG);
      g.drawString(sel ? ">" : " ", 4, y);
      g.drawString(names[i], 14, y);
      g.setTextColor(sel ? theme::TEXT : theme::MUTED, theme::BG);
      g.drawString(vals[i], 70, y);
      y += 11;
    }

    double toa = timeOnAirMs(c, 40);
    uint32_t budget = ctx.lora ? ctx.lora->duty().budgetMs() : 36000;
    int fx = 132, fy = ui::BODY_Y + 4;
    g.setTextColor(theme::MUTED, theme::BG);
    std::snprintf(v, sizeof(v), "region %s", regionName(region_)); g.drawString(v, fx, fy); fy += 12;
    g.setTextColor(theme::TEXT, theme::BG);
    std::snprintf(v, sizeof(v), "ToA40 %dms", (int)toa); g.drawString(v, fx, fy); fy += 11;
    std::snprintf(v, sizeof(v), "budget %ds", (int)(budget / 1000)); g.drawString(v, fx, fy); fy += 11;
    std::snprintf(v, sizeof(v), "max/hr %d", toa > 0 ? (int)(budget / toa) : 0); g.drawString(v, fx, fy); fy += 11;
    std::snprintf(v, sizeof(v), "FSPL1k %ddB", (int)pathLossDb(c, 1000)); g.drawString(v, fx, fy); fy += 11;
    g.setTextColor(theme::MUTED, theme::BG);
    std::snprintf(v, sizeof(v), "slot %u  s/l r", slot_); g.drawString(v, fx, fy);

    ui::footer(g);
  }

private:
  static const int NFIELDS = 6;
  int field_ = 0, region_ = 0;
  uint8_t slot_ = 0;

  void adjust(int d) {
    RadioCfg& c = ctx.cfg;
    switch (field_) {
      case 0: c.freqHz += d * 100000; break;
      case 1: c.sf = clamp(c.sf + d, 7, 12); break;
      case 2: c.bwHz = nextBw(c.bwHz, d); break;
      case 3: c.cr = clamp(c.cr + d, 5, 8); break;
      case 4: c.power = clamp(c.power + d, 2, 22); break;
      case 5: c.preamble = clamp(c.preamble + d, 6, 16); break;
    }
    apply();
  }

  void apply() { if (ctx.lora) ctx.lora->applyConfig(ctx.cfg); }

  static int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

  static uint32_t nextBw(uint32_t bw, int d) {
    static const uint32_t opts[] = {62500, 125000, 250000, 500000};
    int i = 1;
    for (int j = 0; j < 4; j++) if (opts[j] == bw) i = j;
    i = clamp(i + d, 0, 3);
    return opts[i];
  }

  static const char* regionName(int r) {
    switch (r) { case 0: return "EU868"; case 1: return "US915"; default: return "AS923"; }
  }
  void applyRegion() {
    switch (region_) {
      case 0: ctx.cfg.freqHz = 868000000; ctx.cfg.power = 14; break;
      case 1: ctx.cfg.freqHz = 915000000; ctx.cfg.power = 20; break;
      default: ctx.cfg.freqHz = 923000000; ctx.cfg.power = 16; break;
    }
    apply();
  }
};

} // namespace ls
