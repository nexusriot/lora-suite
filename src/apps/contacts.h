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

// The durable contact roster: persist names, set aliases, block/favourite, and
// import currently-heard nodes. Send-by-name lives in Courier; this curates the
// address book it draws from.
class Contacts : public App {
public:
  const char* name() const override { return "Contacts"; }
  const char* callsign() const override { return "CB"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // person
    g.fillCircle(x + 10, y + 6, 4, c);
    g.fillRoundRect(x + 3, y + 12, 14, 8, 3, c);
  }

  // No onEnter reload: the boot-time load + live NODEINFO learning keep the
  // roster authoritative; reloading here would clobber contacts heard since the
  // last save. onExit persists user edits.
  void onExit() override { if (ctx.store) ctx.store->saveRoster(ctx.roster); }
  bool consumesText() const override { return editing_; }

  void onKey(const KeyEvent& k) override {
    if (editing_) { editKey(k); return; }
    size_t n = ctx.roster.size();
    if (k.down && n) sel_ = (sel_ + 1) % n;
    else if (k.up && n) sel_ = (sel_ + n - 1) % n;
    else if (k.ch == 'i') importNodes();
    else if (k.ch == 's' && ctx.store) ctx.store->saveRoster(ctx.roster);
    else if (n && sel_ < n) {
      Contact& c = ctx.roster.at(sel_);
      if (k.ch == 'b') c.blocked = !c.blocked;
      else if (k.ch == 'f') c.favorite = !c.favorite;
      else if (k.ch == 'a') { editing_ = true; elen_ = 0; ebuf_[0] = 0; }
    }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("a alias  b block  f fav  i import", 6, ui::BODY_Y + 2);

    int y = ui::BODY_Y + 14;
    char tmp[16];
    for (size_t i = 0; i < ctx.roster.size() && y < ui::SCREEN_H - ui::FOOTER_H - 10; i++) {
      const Contact& c = ctx.roster.at(i);
      bool s = (i == sel_);
      const char* lbl = ctx.roster.label(c.addr, tmp, sizeof(tmp));
      char row[40];
      std::snprintf(row, sizeof(row), "%s%04X %-10s %s%s", s ? ">" : " ",
                    c.addr, lbl, c.favorite ? "*" : " ", c.blocked ? "X" : " ");
      g.setTextColor(c.blocked ? theme::CRIT : (s ? theme::ACCENT : theme::TEXT), theme::BG);
      g.drawString(row, 6, y);
      y += 10;
    }
    if (ctx.roster.size() == 0) {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("(no contacts - press i to import)", 6, ui::BODY_Y + 16);
    }

    if (editing_) {
      g.fillRect(10, 50, 220, 26, theme::PANEL);
      g.drawRect(10, 50, 220, 26, theme::ACCENT);
      g.setTextColor(theme::MUTED, theme::PANEL);
      g.drawString("alias:", 16, 54);
      g.setTextColor(theme::TEXT, theme::PANEL);
      g.drawString(ebuf_, 60, 54);
    }
    ui::footer(g);
  }

private:
  size_t sel_ = 0;
  bool editing_ = false;
  char ebuf_[12] = {0};
  int elen_ = 0;

  void editKey(const KeyEvent& k) {
    if (k.enter) {
      if (sel_ < ctx.roster.size()) ctx.roster.setAlias(ctx.roster.at(sel_).addr, ebuf_);
      editing_ = false;
    } else if (k.esc) {
      editing_ = false;
    } else if (k.del) {
      if (elen_) ebuf_[--elen_] = 0;
    } else if (k.ch >= 32 && elen_ < (int)sizeof(ebuf_) - 1) {
      ebuf_[elen_++] = k.ch;
      ebuf_[elen_] = 0;
    }
  }

  void importNodes() {
    for (size_t i = 0; i < ctx.nodes.size(); i++) {
      const Node& n = ctx.nodes.at(i);
      Contact& c = ctx.roster.upsert(n.addr);
      if (n.name[0] && !c.name[0]) std::strncpy(c.name, n.name, sizeof(c.name) - 1);
    }
  }
};

} // namespace ls
