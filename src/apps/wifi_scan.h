#pragma once
#include <M5Unified.h>
#include <WiFi.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// WiFi access-point scanner — lights up the otherwise-unused 2.4 GHz radio (the
// LoRa link is a separate SX1262 on SPI, so the two don't clash). Lists SSID,
// signal, channel and whether the AP is secured. Async scan so the UI stays live.
class WifiScan : public App {
public:
  const char* name() const override { return "WiFi"; }
  const char* callsign() const override { return "WIFI"; }
  Cat category() const override { return Cat::RF; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // signal bars
    g.fillRect(x + 2, y + 14, 2, 4, c);
    g.fillRect(x + 6, y + 11, 2, 7, c);
    g.fillRect(x + 10, y + 8, 2, 10, c);
    g.fillRect(x + 14, y + 5, 2, 13, c);
  }

  void onEnter() override { startScan(); }
  void onExit() override { WiFi.scanDelete(); WiFi.mode(WIFI_OFF); }

  void onKey(const KeyEvent& k) override {
    if (k.ch == 'r') startScan();
    else if (k.up && sel_ > 0) sel_--;
    else if (k.down && sel_ + 1 < n_) sel_++;
  }

  void update() override {
    if (!scanning_) return;
    int r = WiFi.scanComplete();
    if (r >= 0) { n_ = r; scanning_ = false; }
    else if (r == WIFI_SCAN_FAILED) { n_ = 0; scanning_ = false; }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 2;
    char s[44];

    if (scanning_) {
      g.setTextColor(theme::WARN, theme::BG);
      g.drawString("scanning...", 6, y);
      ui::footer(g);
      return;
    }

    g.setTextColor(theme::MUTED, theme::BG);
    std::snprintf(s, sizeof(s), "%d APs   r=rescan", n_);
    g.drawString(s, 6, y);
    y += 13;

    if (n_ == 0) {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("(none found)", 6, y);
      ui::footer(g);
      return;
    }

    if (sel_ >= n_) sel_ = n_ - 1;
    if (sel_ < first_) first_ = sel_;
    if (sel_ >= first_ + VIS) first_ = sel_ - VIS + 1;
    if (first_ > n_ - VIS) first_ = n_ - VIS;
    if (first_ < 0) first_ = 0;

    for (int i = first_; i < n_ && i < first_ + VIS; i++) {
      bool sel = (i == sel_);
      char ssid[17];
      std::snprintf(ssid, sizeof(ssid), "%s", WiFi.SSID(i).c_str());
      if (!ssid[0]) std::snprintf(ssid, sizeof(ssid), "(hidden)");
      std::snprintf(s, sizeof(s), "%c%-16s %4d c%-2d %c",
                    sel ? '>' : ' ', ssid, (int)WiFi.RSSI(i), WiFi.channel(i),
                    WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? ' ' : '*');
      g.setTextColor(sel ? theme::ACCENT : theme::TEXT, theme::BG);
      g.drawString(s, 4, y);
      y += 10;
    }
    ui::footer(g);
  }

private:
  static const int VIS = 8;
  bool scanning_ = false;
  int n_ = 0, sel_ = 0, first_ = 0;

  void startScan() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.scanDelete();
    WiFi.scanNetworks(true);   // async — poll in update()
    scanning_ = true;
    n_ = sel_ = first_ = 0;
  }
};

} // namespace ls
