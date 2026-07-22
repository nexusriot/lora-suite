#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../proto/nodetable.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Squad-vitals dashboard: every peer's battery, presence, RSSI and age from the
// Pulse health channel + NodeTable, worst-battery-first. 'p' cycles your own
// Presence (broadcast on the health TLV); Enter hands the selected node to
// Courier as a DM target. Reactor's Survival state auto-forces you to RESTING.
class Fleet : public App {
public:
  const char* name() const override { return "Fleet"; }
  const char* callsign() const override { return "FLT"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // heartbeat
    g.drawLine(x + 2, y + 10, x + 6, y + 10, c);
    g.drawLine(x + 6, y + 10, x + 8, y + 4, c);
    g.drawLine(x + 8, y + 4, x + 11, y + 16, c);
    g.drawLine(x + 11, y + 16, x + 13, y + 10, c);
    g.drawLine(x + 13, y + 10, x + 18, y + 10, c);
  }

  void background() override {
    // Sole writer of ctx.presence: a Survival-power node advertises RESTING;
    // otherwise the user's chosen state (userPres_) stands.
    ctx.presence = (ctx.power == PWR_SURVIVAL) ? (uint8_t)PRES_REST : userPres_;
  }

  void onKey(const KeyEvent& k) override {
    size_t n = ctx.nodes.size();
    if (k.up && sel_ > 0) sel_--;
    else if (k.down && (size_t)(sel_ + 1) < n) sel_++;
    else if (k.ch == 'p') userPres_ = (userPres_ + 1) % 4;
    else if (k.enter && selAddr_ != ADDR_BROADCAST) {
      ctx.pendingPeer = selAddr_;     // hand off to Courier as a DM target
      ctx.navRequest = "CR";
    }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    uint32_t now = millis();

    size_t n = ctx.nodes.size();

    g.setTextColor(presColor(ctx.presence), theme::BG);
    char hd[24];
    std::snprintf(hd, sizeof(hd), "you: %s", presName(ctx.presence));
    g.drawString(hd, 6, ui::BODY_Y + 2);

    if (n == 0) {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("(no peers heard yet)", 6, ui::BODY_Y + 18);
      ui::footer(g);
      return;
    }

    // worst-first: lowest known battery on top (unknown sinks), older breaks ties.
    uint8_t order[NodeTable::CAP];
    for (size_t i = 0; i < n; i++) order[i] = (uint8_t)i;
    for (size_t i = 1; i < n; i++) {
      uint8_t v = order[i];
      size_t j = i;
      while (j > 0 && worse(ctx.nodes.at(v), ctx.nodes.at(order[j - 1]))) { order[j] = order[j - 1]; j--; }
      order[j] = v;
    }

    // keep the selected row inside the visible window (list can exceed the screen)
    const int visRows = 9;
    if (sel_ >= (int)n) sel_ = (int)n - 1;
    if (sel_ < 0) sel_ = 0;
    if (sel_ < first_) first_ = sel_;
    if (sel_ >= first_ + visRows) first_ = sel_ - visRows + 1;
    if (first_ > (int)n - visRows) first_ = (int)n - visRows;
    if (first_ < 0) first_ = 0;

    char rt[10];
    if ((int)n > visRows) std::snprintf(rt, sizeof(rt), "%d/%u", sel_ + 1, (unsigned)n);
    else std::snprintf(rt, sizeof(rt), "p Enter");
    g.setTextColor(theme::MUTED, theme::BG);
    ui::textRight(g, ui::SCREEN_W - 4, ui::BODY_Y + 2, rt);

    selAddr_ = ADDR_BROADCAST;
    int y = ui::BODY_Y + 14;
    for (int i = first_; i < (int)n && i < first_ + visRows; i++) {
      const Node& nd = ctx.nodes.at(order[i]);
      bool s = (i == sel_);
      if (s) selAddr_ = nd.addr;
      char tmp[14];
      const char* lbl = ctx.roster.label(nd.addr, tmp, sizeof(tmp));
      char b[6];
      if (nd.hasHealth) std::snprintf(b, sizeof(b), "%d%%", nd.battPct);
      else std::snprintf(b, sizeof(b), "?");
      unsigned long age = (now - nd.lastHeard) / 1000;
      char row[44];
      std::snprintf(row, sizeof(row), "%s%-7s %-4s %c %4d %3lus",
                    s ? ">" : " ", lbl, b, presLetter(nd.presence), nd.rssi, age);
      uint16_t col = s ? theme::ACCENT
                   : (nd.hasHealth && nd.battPct < 20) ? theme::CRIT
                   : (age > 120) ? theme::MUTED : theme::TEXT;
      g.setTextColor(col, theme::BG);
      g.drawString(row, 4, y);
      y += 10;
    }
    ui::footer(g);
  }

private:
  int sel_ = 0;
  int first_ = 0;
  uint8_t userPres_ = PRES_AVAIL;
  uint16_t selAddr_ = ADDR_BROADCAST;

  static bool worse(const Node& a, const Node& b) {
    uint16_t ka = a.hasHealth ? a.battPct : 101;
    uint16_t kb = b.hasHealth ? b.battPct : 101;
    if (ka != kb) return ka < kb;              // lower battery first
    return a.lastHeard < b.lastHeard;          // older first
  }
  static const char* presName(uint8_t p) {
    switch (p) { case PRES_AVAIL: return "AVAILABLE"; case PRES_BUSY: return "BUSY";
                 case PRES_ENROUTE: return "EN-ROUTE"; default: return "RESTING"; }
  }
  static char presLetter(uint8_t p) {
    switch (p) { case PRES_AVAIL: return 'A'; case PRES_BUSY: return 'B';
                 case PRES_ENROUTE: return 'E'; default: return 'R'; }
  }
  static uint16_t presColor(uint8_t p) {
    switch (p) { case PRES_AVAIL: return theme::GOOD; case PRES_BUSY: return theme::WARN;
                 case PRES_ENROUTE: return theme::LOC; default: return theme::MUTED; }
  }
};

} // namespace ls
