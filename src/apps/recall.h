#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/lora_service.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Pre-TX unsend. The Marshal queue + duty gate hold outbound frames for a real
// (sometimes seconds-long) window before they air; Recall lists your still-queued
// frames and cancels the selected one before it hits the radio. Urgent alerts
// bypass the duty gate, so their window is ~0 — recall may report "too late".
class Recall : public App {
public:
  const char* name() const override { return "Recall"; }
  const char* callsign() const override { return "UNDO"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // return/undo arrow
    g.fillTriangle(x + 2, y + 9, x + 9, y + 4, x + 9, y + 14, c);
    g.fillRect(x + 9, y + 7, 6, 4, c);
    g.drawLine(x + 15, y + 9, x + 15, y + 16, c);
    g.drawLine(x + 15, y + 16, x + 6, y + 16, c);
  }

  void onKey(const KeyEvent& k) override {
    int n = mineCount();
    if (k.up && sel_ > 0) sel_--;
    else if (k.down && sel_ + 1 < n) sel_++;
    else if (k.enter && selMsgid_ != 0) {
      status_ = ctx.lora->cancel(ctx.myAddr, selMsgid_) ? "recalled" : "too late (sent)";
      selMsgid_ = 0;
    }
    if (sel_ < firstVisible_) firstVisible_ = sel_;
    else if (sel_ >= firstVisible_ + ROWS_) firstVisible_ = sel_ - ROWS_ + 1;
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("outbound queue - Enter recalls", 6, ui::BODY_Y + 2);

    const TxQueue& q = ctx.lora->queue();
    uint32_t now = millis();
    int idx = 0, y = ui::BODY_Y + 14, shown = 0;
    selMsgid_ = 0;
    for (size_t i = 0; i < q.size(); i++) {
      const Frame& fr = q.frameAt(i);
      if (fr.src != ctx.myAddr) continue;   // only your own frames (not relays)
      if (idx >= firstVisible_ && shown < ROWS_) {
        bool s = (idx == sel_);
        if (s) selMsgid_ = fr.msgid;
        unsigned long age = now - q.enqAt(i);
        char row[40];
        std::snprintf(row, sizeof(row), "%s%-8s #%u  %lums", s ? ">" : " ",
                      typeName(fr.type), fr.msgid, age);
        g.setTextColor(s ? theme::ACCENT : theme::TEXT, theme::BG);
        g.drawString(row, 6, y);
        y += 10;
        shown++;
      }
      idx++;
    }
    if (idx == 0) {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("(nothing pending)", 6, ui::BODY_Y + 18);
      g.drawString("frames air within the duty window", 6, ui::BODY_Y + 30);
    }
    if (sel_ >= idx) sel_ = idx > 0 ? idx - 1 : 0;
    if (firstVisible_ > sel_) firstVisible_ = sel_;
    else if (idx > ROWS_ && firstVisible_ > idx - ROWS_) firstVisible_ = idx - ROWS_;
    if (firstVisible_ < 0) firstVisible_ = 0;

    if (status_) {
      g.setTextColor(theme::WARN, theme::BG);
      g.drawString(status_, 6, ui::SCREEN_H - ui::FOOTER_H - 12);
    }
    ui::footer(g);
  }

private:
  static const int ROWS_ = 8;
  int sel_ = 0;
  int firstVisible_ = 0;
  uint16_t selMsgid_ = 0;
  const char* status_ = nullptr;

  int mineCount() {
    const TxQueue& q = ctx.lora->queue();
    int n = 0;
    for (size_t i = 0; i < q.size(); i++)
      if (q.frameAt(i).src == ctx.myAddr) n++;
    return n;
  }
  static const char* typeName(uint8_t t) {
    switch (t) {
      case MSG_TEXT: return "text"; case MSG_ACK: return "ack";
      case MSG_BEACON: return "beacon"; case MSG_PING: return "ping";
      case MSG_PONG: return "pong"; case MSG_TELEMETRY: return "telem";
      case MSG_ALERT: return "alert"; case MSG_NODEINFO: return "info";
      case MSG_FILECHUNK: return "file"; case MSG_TIMESYNC: return "time";
      case MSG_WAYPOINT: return "waypt"; default: return "?";
    }
  }
};

} // namespace ls
