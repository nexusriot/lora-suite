#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Uplink control panel. When ON, every frame heard on our channel is re-encoded
// to its on-air bytes and streamed out the USB serial console as one JSON line
// (see main.cpp emitGateway) — piped into `tools/lorakit dissect` or a fork to map
// the fleet. Runs in the background (via the RX chokepoint), so you can leave it
// on and use other apps; toggling it is all this screen does.
class Gateway : public App {
public:
  const char* name() const override { return "Gateway"; }
  const char* callsign() const override { return "GW"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // uplink tower
    g.drawLine(x + 10, y + 18, x + 10, y + 8, c);
    g.drawLine(x + 10, y + 8, x + 5, y + 3, c);
    g.drawLine(x + 10, y + 8, x + 15, y + 3, c);
    g.fillCircle(x + 10, y + 8, 2, c);
    g.drawCircle(x + 10, y + 8, 6, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.enter) ctx.gatewayOn = !ctx.gatewayOn;
    else if (k.ch == 'c') ctx.gatewaySent = 0;
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    int y = ui::BODY_Y + 4;
    g.setTextColor(ctx.gatewayOn ? theme::GOOD : theme::MUTED, theme::BG);
    g.setTextSize(2);
    g.drawString(ctx.gatewayOn ? "UPLINK ON" : "UPLINK OFF", 6, y);
    g.setTextSize(1);
    y += 24;

    char s[40];
    g.setTextColor(theme::TEXT, theme::BG);
    std::snprintf(s, sizeof(s), "sent %lu   rx %lu", (unsigned long)ctx.gatewaySent, (unsigned long)ctx.rxCount);
    g.drawString(s, 6, y); y += 13;

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("USB serial JSON", 6, y); y += 11;
    g.drawString("-> lorakit dissect", 6, y); y += 14;
    g.drawString("Enter=toggle  c=clear", 6, y);

    ui::footer(g);
  }
};

} // namespace ls
