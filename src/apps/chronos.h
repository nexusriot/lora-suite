#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../services/clock.h"
#include "../services/gps_service.h"
#include "../proto/solar.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Mesh time-sync + darkness planner. When this node holds GPS time it broadcasts
// a small TIMESYNC so RTC-less nodes get coherent wall-clock (adoption happens in
// main's RX handler). Shows the time source, UTC, and today's daylight length.
class Chronos : public App {
public:
  const char* name() const override { return "Chronos"; }
  const char* callsign() const override { return "TIME"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // clock
    g.drawCircle(x + 10, y + 10, 9, c);
    g.drawLine(x + 10, y + 10, x + 10, y + 4, c);
    g.drawLine(x + 10, y + 10, x + 15, y + 10, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.enter) {
      broadcast();   // manual sync push
    } else if (k.ch == 'n' || k.ch == 'N') {
      // Blocks up to ~13 s while WiFi associates + SNTP replies; the UI can't
      // repaint mid-call, so we just latch the outcome for the next frame.
      ntp_ = ntpSyncViaWifi() ? 1 : 2;   // 1 ok, 2 failed
    }
  }

  void background() override {
    uint32_t now = millis();
    if (now - last_ < PERIOD_MS) return;
    last_ = now;
    Clock* c = ctx.clock;
    if (c && c->source() >= 2 && c->hasUtc()) broadcast();
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    Clock* c = ctx.clock;
    int y = ui::BODY_Y + 6;
    char s[40];

    const char* src = "none";
    uint8_t sv = c ? c->source() : 0;
    if (sv == 1) src = "mesh"; else if (sv == 2) src = "GPS"; else if (sv == 3) src = "NTP";
    g.setTextColor(sv ? theme::GOOD : theme::WARN, theme::BG);
    std::snprintf(s, sizeof(s), "time source: %s", src);
    g.drawString(s, 6, y); y += 16;

    char clk[9];
    if (c) c->hms(clk); else std::snprintf(clk, sizeof(clk), "--:--:--");
    g.setTextColor(theme::TEXT, theme::BG);
    g.setTextSize(2);
    g.drawString(clk, 6, y); y += 22;
    g.setTextSize(1);
    g.drawString("UTC", 6, y); y += 14;

    GpsService* gp = ctx.gps;
    if (c && c->hasUtc() && gp && gp->hasFix()) {
      int doy = dayOfYear(c->utc());
      double dl = daylightHours(gp->lat(), doy);
      std::snprintf(s, sizeof(s), "daylight %.1f h  (day %d)", dl, doy);
      g.setTextColor(theme::LOC, theme::BG);
      g.drawString(s, 6, y);
    } else {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("almanac needs a GPS fix", 6, y);
    }

    if (ntp_) {
      g.setTextColor(ntp_ == 1 ? theme::GOOD : theme::WARN, theme::BG);
      g.drawString(ntp_ == 1 ? "NTP sync ok" : "NTP sync failed", 6, y + 14);
    }

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("Enter: push  n: NTP/WiFi", 6, ui::SCREEN_H - ui::FOOTER_H - 12);
    ui::footer(g);
  }

private:
  static const uint32_t PERIOD_MS = 300000;   // 5 min
  uint32_t last_ = 0;
  uint8_t  ntp_ = 0;   // 0 idle, 1 last sync ok, 2 last sync failed

  void broadcast() {
    Clock* c = ctx.clock;
    if (!c || !c->hasUtc()) return;
    Frame f = makeTimeSync(c->utc(), c->source());
    netSend(f);
  }

  static bool leap(int y) { return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)); }
  static int dayOfYear(uint32_t utc) {
    long days = (long)(utc / 86400);
    int y = 1970;
    while (true) { int dy = leap(y) ? 366 : 365; if (days < dy) break; days -= dy; y++; }
    return (int)days + 1;
  }
};

} // namespace ls
