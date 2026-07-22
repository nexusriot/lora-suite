#include "screen_manager.h"

namespace ls {

void ScreenManager::begin(App* home) {
  depth_ = 0;
  stack_[depth_++] = home;
  home->onEnter();
}

void ScreenManager::push(App* a) {
  if (depth_ >= MAXD) return;
  if (App* t = top()) t->onExit();
  stack_[depth_++] = a;
  a->onEnter();
}

void ScreenManager::pop() {
  if (depth_ <= 1) return;   // never pop the launcher
  stack_[--depth_]->onExit();
  top()->onEnter();
}

void ScreenManager::onKey(const KeyEvent& k) {
  if (k.esc && depth_ > 1) { pop(); return; }
  if (App* t = top()) t->onKey(k);
}

void ScreenManager::onPacket(const Frame& f, const RxMeta& m) {
  if (App* t = top()) t->onPacket(f, m);
}

void ScreenManager::onRaw(const Frame& f, const RxMeta& m) {
  if (App* t = top()) t->onRawPacket(f, m);
}

void ScreenManager::update() {
  if (App* t = top()) t->update();
}

void ScreenManager::draw(M5Canvas& g) {
  if (App* t = top()) t->draw(g);
}

} // namespace ls
