#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/storage.h"
#include "../services/clock.h"
#include "../proto/battlog.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Battery history + discharge forecast. Reactor decides what to do about the
// current level; this shows the trend — how fast the pack is draining and how
// long it has left — from a least-squares fit over the recent samples, plus a
// graph of the window. Sampling runs in background() so the history keeps
// building whatever screen is open, and each sample is appended to /batt.csv.
class Coulomb : public App {
public:
  const char* name() const override { return "Coulomb"; }
  const char* callsign() const override { return "BATT"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // battery
    g.drawRect(x + 2, y + 6, 15, 9, c);
    g.fillRect(x + 17, y + 9, 2, 3, c);
    g.fillRect(x + 4, y + 8, 6, 5, c);
  }

  void background() override {
    uint32_t now = millis();
    if (last_ && now - last_ < PERIOD_MS) return;
    last_ = now;
    uint8_t pct = (uint8_t)M5.Power.getBatteryLevel();
    uint32_t mins = now / 60000;
    log_.add(mins, pct);

    if (ctx.store && ctx.store->sdReady()) {
      char t[9];
      if (ctx.clock) ctx.clock->hms(t); else std::snprintf(t, sizeof(t), "%lu", (unsigned long)mins);
      char line[48];
      std::snprintf(line, sizeof(line), "%s,%u,%u", t, (unsigned)mins, (unsigned)pct);
      ctx.store->appendLine("/batt.csv", line);
    }
  }

  void onKey(const KeyEvent& k) override {
    if (k.ch == 'c') log_.clear();   // restart the trend after a charge
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 2;
    char s[44];

    uint8_t pct = (uint8_t)M5.Power.getBatteryLevel();
    g.setTextColor(pct > 30 ? theme::GOOD : theme::WARN, theme::BG);
    std::snprintf(s, sizeof(s), "%u%%   %s", (unsigned)pct,
                  M5.Power.isCharging() ? "charging" : "on battery");
    g.drawString(s, 6, y);
    y += 13;

    g.setTextColor(theme::TEXT, theme::BG);
    float slope = log_.slopePctPerMin();
    if (log_.size() < 3) {
      std::snprintf(s, sizeof(s), "sampling... (%u)", (unsigned)log_.size());
    } else if (slope < 0) {
      std::snprintf(s, sizeof(s), "%.2f %%/h", (double)(slope * 60.0f));
    } else if (slope > 0) {
      std::snprintf(s, sizeof(s), "+%.2f %%/h", (double)(slope * 60.0f));
    } else {
      std::snprintf(s, sizeof(s), "steady");
    }
    g.drawString(s, 6, y);
    y += 12;

    int32_t empty = log_.minutesToEmpty();
    int32_t full = log_.minutesToFull();
    g.setTextColor(theme::ACCENT, theme::BG);
    if (empty >= 0) std::snprintf(s, sizeof(s), "empty in %ldh %02ldm", (long)(empty / 60), (long)(empty % 60));
    else if (full >= 0) std::snprintf(s, sizeof(s), "full in %ldh %02ldm", (long)(full / 60), (long)(full % 60));
    else std::snprintf(s, sizeof(s), "no trend yet");
    g.drawString(s, 6, y);
    y += 14;

    drawGraph(g, 6, y, ui::SCREEN_W - 12, 34);

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("c = reset trend", 6, ui::SCREEN_H - ui::FOOTER_H - 10);
    ui::footer(g);
  }

private:
  static const uint32_t PERIOD_MS = 60000;   // one sample a minute
  uint32_t last_ = 0;
  BattLog log_;

  // Sparkline of the retained window, autoscaled to its own min/max so a slow
  // drain is still visible (a fixed 0-100 axis would look flat).
  void drawGraph(M5Canvas& g, int x, int y, int w, int h) {
    g.drawRect(x, y, w, h, theme::LINE);
    size_t n = log_.size();
    if (n < 2) return;

    uint8_t lo = 100, hi = 0;
    for (size_t i = 0; i < n; i++) {
      uint8_t v = log_.pctAt(i);
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
    if (hi == lo) { hi = (uint8_t)(lo + 1); }

    int prevX = 0, prevY = 0;
    for (size_t i = 0; i < n; i++) {
      int px = x + 1 + (int)((w - 2) * i / (n - 1));
      int py = y + h - 2 - (int)((h - 3) * (log_.pctAt(i) - lo) / (hi - lo));
      if (i) g.drawLine(prevX, prevY, px, py, theme::GOOD);
      prevX = px;
      prevY = py;
    }

    char lbl[12];
    g.setTextColor(theme::MUTED, theme::BG);
    std::snprintf(lbl, sizeof(lbl), "%u", (unsigned)hi);
    g.drawString(lbl, x + w - 20, y + 1);
    std::snprintf(lbl, sizeof(lbl), "%u", (unsigned)lo);
    g.drawString(lbl, x + w - 20, y + h - 10);
  }
};

} // namespace ls
