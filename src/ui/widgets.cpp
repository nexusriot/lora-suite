#include "widgets.h"
#include <M5Unified.h>
#include <cstdio>
#include "theme.h"
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/lora_service.h"
#include "../services/gps_service.h"
#include "../services/clock.h"

namespace ls {
namespace ui {

using namespace theme;

uint16_t rssiColor(int16_t rssi) {
  if (rssi > -80) return GOOD;
  if (rssi > -105) return WARN;
  return CRIT;
}

void textRight(M5Canvas& g, int x, int y, const char* s) {
  g.setTextDatum(top_right);
  g.drawString(s, x, y);
  g.setTextDatum(top_left);
}

void header(M5Canvas& g, const App& app) {
  uint16_t c = forCat(app.category());
  g.fillRect(0, 0, SCREEN_W, HEADER_H, PANEL);
  g.fillRect(0, 0, 3, HEADER_H, c);

  g.setTextColor(c, PANEL);
  g.setTextDatum(top_left);
  g.drawString(app.callsign(), 8, 4);

  g.setTextColor(TEXT, PANEL);
  g.drawString(app.name(), 40, 4);

  char clk[9];
  if (ctx.clock) ctx.clock->hms(clk); else std::snprintf(clk, sizeof(clk), "--:--:--");
  g.setTextColor(MUTED, PANEL);
  textRight(g, SCREEN_W - 4, 4, clk);

  g.drawFastHLine(0, HEADER_H, SCREEN_W, LINE);
}

void footer(M5Canvas& g) {
  const int y = SCREEN_H - FOOTER_H;
  g.fillRect(0, y, SCREEN_W, FOOTER_H, PANEL);
  g.drawFastHLine(0, y - 1, SCREEN_W, LINE);

  uint32_t now = millis();
  double used = ctx.lora ? ctx.lora->duty().usedFraction(now) : 0.0;
  if (used > 1) used = 1;

  int bw = 38, bx = 4, by = y + 3;
  g.drawRect(bx, by, bw, 6, LINE);
  uint16_t dc = used < 0.7 ? GOOD : (used < 1.0 ? WARN : CRIT);
  g.fillRect(bx + 1, by + 1, (int)((bw - 2) * used), 4, dc);

  char s[16];
  int x = bx + bw + 4;

  // Time until a typical (40-byte) frame may legally transmit.
  uint32_t toa = (uint32_t)timeOnAirMs(ctx.cfg, 40);
  uint32_t wait = ctx.lora ? ctx.lora->duty().timeToNextTxMs(now, toa) : 0;
  if (wait == 0) { g.setTextColor(GOOD, PANEL); std::snprintf(s, sizeof(s), "%d%%", (int)(used * 100)); }
  else if (wait == DutyGovernor::NEVER) { g.setTextColor(CRIT, PANEL); std::snprintf(s, sizeof(s), "FULL"); }
  else { g.setTextColor(WARN, PANEL); std::snprintf(s, sizeof(s), "%lus", (unsigned long)((wait + 999) / 1000)); }
  g.drawString(s, x, y + 2); x += 30;

  g.setTextColor(MUTED, PANEL);
  std::snprintf(s, sizeof(s), "ch%02X", ctx.channel.id());
  g.drawString(s, x, y + 2); x += 30;

  size_t q = ctx.lora ? ctx.lora->queueDepth() : 0;
  if (q) { std::snprintf(s, sizeof(s), "Q%u", (unsigned)q); g.drawString(s, x, y + 2); }
  x += 22;

  if (ctx.unread) {
    g.setTextColor(CRIT, PANEL);
    std::snprintf(s, sizeof(s), "@%u", (unsigned)ctx.unread);
    g.drawString(s, x, y + 2);
  }

  if (ctx.power >= PWR_SURVIVAL) {
    g.setTextColor(CRIT, PANEL);
    g.drawString("LP", SCREEN_W - 62, y + 2);
  }

  int batt = M5.Power.getBatteryLevel();
  g.setTextColor(batt >= 0 && batt < 20 ? CRIT : MUTED, PANEL);
  std::snprintf(s, sizeof(s), "%d%%", batt);
  textRight(g, SCREEN_W - 12, y + 2, s);

  bool fix = ctx.gps && ctx.gps->hasFix();
  g.fillCircle(SCREEN_W - 5, y + FOOTER_H / 2, 3, fix ? GOOD : CRIT);
}

} // namespace ui
} // namespace ls
