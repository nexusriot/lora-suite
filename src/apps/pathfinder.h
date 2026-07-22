#pragma once
#include <M5Unified.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../services/gps_service.h"
#include "../proto/geo.h"
#include "../proto/payloads.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Capture/share waypoints and home to a target (waypoint or heard node) with a
// bearing arrow + range + closing speed. Heading comes from GPS course-over-
// ground; it is only valid while moving, so a "heading stale" cue shows at rest.
class Pathfinder : public App {
public:
  const char* name() const override { return "Pathfinder"; }
  const char* callsign() const override { return "PF"; }
  Cat category() const override { return Cat::Location; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // map pin
    g.fillCircle(x + 10, y + 7, 6, c);
    g.fillTriangle(x + 4, y + 9, x + 16, y + 9, x + 10, y + 19, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.ch == 'w') capture();
    else if (k.down) sel_++;
    else if (k.up && sel_ > 0) sel_--;
    else if (k.enter) shareSelected();
  }

  void onPacket(const Frame& f, const RxMeta&) override {
    if (f.type != MSG_WAYPOINT) return;
    Waypoint w;
    if (unpackWaypoint(f.payload, f.len, w) && count_ < MAXW) wps_[count_++] = w;
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    GpsService* gp = ctx.gps;
    bool fix = gp && gp->hasFix();

    if (count_ == 0) {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("press w to mark a waypoint", 6, ui::BODY_Y + 20);
      ui::footer(g);
      return;
    }
    if (sel_ >= count_) sel_ = count_ - 1;
    const Waypoint& t = wps_[sel_];

    char s[40];
    std::snprintf(s, sizeof(s), "target %u/%u  %s", (unsigned)(sel_ + 1), (unsigned)count_, t.label);
    g.setTextColor(theme::LOC, theme::BG);
    g.drawString(s, 6, ui::BODY_Y + 2);

    const int cx = 60, cy = ui::BODY_Y + 46, R = 30;
    g.drawCircle(cx, cy, R, theme::LINE);

    if (fix) {
      double d = haversineMeters(gp->lat(), gp->lon(), t.lat, t.lon);
      double brg = bearingDeg(gp->lat(), gp->lon(), t.lat, t.lon);
      double cog = gp->courseDeg();
      bool moving = gp->speedKmh() > 2.0;
      double rel = (moving ? brg - cog : brg) * M_PI / 180.0;
      int ax = cx + (int)(R * std::sin(rel));
      int ay = cy - (int)(R * std::cos(rel));
      g.drawLine(cx, cy, ax, ay, theme::ACCENT);
      g.fillCircle(ax, ay, 3, theme::ACCENT);

      int tx = 100, ty = ui::BODY_Y + 20;
      g.setTextColor(theme::TEXT, theme::BG);
      std::snprintf(s, sizeof(s), "range %d m", (int)d); g.drawString(s, tx, ty); ty += 12;
      std::snprintf(s, sizeof(s), "brg %03d", (int)brg); g.drawString(s, tx, ty); ty += 12;
      std::snprintf(s, sizeof(s), "spd %d kmh", (int)gp->speedKmh()); g.drawString(s, tx, ty); ty += 12;
      g.setTextColor(moving ? theme::MUTED : theme::WARN, theme::BG);
      g.drawString(moving ? "(heading live)" : "(heading stale)", tx, ty);
    } else {
      g.setTextColor(theme::WARN, theme::BG);
      g.drawString("no GPS fix", 100, ui::BODY_Y + 24);
    }

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("Enter share  w mark", 6, ui::SCREEN_H - ui::FOOTER_H - 12);
    ui::footer(g);
  }

private:
  static const int MAXW = 16;
  Waypoint wps_[MAXW];
  int count_ = 0;
  int sel_ = 0;

  void capture() {
    GpsService* gp = ctx.gps;
    if (!gp || !gp->hasFix() || count_ >= MAXW) return;
    Waypoint& w = wps_[count_];
    w.lat = gp->lat(); w.lon = gp->lon(); w.altM = (int16_t)gp->altM();
    std::snprintf(w.label, sizeof(w.label), "WP%d", count_ + 1);
    sel_ = count_;
    count_++;
  }

  void shareSelected() {
    if (sel_ >= count_) return;
    Frame f = makeWaypoint(wps_[sel_]);
    netSend(f);
  }
};

} // namespace ls
