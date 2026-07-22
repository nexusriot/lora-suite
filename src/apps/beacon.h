#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../services/gps_service.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Periodic GPS position beacon. Interval auto-extends if the duty budget is low.
// Enter toggles auto, Space bursts now, up/down change the interval.
class Beacon : public App {
public:
  const char* name() const override { return "Beacon"; }
  const char* callsign() const override { return "BCN"; }
  Cat category() const override { return Cat::Location; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // broadcast ripples
    g.fillCircle(x + 10, y + 10, 3, c);
    g.drawCircle(x + 10, y + 10, 6, c);
    g.drawCircle(x + 10, y + 10, 9, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.enter) on_ = !on_;
    else if (k.ch == ' ') burst();
    else if (k.up && intervalS_ < 600) intervalS_ += 15;
    else if (k.down && intervalS_ > 15) intervalS_ -= 15;
  }

  void update() override {
    if (!on_) return;
    uint32_t now = millis();
    if (now - lastTx_ >= (uint32_t)intervalS_ * 1000) burst();
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    GpsService* gp = ctx.gps;

    char s[40];
    int y = ui::BODY_Y + 4;
    g.setTextColor(on_ ? theme::GOOD : theme::MUTED, theme::BG);
    std::snprintf(s, sizeof(s), "auto %s  every %ds", on_ ? "ON" : "off", intervalS_);
    g.drawString(s, 6, y); y += 14;

    g.setTextColor(theme::TEXT, theme::BG);
    if (gp && gp->hasFix()) {
      std::snprintf(s, sizeof(s), "lat %.5f", gp->lat()); g.drawString(s, 6, y); y += 11;
      std::snprintf(s, sizeof(s), "lon %.5f", gp->lon()); g.drawString(s, 6, y); y += 11;
      std::snprintf(s, sizeof(s), "alt %dm  spd %dkmh", (int)gp->altM(), (int)gp->speedKmh());
      g.drawString(s, 6, y); y += 11;
      std::snprintf(s, sizeof(s), "sats %d  hdop %.1f", gp->sats(), gp->hdop());
      g.drawString(s, 6, y); y += 11;
    } else {
      g.setTextColor(theme::WARN, theme::BG);
      g.drawString("acquiring GPS fix...", 6, y); y += 12;
    }

    uint32_t since = (millis() - lastTx_) / 1000;
    int nxt = intervalS_ - (int)since;
    std::snprintf(s, sizeof(s), "sent %lu  next %ds", (unsigned long)sent_, nxt < 0 ? 0 : nxt);
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString(s, 6, ui::SCREEN_H - ui::FOOTER_H - 12);
    ui::footer(g);
  }

private:
  bool on_ = false;
  int intervalS_ = 60;
  uint32_t lastTx_ = 0;
  uint32_t sent_ = 0;

  void burst() {
    lastTx_ = millis();
    GpsService* gp = ctx.gps;
    if (!gp || !gp->hasFix()) return;
    Position p;
    p.lat = gp->lat(); p.lon = gp->lon();
    p.altM = (int16_t)gp->altM();
    p.speedKmh = (uint8_t)gp->speedKmh();
    p.course2 = (uint8_t)(gp->courseDeg() / 2);
    Frame f = makeBeacon(p);
    if (netSend(f)) sent_++;
  }
};

} // namespace ls
