#pragma once
#include <Arduino.h>
#include "../proto/nec.h"

namespace ls {

// Minimal IR transmitter (NEC protocol) over the Cardputer's IR LED, using an
// LEDC 38 kHz carrier gated by bit timing. VERIFY the IR LED GPIO for the Adv on
// hardware (44 is the original Cardputer's IR pin).
namespace ir {

constexpr int PIN = 44;    // IR LED GPIO — VERIFY on the Adv
constexpr int CH = 4;      // LEDC channel (0/1 are typically the display backlight)

inline void init() {
  ledcSetup(CH, 38000, 8);
  ledcAttachPin(PIN, CH);
  ledcWrite(CH, 0);
}

inline void mark(uint32_t us)  { ledcWrite(CH, 128); delayMicroseconds(us); ledcWrite(CH, 0); }
inline void space(uint32_t us) { delayMicroseconds(us); }

// Standard NEC: 9 ms lead mark, 4.5 ms space, 32 bits LSB-first
// (addr, ~addr, cmd, ~cmd), 560 us stop mark. ~68 ms total (blocks briefly).
inline void sendNEC(uint8_t addr, uint8_t cmd) {
  uint32_t data = necWord(addr, cmd);
  mark(9000);
  space(4500);
  for (int i = 0; i < 32; i++) {
    mark(560);
    space((data & 1) ? 1690 : 560);
    data >>= 1;
  }
  mark(560);
}

} // namespace ir
} // namespace ls
