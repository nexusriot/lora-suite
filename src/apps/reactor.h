#pragma once
#include <M5Unified.h>
#include <Arduino.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Battery-aware power state machine. Auto-degrades CPU clock + LCD brightness as
// the cell falls (with hysteresis so it doesn't oscillate); on entering Survival
// it broadcasts a low-power ALERT so the squad knows a node is about to go quiet.
// (Renamed from "Governor" to avoid colliding with the duty DutyGovernor.)
class Reactor : public App {
public:
  const char* name() const override { return "Reactor"; }
  const char* callsign() const override { return "PWR"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // battery + bolt
    g.drawRoundRect(x + 2, y + 5, 14, 11, 2, c);
    g.fillRect(x + 16, y + 8, 2, 5, c);
    g.fillTriangle(x + 10, y + 6, x + 6, y + 12, x + 9, y + 12, c);
    g.fillTriangle(x + 8, y + 15, x + 12, y + 9, x + 9, y + 9, c);
  }

  void onEnter() override { apply(ctx.power); }

  void onKey(const KeyEvent& k) override {
    if (k.ch == 'a') manual_ = false;
    else if (k.up) { manual_ = true; if (ctx.power > PWR_PERF) enter((PowerState)(ctx.power - 1)); }
    else if (k.down) { manual_ = true; if (ctx.power < PWR_SURVIVAL) enter((PowerState)(ctx.power + 1)); }
  }

  void background() override {
    uint32_t now = millis();
    if (now - lastEval_ < 3000) return;
    lastEval_ = now;
    if (manual_) return;
    int b = M5.Power.getBatteryLevel();
    if (b < 0) return;                       // unknown battery: hold state
    PowerState target = classify(b);
    if (target > ctx.power) enter(target);   // degrade immediately
    else if (target < ctx.power && b >= upThresh(target) + 5) enter(target); // recover w/ margin
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 6;
    char s[40];

    uint16_t c = ctx.power == PWR_SURVIVAL ? theme::CRIT
               : ctx.power == PWR_ENDURANCE ? theme::WARN : theme::GOOD;
    g.setTextColor(c, theme::BG);
    std::snprintf(s, sizeof(s), "state: %s", stateName(ctx.power));
    g.drawString(s, 6, y); y += 16;

    g.setTextColor(theme::TEXT, theme::BG);
    std::snprintf(s, sizeof(s), "battery %d%%   mode %s",
                  M5.Power.getBatteryLevel(), manual_ ? "MANUAL" : "auto");
    g.drawString(s, 6, y); y += 14;
    std::snprintf(s, sizeof(s), "cpu %dMHz  bright %d", cpuFor(ctx.power), brightFor(ctx.power));
    g.drawString(s, 6, y); y += 14;

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("Perf>=60  Bal>=25  End>=10  Surv<10", 6, y); y += 12;
    g.drawString("up/down override   a: auto", 6, y);
    ui::footer(g);
  }

private:
  bool manual_ = false;
  uint32_t lastEval_ = 0;

  static const char* stateName(PowerState p) {
    switch (p) { case PWR_PERF: return "Performance"; case PWR_BALANCED: return "Balanced";
                 case PWR_ENDURANCE: return "Endurance"; default: return "SURVIVAL"; }
  }
  static PowerState classify(int b) {
    if (b < 10) return PWR_SURVIVAL;
    if (b < 25) return PWR_ENDURANCE;
    if (b < 60) return PWR_BALANCED;
    return PWR_PERF;
  }
  static int upThresh(PowerState p) {
    switch (p) { case PWR_PERF: return 60; case PWR_BALANCED: return 25;
                 case PWR_ENDURANCE: return 10; default: return 0; }
  }
  static int cpuFor(PowerState p) {
    switch (p) { case PWR_PERF: return 240; case PWR_BALANCED: return 160;
                 default: return 80; }
  }
  static int brightFor(PowerState p) {
    switch (p) { case PWR_PERF: return 180; case PWR_BALANCED: return 120;
                 case PWR_ENDURANCE: return 70; default: return 30; }
  }

  void apply(PowerState p) {
    setCpuFrequencyMhz(cpuFor(p));
    M5.Display.setBrightness(brightFor(p));
  }

  void enter(PowerState p) {
    bool toSurvival = (p == PWR_SURVIVAL && ctx.power != PWR_SURVIVAL);
    ctx.power = p;
    apply(p);
    if (toSurvival) {
      Frame f = makeAlert(ALERT_LOWPWR, "LOWPWR");
      netSend(f, true);
    }
  }
};

} // namespace ls
