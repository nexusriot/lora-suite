#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/ir.h"
#include "../services/audio.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// IR remote — sends canned NEC codes over the (otherwise unused) IR LED. The codes
// below are generic examples; real remotes are device-specific, so reprogram the
// table for your gear. Uses the raw NEC protocol (see services/ir.h).
class IrBlaster : public App {
public:
  const char* name() const override { return "IR"; }
  const char* callsign() const override { return "IR"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // IR LED + beam
    g.fillCircle(x + 5, y + 10, 3, c);
    g.drawLine(x + 9, y + 6, x + 16, y + 3, c);
    g.drawLine(x + 9, y + 10, x + 17, y + 10, c);
    g.drawLine(x + 9, y + 14, x + 16, y + 17, c);
  }

  void onEnter() override { ir::init(); }

  void onKey(const KeyEvent& k) override {
    if (k.up && sel_ > 0) sel_--;
    else if (k.down && sel_ + 1 < N) sel_++;
    else if (k.enter) {
      ir::sendNEC(CODES[sel_].addr, CODES[sel_].cmd);
      audio::tick();
      sent_ = sel_ + 1;
    }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 2;
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("generic NEC codes:", 6, y);
    y += 13;

    for (int i = 0; i < N; i++) {
      bool s = (i == sel_);
      char row[32];
      std::snprintf(row, sizeof(row), "%c%-8s  %02X:%02X", s ? '>' : ' ', CODES[i].label, CODES[i].addr, CODES[i].cmd);
      g.setTextColor(s ? theme::ACCENT : theme::TEXT, theme::BG);
      g.drawString(row, 6, y);
      y += 11;
    }

    g.setTextColor(theme::MUTED, theme::BG);
    if (sent_) {
      char s[24];
      std::snprintf(s, sizeof(s), "sent %s", CODES[sent_ - 1].label);
      g.drawString(s, 6, ui::SCREEN_H - ui::FOOTER_H - 20);
    }
    g.drawString("Enter = transmit", 6, ui::SCREEN_H - ui::FOOTER_H - 10);
    ui::footer(g);
  }

private:
  struct Code { const char* label; uint8_t addr; uint8_t cmd; };
  static const int N = 6;
  static constexpr Code CODES[N] = {
    {"Power", 0x00, 0x0C}, {"Vol +", 0x00, 0x10}, {"Vol -", 0x00, 0x11},
    {"Mute", 0x00, 0x0D},  {"Ch +", 0x00, 0x20},  {"Ch -", 0x00, 0x21},
  };
  int sel_ = 0, sent_ = 0;
};

constexpr IrBlaster::Code IrBlaster::CODES[];

} // namespace ls
