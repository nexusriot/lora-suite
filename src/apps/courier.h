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

  void onPacket(const Frame& f, const RxMeta& m) override {
    if (f.type == MSG_TEXT) {
      char body[41];
      uint8_t n = f.len < 40 ? f.len : 40;
      memcpy(body, f.payload, n);
      body[n] = 0;
      add(f.src, body, m.rssi);
      if ((f.flags & FLAG_ACK_REQ) && f.dst == ctx.myAddr) {
        Frame a = makeAck(f.src, f.msgid);
        netSend(a, true);
      }
    } else if (f.type == MSG_ACK && f.len >= 2) {
      lastAcked_ = (uint16_t)(f.payload[0] | (f.payload[1] << 8));
    }
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
    g.drawString(toBroadcast_ ? "ALL>" : "DM >", 6, iy);
    g.setTextColor(theme::TEXT, theme::BG);
    g.drawString(input_, 34, iy);
    ui::footer(g);
  }

private:
  struct Msg { uint16_t src; char text[41]; int16_t rssi; };
  static const int RING = 24;
  static const int ROWS = 8;
  Msg log_[RING] = {};
  int count_ = 0;
  char input_[64] = {0};
  int inlen_ = 0;
  bool toBroadcast_ = true;
  uint16_t lastAcked_ = 0;

  void add(uint16_t src, const char* text, int16_t rssi) {
    Msg& m = log_[count_ % RING];
    m.src = src;
    std::strncpy(m.text, text, sizeof(m.text) - 1);
    m.text[sizeof(m.text) - 1] = 0;
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

    Frame f = makeText(dst, body, dst != ADDR_BROADCAST);
    if (netSend(f)) {
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
