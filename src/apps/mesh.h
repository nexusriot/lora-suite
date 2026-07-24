#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/storage.h"
#include "../services/gps_service.h"
#include "../services/clock.h"
#include "../proto/meshoverlay.h"
#include "../proto/geo.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Foreign Meshtastic nodes imported from a meshmap.net snapshot (SD:/mesh/import.csv,
// produced by tools/meshpull) — and, once the scanner lands, heard over the air.
// Read-only situational awareness: these are NOT our peers, so we can't message or
// relay them. Nearest-first when we have a fix; 'r' reloads, up/down scroll, Enter
// opens a full detail card. Rows dim as their meshmap "last heard" ages.
class Mesh : public App {
public:
  const char* name() const override { return "Mesh"; }
  const char* callsign() const override { return "MESH"; }
  Cat category() const override { return Cat::Location; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // crosshaired globe
    g.drawCircle(x + 10, y + 10, 8, c);
    g.drawFastHLine(x + 2, y + 10, 17, c);
    g.drawFastVLine(x + 10, y + 2, 17, c);
  }

  void onEnter() override { reload(); }

  void onKey(const KeyEvent& k) override {
    size_t n = ctx.mesh.size();
    if (k.ch == 'r') { reload(); card_ = false; }
    else if (k.enter) { if (n > 0) card_ = !card_; }
    else if (!card_ && k.up && sel_ > 0) sel_--;
    else if (!card_ && k.down && (size_t)(sel_ + 1) < n) sel_++;
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    MeshOverlay& mesh = ctx.mesh;
    size_t n = mesh.size();

    if (n == 0) {                       // nothing imported or scanned yet
      if (!ctx.store || !ctx.store->sdReady()) {
        g.setTextColor(theme::WARN, theme::BG);
        g.drawString("no nodes", 6, ui::BODY_Y + 4);
        g.setTextColor(theme::MUTED, theme::BG);
        g.drawString("SCAN, or import to SD", 6, ui::BODY_Y + 18);
      } else if (!loaded_) {
        g.setTextColor(theme::WARN, theme::BG);
        g.drawString("no /mesh/import.csv", 6, ui::BODY_Y + 4);
        g.setTextColor(theme::MUTED, theme::BG);
        g.drawString("run meshpull -> SD, 'r'", 6, ui::BODY_Y + 18);
      } else {
        g.setTextColor(theme::MUTED, theme::BG);
        g.drawString("(empty snapshot)", 6, ui::BODY_Y + 4);
      }
      ui::footer(g);
      return;
    }

    GpsService* gp = ctx.gps;
    bool haveSelf = gp && gp->hasFix();

    char hd[32], age[12];
    ageAgo(age, sizeof(age), mesh.generatedUnix(), "snapshot");
    std::snprintf(hd, sizeof(hd), "%u nodes  %s", (unsigned)n, age);
    g.setTextColor(theme::LOC, theme::BG);
    g.drawString(hd, 6, ui::BODY_Y + 2);

    if (sel_ >= (int)n) sel_ = (int)n - 1;
    if (sel_ < 0) sel_ = 0;

    char rt[10];
    if (card_) std::snprintf(rt, sizeof(rt), "Enter=x");
    else if ((int)n > VIS) std::snprintf(rt, sizeof(rt), "%d/%u", sel_ + 1, (unsigned)n);
    else std::snprintf(rt, sizeof(rt), "Enter");
    g.setTextColor(theme::MUTED, theme::BG);
    ui::textRight(g, ui::SCREEN_W - 4, ui::BODY_Y + 2, rt);

    // nearest-first when we have a fix; unknown/without position sink to the end.
    uint8_t order[MeshOverlay::CAP];
    double  key[MeshOverlay::CAP];
    for (size_t i = 0; i < n; i++) {
      order[i] = (uint8_t)i;
      const MeshNode& m = mesh.at(i);
      key[i] = (haveSelf && m.hasPos) ? haversineMeters(gp->lat(), gp->lon(), m.lat, m.lon) : 1e18;
    }
    if (haveSelf) {
      for (size_t i = 1; i < n; i++) {
        uint8_t v = order[i];
        double kv = key[v];
        size_t j = i;
        while (j > 0 && key[order[j - 1]] > kv) { order[j] = order[j - 1]; j--; }
        order[j] = v;
      }
    }

    if (card_) { drawCard(g, mesh.at(order[sel_]), gp, haveSelf); ui::footer(g); return; }

    if (sel_ < first_) first_ = sel_;
    if (sel_ >= first_ + VIS) first_ = sel_ - VIS + 1;
    if (first_ > (int)n - VIS) first_ = (int)n - VIS;
    if (first_ < 0) first_ = 0;

    int y = ui::BODY_Y + 15;
    for (int i = first_; i < (int)n && i < first_ + VIS; i++) {
      const MeshNode& m = mesh.at(order[i]);
      bool s = (i == sel_);
      char shortLbl[6];
      if (m.shortName[0]) std::snprintf(shortLbl, sizeof(shortLbl), "%s", m.shortName);
      else std::snprintf(shortLbl, sizeof(shortLbl), "%04X", (unsigned)(m.id & 0xFFFF));
      char b[6];
      battLabel(b, sizeof(b), m.battPct);

      char row[44];
      if (haveSelf) {
        char dist[8];
        distStr(dist, sizeof(dist), m.hasPos ? haversineMeters(gp->lat(), gp->lon(), m.lat, m.lon) : -1);
        int brg = m.hasPos ? (int)bearingDeg(gp->lat(), gp->lon(), m.lat, m.lon) : -1;
        char brgs[5];
        if (brg >= 0) std::snprintf(brgs, sizeof(brgs), "%03d", brg);
        else std::snprintf(brgs, sizeof(brgs), " - ");
        std::snprintf(row, sizeof(row), "%c%-4s %6s %s %-4s %-4s",
                      s ? '>' : ' ', shortLbl, dist, brgs, b, meshRoleLabel(m.role));
      } else {
        std::snprintf(row, sizeof(row), "%c%-4s  %-4s %-5s",
                      s ? '>' : ' ', shortLbl, b, meshRoleLabel(m.role));
      }
      g.setTextColor(rowColor(m.seenEpoch, s), theme::BG);
      g.drawString(row, 4, y);
      y += 10;
    }
    ui::footer(g);
  }

private:
  static const int VIS = 8;
  int sel_ = 0;
  int first_ = 0;
  bool loaded_ = false;
  bool card_ = false;

  void reload() {
    loaded_ = ctx.store && ctx.store->loadMeshImport(ctx.mesh, millis());
    sel_ = 0;
    first_ = 0;
  }

  void drawCard(M5Canvas& g, const MeshNode& m, GpsService* gp, bool haveSelf) {
    char line[48];
    int y = ui::BODY_Y + 15;
    g.setTextColor(theme::ACCENT, theme::BG);
    std::snprintf(line, sizeof(line), "%s  !%08lx", m.shortName[0] ? m.shortName : "----", (unsigned long)m.id);
    g.drawString(line, 6, y); y += 12;

    g.setTextColor(theme::TEXT, theme::BG);
    g.drawString(m.longName[0] ? m.longName : "(no name)", 6, y); y += 11;

    std::snprintf(line, sizeof(line), "role %s   hw %s", meshRoleLabel(m.role), m.hw[0] ? m.hw : "?");
    g.drawString(line, 6, y); y += 11;

    char b[6];
    battLabel(b, sizeof(b), m.battPct);
    if (m.voltCv) std::snprintf(line, sizeof(line), "batt %-4s  %u.%02uV", b, m.voltCv / 100, m.voltCv % 100);
    else std::snprintf(line, sizeof(line), "batt %s", b);
    g.drawString(line, 6, y); y += 11;

    if (m.hasPos) {
      std::snprintf(line, sizeof(line), "%.5f %.5f", m.lat, m.lon);
      g.drawString(line, 6, y); y += 11;
      if (haveSelf) {
        char ds[8];
        distStr(ds, sizeof(ds), haversineMeters(gp->lat(), gp->lon(), m.lat, m.lon));
        int brg = (int)bearingDeg(gp->lat(), gp->lon(), m.lat, m.lon);
        std::snprintf(line, sizeof(line), "%s  %03d deg", ds, brg);
        g.drawString(line, 6, y); y += 11;
      }
    } else {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("no position", 6, y); y += 11;
    }

    ageAgo(line, sizeof(line), m.seenEpoch, "?");
    char full[40];
    std::snprintf(full, sizeof(full), "seen %s  %s", line, m.source == SRC_SCAN ? "scanned" : "import");
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString(full, 6, y);
  }

  static void battLabel(char* buf, size_t cap, uint8_t batt) {
    if (batt == 0) std::snprintf(buf, cap, "?");
    else if (batt > 100) std::snprintf(buf, cap, "EXT");
    else std::snprintf(buf, cap, "%d%%", batt);
  }

  static void distStr(char* buf, size_t cap, double d) {
    if (d < 0) std::snprintf(buf, cap, "   ?");
    else if (d < 1000) std::snprintf(buf, cap, "%dm", (int)d);
    else if (d < 100000) std::snprintf(buf, cap, "%.1fk", d / 1000.0);
    else std::snprintf(buf, cap, ">99k");
  }

  // Fresh (<1h via MQTT) reads bright; older dims. Selected always wins.
  uint16_t rowColor(uint32_t seenEpoch, bool sel) {
    if (sel) return theme::ACCENT;
    uint32_t nowUtc = ctx.clock ? ctx.clock->utc() : 0;
    if (seenEpoch == 0 || nowUtc == 0 || nowUtc < seenEpoch) return theme::TEXT;
    return (nowUtc - seenEpoch < 3600) ? theme::TEXT : theme::MUTED;
  }

  // "4m ago" / "2h ago" from an epoch vs our UTC clock, or `unknown` if we can't tell.
  void ageAgo(char* buf, size_t cap, uint32_t epoch, const char* unknown) {
    uint32_t nowUtc = ctx.clock ? ctx.clock->utc() : 0;
    if (epoch == 0 || nowUtc == 0 || nowUtc < epoch) { std::snprintf(buf, cap, "%s", unknown); return; }
    uint32_t s = nowUtc - epoch;
    if (s < 90) std::snprintf(buf, cap, "%us ago", (unsigned)s);
    else if (s < 5400) std::snprintf(buf, cap, "%um ago", (unsigned)(s / 60));
    else std::snprintf(buf, cap, "%uh ago", (unsigned)(s / 3600));
  }
};

} // namespace ls
