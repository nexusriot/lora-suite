#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Mesh control panel. Forwarding itself runs in the main RX handler for every
// screen; this app shows the node table and lets you tune it. E toggles relay,
// up/down set the hop limit.
class Relay : public App {
public:
  const char* name() const override { return "Relay"; }
  const char* callsign() const override { return "RLY"; }
  Cat category() const override { return Cat::Comms; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // 3 linked nodes
    int ax = x + 3, ay = y + 4, bx = x + 16, by = y + 5, dx = x + 10, dy = y + 16;
    g.drawLine(ax, ay, dx, dy, c);
    g.drawLine(bx, by, dx, dy, c);
    g.drawLine(ax, ay, bx, by, c);
    g.fillCircle(ax, ay, 3, c);
    g.fillCircle(bx, by, 3, c);
    g.fillCircle(dx, dy, 3, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.ch == 'e' || k.ch == 'E') ctx.relayOn = !ctx.relayOn;
    else if (k.up && ctx.relayHops < 7) ctx.relayHops++;
    else if (k.down && ctx.relayHops > 1) ctx.relayHops--;
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    char s[40];
    std::snprintf(s, sizeof(s), "mesh %s  hops %u  fwd %lu",
                  ctx.relayOn ? "ON" : "off", ctx.relayHops,
                  (unsigned long)ctx.relayForwarded);
    g.setTextColor(ctx.relayOn ? theme::GOOD : theme::MUTED, theme::BG);
    g.drawString(s, 6, ui::BODY_Y + 2);

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("nodes  rssi  age", 6, ui::BODY_Y + 14);
    int y = ui::BODY_Y + 24;
    uint32_t now = millis();
    for (size_t i = 0; i < ctx.nodes.size() && y < ui::SCREEN_H - ui::FOOTER_H - 8; i++) {
      const Node& n = ctx.nodes.at(i);
      char row[40];
      std::snprintf(row, sizeof(row), "%04X %-6s %4d %3lus", n.addr,
                    n.name[0] ? n.name : "-", n.rssi,
                    (unsigned long)((now - n.lastHeard) / 1000));
      g.setTextColor(ui::rssiColor(n.rssi), theme::BG);
      g.drawString(row, 6, y);
      y += 10;
    }
    if (ctx.nodes.size() == 0) {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("(no nodes heard yet)", 6, ui::BODY_Y + 24);
    }
    ui::footer(g);
  }
};

} // namespace ls
