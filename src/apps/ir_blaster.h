#pragma once
#include <M5Unified.h>
#include <cstdio>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/ir.h"
#include "../services/audio.h"
#include "../services/storage.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// IR remote — sends NEC codes over the IR LED. The code table is user data
// (`ctx.irCodes`, persisted in NVS and editable from the phone over the BLE
// bridge) rather than a hardcoded list, so the device can be programmed to drive
// your own gear; a generic set is loaded the first time it runs. Uses the raw NEC
// protocol (see services/ir.h).
class IrBlaster : public App {
public:
  const char* name() const override { return "IR"; }
  const char* callsign() const override { return "IR"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // IR LED + beam
    g.fillCircle(x + 5, y + 10, 3, c);
    g.drawLine(x + 9, y + 6, x + 16, y + 3, c);
    g.drawLine(x + 9, y + 10, x + 17, y + 10, c);
    g.drawLine(x + 9, y + 14, x + 16, y + 17, c);
  }

  void onEnter() override { ir::init(); }

  void onKey(const KeyEvent& k) override {
    int n = (int)ctx.irCodes.size();
    if (k.up && sel_ > 0) sel_--;
    else if (k.down && sel_ + 1 < n) sel_++;
    else if (k.enter && n) {
      const IrCode& c = ctx.irCodes.at(sel_);
      ir::sendNEC(c.addr, c.cmd);
      audio::tick();
      sent_ = sel_ + 1;
    } else if (k.ch == 'd' && n) {            // delete the selected code
      ctx.irCodes.remove(sel_);
      if (sel_ && sel_ >= (int)ctx.irCodes.size()) sel_--;
      if (ctx.store) ctx.store->saveIrCodes(ctx.irCodes);
      sent_ = 0;
    } else if (k.ch == 'r') {                 // restore the generic set
      ctx.irCodes.loadDefaults();
      if (ctx.store) ctx.store->saveIrCodes(ctx.irCodes);
      sel_ = 0;
      sent_ = 0;
    }
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 2;
    int n = (int)ctx.irCodes.size();

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("NEC codes (edit from phone):", 6, y);
    y += 12;

    if (!n) {
      g.setTextColor(theme::WARN, theme::BG);
      g.drawString("no codes - press r", 6, y);
    } else {
      int start = sel_ > 5 ? sel_ - 5 : 0;
      for (int i = start; i < n && i < start + 6; i++) {
        const IrCode& c = ctx.irCodes.at(i);
        bool s = (i == sel_);
        char row[36];
        std::snprintf(row, sizeof(row), "%c%-11s %02X:%02X", s ? '>' : ' ', c.label, c.addr, c.cmd);
        g.setTextColor(s ? theme::ACCENT : theme::TEXT, theme::BG);
        g.drawString(row, 6, y);
        y += 11;
      }
    }

    g.setTextColor(theme::MUTED, theme::BG);
    if (sent_ && sent_ <= n) {
      char s[28];
      std::snprintf(s, sizeof(s), "sent %s", ctx.irCodes.at(sent_ - 1).label);
      g.drawString(s, 6, ui::SCREEN_H - ui::FOOTER_H - 20);
    }
    g.drawString("Enter=send d=del r=reset", 6, ui::SCREEN_H - ui::FOOTER_H - 10);
    ui::footer(g);
  }

private:
  int sel_ = 0, sent_ = 0;
};

} // namespace ls
