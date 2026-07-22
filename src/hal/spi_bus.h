#pragma once

// The SX1262 radio and the microSD card share one SPI bus (SCK G40 / MOSI G14 /
// MISO G39) with separate chip-selects. Every SPI transaction — radio TX/RX and
// card read/write alike — must be wrapped in SpiBus::Guard so a card write can
// never interleave with a radio transfer and corrupt either device.
namespace ls {

class SpiBus {
public:
  static void begin();
  static void lock();
  static void unlock();

  struct Guard {
    Guard()  { SpiBus::lock(); }
    ~Guard() { SpiBus::unlock(); }
  };
};

} // namespace ls
