#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "app.h"
#include "screen_manager.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Home screen: a grid of category-coloured app chips, arrow-key navigation,
// Enter to open. Never popped (it is the base of the screen stack).
class Launcher : public App {
public:
  Launcher(App** apps, int count, ScreenManager* sm)
      : apps_(apps), count_(count), sm_(sm) {}

  const char* name() const override { return "LoRa Suite"; }
  const char* callsign() const override { return "HOME"; }
  Cat category() const override { return Cat::Util; }

  void onKey(const KeyEvent& k) override {
    if (k.right)      sel_ = (sel_ + 1) % count_;
    else if (k.left)  sel_ = (sel_ + count_ - 1) % count_;
    else if (k.down)  sel_ = (sel_ + COLS) % count_;
    else if (k.up)    sel_ = (sel_ + count_ - COLS) % count_;
    else if (k.enter) sm_->push(apps_[sel_]);
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);

    const int cw = 56, ch = 30, gap = 2, x0 = 4, y0 = ui::BODY_Y + 1;
    const int rowH = ch + gap;
    const int visRows = ui::BODY_H / rowH;                 // rows that fit
    const int totalRows = (count_ + COLS - 1) / COLS;
    int selRow = sel_ / COLS;
    if (selRow < first_) first_ = selRow;
    if (selRow >= first_ + visRows) first_ = selRow - visRows + 1;
    if (first_ > totalRows - visRows) first_ = totalRows - visRows;
    if (first_ < 0) first_ = 0;

    for (int i = 0; i < count_; i++) {
      int r = i / COLS, c = i % COLS;
      if (r < first_ || r >= first_ + visRows) continue;
      int x = x0 + c * (cw + gap), y = y0 + (r - first_) * rowH;
      bool s = (i == sel_);
      uint16_t col = theme::forCat(apps_[i]->category());
      uint16_t bg = s ? theme::LINE : theme::PANEL;   // lifted bg makes selection read on any category
      g.fillRect(x, y, cw, ch, bg);
      g.drawRect(x, y, cw, ch, s ? col : theme::LINE);
      if (s) g.drawRect(x - 1, y - 1, cw + 2, ch + 2, col);
      apps_[i]->drawIcon(g, x + (cw - 20) / 2, y + 2, s ? col : theme::MUTED);
      g.setTextColor(s ? col : theme::MUTED, bg);
      g.setTextDatum(top_center);
      g.drawString(apps_[i]->callsign(), x + cw / 2, y + 22);
      g.setTextDatum(top_left);
    }

    if (visRows < totalRows) {   // scroll hint
      g.setTextColor(theme::MUTED, theme::BG);
      char s[8];
      std::snprintf(s, sizeof(s), "%d/%d", sel_ + 1, count_);
      ui::textRight(g, ui::SCREEN_W - 4, ui::BODY_Y + 2, s);
    }
    ui::footer(g);
  }

private:
  static const int COLS = 4;
  App** apps_;
  int count_;
  ScreenManager* sm_;
  int sel_ = 0;
  int first_ = 0;
};

} // namespace ls
