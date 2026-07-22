#pragma once
#include <M5Unified.h>
#include <cstdio>
#include <cstring>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/lora_service.h"
#include "../services/storage.h"
#include "../services/clock.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Long-term airtime audit. Shows how the 1% EU868 budget is being spent, broken
// down by traffic type (+ a relay bucket), and appends a daily total to SD on the
// GPS-UTC day rollover. Attribution is charged in LoRaService::pump via AirLedger.
class Ledger : public App {
public:
  const char* name() const override { return "Ledger"; }
  const char* callsign() const override { return "LOG"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // receipt
    g.drawFastVLine(x + 4, y + 2, 15, c);
    g.drawFastVLine(x + 16, y + 2, 15, c);
    g.drawFastHLine(x + 4, y + 2, 13, c);
    g.drawLine(x + 4, y + 17, x + 7, y + 15, c);
    g.drawLine(x + 7, y + 15, x + 10, y + 17, c);
    g.drawLine(x + 10, y + 17, x + 13, y + 15, c);
    g.drawLine(x + 13, y + 15, x + 16, y + 17, c);
    g.drawFastHLine(x + 6, y + 6, 9, c);
    g.drawFastHLine(x + 6, y + 9, 9, c);
    g.drawFastHLine(x + 6, y + 12, 6, c);
  }

  void background() override {
    // Daily rollover: log the closing day's totals to SD, then reset.
    Clock* c = ctx.clock;
    if (!c || !c->hasUtc()) return;
    char ymd[9];
    c->ymd(ymd);
    if (lastYmd_[0] == 0) { std::strncpy(lastYmd_, ymd, 8); lastYmd_[8] = 0; return; }
    if (std::strcmp(ymd, lastYmd_) != 0) {
      AirLedger& L = ctx.lora->ledger();
      if (ctx.store) {
        char line[80];
        std::snprintf(line, sizeof(line), "%s,airtime_ms,%lu,frames,%lu",
                      lastYmd_, (unsigned long)L.total(), (unsigned long)L.frames());
        ctx.store->appendLine("/duty.csv", line);
      }
      L.reset();
      std::strncpy(lastYmd_, ymd, 8);
      lastYmd_[8] = 0;
    }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    AirLedger& L = ctx.lora->ledger();
    uint32_t total = L.total();
    double duty = ctx.lora->duty().usedFraction(millis());

    char s[40];
    g.setTextColor(theme::TEXT, theme::BG);
    std::snprintf(s, sizeof(s), "airtime %lus  frames %lu",
                  (unsigned long)(total / 1000), (unsigned long)L.frames());
    g.drawString(s, 6, ui::BODY_Y + 2);
    g.setTextColor(duty < 0.7 ? theme::GOOD : (duty < 1.0 ? theme::WARN : theme::CRIT), theme::BG);
    std::snprintf(s, sizeof(s), "duty %d%%", (int)(duty * 100));
    ui::textRight(g, ui::SCREEN_W - 4, ui::BODY_Y + 2, s);

    // buckets, highest airtime first
    int y = ui::BODY_Y + 16;
    for (int shown = 0; shown < 8 && y < ui::SCREEN_H - ui::FOOTER_H - 6; shown++) {
      int best = -1;
      uint32_t bestv = 0;
      for (int i = 1; i < AirLedger::SLOTS; i++) {
        uint32_t v = L.bucket(i);
        if (v > bestv && !used_[i]) { bestv = v; best = i; }
      }
      if (best < 0 || bestv == 0) break;
      used_[best] = true;
      int pct = total ? (int)((uint64_t)bestv * 100 / total) : 0;
      std::snprintf(s, sizeof(s), "%-9s %4lus  %3d%%", slotName(best),
                    (unsigned long)(bestv / 1000), pct);
      g.setTextColor(theme::TEXT, theme::BG);
      g.drawString(s, 6, y);
      // tiny bar
      g.fillRect(150, y + 1, pct * 80 / 100, 6, theme::ACCENT);
      y += 11;
    }
    for (int i = 0; i < AirLedger::SLOTS; i++) used_[i] = false;   // reset draw scratch

    if (total == 0) {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("(no airtime spent yet)", 6, ui::BODY_Y + 18);
    }
    ui::footer(g);
  }

private:
  char lastYmd_[9] = {0};
  bool used_[AirLedger::SLOTS] = {false};

  static const char* slotName(int i) {
    switch (i) {
      case MSG_TEXT: return "text"; case MSG_ACK: return "ack";
      case MSG_BEACON: return "beacon"; case MSG_PING: return "ping";
      case MSG_PONG: return "pong"; case MSG_TELEMETRY: return "telem";
      case MSG_ALERT: return "alert"; case MSG_NODEINFO: return "nodeinfo";
      case MSG_FILECHUNK: return "file"; case MSG_TIMESYNC: return "timesync";
      case MSG_WAYPOINT: return "waypt"; case 12: return "relay"; default: return "other";
    }
  }
};

} // namespace ls
