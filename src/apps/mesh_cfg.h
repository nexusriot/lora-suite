#pragma once
#include <M5Unified.h>
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../shell/net.h"
#include "../services/storage.h"
#include "../proto/meshtastic.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Which Meshtastic network we speak to: region, modem preset, channel name and
// PSK, plus the name/role we announce ourselves under. Every Meshtastic path
// (MeshScan, MeshChat, MeshTX, Sweep 'm', Console 'm', the BLE bridge) reads the
// resulting ctx.meshCfg, so this screen is the single place a mesh is joined.
//
// Editing follows Console: up/down pick a field, left/right change an enumerated
// one, Enter opens a text field for typing. 'a' announces NodeInfo immediately,
// 's' saves to NVS.
class MeshCfg : public App {
public:
  const char* name() const override { return "MeshCfg"; }
  const char* callsign() const override { return "MCFG"; }
  Cat category() const override { return Cat::RF; }
  bool consumesText() const override { return editing_; }   // only while typing

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // gear
    g.drawCircle(x + 9, y + 11, 5, c);
    g.fillCircle(x + 9, y + 11, 1, c);
    g.fillRect(x + 8, y + 3, 3, 3, c);
    g.fillRect(x + 8, y + 17, 3, 3, c);
    g.fillRect(x + 1, y + 10, 3, 3, c);
    g.fillRect(x + 16, y + 10, 3, 3, c);
  }

  void onKey(const KeyEvent& k) override {
    if (editing_) {
      if (k.enter) { commit(); return; }
      if (k.del) { if (elen_) ebuf_[--elen_] = 0; return; }
      if (k.ch >= 0x20 && k.ch < 0x7f && elen_ < (int)sizeof(ebuf_) - 1) {
        ebuf_[elen_++] = k.ch;
        ebuf_[elen_] = 0;
      }
      return;
    }
    if (k.down) field_ = (field_ + 1) % NFIELDS;
    else if (k.up) field_ = (field_ + NFIELDS - 1) % NFIELDS;
    else if (k.left) adjust(-1);
    else if (k.right) adjust(+1);
    else if (k.enter) beginEdit();
    else if (k.ch == 'a') announceNow();
    else if (k.ch == 's') save();
  }

  // Auto-announce runs wherever we are, because being visible on the mesh is not
  // something you want to depend on leaving a screen open. It is off by default:
  // each announce retunes the radio to the Meshtastic preset and back, so we are
  // briefly deaf to our own channel.
  void background() override {
    if (!ctx.meshCfg.announceMin) return;
    uint32_t now = millis();
    if (!lastAnnounce_) {
      if (now < 30000) return;             // let the radio settle after boot
    } else if (now - lastAnnounce_ < (uint32_t)ctx.meshCfg.announceMin * 60000UL) {
      return;
    }
    lastAnnounce_ = now;
    meshtasticSendNodeInfo();
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    const MeshtasticCfg& c = ctx.meshCfg;
    char val[16];   // sized so a long name truncates rather than running into the right column
    int y = ui::BODY_Y + 2;
    for (int i = 0; i < NFIELDS; i++) {
      fieldValue(i, val, sizeof(val));
      bool sel = i == field_;
      g.setTextColor(sel ? theme::ACCENT : theme::MUTED, theme::BG);
      g.drawString(sel ? ">" : " ", 2, y);
      g.drawString(FIELDS[i], 10, y);
      g.setTextColor(sel ? theme::TEXT : theme::MUTED, theme::BG);
      g.drawString(val, 56, y);
      y += 11;
    }

    // Derived values — what these settings actually put on the air.
    RadioCfg rc = meshtasticRadioCfg(c, c.preset);
    char d[20];
    int ry = ui::BODY_Y + 2;
    g.setTextColor(theme::RF, theme::BG);
    std::snprintf(d, sizeof(d), "%.4f MHz", rc.freqHz / 1e6);
    g.drawString(d, 150, ry); ry += 11;
    g.setTextColor(theme::MUTED, theme::BG);
    std::snprintf(d, sizeof(d), "SF%u %uk 4/%u", rc.sf, (unsigned)(rc.bwHz / 1000), rc.cr);
    g.drawString(d, 150, ry); ry += 11;
    std::snprintf(d, sizeof(d), "ch %u/%u", (unsigned)meshtasticChannelNum(c, c.preset),
                  (unsigned)meshtasticChannelCount(c, c.preset));
    g.drawString(d, 150, ry); ry += 11;
    std::snprintf(d, sizeof(d), "hash %02X", meshtasticChannelHash(c, c.preset));
    g.drawString(d, 150, ry); ry += 11;
    std::snprintf(d, sizeof(d), "!%08x", (unsigned)meshtasticNodeId(c, ctx.myAddr));
    g.drawString(d, 150, ry); ry += 11;
    std::snprintf(d, sizeof(d), "%d dBm", rc.power);
    g.drawString(d, 150, ry);

    int sy = ui::SCREEN_H - ui::FOOTER_H - 12;
    g.drawFastHLine(0, sy - 2, ui::SCREEN_W, theme::LINE);
    if (editing_) {
      g.setTextColor(theme::ACCENT, theme::BG);
      char line[44];
      std::snprintf(line, sizeof(line), "%s: %s_", FIELDS[field_], ebuf_);
      g.drawString(line, 4, sy);
    } else {
      g.setTextColor(status_[0] ? theme::GOOD : theme::MUTED, theme::BG);
      g.drawString(status_[0] ? status_ : "Enter=edit  a=announce  s=save", 4, sy);
    }
    ui::footer(g);
  }

private:
  enum { F_REGION = 0, F_PRESET, F_CHAN, F_PSK, F_LONG, F_SHORT, F_ROLE, F_ANNOUNCE, NFIELDS };
  static constexpr const char* FIELDS[NFIELDS] = {
      "REGION", "PRESET", "CHAN", "PSK", "LONG", "SHORT", "ROLE", "ANNC"};
  static constexpr uint16_t ANNOUNCE_OPTS[4] = {0, 15, 30, 60};

  int field_ = 0;
  bool editing_ = false;
  char ebuf_[48] = {0};
  int elen_ = 0;
  char status_[36] = {0};
  uint32_t lastAnnounce_ = 0;

  static bool isTextField(int f) {
    return f == F_CHAN || f == F_PSK || f == F_LONG || f == F_SHORT;
  }

  void fieldValue(int f, char* out, size_t cap) {
    const MeshtasticCfg& c = ctx.meshCfg;
    switch (f) {
      case F_REGION: std::snprintf(out, cap, "%s", meshtasticRegionName(c.region)); break;
      case F_PRESET: std::snprintf(out, cap, "%s", meshtasticPresetName(c.preset)); break;
      case F_CHAN:
        std::snprintf(out, cap, "%s", c.chanName[0] ? c.chanName : "(preset)");
        break;
      case F_PSK: pskLabel(out, cap); break;
      case F_LONG:
        std::snprintf(out, cap, "%s", c.longName[0] ? c.longName : "(callsign)");
        break;
      case F_SHORT:
        std::snprintf(out, cap, "%s", c.shortName[0] ? c.shortName : "(auto)");
        break;
      case F_ROLE: std::snprintf(out, cap, "%s", meshtasticRoleName(c.role)); break;
      default:
        if (c.announceMin) std::snprintf(out, cap, "%u min", c.announceMin);
        else std::snprintf(out, cap, "off");
        break;
    }
  }

  // The full base64 rarely fits the column, so show what actually matters: which
  // key this is and how long it is.
  void pskLabel(char* out, size_t cap) {
    const MeshtasticCfg& c = ctx.meshCfg;
    if (c.keyLen == 0) { std::snprintf(out, cap, "none"); return; }
    char b64[48];
    meshtasticFormatPsk(c.key, c.keyLen, b64, sizeof(b64));
    if (std::strlen(b64) <= 10) std::snprintf(out, cap, "%s", b64);
    else std::snprintf(out, cap, "%.7s.. %uB", b64, c.keyLen);
  }

  void adjust(int d) {
    MeshtasticCfg& c = ctx.meshCfg;
    status_[0] = 0;
    switch (field_) {
      case F_REGION:
        c.region = (uint8_t)((c.region + MESH_REGION_COUNT + d) % MESH_REGION_COUNT);
        break;
      case F_PRESET:
        c.preset = (uint8_t)((c.preset + MESH_PRESET_COUNT + d) % MESH_PRESET_COUNT);
        break;
      case F_ROLE: {
        uint8_t i = meshtasticRoleIndex(c.role);
        c.role = meshtasticRoleAt((uint8_t)((i + MESH_ROLE_COUNT + d) % MESH_ROLE_COUNT));
        break;
      }
      case F_ANNOUNCE: {
        int i = 0;
        for (int j = 0; j < 4; j++) if (ANNOUNCE_OPTS[j] == c.announceMin) i = j;
        i = (i + 4 + d) % 4;
        c.announceMin = ANNOUNCE_OPTS[i];
        lastAnnounce_ = 0;
        break;
      }
      default: break;   // text fields are opened with Enter, not stepped
    }
  }

  void beginEdit() {
    if (!isTextField(field_)) return;
    const MeshtasticCfg& c = ctx.meshCfg;
    switch (field_) {
      case F_CHAN:  std::snprintf(ebuf_, sizeof(ebuf_), "%s", c.chanName); break;
      case F_PSK:   meshtasticFormatPsk(c.key, c.keyLen, ebuf_, sizeof(ebuf_)); break;
      case F_LONG:  std::snprintf(ebuf_, sizeof(ebuf_), "%s", c.longName); break;
      default:      std::snprintf(ebuf_, sizeof(ebuf_), "%s", c.shortName); break;
    }
    elen_ = (int)std::strlen(ebuf_);
    editing_ = true;
    status_[0] = 0;
  }

  void commit() {
    MeshtasticCfg& c = ctx.meshCfg;
    editing_ = false;
    switch (field_) {
      case F_CHAN:
        std::snprintf(c.chanName, sizeof(c.chanName), "%s", ebuf_);
        break;
      case F_PSK: {
        uint8_t key[32], len;
        if (!meshtasticParsePsk(ebuf_, key, len)) {
          std::snprintf(status_, sizeof(status_), "bad PSK - key unchanged");
          return;
        }
        std::memcpy(c.key, key, sizeof(key));
        c.keyLen = len;
        break;
      }
      case F_LONG:
        std::snprintf(c.longName, sizeof(c.longName), "%s", ebuf_);
        break;
      default:
        std::snprintf(c.shortName, sizeof(c.shortName), "%s", ebuf_);
        break;
    }
    std::snprintf(status_, sizeof(status_), "set - press s to save");
  }

  void announceNow() {
    if (meshtasticSendNodeInfo()) {
      char ln[20], sn[5];
      meshtasticNames(ln, sizeof(ln), sn, sizeof(sn));
      std::snprintf(status_, sizeof(status_), "announced as %s (%s)", sn, ln);
      lastAnnounce_ = millis();
    } else {
      std::snprintf(status_, sizeof(status_), "announce failed");
    }
  }

  void save() {
    if (!ctx.store) { std::snprintf(status_, sizeof(status_), "no storage"); return; }
    ctx.store->saveMeshCfg(ctx.meshCfg);
    std::snprintf(status_, sizeof(status_), "saved");
  }
};

} // namespace ls
