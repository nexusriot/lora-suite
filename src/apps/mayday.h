#pragma once
#include <M5Unified.h>
#include <cstdio>
#include <cmath>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../services/gps_service.h"
#include "../proto/geo.h"
#include "../proto/payloads.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Distress + dead-man switch. Enter fires a manual panic that broadcasts a
// live-position last-will at a bounded elevated cadence (auto-stand-down after a
// cap); 'a' arms a dead-man that triggers on IMU stillness + critical battery.
// Receiving a distress frame flips every node here (main pushes us) into a homing
// view with a responder ACK. The elevated cadence is a deliberate, capped
// exception to the duty governor (urgent), never routine.
class Mayday : public App {
public:
  enum Mode { IDLE, ACTIVE, RECEIVING };

  const char* name() const override { return "Mayday"; }
  const char* callsign() const override { return "SOS"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // warning triangle + !
    g.drawTriangle(x + 10, y + 2, x + 2, y + 18, x + 18, y + 18, c);
    g.drawTriangle(x + 10, y + 4, x + 4, y + 17, x + 16, y + 17, c);
    g.fillRect(x + 9, y + 8, 2, 5, c);
    g.fillRect(x + 9, y + 15, 2, 2, c);
  }

  void onKey(const KeyEvent& k) override {
    if (armedConfirm_) {                          // confirm gate for the global hotkey
      if (k.enter) { armedConfirm_ = false; activate(); }
      else if (k.ch == ' ') armedConfirm_ = false;
      return;
    }
    if (k.enter) mode_ == ACTIVE ? standDown() : activate();
    else if (k.ch == 'a') armed_ = !armed_;
    else if (k.ch == 'r' && mode_ == RECEIVING) respond();
    else if (k.ch == ' ') { standDown(); M5.Speaker.stop(); }
  }

  // Called by main when a distress ALERT arrives (already channel-filtered).
  void receiveDistress(uint16_t src, const Position& p, uint8_t batt) {
    if (mode_ == ACTIVE) return;          // don't clobber our own outbound distress
    mode_ = RECEIVING;
    rxSrc_ = src; rxPos_ = p; rxBatt_ = batt; rxAt_ = millis();
    M5.Speaker.tone(2000, 300);
  }

  void background() override {
    uint32_t now = millis();

    // Stillness for the dead-man switch — only if the board actually has an IMU
    // (getAccel returns false on the IMU-less StampS3); don't early-return, the
    // active-distress cadence + alarm below must still run.
    float ax = 0, ay = 0, az = 0;
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
      float mag = std::sqrt(ax * ax + ay * ay + az * az);
      if (std::fabs(mag - 1.0f) < 0.06f) { if (!still_) { still_ = true; stillStart_ = now; } }
      else still_ = false;
    } else {
      still_ = false;
    }

    if (armed_ && mode_ == IDLE && still_ && now - stillStart_ > STILL_MS) {
      int batt = M5.Power.getBatteryLevel();
      if (batt >= 0 && batt < BATT_CRIT) activate();
    }

    if (mode_ == ACTIVE) {
      if (now - lastTx_ >= CADENCE_MS) { sendDistress(); lastTx_ = now; }
      if (now - activatedAt_ > MAX_DUR_MS) standDown();
    }
    // RECEIVING self-clears once the sender goes quiet (each rx refreshes rxAt_).
    if (mode_ == RECEIVING && now - rxAt_ > RX_HOLD_MS) { standDown(); M5.Speaker.stop(); }
    if ((mode_ == ACTIVE || mode_ == RECEIVING) && (now / 500) % 2 == 0)
      M5.Speaker.tone(2000, 140);
  }

  void draw(M5Canvas& g) override {
    bool flash = (mode_ != IDLE || armedConfirm_) && (millis() / 300) % 2 == 0;
    g.fillScreen(flash ? theme::CRIT : theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 6;
    char s[40];

    if (armedConfirm_) {
      g.setTextColor(theme::CRIT, theme::BG);
      g.drawString("CONFIRM DISTRESS", 6, y); y += 16;
      g.setTextColor(theme::TEXT, theme::BG);
      g.drawString("Enter: broadcast SOS now", 6, y); y += 12;
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("Space: cancel", 6, y);
      ui::footer(g);
      return;
    }

    if (mode_ == ACTIVE) {
      g.setTextColor(theme::CRIT, theme::BG);
      g.drawString("*** DISTRESS ACTIVE ***", 6, y); y += 16;
      g.setTextColor(theme::TEXT, theme::BG);
      std::snprintf(s, sizeof(s), "broadcasts sent: %lu", (unsigned long)sent_);
      g.drawString(s, 6, y); y += 12;
      uint32_t left = (MAX_DUR_MS - (millis() - activatedAt_)) / 1000;
      std::snprintf(s, sizeof(s), "auto stand-down in %lus", (unsigned long)left);
      g.drawString(s, 6, y); y += 12;
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("Enter/space: cancel", 6, y);
    } else if (mode_ == RECEIVING) {
      g.setTextColor(theme::CRIT, theme::BG);
      std::snprintf(s, sizeof(s), "DISTRESS from %04X", rxSrc_);
      g.drawString(s, 6, y); y += 16;
      GpsService* gp = ctx.gps;
      g.setTextColor(theme::TEXT, theme::BG);
      if (gp && gp->hasFix()) {
        double d = haversineMeters(gp->lat(), gp->lon(), rxPos_.lat, rxPos_.lon);
        double b = bearingDeg(gp->lat(), gp->lon(), rxPos_.lat, rxPos_.lon);
        std::snprintf(s, sizeof(s), "range %d m  brg %03d", (int)d, (int)b);
        g.drawString(s, 6, y); y += 12;
      } else {
        std::snprintf(s, sizeof(s), "pos %.4f,%.4f", rxPos_.lat, rxPos_.lon);
        g.drawString(s, 6, y); y += 12;
      }
      std::snprintf(s, sizeof(s), "their batt %u%%", rxBatt_);
      g.drawString(s, 6, y); y += 12;
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("r: respond   space: silence", 6, y);
    } else {
      g.setTextColor(theme::TEXT, theme::BG);
      g.drawString("Enter: broadcast distress", 6, y); y += 14;
      g.setTextColor(armed_ ? theme::WARN : theme::MUTED, theme::BG);
      std::snprintf(s, sizeof(s), "dead-man: %s (a to %s)",
                    armed_ ? "ARMED" : "off", armed_ ? "disarm" : "arm");
      g.drawString(s, 6, y); y += 12;
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("arms on stillness + low battery", 6, y);
    }
    ui::footer(g);
  }

  Mode mode() const { return mode_; }
  void panic() { if (mode_ != ACTIVE) armedConfirm_ = true; }   // arm confirm, don't TX yet

private:
  static const uint32_t CADENCE_MS = 15000;
  static const uint32_t MAX_DUR_MS = 600000;   // 10 min cap
  static const uint32_t STILL_MS   = 300000;   // 5 min stillness
  static const uint32_t RX_HOLD_MS = 45000;    // clear RECEIVING ~3 cadences after last rx
  static const int      BATT_CRIT  = 10;

  Mode mode_ = IDLE;
  bool armedConfirm_ = false;
  bool armed_ = false;
  bool still_ = false;
  uint32_t stillStart_ = 0, activatedAt_ = 0, lastTx_ = 0, sent_ = 0;
  uint16_t rxSrc_ = 0;
  uint8_t rxBatt_ = 0;
  uint32_t rxAt_ = 0;
  Position rxPos_;

  void activate() {
    mode_ = ACTIVE;
    activatedAt_ = millis();
    lastTx_ = 0;   // send immediately next background tick
    sent_ = 0;
  }
  void standDown() {
    if (mode_ == ACTIVE || mode_ == RECEIVING) mode_ = IDLE;
    still_ = false;              // require a fresh STILL_MS window before dead-man can re-arm
    stillStart_ = millis();
  }

  void sendDistress() {
    GpsService* gp = ctx.gps;
    Position p;
    if (gp && gp->hasFix()) { p.lat = gp->lat(); p.lon = gp->lon(); p.altM = (int16_t)gp->altM(); }
    int batt = M5.Power.getBatteryLevel();
    Frame f = makeDistress(p, (uint8_t)(batt < 0 ? 0 : batt));
    netSend(f, true);
    sent_++;
  }

  void respond() {
    Frame f = makeText(rxSrc_, "ON MY WAY", true);
    netSend(f, true);
  }
};

} // namespace ls
