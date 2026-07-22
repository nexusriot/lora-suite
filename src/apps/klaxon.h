#pragma once
#include <M5Unified.h>
#include <cstdio>
#include <cstring>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// LoRa pager. Pick a canned alert and Enter to broadcast it; an incoming ALERT
// flashes the screen and beeps the 1 W speaker until acknowledged.
class Klaxon : public App {
public:
  const char* name() const override { return "Klaxon"; }
  const char* callsign() const override { return "ALRT"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // bell
    g.fillTriangle(x + 10, y + 3, x + 4, y + 14, x + 16, y + 14, c);
    g.fillRoundRect(x + 3, y + 13, 14, 3, 1, c);
    g.fillCircle(x + 10, y + 18, 2, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.right) sel_ = (sel_ + 1) % NCODES;
    else if (k.left) sel_ = (sel_ + NCODES - 1) % NCODES;
    else if (k.enter) { Frame f = makeAlert(sel_, CODES[sel_]); netSend(f, true); }
    else if (k.ch == ' ') { alarm_ = false; M5.Speaker.stop(); }
  }

  void onPacket(const Frame& f, const RxMeta& m) override {
    if (f.type != MSG_ALERT || f.len < 1) return;
    inCode_ = f.payload[0];
    uint8_t n = f.len - 1 < 15 ? f.len - 1 : 15;
    memcpy(inLabel_, f.payload + 1, n); inLabel_[n] = 0;
    inFrom_ = f.src;
    alarm_ = true;
    alarmStart_ = millis();
    M5.Speaker.tone(2200, 400);
  }

  void update() override {
    if (alarm_ && (millis() / 300) % 2 == 0) M5.Speaker.tone(2200, 120);
  }

  void draw(M5Canvas& g) override {
    bool flash = alarm_ && (millis() / 250) % 2 == 0;
    g.fillScreen(flash ? theme::CRIT : theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("send alert:", 6, ui::BODY_Y + 2);
    int x = 6, y = ui::BODY_Y + 14;
    for (int i = 0; i < NCODES; i++) {
      bool s = i == sel_;
      int w = 44;
      g.drawRect(x, y, w, 18, s ? theme::ACCENT : theme::LINE);
      g.setTextColor(s ? theme::ACCENT : theme::TEXT, theme::BG);
      g.drawString(CODES[i], x + 5, y + 5);
      x += w + 4;
      if (x + w > ui::SCREEN_W) { x = 6; y += 22; }
    }

    if (inCode_ != 0xFF) {
      char s[40];
      std::snprintf(s, sizeof(s), "RX %04X: %s", inFrom_, inLabel_);
      g.setTextColor(theme::WARN, theme::BG);
      g.drawString(s, 6, ui::SCREEN_H - ui::FOOTER_H - 12);
    }
    ui::footer(g);
  }

private:
  static const int NCODES = 6;
  static constexpr const char* CODES[NCODES] = {"SOS", "RALLY", "MOVE", "HOLD", "OK", "PING"};
  int sel_ = 0;
  bool alarm_ = false;
  uint32_t alarmStart_ = 0;
  uint8_t inCode_ = 0xFF, inFrom_ = 0;
  char inLabel_[16] = {0};
};

constexpr const char* Klaxon::CODES[];

} // namespace ls
