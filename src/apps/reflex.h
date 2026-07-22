#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../services/storage.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// On-device IFTTT: event -> action rules. Frame events are evaluated in the RX
// handler (main), timer/battery events on this app's background tick. TX actions
// route through the duty-gated send path with per-rule cooldowns, so automation
// can't runaway. Rules persist to NVS. Edited by single-key field cycling.
class Reflex : public App {
public:
  const char* name() const override { return "Reflex"; }
  const char* callsign() const override { return "RULE"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // if -> then branch
    g.fillCircle(x + 5, y + 10, 3, c);
    g.drawLine(x + 8, y + 10, x + 13, y + 6, c);
    g.drawLine(x + 8, y + 10, x + 13, y + 14, c);
    g.fillTriangle(x + 12, y + 3, x + 12, y + 9, x + 18, y + 6, c);
    g.fillTriangle(x + 12, y + 11, x + 12, y + 17, x + 18, y + 14, c);
  }

  void onExit() override { if (ctx.store) ctx.store->saveRules(ctx.rules); }

  void background() override {
    uint32_t now = millis();
    if (now - lastTick_ < 1000) return;
    lastTick_ = now;
    RuleAction a;
    if (ctx.rules.tick(now, M5.Power.getBatteryLevel(), a)) runRuleAction(a);
  }

  void onKey(const KeyEvent& k) override {
    if (k.ch == 'a') { if (ctx.rules.add()) sel_ = ctx.rules.count() - 1; return; }
    int n = ctx.rules.count();
    if (n == 0) return;
    if (k.up && sel_ > 0) sel_--;
    else if (k.down && sel_ + 1 < n) sel_++;
    else {
      Rule& r = ctx.rules.at(sel_);
      if (k.ch == 'e') r.enabled = !r.enabled;
      else if (k.ch == 'v') { r.event = (r.event + 1) % 5; }
      else if (k.ch == 't') cycleParam(r);
      else if (k.ch == 'c') r.action = (r.action + 1) % 5;
      else if (k.ch == 'o') r.acParam = (r.acParam + 1) % 6;
      else if (k.ch == 'x') { ctx.rules.remove(sel_); if (sel_ > 0) sel_--; }
      else if (k.ch == 's' && ctx.store) ctx.store->saveRules(ctx.rules);
    }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("a add x del e on  v t c o edit  s save", 6, ui::BODY_Y + 2);

    int n = ctx.rules.count();
    if (n == 0) {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("(no rules - press a to add)", 6, ui::BODY_Y + 18);
      ui::footer(g);
      return;
    }
    if (sel_ >= n) sel_ = n - 1;
    int y = ui::BODY_Y + 14;
    for (int i = 0; i < n && y < ui::SCREEN_H - ui::FOOTER_H - 8; i++) {
      const Rule& r = ctx.rules.at(i);
      bool s = (i == sel_);
      char body[40];
      describe(r, body, sizeof(body));
      char row[48];
      std::snprintf(row, sizeof(row), "%s%c %s", s ? ">" : " ", r.enabled ? '*' : '.', body);
      g.setTextColor(s ? theme::ACCENT : (r.enabled ? theme::TEXT : theme::MUTED), theme::BG);
      g.drawString(row, 4, y);
      y += 11;
    }
    ui::footer(g);
  }

private:
  int sel_ = 0;
  uint32_t lastTick_ = 0;

  static void cycleParam(Rule& r) {
    if (r.event == EV_RX_TYPE) {
      r.evParam++;
      if (r.evParam < MSG_TEXT || r.evParam > MSG_COUNTDOWN) r.evParam = MSG_TEXT;
    } else if (r.event == EV_BATT_LOW) {
      static const uint16_t B[] = {10, 20, 30, 50};
      int i = 0;
      for (int j = 0; j < 4; j++) if (B[j] == r.evArg) i = j;
      r.evArg = B[(i + 1) % 4];
    } else if (r.event == EV_PERIODIC) {
      static const uint16_t P[] = {30, 60, 300, 600, 1800};
      int i = 0;
      for (int j = 0; j < 5; j++) if (P[j] == r.evArg) i = j;
      r.evArg = P[(i + 1) % 5];
    }
  }

  static void describe(const Rule& r, char* out, size_t cap) {
    char ev[22], ac[16];
    switch (r.event) {
      case EV_RX_TYPE:  std::snprintf(ev, sizeof(ev), "rx:%s", typeName(r.evParam)); break;
      case EV_ALERT:    std::snprintf(ev, sizeof(ev), "on alert"); break;
      case EV_BATT_LOW: std::snprintf(ev, sizeof(ev), "batt<%u", r.evArg); break;
      case EV_PERIODIC: std::snprintf(ev, sizeof(ev), "every %us", r.evArg); break;
      default:          std::snprintf(ev, sizeof(ev), "(off)"); break;
    }
    switch (r.action) {
      case AC_BEEP:       std::snprintf(ac, sizeof(ac), "beep"); break;
      case AC_SEND_TEXT:  std::snprintf(ac, sizeof(ac), "text%u", r.acParam); break;
      case AC_SEND_ALERT: std::snprintf(ac, sizeof(ac), "alert%u", r.acParam); break;
      case AC_BEACON:     std::snprintf(ac, sizeof(ac), "beacon"); break;
      default:            std::snprintf(ac, sizeof(ac), "-"); break;
    }
    std::snprintf(out, cap, "%s -> %s", ev, ac);
  }

  static const char* typeName(uint8_t t) {
    switch (t) {
      case MSG_TEXT: return "text"; case MSG_BEACON: return "beacon";
      case MSG_ALERT: return "alert"; case MSG_PING: return "ping";
      case MSG_PONG: return "pong"; case MSG_TELEMETRY: return "telem";
      case MSG_NODEINFO: return "info"; case MSG_TIMESYNC: return "time";
      case MSG_WAYPOINT: return "waypt"; case MSG_COUNTDOWN: return "cdwn";
      case MSG_ACK: return "ack"; case MSG_FILECHUNK: return "file";
      default: return "?";
    }
  }
};

} // namespace ls
