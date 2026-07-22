#pragma once
#include <M5Unified.h>
#include <cstdio>
#include <cstring>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/storage.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Persistent message history. Captured on the hot paths into ctx.archive; this
// app drains that FIFO to /archive.csv from background() (off the radio RX path)
// and browses the tail of the file with scroll + a substring filter. Survives
// reboots, unlike Courier's RAM ring.
class Archive : public App {
public:
  const char* name() const override { return "Archive"; }
  const char* callsign() const override { return "HIST"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // archive box
    g.drawRect(x + 2, y + 5, 16, 13, c);
    g.drawFastHLine(x + 2, y + 9, 16, c);
    g.fillRect(x + 8, y + 11, 4, 2, c);
  }

  bool consumesText() const override { return filterMode_; }

  void onEnter() override { load(); }

  void background() override {   // drain the capture FIFO to SD, off the RX path
    if (!ctx.store) return;
    uint32_t now = millis();
    if (now - lastFlush_ < 1000) return;
    lastFlush_ = now;
    int guard = 0;
    while (ctx.archive.pending() && guard++ < 32) {
      if (ctx.store->appendLine(PATH, ctx.archive.front())) ctx.archive.popFront();
      else break;
    }
  }

  void onKey(const KeyEvent& k) override {
    if (filterMode_) {
      if (k.enter) { filterMode_ = false; toEnd_ = true; }
      else if (k.del) { if (flen_) filter_[--flen_] = 0; }
      else if (k.ch >= 32 && flen_ < (int)sizeof(filter_) - 1) { filter_[flen_++] = k.ch; filter_[flen_] = 0; }
      return;
    }
    if (k.up && first_ > 0) first_--;
    else if (k.down) first_++;                 // clamped in draw
    else if (k.ch == 'f') { filterMode_ = true; filter_[0] = 0; flen_ = 0; }  // '/' is remapped to arrow
    else if (k.ch == 'r') load();
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    // filtered view
    int fidx[MAXL];
    int fn = 0;
    for (int i = 0; i < lineCount_; i++)
      if (!filter_[0] || std::strstr(lines_[i], filter_)) fidx[fn++] = i;

    char info[40];
    if (filterMode_) std::snprintf(info, sizeof(info), "find:%s_", filter_);
    else if (filter_[0]) std::snprintf(info, sizeof(info), "filter:%s (%d)", filter_, fn);
    else std::snprintf(info, sizeof(info), "%d msgs   f filter  r reload", lineCount_);
    g.setTextColor(filter_[0] ? theme::ACCENT : theme::MUTED, theme::BG);
    g.drawString(info, 6, ui::BODY_Y + 2);

    const int visRows = 9;
    if (toEnd_) { first_ = fn - visRows; toEnd_ = false; }
    if (first_ > fn - visRows) first_ = fn - visRows;
    if (first_ < 0) first_ = 0;

    if (lineCount_ == 0) {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString(ctx.store && ctx.store->sdReady() ? "(no history yet)" : "(no microSD)",
                   6, ui::BODY_Y + 18);
      ui::footer(g);
      return;
    }

    int y = ui::BODY_Y + 14;
    for (int i = first_; i < fn && i < first_ + visRows; i++) {
      const char* line = lines_[fidx[i]];
      // dir char sits just after the 8-char time + comma: "hh:mm:ss,X,..."
      char dir = (std::strlen(line) > 9) ? line[9] : 'I';
      g.setTextColor(dir == 'O' ? theme::ACCENT : theme::TEXT, theme::BG);
      g.drawString(line, 4, y);
      y += 10;
    }
    ui::footer(g);
  }

private:
  static constexpr const char* PATH = "/archive.csv";
  static const int BUFSZ = 1500;
  static const int MAXL = 48;

  char buf_[BUFSZ + 1] = {0};
  char* lines_[MAXL] = {nullptr};
  int lineCount_ = 0;
  int first_ = 0;
  bool toEnd_ = true;
  bool filterMode_ = false;
  char filter_[16] = {0};
  int flen_ = 0;
  uint32_t lastFlush_ = 0;

  void load() {
    lineCount_ = 0;
    first_ = 0;
    toEnd_ = true;
    buf_[0] = 0;
    if (!ctx.store) return;
    int n = ctx.store->readTail(PATH, (uint8_t*)buf_, BUFSZ);
    if (n <= 0) return;
    buf_[n] = 0;

    char* p = buf_;
    if (n >= BUFSZ) {                         // tail likely mid-line: drop the partial head
      char* nl = std::strchr(buf_, '\n');
      if (nl) p = nl + 1;
    }
    while (*p) {
      char* line = p;
      char* nl = std::strchr(p, '\n');
      if (nl) {
        *nl = 0;
        if (nl > line && nl[-1] == '\r') nl[-1] = 0;   // strip CR (appendLine writes CRLF)
        p = nl + 1;
      } else {
        p += std::strlen(p);
      }
      if (*line) {
        if (lineCount_ < MAXL) lines_[lineCount_++] = line;
        else { for (int i = 1; i < MAXL; i++) lines_[i - 1] = lines_[i]; lines_[MAXL - 1] = line; }
      }
      if (!nl) break;
    }
  }
};

} // namespace ls
