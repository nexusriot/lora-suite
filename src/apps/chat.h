#pragma once
#include <M5Unified.h>
#include <cstring>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../services/clock.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// A simple broadcast LoRa messenger, in the spirit of the Cardputer demo's LoRa
// Chat: everyone on the channel sees everyone's messages in one scrolling feed.
// Type + Enter broadcasts a TEXT to the whole channel (no addressing/ACK — that's
// Courier's job). Received text scrolls in with the sender label + RSSI.
class Chat : public App {
public:
  const char* name() const override { return "Chat"; }
  const char* callsign() const override { return "CHAT"; }
  Cat category() const override { return Cat::Comms; }
  bool consumesText() const override { return true; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // two chat bubbles
    g.drawRoundRect(x + 1, y + 2, 13, 9, 2, c);
    g.fillTriangle(x + 3, y + 10, x + 3, y + 14, x + 7, y + 10, c);
    g.fillRoundRect(x + 8, y + 8, 11, 8, 2, c);
    g.fillTriangle(x + 15, y + 15, x + 17, y + 18, x + 12, y + 15, c);
  }

  void onEnter() override { ctx.unread = 0; }

  void onKey(const KeyEvent& k) override {
    if (k.enter) { send(); return; }
    if (k.del) { if (len_) input_[--len_] = 0; return; }
    if (k.ch >= 0x20 && k.ch < 0x7f && len_ < (int)sizeof(input_) - 1) {
      input_[len_++] = k.ch;
      input_[len_] = 0;
    }
  }

  void onPacket(const Frame& f, const RxMeta& m) override {
    if (f.type != MSG_TEXT) return;
    char body[41];
    uint8_t n = f.len < 40 ? f.len : 40;
    std::memcpy(body, f.payload, n);
    body[n] = 0;
    add(f.src, body, m.rssi);
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    int y = ui::BODY_Y + 2;
    int start = count_ > ROWS ? count_ - ROWS : 0;
    for (int i = start; i < count_; i++) {
      const Msg& mm = log_[i % RING];
      char line[48];
      if (mm.src == ctx.myAddr) {
        std::snprintf(line, sizeof(line), "me> %s", mm.text);
      } else {
        char tmp[14];
        const char* lbl = ctx.roster.label(mm.src, tmp, sizeof(tmp));
        std::snprintf(line, sizeof(line), "%s> %s", lbl, mm.text);
      }
      g.setTextColor(mm.src == ctx.myAddr ? theme::ACCENT : theme::TEXT, theme::BG);
      g.drawString(line, 6, y);
      y += 10;
    }

    int iy = ui::SCREEN_H - ui::FOOTER_H - 12;
    g.drawFastHLine(0, iy - 2, ui::SCREEN_W, theme::LINE);
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString(">", 6, iy);
    g.setTextColor(theme::TEXT, theme::BG);
    g.drawString(input_, 18, iy);
    ui::footer(g);
  }

private:
  struct Msg { uint16_t src; char text[41]; int16_t rssi; };
  static const int RING = 24;
  static const int ROWS = 9;
  Msg log_[RING] = {};
  int count_ = 0;
  char input_[64] = {0};
  int len_ = 0;

  void add(uint16_t src, const char* text, int16_t rssi) {
    Msg& m = log_[count_ % RING];
    m.src = src;
    std::strncpy(m.text, text, sizeof(m.text) - 1);
    m.text[sizeof(m.text) - 1] = 0;
    m.rssi = rssi;
    count_++;
  }

  void send() {
    if (!len_) return;
    Frame f = makeText(ADDR_BROADCAST, input_, false);
    if (netSend(f)) {
      add(ctx.myAddr, input_, 0);
      char t[9];
      if (ctx.clock) ctx.clock->hms(t); else t[0] = 0;
      ctx.archive.add(t, 'O', ADDR_BROADCAST, input_);
    }
    input_[0] = 0;
    len_ = 0;
  }
};

} // namespace ls
