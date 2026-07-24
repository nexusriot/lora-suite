#pragma once
#include <M5Unified.h>

namespace ls {

// Shared audio helper over the Adv's ES8311 codec + 1 W speaker. The tone calls
// existed in Klaxon/Mayday/Countdown/Reflex all along — they were silent only
// because the speaker was never given a volume; audio::init() (called at boot)
// fixes that, and routing through here keeps one place to tune it.
namespace audio {

inline void init(uint8_t volume = 200) {
  M5.Speaker.begin();
  M5.Speaker.setVolume(volume);
}

inline void tone(uint32_t freqHz, uint32_t ms) { M5.Speaker.tone((float)freqHz, ms); }
inline void stop() { M5.Speaker.stop(); }

inline void beep()  { tone(2000, 80); }
inline void alert() { tone(2600, 250); }
inline void tick()  { tone(3200, 30); }

} // namespace audio
} // namespace ls
