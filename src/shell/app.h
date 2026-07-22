#pragma once
#include <cstdint>
#include "../proto/frame.h"

class M5Canvas; // LovyanGFX sprite from the M5GFX library

namespace ls {

struct KeyEvent {
  char ch    = 0;      // printable character, or 0
  bool enter = false;
  bool del   = false;  // backspace
  bool esc   = false;  // handled by the shell to pop the app
  bool tab   = false;
  bool up = false, down = false, left = false, right = false;
};

struct RxMeta {
  int16_t  rssi = 0;
  int8_t   snr  = 0;
  uint32_t when = 0;   // millis()
};

enum class Cat : uint8_t { Comms, Location, RF, Util };

// Every screen implements this. The shell owns the lifecycle; apps stay thin
// and reach shared radio/GPS/storage state through the global Context (ctx).
class App {
public:
  virtual ~App() {}
  virtual const char* name() const = 0;
  virtual const char* callsign() const = 0;
  virtual Cat category() const = 0;

  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void onKey(const KeyEvent&) {}
  virtual void onPacket(const Frame&, const RxMeta&) {}  // frames on our channel
  virtual void onRawPacket(const Frame&, const RxMeta&) {} // every frame (Monitor)
  virtual void update() {}                                // only while foreground
  virtual void background() {}                            // every loop, foreground or not
  virtual bool consumesText() const { return false; }     // true while capturing typed text
  // Draw a ~20x20 launcher glyph in `color` at (x,y). Default: no glyph (the
  // launcher still labels the tile with the callsign). Apps override to add one.
  virtual void drawIcon(M5Canvas& g, int x, int y, uint16_t color) { (void)g; (void)x; (void)y; (void)color; }
  virtual void draw(M5Canvas& g) = 0;
};

} // namespace ls
