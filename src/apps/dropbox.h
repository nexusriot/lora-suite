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

// Chunked note/file transfer (stretch app). A note is split into FILECHUNK
// frames [total:2][seq:2][data...]; the receiver reassembles and (when SD is
// present) writes it out. Slow, but proves the link end to end.
//
// This is a working skeleton: it sends a built-in demo note and reassembles
// incoming chunks in RAM. Wire it to Storage for real file picking/writing.
class Dropbox : public App {
public:
  const char* name() const override { return "Dropbox"; }
  const char* callsign() const override { return "DROP"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // download to tray
    g.fillRect(x + 9, y + 2, 2, 8, c);
    g.fillTriangle(x + 5, y + 9, x + 15, y + 9, x + 10, y + 15, c);
    g.drawFastHLine(x + 3, y + 17, 14, c);
    g.drawFastVLine(x + 3, y + 14, 3, c);
    g.drawFastVLine(x + 16, y + 14, 3, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.enter && !sending_) beginSend();
  }

  void update() override {
    if (!sending_) return;
    uint32_t now = millis();
    if (now - last_ < 400) return;      // pace to respect duty
    last_ = now;
    sendChunk(seq_);
    if (++seq_ >= total_) sending_ = false;
  }

  void onPacket(const Frame& f, const RxMeta& m) override {
    if (f.type != MSG_FILECHUNK || f.len < 4) return;
    uint16_t total = f.payload[0] | (f.payload[1] << 8);
    uint16_t seq   = f.payload[2] | (f.payload[3] << 8);
    uint8_t dlen = f.len - 4;
    if (seq == 0) { rxTotal_ = total; rxGot_ = 0; rxLen_ = 0; }
    if (seq < total && rxLen_ + dlen < (int)sizeof(rxBuf_)) {
      memcpy(rxBuf_ + rxLen_, f.payload + 4, dlen);
      rxLen_ += dlen;
      rxGot_++;
    }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 4;
    char s[40];

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("Enter: send demo note", 6, y); y += 14;

    g.setTextColor(theme::TEXT, theme::BG);
    std::snprintf(s, sizeof(s), "TX %s  %u/%u", sending_ ? "sending" : "idle", seq_, total_);
    g.drawString(s, 6, y); y += 12;
    bar(g, 24, y, seq_, total_); y += 14;

    std::snprintf(s, sizeof(s), "RX chunks %u/%u  %db", rxGot_, rxTotal_, rxLen_);
    g.drawString(s, 6, y); y += 12;
    bar(g, 24, y, rxGot_, rxTotal_);
    ui::footer(g);
  }

private:
  const char* note_ =
      "Rendezvous 0300 at the north relay. Bring the spare 3dBi whip and a "
      "charged pack. Keep to channel 07, encrypted. Ack on arrival.";
  bool sending_ = false;
  uint16_t seq_ = 0, total_ = 0;
  uint32_t last_ = 0;
  uint16_t rxTotal_ = 0, rxGot_ = 0;
  int rxLen_ = 0;
  char rxBuf_[512] = {0};

  void beginSend() {
    len_ = (uint16_t)strlen(note_);
    total_ = (len_ + CHUNK - 1) / CHUNK;
    seq_ = 0; sending_ = true;
  }

  void sendChunk(uint16_t seq) {
    uint16_t off = seq * CHUNK;
    uint8_t dlen = (uint8_t)((len_ - off) < CHUNK ? (len_ - off) : CHUNK);
    uint8_t p[4 + CHUNK];
    p[0] = total_ & 0xFF; p[1] = total_ >> 8;
    p[2] = seq & 0xFF;    p[3] = seq >> 8;
    memcpy(p + 4, note_ + off, dlen);
    Frame f;
    f.type = MSG_FILECHUNK; f.flags = FLAG_MESH;
    f.setPayload(p, 4 + dlen);
    netSend(f);
  }

  static void bar(M5Canvas& g, int x, int y, uint16_t v, uint16_t max) {
    int w = 160;
    g.drawRect(x, y, w, 6, theme::LINE);
    if (max) g.fillRect(x + 1, y + 1, (w - 2) * v / max, 4, theme::ACCENT);
  }

  static const int CHUNK = 160;
  uint16_t len_ = 0;
};

} // namespace ls
