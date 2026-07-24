#pragma once
#include <M5Unified.h>
#include <cstdio>
#include <cmath>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/gps_service.h"
#include "../proto/geo.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Polar plot of nodes with a known position, placed by bearing + distance from
// our own fix. Up/down change the display range.
class Radar : public App {
public:
  const char* name() const override { return "Radar"; }
  const char* callsign() const override { return "RDR"; }
  Cat category() const override { return Cat::Location; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // radar + sweep + blip
    g.drawCircle(x + 10, y + 10, 9, c);
    g.drawCircle(x + 10, y + 10, 4, c);
    g.drawLine(x + 10, y + 10, x + 17, y + 4, c);
    g.fillCircle(x + 15, y + 6, 1, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.up && rangeM_ < 20000) rangeM_ *= 2;
    else if (k.down && rangeM_ > 250) rangeM_ /= 2;
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    const int cx = 150, cy = ui::BODY_Y + ui::BODY_H / 2 + 2;
    const int R = 48;
    for (int r = R; r > 0; r -= R / 3) g.drawCircle(cx, cy, r, theme::LINE);
    g.drawFastVLine(cx, cy - R, 2 * R, theme::LINE);
    g.drawFastHLine(cx - R, cy, 2 * R, theme::LINE);
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("N", cx - 2, cy - R - 9);

    GpsService* gp = ctx.gps;
    bool haveSelf = gp && gp->hasFix();
    char s[32];
    std::snprintf(s, sizeof(s), "range %dm", rangeM_);
    g.drawString(s, 6, ui::BODY_Y + 4);

    if (!haveSelf) {
      g.setTextColor(theme::WARN, theme::BG);
      g.drawString("no own fix", 6, ui::BODY_Y + 18);
    }

    int listY = ui::BODY_Y + 18;
    for (size_t i = 0; i < ctx.nodes.size(); i++) {
      const Node& n = ctx.nodes.at(i);
      if (!n.hasPos || !haveSelf) continue;
      double d = haversineMeters(gp->lat(), gp->lon(), n.lat, n.lon);
      double b = bearingDeg(gp->lat(), gp->lon(), n.lat, n.lon);
      double rr = (d > rangeM_ ? rangeM_ : d) / (double)rangeM_ * R;
      int px = cx + (int)(rr * std::sin(b * M_PI / 180.0));
      int py = cy - (int)(rr * std::cos(b * M_PI / 180.0));
      g.fillCircle(px, py, 3, ui::rssiColor(n.rssi));

      if (listY < ui::SCREEN_H - ui::FOOTER_H - 8) {
        std::snprintf(s, sizeof(s), "%04X %4dm %03d", n.addr, (int)d, (int)b);
        g.setTextColor(theme::TEXT, theme::BG);
        g.drawString(s, 6, listY);
        listY += 10;
      }
    }

    // Foreign Meshtastic nodes (Mesh app import / scan): hollow blue markers,
    // never in the peer list — they are not nodes we can talk to.
    int meshShown = 0;
    if (haveSelf) {
      for (size_t i = 0; i < ctx.mesh.size(); i++) {
        const MeshNode& mn = ctx.mesh.at(i);
        if (!mn.hasPos) continue;
        double d = haversineMeters(gp->lat(), gp->lon(), mn.lat, mn.lon);
        double b = bearingDeg(gp->lat(), gp->lon(), mn.lat, mn.lon);
        double rr = (d > rangeM_ ? rangeM_ : d) / (double)rangeM_ * R;
        int px = cx + (int)(rr * std::sin(b * M_PI / 180.0));
        int py = cy - (int)(rr * std::cos(b * M_PI / 180.0));
        g.drawRect(px - 2, py - 2, 5, 5, theme::LOC);
        meshShown++;
      }
    }
    if (meshShown) {
      std::snprintf(s, sizeof(s), "mesh %d", meshShown);
      g.setTextColor(theme::LOC, theme::BG);
      ui::textRight(g, ui::SCREEN_W - 4, ui::SCREEN_H - ui::FOOTER_H - 9, s);
    }
    ui::footer(g);
  }

private:
  int rangeM_ = 2000;
};

} // namespace ls
