#pragma once
#include "app.h"

namespace ls {

// A small screen stack. The launcher sits at the base; opening an app pushes it,
// ESC pops back. Received frames and key events route to the top screen.
class ScreenManager {
public:
  void begin(App* home);
  void push(App* a);
  void pop();
  App* top() { return depth_ ? stack_[depth_ - 1] : nullptr; }
  int  depth() const { return depth_; }

  void onKey(const KeyEvent& k);
  void onPacket(const Frame& f, const RxMeta& m);   // channel-matched, decrypted
  void onRaw(const Frame& f, const RxMeta& m);       // every frame heard
  void update();
  void draw(M5Canvas& g);

private:
  static const int MAXD = 8;
  App* stack_[MAXD] = {};
  int depth_ = 0;
};

} // namespace ls
