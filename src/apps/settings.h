#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/storage.h"
#include "../services/audio.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Runtime device settings — display brightness and speaker volume — persisted to
// NVS (so you don't have to reflash to change brightness). Applied live; saved on
// exit. Loaded + applied at boot in main.cpp.
class Settings : public App {
public:
  const char* name() const override { return "Settings"; }
  const char* callsign() const override { return "SET"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // gear
    g.drawCircle(x + 10, y + 10, 5, c);
    g.fillCircle(x + 10, y + 10, 2, c);
    g.fillRect(x + 9, y + 1, 2, 3, c);   g.fillRect(x + 9, y + 16, 2, 3, c);
    g.fillRect(x + 1, y + 9, 3, 2, c);   g.fillRect(x + 16, y + 9, 3, 2, c);
  }

  void onEnter() override { if (ctx.store) ctx.store->loadSettings(bright_, vol_); }
  void onExit() override { if (ctx.store) ctx.store->saveSettings(bright_, vol_); }

  void onKey(const KeyEvent& k) override {
    if (k.up) field_ = (field_ + NF - 1) % NF;
    else if (k.down) field_ = (field_ + 1) % NF;
    else if (k.left) adjust(-16);
    else if (k.right) adjust(16);
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    const char* names[NF] = {"Brightness", "Volume"};
    uint8_t vals[NF] = {bright_, vol_};

    int y = ui::BODY_Y + 10;
    for (int i = 0; i < NF; i++) {
      bool sel = i == field_;
      g.setTextColor(sel ? theme::ACCENT : theme::MUTED, theme::BG);
      g.drawString(sel ? ">" : " ", 4, y);
      g.drawString(names[i], 14, y);
      const int bx = 96, bw = 120;
      g.drawRect(bx, y, bw, 9, theme::LINE);
      g.fillRect(bx + 1, y + 1, vals[i] * (bw - 2) / 255, 7, sel ? theme::ACCENT : theme::MUTED);
      char v[8];
      std::snprintf(v, sizeof(v), "%d%%", vals[i] * 100 / 255);
      g.setTextColor(theme::TEXT, theme::BG);
      ui::textRight(g, ui::SCREEN_W - 4, y + 12, v);
      y += 26;
    }
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("up/down pick  left/right adjust", 6, ui::SCREEN_H - ui::FOOTER_H - 10);
    ui::footer(g);
  }

private:
  static const int NF = 2;
  int field_ = 0;
  uint8_t bright_ = 26, vol_ = 200;

  void adjust(int d) {
    int v;
    if (field_ == 0) {
      v = (int)bright_ + d;
      bright_ = (uint8_t)(v < 8 ? 8 : (v > 255 ? 255 : v));   // never fully dark
      M5Cardputer.Display.setBrightness(bright_);
    } else {
      v = (int)vol_ + d;
      vol_ = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
      audio::setVolume(vol_);
      audio::tick();
    }
  }
};

} // namespace ls
