#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/gps_service.h"
#include "../services/storage.h"
#include "../services/clock.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Logs own track (and heard nodes) to microSD as CSV, GPS-timestamped. Enter
// starts/stops; the file name is stamped from GPS date + uptime.
class Breadcrumb : public App {
public:
  const char* name() const override { return "Breadcrumb"; }
  const char* callsign() const override { return "TRK"; }
  Cat category() const override { return Cat::Location; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // trail of dots
    g.fillCircle(x + 4, y + 16, 2, c);
    g.fillCircle(x + 8, y + 11, 2, c);
    g.fillCircle(x + 12, y + 8, 2, c);
    g.fillCircle(x + 16, y + 4, 2, c);
  }

  void onEnter() override {
    if (ctx.store && !ctx.store->sdReady()) sd_ = ctx.store->sdBegin();
    else sd_ = ctx.store && ctx.store->sdReady();
  }

  void onKey(const KeyEvent& k) override {
    if (k.enter) logging_ ? stop() : start();
    else if (k.ch == 'm') marks_++;   // drop a marker on the next point
  }

  void update() override {
    if (!logging_) return;
    uint32_t now = millis();
    if (now - lastPt_ < 3000) return;
    GpsService* gp = ctx.gps;
    if (!gp || !gp->hasFix()) return;
    lastPt_ = now;
    char clk[9]; ctx.clock ? ctx.clock->hms(clk) : (void)std::snprintf(clk, 9, "--:--:--");
    char line[80];
    std::snprintf(line, sizeof(line), "%s,%.6f,%.6f,%d,%d,%d",
                  clk, gp->lat(), gp->lon(), (int)gp->altM(), (int)gp->speedKmh(), marks_);
    if (ctx.store && ctx.store->appendLine(path_, line)) points_++;
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 4;
    char s[48];

    g.setTextColor(sd_ ? theme::GOOD : theme::CRIT, theme::BG);
    g.drawString(sd_ ? "microSD ready" : "no microSD", 6, y); y += 14;

    g.setTextColor(logging_ ? theme::GOOD : theme::MUTED, theme::BG);
    std::snprintf(s, sizeof(s), "logging %s", logging_ ? "ON" : "off");
    g.drawString(s, 6, y); y += 12;

    g.setTextColor(theme::TEXT, theme::BG);
    if (logging_) { g.drawString(path_, 6, y); y += 11; }
    std::snprintf(s, sizeof(s), "points %lu  marks %d", (unsigned long)points_, marks_);
    g.drawString(s, 6, y); y += 11;

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("Enter start/stop   m mark", 6, ui::SCREEN_H - ui::FOOTER_H - 12);
    ui::footer(g);
  }

private:
  bool sd_ = false, logging_ = false;
  uint32_t lastPt_ = 0, points_ = 0;
  int marks_ = 0;
  char path_[24] = {0};

  void start() {
    if (!sd_) return;
    char ymd[9]; ctx.clock ? ctx.clock->ymd(ymd) : (void)std::snprintf(ymd, 9, "00000000");
    std::snprintf(path_, sizeof(path_), "/trk_%s_%lu.csv", ymd, (unsigned long)(millis() / 1000));
    ctx.store->appendLine(path_, "time,lat,lon,alt,spd,mark");
    points_ = 0; logging_ = true;
  }
  void stop() { logging_ = false; }
};

} // namespace ls
