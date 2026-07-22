#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../services/clock.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Mesh-wide synchronized timer. Broadcasts an absolute UTC fire-time, so every
// node counts down to the same instant regardless of receive delay and fires
// together at T-0 (beep + flash). Needs a UTC-holding node (Chronos/GPS).
class CountdownApp : public App {
public:
  const char* name() const override { return "Countdown"; }
  const char* callsign() const override { return "CDWN"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // hourglass
    g.drawLine(x + 4, y + 3, x + 16, y + 3, c);
    g.drawLine(x + 4, y + 17, x + 16, y + 17, c);
    g.drawLine(x + 4, y + 3, x + 10, y + 10, c);
    g.drawLine(x + 16, y + 3, x + 10, y + 10, c);
    g.drawLine(x + 4, y + 17, x + 10, y + 10, c);
    g.drawLine(x + 16, y + 17, x + 10, y + 10, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.up) offIdx_ = (offIdx_ + 1) % NOFF;
    else if (k.down) offIdx_ = (offIdx_ + NOFF - 1) % NOFF;
    else if (k.left && codeIdx_ > 0) codeIdx_--;
    else if (k.right && codeIdx_ < NCODES - 1) codeIdx_++;
    else if (k.enter) start(OFFS[offIdx_]);
    else if (k.ch == ' ') { ctx.cdTarget = 0; ctx.cdFired = false; M5.Speaker.stop(); }
  }

  void background() override {
    Clock* cl = ctx.clock;
    if (ctx.cdTarget == 0 || !cl || !cl->hasUtc()) return;
    uint32_t now = cl->utc();
    if (!ctx.cdFired) {
      if (now >= ctx.cdTarget) { ctx.cdFired = true; M5.Speaker.tone(2600, 600); }
    } else if (now >= ctx.cdTarget + 10) {
      ctx.cdTarget = 0;   // auto-clear ~10s after firing
      ctx.cdFired = false;
    }
  }

  void draw(M5Canvas& g) override {
    Clock* cl = ctx.clock;
    bool flash = ctx.cdFired && (millis() / 250) % 2 == 0;
    g.fillScreen(flash ? theme::CRIT : theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    char s[40];
    int y = ui::BODY_Y + 4;

    if (!cl || !cl->hasUtc()) {
      g.setTextColor(theme::WARN, theme::BG);
      g.drawString("needs time sync (Chronos)", 6, y + 8);
      ui::footer(g);
      return;
    }

    if (ctx.cdTarget) {
      long rem = (long)ctx.cdTarget - (long)cl->utc();
      if (rem < 0) rem = 0;
      g.setTextColor(rem <= 5 ? theme::CRIT : theme::ACCENT, flash ? theme::CRIT : theme::BG);
      g.setTextSize(3);
      std::snprintf(s, sizeof(s), "T-%02ld:%02ld", rem / 60, rem % 60);
      g.drawString(s, 44, y + 6);
      g.setTextSize(1);
      g.setTextColor(theme::TEXT, flash ? theme::CRIT : theme::BG);
      std::snprintf(s, sizeof(s), "%s  from %04X", CODES[ctx.cdCode < NCODES ? ctx.cdCode : 0], ctx.cdFrom);
      g.drawString(s, 6, y + 42);
    } else {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("no active countdown", 6, y + 10);
    }

    std::snprintf(s, sizeof(s), "set +%ds  label %s", OFFS[offIdx_], CODES[codeIdx_]);
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString(s, 6, ui::SCREEN_H - ui::FOOTER_H - 24);
    g.drawString("up/dn time  l/r label  Enter go  spc cancel", 6, ui::SCREEN_H - ui::FOOTER_H - 13);
    ui::footer(g);
  }

private:
  static const int NOFF = 5;
  static const int NCODES = 4;
  static constexpr int OFFS[NOFF] = {10, 30, 60, 300, 600};
  static constexpr const char* CODES[NCODES] = {"GO", "MOVE", "FIRE", "SYNC"};
  int offIdx_ = 1;
  int codeIdx_ = 0;

  void start(int offs) {
    Clock* cl = ctx.clock;
    if (!cl || !cl->hasUtc()) return;
    uint32_t target = cl->utc() + offs;
    ctx.cdTarget = target;
    ctx.cdCode = (uint8_t)codeIdx_;
    ctx.cdFrom = ctx.myAddr;
    ctx.cdFired = false;
    Frame f = makeCountdown(target, (uint8_t)codeIdx_);
    netSend(f);
  }
};

constexpr int CountdownApp::OFFS[];
constexpr const char* CountdownApp::CODES[];

} // namespace ls
