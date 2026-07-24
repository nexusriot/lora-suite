#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/gps_service.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// GPS status: fix state, position, altitude, speed/course, satellites, HDOP and
// UTC time straight from the ATGM336H (the module's GPS is also the fleet's clock
// source, since the Adv has no RTC).
class Gps : public App {
public:
  const char* name() const override { return "GPS"; }
  const char* callsign() const override { return "GPS"; }
  Cat category() const override { return Cat::Location; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // reticle
    g.drawCircle(x + 10, y + 10, 7, c);
    g.fillCircle(x + 10, y + 10, 2, c);
    g.drawFastVLine(x + 10, y + 1, 4, c);
    g.drawFastVLine(x + 10, y + 15, 4, c);
    g.drawFastHLine(x + 1, y + 10, 4, c);
    g.drawFastHLine(x + 15, y + 10, 4, c);
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    GpsService* gp = ctx.gps;
    int y = ui::BODY_Y + 2;
    char s[40];

    if (!gp) {
      g.setTextColor(theme::CRIT, theme::BG);
      g.drawString("no GPS service", 6, y);
      ui::footer(g);
      return;
    }

    bool fix = gp->hasFix();
    g.setTextColor(fix ? theme::GOOD : theme::WARN, theme::BG);
    std::snprintf(s, sizeof(s), "%s   %u sats", fix ? "FIX" : "NO FIX", gp->sats());
    g.drawString(s, 6, y); y += 15;

    g.setTextColor(theme::TEXT, theme::BG);
    std::snprintf(s, sizeof(s), "lat %.6f", gp->lat()); g.drawString(s, 6, y); y += 11;
    std::snprintf(s, sizeof(s), "lon %.6f", gp->lon()); g.drawString(s, 6, y); y += 11;
    std::snprintf(s, sizeof(s), "alt %dm   hdop %.1f", (int)gp->altM(), gp->hdop());
    g.drawString(s, 6, y); y += 11;
    std::snprintf(s, sizeof(s), "spd %.1f km/h  crs %03d", gp->speedKmh(), (int)gp->courseDeg());
    g.drawString(s, 6, y); y += 13;

    g.setTextColor(theme::LOC, theme::BG);
    if (gp->hasTime()) {
      uint32_t sod = gp->unixTime() % 86400u;
      std::snprintf(s, sizeof(s), "UTC %02u:%02u:%02u", sod / 3600, (sod / 60) % 60, sod % 60);
    } else {
      std::snprintf(s, sizeof(s), "UTC --:--:--");
    }
    g.drawString(s, 6, y);
    ui::footer(g);
  }
};

} // namespace ls
