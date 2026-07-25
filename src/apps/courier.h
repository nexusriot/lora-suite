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

// Addressed + broadcast text chat. Type and Enter to send; messages scroll in
// the body. ACK-requested sends flip to "ok" when the ACK returns.
class Courier : public App {
public:
  const char* name() const override { return "Courier"; }
  const char* callsign() const override { return "CR"; }
  Cat category() const override { return Cat::Comms; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // chat bubble
    g.fillRoundRect(x + 1, y + 3, 18, 12, 3, c);
    g.fillTriangle(x + 4, y + 14, x + 4, y + 19, x + 10, y + 14, c);
  }

  void onEnter() override {
    ctx.unread = 0;                             // opening Courier clears unread
    if (ctx.pendingPeer != ADDR_BROADCAST) {    // handoff from Fleet: DM this node
      peer_ = ctx.pendingPeer;
      toBroadcast_ = false;
      ctx.pendingPeer = ADDR_BROADCAST;
    }
  }
  bool consumesText() const override { return true; }

  void onKey(const KeyEvent& k) override {
    if (k.enter) { sendCurrent(); return; }
    if (k.del) { if (inlen_) input_[--inlen_] = 0; return; }
    if (k.tab) { toBroadcast_ = !toBroadcast_; return; }
    if (k.ch >= 32 && inlen_ < (int)sizeof(input_) - 1) {
      input_[inlen_++] = k.ch;
      input_[inlen_] = 0;
    }
  }

  // Text arrives already reassembled and decompressed; the shell also sends the
  // ACK, so a DM is confirmed even when Courier isn't the open screen.
  void onTextMessage(uint16_t src, const char* text, uint16_t, const RxMeta& m) override {
    add(src, text, m.rssi);
  }

  void onPacket(const Frame& f, const RxMeta&) override {
    if (f.type == MSG_ACK && f.len >= 2)
      lastAcked_ = (uint16_t)(f.payload[0] | (f.payload[1] << 8));
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    // Long messages wrap over several lines, so lay the log out from the newest
    // backwards until the visible rows are full, then draw top-down.
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
      // Take the tail chunks first (they're nearest the bottom of the screen).
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

    int y = ui::BODY_Y + 2;
    for (int i = nrows - 1; i >= 0; i--) {
      g.setTextColor(rowSrc[i] == ctx.myAddr ? theme::ACCENT : theme::TEXT, theme::BG);
      g.drawString(rows[i], 6, y);
      y += 10;
    }

    int iy = ui::SCREEN_H - ui::FOOTER_H - 12;
    g.drawFastHLine(0, iy - 2, ui::SCREEN_W, theme::LINE);
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString(toBroadcast_ ? "ALL>" : "DM >", 6, iy);
    g.setTextColor(theme::TEXT, theme::BG);
    // Show the tail of a long compose so the cursor stays visible.
    const int VIS = 36;
    const char* shown = inlen_ > VIS ? input_ + (inlen_ - VIS) : input_;
    g.drawString(shown, 34, iy);
    if (inlen_ > VIS) {
      char cnt[8];
      std::snprintf(cnt, sizeof(cnt), "%d", inlen_);
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString(cnt, ui::SCREEN_W - 24, iy);
    }
    ui::footer(g);
  }

private:
  struct Msg {
    static const int CAP = 160;   // keep more than fits on screen; draw() wraps it
    uint16_t src;
    char text[CAP + 1];
    int16_t rssi;
  };
  static const int RING = 16;
  static const int ROWS = 8;
  static const int COLS = 44;     // characters per rendered line at text size 1
  Msg log_[RING] = {};
  int count_ = 0;
  char input_[TEXT_MAX + 1] = {0};
  int inlen_ = 0;
  bool toBroadcast_ = true;
  uint16_t lastAcked_ = 0;

  void add(uint16_t src, const char* text, int16_t rssi) {
    Msg& m = log_[count_ % RING];
    m.src = src;
    std::strncpy(m.text, text, Msg::CAP);
    m.text[Msg::CAP] = 0;
    m.rssi = rssi;
    count_++;
  }

  void sendCurrent() {
    if (!inlen_) return;
    const char* body = input_;
    uint16_t dst = toBroadcast_ ? ADDR_BROADCAST : peer_;

    // "@name message" resolves a roster alias/name to an address.
    if (input_[0] == '@') {
      char tok[14];
      int i = 1, j = 0;
      while (input_[i] && input_[i] != ' ' && j < 13) tok[j++] = input_[i++];
      tok[j] = 0;
      uint16_t a;
      if (ctx.roster.lookup(tok, a)) {
        dst = a;
        peer_ = a;
        if (input_[i] == ' ') i++;
        body = input_ + i;
      }
    }

    // netSendText compresses and, if still too long for one frame, fragments.
    if (netSendText(dst, body, dst != ADDR_BROADCAST)) {
      add(ctx.myAddr, body, 0);
      char t[9];
      if (ctx.clock) ctx.clock->hms(t); else t[0] = 0;
      ctx.archive.add(t, 'O', dst, body);   // persist sent text (Archive)
    }
    input_[0] = 0;
    inlen_ = 0;
  }

  uint16_t peer_ = ADDR_BROADCAST;
};

} // namespace ls
