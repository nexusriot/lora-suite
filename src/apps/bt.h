#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/ble_bridge.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Bluetooth settings: enable/disable the BLE companion bridge and show its state.
// Pair the phone app while this is ON. BLE and the WiFi scanner share the 2.4 GHz
// radio, so don't run a WiFi scan while connected.
class Bluetooth : public App {
public:
  const char* name() const override { return "Bluetooth"; }
  const char* callsign() const override { return "BT"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // BT rune
    int cx = x + 9;
    g.drawLine(cx, y + 2, cx, y + 18, c);
    g.drawLine(cx, y + 2, cx + 5, y + 7, c);
    g.drawLine(cx + 5, y + 7, cx - 4, y + 13, c);
    g.drawLine(cx, y + 18, cx + 5, y + 13, c);
    g.drawLine(cx + 5, y + 13, cx - 4, y + 7, c);
  }

  void onKey(const KeyEvent& k) override {
    if (!k.enter) return;
    if (ble::enabled()) {
      ble::end();
    } else {
      char nm[24];
      std::snprintf(nm, sizeof(nm), "LoRa-%s", ctx.callName);
      ble::begin(nm);
    }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 4;

    bool on = ble::enabled();
    g.setTextColor(on ? theme::GOOD : theme::MUTED, theme::BG);
    g.setTextSize(2);
    g.drawString(on ? "BT ON" : "BT OFF", 6, y);
    g.setTextSize(1);
    y += 26;

    char s[40];
    if (on) {
      bool conn = ble::connected();
      g.setTextColor(conn ? theme::ACCENT : theme::WARN, theme::BG);
      g.drawString(conn ? "connected" : "advertising...", 6, y);
      y += 13;
      g.setTextColor(theme::TEXT, theme::BG);
      std::snprintf(s, sizeof(s), "name: LoRa-%s", ctx.callName);
      g.drawString(s, 6, y);
      y += 12;
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("pair from the phone app", 6, y);
    } else {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("BLE companion bridge", 6, y);
      y += 12;
      g.drawString("for the phone app", 6, y);
    }

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("Enter=toggle", 6, ui::SCREEN_H - ui::FOOTER_H - 10);
    ui::footer(g);
  }
};

} // namespace ls
