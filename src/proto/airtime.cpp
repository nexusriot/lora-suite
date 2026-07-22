#include "airtime.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ls {

double timeOnAirMs(const RadioCfg& c, uint8_t payloadLen) {
  const double sf = c.sf;
  const double bw = c.bwHz;
  const double tSym = std::pow(2.0, sf) / bw;          // seconds per symbol
  const int de = (tSym > 0.016) ? 1 : 0;                // low data rate optimize
  const int ih = c.explicitHeader ? 0 : 1;             // implicit header
  const int crc = c.crc ? 1 : 0;
  const int crDenom = c.cr;                             // 5..8
  const double crBits = (double)(crDenom - 4);         // 1..4

  const double tPreamble = (c.preamble + 4.25) * tSym;

  double num = 8.0 * payloadLen - 4.0 * sf + 28.0 + 16.0 * crc - 20.0 * ih;
  double den = 4.0 * (sf - 2.0 * de);
  double payloadSymb = 8.0 + std::fmax(std::ceil(num / den) * (crBits + 4.0), 0.0);

  const double tPayload = payloadSymb * tSym;
  return (tPreamble + tPayload) * 1000.0;
}

double pathLossDb(const RadioCfg& c, double metres) {
  if (metres < 1.0) metres = 1.0;
  const double fMHz = c.freqHz / 1.0e6;
  const double km = metres / 1000.0;
  return 20.0 * std::log10(km) + 20.0 * std::log10(fMHz) + 32.44;
}

} // namespace ls
