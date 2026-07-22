#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../services/gps_service.h"
#include "../proto/geo.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Ping/echo link tester. As initiator it pings periodically and measures RSSI,
// SNR, round-trip and loss from the returning PONGs; as responder it echoes
// PINGs back with its own measured RSSI/SNR. Enter toggles role.
class Ranger : public App {
public:
  const char* name() const override { return "Ranger"; }
  const char* callsign() const override { return "RNG"; }
  Cat category() const override { return Cat::RF; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // crosshair target
    g.drawCircle(x + 10, y + 10, 8, c);
    g.drawCircle(x + 10, y + 10, 3, c);
    g.drawFastHLine(x + 0, y + 10, 4, c);
    g.drawFastHLine(x + 16, y + 10, 4, c);
    g.drawFastVLine(x + 10, y + 0, 4, c);
    g.drawFastVLine(x + 10, y + 16, 4, c);
  }

  void onKey(const KeyEvent& k) override {
    if (k.enter) initiator_ = !initiator_;
    else if (k.ch == 'r') { sent_ = recv_ = 0; }
  }

  void update() override {
    if (!initiator_) return;
    uint32_t now = millis();
    if (now - lastPing_ < 1500) return;
    lastPing_ = now;
    Frame f = makePing(ADDR_BROADCAST, ++seq_);
    sentAt_[seq_ % WIN] = now;
    if (netSend(f)) sent_++;
  }

  void onPacket(const Frame& f, const RxMeta& m) override {
    uint32_t now = millis();
    if (f.type == MSG_PING) {
      uint16_t seq = f.len >= 2 ? (uint16_t)(f.payload[0] | (f.payload[1] << 8)) : 0;
      Frame p = makePong(f.src, seq, m.rssi, m.snr);
      netSend(p, true);
      peer_ = f.src;
    } else if (f.type == MSG_PONG && f.len >= 5 && initiator_) {
      uint16_t seq = (uint16_t)(f.payload[0] | (f.payload[1] << 8));
      recv_++;
      rtt_ = now - sentAt_[seq % WIN];
      remoteRssi_ = (int16_t)(f.payload[2] | (f.payload[3] << 8));
      rssi_ = m.rssi; snr_ = m.snr;
      peer_ = f.src;
    }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 4;
    char s[40];

    g.setTextColor(theme::ACCENT, theme::BG);
    g.drawString(initiator_ ? "role: INITIATOR" : "role: responder", 6, y); y += 14;

    g.setTextColor(theme::TEXT, theme::BG);
    std::snprintf(s, sizeof(s), "peer %04X", peer_); g.drawString(s, 6, y); y += 11;
    std::snprintf(s, sizeof(s), "rssi %d dBm  snr %d", rssi_, snr_); g.drawString(s, 6, y); y += 11;
    std::snprintf(s, sizeof(s), "remote rssi %d dBm", remoteRssi_); g.drawString(s, 6, y); y += 11;
    std::snprintf(s, sizeof(s), "rtt %lu ms", (unsigned long)rtt_); g.drawString(s, 6, y); y += 11;

    int loss = sent_ ? (int)(100 * (sent_ - recv_) / sent_) : 0;
    std::snprintf(s, sizeof(s), "tx %lu  rx %lu  loss %d%%",
                  (unsigned long)sent_, (unsigned long)recv_, loss);
    g.setTextColor(loss < 20 ? theme::GOOD : theme::WARN, theme::BG);
    g.drawString(s, 6, y); y += 11;

    Node* pn = ctx.nodes.find(peer_);
    if (pn && pn->hasPos && ctx.gps && ctx.gps->hasFix()) {
      double d = haversineMeters(ctx.gps->lat(), ctx.gps->lon(), pn->lat, pn->lon);
      std::snprintf(s, sizeof(s), "distance %d m", (int)d);
      g.setTextColor(theme::LOC, theme::BG);
      g.drawString(s, 6, y);
    }
    ui::footer(g);
  }

private:
  static const int WIN = 64;
  bool initiator_ = false;
  uint16_t seq_ = 0, peer_ = 0;
  uint32_t sentAt_[WIN] = {};
  uint32_t lastPing_ = 0, rtt_ = 0, sent_ = 0, recv_ = 0;
  int16_t rssi_ = 0, remoteRssi_ = 0;
  int8_t snr_ = 0;
};

} // namespace ls
