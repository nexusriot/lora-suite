#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Promiscuous frame monitor: every decoded frame on air (any channel) lands
// here with type, addresses, length and RSSI/SNR.
class Monitor : public App {
public:
  const char* name() const override { return "Monitor"; }
  const char* callsign() const override { return "MON"; }
  Cat category() const override { return Cat::RF; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // frame log rows
    for (int r = 0; r < 3; r++) {
      int yy = y + 4 + r * 5;
      g.fillRect(x + 2, yy, 3, 3, c);
      g.drawFastHLine(x + 7, yy + 1, 10, c);
    }
  }

  void onKey(const KeyEvent& k) override {
    if (k.ch == ' ') frozen_ = !frozen_;
  }

  void onRawPacket(const Frame& f, const RxMeta& m) override {
    if (frozen_) return;
    Row& r = rows_[count_ % RING];
    r.type = f.type; r.src = f.src; r.dst = f.dst;
    r.len = f.len; r.chan = f.chan; r.enc = (f.flags & FLAG_ENCRYPTED) != 0;
    r.rssi = m.rssi; r.snr = m.snr;
    count_++;
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    g.setTextColor(frozen_ ? theme::WARN : theme::MUTED, theme::BG);
    char hd[40];
    std::snprintf(hd, sizeof(hd), "%s  %lu frames  space=freeze",
                  frozen_ ? "FROZEN" : "live", (unsigned long)count_);
    g.drawString(hd, 6, ui::BODY_Y + 2);

    int y = ui::BODY_Y + 14;
    int start = count_ > ROWS ? count_ - ROWS : 0;
    for (int i = start; i < count_; i++) {
      const Row& r = rows_[i % RING];
      char line[44];
      std::snprintf(line, sizeof(line), "%-4s %04X>%04X %db ch%02X %d%s",
                    typeName(r.type), r.src, r.dst, r.len, r.chan, r.rssi,
                    r.enc ? "*" : "");
      g.setTextColor(ui::rssiColor(r.rssi), theme::BG);
      g.drawString(line, 6, y);
      y += 10;
    }
    ui::footer(g);
  }

private:
  struct Row { uint8_t type, len, chan; bool enc; uint16_t src, dst; int16_t rssi; int8_t snr; };
  static const int RING = 24, ROWS = 9;
  Row rows_[RING] = {};
  int count_ = 0;
  bool frozen_ = false;

  static const char* typeName(uint8_t t) {
    switch (t) {
      case MSG_TEXT: return "TXT"; case MSG_ACK: return "ACK";
      case MSG_BEACON: return "BCN"; case MSG_PING: return "PING";
      case MSG_PONG: return "PONG"; case MSG_TELEMETRY: return "TLM";
      case MSG_ALERT: return "ALRT"; case MSG_NODEINFO: return "INFO";
      case MSG_FILECHUNK: return "FILE"; default: return "?";
    }
  }
};

} // namespace ls
