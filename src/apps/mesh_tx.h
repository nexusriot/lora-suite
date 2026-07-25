#pragma once
#include <M5Unified.h>
#include <cstdio>
#include <cstring>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../services/lora_service.h"
#include "../services/gps_service.h"
#include "../proto/meshtastic.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Two-way Meshtastic (Direction B, TX side): compose a short text and broadcast
// it INTO the local Meshtastic public channel, readable by real Meshtastic nodes.
// Momentarily retunes the radio to the Meshtastic preset for the send, then
// restores our own config. Our node id and the channel hash/flags are marked
// VERIFY-on-hardware in proto/meshtastic. Be a good neighbour — this airs on a
// shared public channel.
class MeshTX : public App {
public:
  const char* name() const override { return "MeshTX"; }
  const char* callsign() const override { return "MTX"; }
  Cat category() const override { return Cat::RF; }
  bool consumesText() const override { return true; }   // typing — gate global keys

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // send arrow
    g.drawLine(x + 3, y + 16, x + 16, y + 4, c);
    g.drawLine(x + 16, y + 4, x + 10, y + 4, c);
    g.drawLine(x + 16, y + 4, x + 16, y + 10, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.enter) { send(); return; }
    if (k.tab) { sendPosition(); return; }
    if (k.del) { if (len_ > 0) buf_[--len_] = 0; return; }
    if (k.ch >= 0x20 && k.ch < 0x7f && len_ < (int)sizeof(buf_) - 1) {
      buf_[len_++] = k.ch;
      buf_[len_] = 0;
    }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 2;
    char s[48];

    g.setTextColor(theme::RF, theme::BG);
    std::snprintf(s, sizeof(s), "-> Meshtastic %.3f MHz", meshtasticPresetEU868().freqHz / 1e6);
    g.drawString(s, 6, y); y += 14;

    g.setTextColor(theme::TEXT, theme::BG);
    std::snprintf(s, sizeof(s), "%s_", buf_);   // trailing cursor
    g.drawString(s, 6, y); y += 14;

    g.setTextColor(theme::MUTED, theme::BG);
    std::snprintf(s, sizeof(s), "%d/%d  sent %lu", len_, (int)sizeof(buf_) - 1, (unsigned long)sent_);
    g.drawString(s, 6, y); y += 12;
    if (status_[0]) {
      g.setTextColor(theme::ACCENT, theme::BG);
      g.drawString(status_, 6, y);
    }
    y += 14;
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("Enter=text  Tab=GPS pos", 6, y);
    ui::footer(g);
  }

private:
  char buf_[64] = {0};
  int len_ = 0;
  uint32_t sent_ = 0;
  char status_[16] = {0};

  void send() {
    if (len_ == 0) return;
    if (meshtasticSendText(buf_)) {
      std::strncpy(status_, "sent", sizeof(status_)); sent_++; len_ = 0; buf_[0] = 0;
    } else std::strncpy(status_, "TX failed", sizeof(status_));
  }

  void sendPosition() {   // Tab: broadcast our GPS as a Meshtastic Position
    if (!ctx.gps || !ctx.gps->hasFix()) { std::strncpy(status_, "no GPS fix", sizeof(status_)); return; }
    if (meshtasticSendPosition()) { std::strncpy(status_, "pos sent", sizeof(status_)); sent_++; }
    else std::strncpy(status_, "TX failed", sizeof(status_));
  }
};

} // namespace ls
