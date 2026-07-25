#pragma once
#include <cstdint>

namespace ls {

// NEC IR frame: 32 bits sent LSB-first as address, ~address, command, ~command.
// The complements let a receiver validate each byte. Kept Arduino-free so the
// bit layout can be unit-tested on the host; the timing/carrier lives in
// services/ir.h.
inline uint32_t necWord(uint8_t addr, uint8_t cmd) {
  return (uint32_t)addr | ((uint32_t)(uint8_t)~addr << 8) |
         ((uint32_t)cmd << 16) | ((uint32_t)(uint8_t)~cmd << 24);
}

} // namespace ls
