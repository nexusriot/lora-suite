#pragma once
#include <cstdint>

namespace m5gfx { class M5Canvas; }
using M5Canvas = m5gfx::M5Canvas;

namespace ls {

class App;

namespace ui {

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;
constexpr int HEADER_H = 16;
constexpr int FOOTER_H = 12;
constexpr int BODY_Y   = HEADER_H + 1;
constexpr int BODY_H   = SCREEN_H - HEADER_H - FOOTER_H - 2;

// Title bar: category-coloured callsign chip, app name, UTC clock, battery.
void header(M5Canvas& g, const App& app);

// Status bar: duty-cycle budget bar, channel id, GPS-fix dot, RX blink.
void footer(M5Canvas& g);

// Signal-strength colour from an RSSI value.
uint16_t rssiColor(int16_t rssi);

// Right-aligned helper.
void textRight(M5Canvas& g, int x, int y, const char* s);

} // namespace ui
} // namespace ls
