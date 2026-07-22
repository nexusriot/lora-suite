#pragma once
#include <cstdint>

namespace ls {

// Radio parameters shared by every app and persisted by Console.
struct RadioCfg {
  uint32_t freqHz    = 868000000; // 868.0 MHz (EU868 default)
  uint32_t bwHz      = 125000;    // 125 kHz
  uint8_t  sf        = 9;         // spreading factor 7..12
  uint8_t  cr         = 5;        // coding rate denominator 5..8 (=> 4/5..4/8)
  uint16_t preamble  = 8;         // preamble symbols
  bool     explicitHeader = true; // LoRa explicit header on
  bool     crc       = true;      // hardware CRC on
  int8_t   power     = 14;        // TX power dBm (EU868 legal default)
  uint8_t  syncWord  = 0x12;      // 0x12 private / 0x34 public (LoRaWAN)
};

// LoRa time-on-air in milliseconds (Semtech formula). Low-data-rate optimize
// is auto-enabled when the symbol time exceeds 16 ms (SF11/SF12 @ BW125).
double timeOnAirMs(const RadioCfg& c, uint8_t payloadLen);

// Free-space path loss (dB) for a distance in metres at the config frequency.
double pathLossDb(const RadioCfg& c, double metres);

} // namespace ls
