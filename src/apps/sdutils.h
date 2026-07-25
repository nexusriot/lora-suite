#pragma once
#include <M5Unified.h>
#include <SD.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/storage.h"
#include "../hal/spi_bus.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// microSD utilities: mount status + capacity, remount, and a confirm-gated
// erase-all (recursive delete of every file — the practical "format", since the
// Arduino SD API doesn't expose a low-level FAT reformat). All SD access goes
// through the shared SPI-bus guard so it never collides with the radio.
class SdUtils : public App {
public:
  const char* name() const override { return "SD"; }
  const char* callsign() const override { return "SD"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // SD card
    g.drawRect(x + 4, y + 2, 12, 16, c);
    g.drawLine(x + 4, y + 5, x + 7, y + 2, c);
    for (int i = 0; i < 3; i++) g.drawFastVLine(x + 7 + i * 2, y + 3, 3, c);
  }

  void onEnter() override { refresh(); }

  void onKey(const KeyEvent& k) override {
    if (k.ch == 'r') { remount(); armed_ = false; }
    else if (k.ch == 'e') armed_ = true;
    else if (k.enter && armed_) { eraseAll(); armed_ = false; refresh(); }
    else if (k.esc) armed_ = false;   // note: shell pops on esc; harmless
    else armed_ = false;
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 4;
    char s[40];

    if (!mounted_) {
      g.setTextColor(theme::WARN, theme::BG);
      g.drawString("no card / not mounted", 6, y); y += 14;
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("insert a card, r = remount", 6, y);
      ui::footer(g);
      return;
    }

    g.setTextColor(theme::GOOD, theme::BG);
    std::snprintf(s, sizeof(s), "%s   %llu MB", typeStr_, (unsigned long long)sizeMB_);
    g.drawString(s, 6, y); y += 14;
    g.setTextColor(theme::TEXT, theme::BG);
    std::snprintf(s, sizeof(s), "used %llu MB", (unsigned long long)usedMB_);
    g.drawString(s, 6, y); y += 11;
    std::snprintf(s, sizeof(s), "free %llu MB", (unsigned long long)(totalMB_ - usedMB_));
    g.drawString(s, 6, y); y += 16;

    if (armed_) {
      g.setTextColor(theme::CRIT, theme::BG);
      g.drawString("ERASE ALL? Enter=yes", 6, y);
    } else {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("r=remount   e=erase all", 6, y);
    }
    ui::footer(g);
  }

private:
  bool mounted_ = false, armed_ = false;
  const char* typeStr_ = "?";
  uint64_t sizeMB_ = 0, usedMB_ = 0, totalMB_ = 0;

  void refresh() {
    SpiBus::Guard g;
    mounted_ = ctx.store && ctx.store->sdReady();
    if (!mounted_) return;
    switch (SD.cardType()) {
      case CARD_MMC:  typeStr_ = "MMC"; break;
      case CARD_SD:   typeStr_ = "SD"; break;
      case CARD_SDHC: typeStr_ = "SDHC"; break;
      default:        typeStr_ = "card"; break;
    }
    sizeMB_ = SD.cardSize() / (1024ull * 1024ull);
    totalMB_ = SD.totalBytes() / (1024ull * 1024ull);
    usedMB_ = SD.usedBytes() / (1024ull * 1024ull);
  }

  void remount() {
    SpiBus::Guard g;
    SD.end();
    if (ctx.store) ctx.store->sdBegin();
  }

  // Re-open "/" and delete its first entry until empty (avoids iterator invalidation).
  static bool removeOne(const char* dirPath) {
    File dir = SD.open(dirPath);
    if (!dir) return false;
    File e = dir.openNextFile();
    if (!e) { dir.close(); return false; }
    String p = e.path();
    bool isDir = e.isDirectory();
    e.close();
    dir.close();
    if (isDir) { while (removeOne(p.c_str())) {} SD.rmdir(p.c_str()); }
    else SD.remove(p.c_str());
    return true;
  }

  void eraseAll() {
    SpiBus::Guard g;
    while (removeOne("/")) {}
  }
};

} // namespace ls
