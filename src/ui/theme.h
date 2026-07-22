#pragma once
#include <cstdint>
#include "../shell/app.h"

namespace ls {
namespace theme {

// RGB565, matching the field-radio instrument palette of the design brief.
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t BG     = rgb(10, 16, 19);
constexpr uint16_t PANEL  = rgb(18, 26, 31);
constexpr uint16_t LINE   = rgb(33, 48, 56);
constexpr uint16_t TEXT   = rgb(221, 229, 234);
constexpr uint16_t MUTED  = rgb(138, 153, 163);
constexpr uint16_t ACCENT = rgb(52, 208, 189);
constexpr uint16_t LOC    = rgb(90, 169, 230);
constexpr uint16_t RF     = rgb(240, 168, 80);
constexpr uint16_t UTIL   = rgb(138, 153, 163);
constexpr uint16_t GOOD   = rgb(78, 195, 122);
constexpr uint16_t WARN   = rgb(232, 182, 74);
constexpr uint16_t CRIT   = rgb(240, 112, 138);

inline uint16_t forCat(Cat c) {
  switch (c) {
    case Cat::Comms:    return ACCENT;
    case Cat::Location: return LOC;
    case Cat::RF:       return RF;
    default:            return UTIL;
  }
}

} // namespace theme
} // namespace ls
