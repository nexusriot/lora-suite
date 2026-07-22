#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../proto/payloads.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Streams the Adv's BMI270 motion + battery as TELEMETRY; a paired unit shows
// the incoming values. Enter toggles transmit. Template for any sensor feed.
class Telemetry : public App {
public:
  const char* name() const override { return "Telemetry"; }
  const char* callsign() const override { return "TLM"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // waveform
    g.drawLine(x + 2, y + 14, x + 6, y + 6, c);
    g.drawLine(x + 6, y + 6, x + 10, y + 15, c);
    g.drawLine(x + 10, y + 15, x + 14, y + 9, c);
    g.drawLine(x + 14, y + 9, x + 18, y + 12, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.enter) tx_ = !tx_;
    else if (k.up && rateS_ < 30) rateS_++;
    else if (k.down && rateS_ > 1) rateS_--;
  }

  void update() override {
    float ax = 0, ay = 0, az = 0;
    if (M5.Imu.getAccel(&ax, &ay, &az)) { ax_ = ax; ay_ = ay; az_ = az; }

    if (!tx_) return;
    uint32_t now = millis();
    if (now - last_ < (uint32_t)rateS_ * 1000) return;
    last_ = now;
    ls::Telemetry t;
    t.ax = (int16_t)(ax * 1000); t.ay = (int16_t)(ay * 1000); t.az = (int16_t)(az * 1000);
    t.battPct = (uint8_t)M5.Power.getBatteryLevel();
    float tmp = 0; M5.Imu.getTemp(&tmp);
    t.tempC = (int8_t)tmp;
    Frame f;
    f.type = MSG_TELEMETRY; f.flags = FLAG_MESH;
    uint8_t buf[TELEMETRY_LEN];
    packTelemetry(t, buf, sizeof(buf));
    f.setPayload(buf, TELEMETRY_LEN);
    if (netSend(f)) sent_++;
  }

  void onPacket(const Frame& f, const RxMeta& m) override {
    if (f.type != MSG_TELEMETRY) return;
    if (unpackTelemetry(f.payload, f.len, rx_)) { rxFrom_ = f.src; hasRx_ = true; }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 4;
    char s[40];

    g.setTextColor(tx_ ? theme::GOOD : theme::MUTED, theme::BG);
    std::snprintf(s, sizeof(s), "tx %s  every %ds  sent %lu",
                  tx_ ? "ON" : "off", rateS_, (unsigned long)sent_);
    g.drawString(s, 6, y); y += 14;

    g.setTextColor(theme::TEXT, theme::BG);
    std::snprintf(s, sizeof(s), "self a %+.2f %+.2f %+.2f g", ax_, ay_, az_);
    g.drawString(s, 6, y); y += 11;
    std::snprintf(s, sizeof(s), "batt %d%%", M5.Power.getBatteryLevel());
    g.drawString(s, 6, y); y += 14;

    g.setTextColor(theme::LOC, theme::BG);
    if (hasRx_) {
      std::snprintf(s, sizeof(s), "rx %04X a %d %d %d mg", rxFrom_, rx_.ax, rx_.ay, rx_.az);
      g.drawString(s, 6, y); y += 11;
      std::snprintf(s, sizeof(s), "   batt %d%%  %dC", rx_.battPct, rx_.tempC);
      g.drawString(s, 6, y);
    } else {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("(no telemetry received)", 6, y);
    }
    ui::footer(g);
  }

private:
  bool tx_ = false;
  int rateS_ = 3;
  uint32_t last_ = 0, sent_ = 0;
  float ax_ = 0, ay_ = 0, az_ = 0;
  bool hasRx_ = false;
  uint16_t rxFrom_ = 0;
  ls::Telemetry rx_;
};

} // namespace ls
