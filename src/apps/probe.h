#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/lora_service.h"
#include "../services/gps_service.h"
#include "../services/storage.h"
#include "../services/clock.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Hardware self-test / bring-up aid. Scans the internal I2C bus and reports the
// state of every peripheral (radio, GPS, SD, IMU, keyboard) on one screen. This
// is the tool that would have made the dead-keyboard diagnosis instant — an
// absent 0x34 (TCA8418) or a mis-detected board shows up immediately. 'r' re-scans.
class Probe : public App {
public:
  const char* name() const override { return "Probe"; }
  const char* callsign() const override { return "PRB"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // magnifier + tick
    g.drawCircle(x + 8, y + 8, 5, c);
    g.drawLine(x + 12, y + 12, x + 17, y + 17, c);
    g.drawLine(x + 5, y + 8, x + 7, y + 10, c);
    g.drawLine(x + 7, y + 10, x + 11, y + 5, c);
  }

  void onEnter() override { scan(); }
  void onKey(const KeyEvent& k) override { if (k.ch == 'r') scan(); }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 2;
    char s[44];

    row(g, y, "board", board_ == 24 ? "Adv (24)" : hint_, board_ == 24); y += 11;

    GpsService* gp = ctx.gps;
    bool radioOk = ctx.lora && ctx.lora->ready();
    std::snprintf(s, sizeof(s), "%.1f MHz", ctx.lora ? ctx.lora->config().freqHz / 1e6 : 0);
    row(g, y, "radio", radioOk ? s : "FAIL", radioOk); y += 11;

    bool fix = gp && gp->hasFix();
    std::snprintf(s, sizeof(s), fix ? "fix %dsat" : "no fix (%dsat)", gp ? gp->sats() : 0);
    row(g, y, "gps", s, fix); y += 11;

    bool sd = ctx.store && ctx.store->sdReady();
    row(g, y, "sd", sd ? "mounted" : "none", sd); y += 11;

    bool kbd = hasAddr(0x34);
    row(g, y, "keyboard", kbd ? "TCA8418 0x34" : "MISSING", kbd); y += 11;

    // Raw I2C address list (the ground truth).
    g.setTextColor(theme::MUTED, theme::BG);
    char list[44];
    int off = std::snprintf(list, sizeof(list), "i2c:");
    for (int i = 0; i < foundN_ && off < (int)sizeof(list) - 4; i++)
      off += std::snprintf(list + off, sizeof(list) - off, " %02x", found_[i]);
    if (foundN_ == 0) std::snprintf(list, sizeof(list), "i2c: (none)");
    g.drawString(list, 6, y); y += 12;

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("r = re-scan", 6, y);
    ui::footer(g);
  }

private:
  static const int MAXF = 12;
  uint8_t found_[MAXF] = {};
  int foundN_ = 0;
  int board_ = -1;
  char hint_[16] = {0};

  void scan() {
    board_ = (int)M5.getBoard();
    std::snprintf(hint_, sizeof(hint_), "id %d", board_);
    foundN_ = 0;
    for (uint8_t a = 0x08; a <= 0x77 && foundN_ < MAXF; a++)
      if (M5.In_I2C.scanID(a)) found_[foundN_++] = a;
  }

  bool hasAddr(uint8_t a) const {
    for (int i = 0; i < foundN_; i++) if (found_[i] == a) return true;
    return false;
  }

  static void row(M5Canvas& g, int y, const char* label, const char* val, bool ok) {
    g.setTextColor(ok ? theme::GOOD : theme::CRIT, theme::BG);
    g.drawString(ok ? "+" : "x", 6, y);
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString(label, 16, y);
    g.setTextColor(theme::TEXT, theme::BG);
    g.drawString(val, 96, y);
  }
};

} // namespace ls
