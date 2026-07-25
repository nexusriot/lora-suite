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

  // Already reassembled + decompressed by the shell.
  void onTextMessage(uint16_t src, const char* text, uint16_t, const RxMeta& m) override {
    add(src, text, m.rssi);
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    // Wrap long messages, filling the visible rows from the newest backwards.
    int y = ui::BODY_Y + 2;
    char rows[ROWS][COLS + 1];
    uint16_t rowSrc[ROWS];
    int nrows = 0;
    for (int i = count_ - 1; i >= 0 && nrows < ROWS; i--) {
      const Msg& mm = log_[i % RING];
      char full[Msg::CAP + 16];
      if (mm.src == ctx.myAddr) {
        std::snprintf(full, sizeof(full), "me> %s", mm.text);
      } else {
        char tmp[14];
        std::snprintf(full, sizeof(full), "%s> %s", ctx.roster.label(mm.src, tmp, sizeof(tmp)), mm.text);
      }
      int len = (int)std::strlen(full);
      int chunks = (len + COLS - 1) / COLS;
      if (chunks < 1) chunks = 1;
      for (int c = chunks - 1; c >= 0 && nrows < ROWS; c--) {
        int off = c * COLS;
        int n = len - off;
        if (n > COLS) n = COLS;
        if (n < 0) n = 0;
        std::memcpy(rows[nrows], full + off, n);
        rows[nrows][n] = 0;
        rowSrc[nrows] = mm.src;
        nrows++;
      }
    }

    for (int i = nrows - 1; i >= 0; i--) {
      g.setTextColor(rowSrc[i] == ctx.myAddr ? theme::ACCENT : theme::TEXT, theme::BG);
      g.drawString(rows[i], 6, y);
      y += 10;
    }

    int iy = ui::SCREEN_H - ui::FOOTER_H - 12;
    g.drawFastHLine(0, iy - 2, ui::SCREEN_W, theme::LINE);
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString(">", 6, iy);
    g.setTextColor(theme::TEXT, theme::BG);
    const int VIS = 38;
    g.drawString(len_ > VIS ? input_ + (len_ - VIS) : input_, 18, iy);
    ui::footer(g);
  }

private:
  struct Msg {
    static const int CAP = 160;
    uint16_t src;
    char text[CAP + 1];
    int16_t rssi;
  };
  static const int RING = 16;
  static const int ROWS = 9;
  static const int COLS = 44;
  Msg log_[RING] = {};
  int count_ = 0;
  char input_[TEXT_MAX + 1] = {0};
  int len_ = 0;

  void add(uint16_t src, const char* text, int16_t rssi) {
    Msg& m = log_[count_ % RING];
    m.src = src;
    std::strncpy(m.text, text, Msg::CAP);
    m.text[Msg::CAP] = 0;
    m.rssi = rssi;
    count_++;
  }

  void send() {
    if (!len_) return;
    if (netSendText(ADDR_BROADCAST, input_, false)) {
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
