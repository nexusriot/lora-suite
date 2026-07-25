#include <cstdio>
#include "check.h"
#include "../../src/proto/battlog.h"

using namespace ls;

void run_battlog_tests() {
  std::printf("[battlog]\n");

  // Not enough data yet: no slope, no forecast (rather than a wild guess).
  {
    BattLog b;
    CHECK(b.slopePctPerMin() == 0.0f);
    CHECK(b.minutesToEmpty() == -1);
    b.add(0, 100);
    b.add(1, 99);
    CHECK(b.slopePctPerMin() == 0.0f);
    CHECK(b.minutesToEmpty() == -1);
  }

  // Clean linear discharge: 1%/min from 100% => 60 min to empty at 60%.
  {
    BattLog b;
    for (uint32_t t = 0; t <= 40; t++) b.add(t, (uint8_t)(100 - t));
    CHECK_NEAR(b.slopePctPerMin(), -1.0f, 0.001);
    CHECK(b.latest() == 60);
    CHECK(b.minutesToEmpty() == 60);
    CHECK(b.minutesToFull() == -1);
    CHECK(!b.charging());
  }

  // Slower discharge: 0.1%/min from 50% => 500 min.
  {
    BattLog b;
    for (uint32_t t = 0; t <= 100; t++) b.add(t * 10, (uint8_t)(100 - t));
    CHECK_NEAR(b.slopePctPerMin(), -0.1f, 0.0001);
    CHECK(b.latest() == 0);
    CHECK(b.minutesToEmpty() == 0);
  }

  // Charging: positive slope reports time-to-full, never time-to-empty.
  {
    BattLog b;
    for (uint32_t t = 0; t <= 20; t++) b.add(t, (uint8_t)(40 + t));
    CHECK(b.slopePctPerMin() > 0.0f);
    CHECK(b.charging());
    CHECK(b.minutesToEmpty() == -1);
    CHECK(b.minutesToFull() == 40);     // 60% -> 100% at 1%/min
  }

  // Flat (idle / plugged in at 100%): treated as neither, not as "infinite life".
  {
    BattLog b;
    for (uint32_t t = 0; t < 30; t++) b.add(t, 100);
    CHECK(b.slopePctPerMin() == 0.0f);
    CHECK(b.minutesToEmpty() == -1);
    CHECK(b.minutesToFull() == -1);
    CHECK(!b.charging());
  }

  // Noise around a trend must not flip the sign of the forecast.
  {
    BattLog b;
    for (uint32_t t = 0; t < 40; t++) {
      int jitter = (t % 3 == 0) ? 1 : (t % 3 == 1 ? -1 : 0);
      int v = 80 - (int)(t / 2) + jitter;
      if (v < 0) v = 0;
      b.add(t, (uint8_t)v);
    }
    CHECK(b.slopePctPerMin() < 0.0f);
    CHECK(b.minutesToEmpty() > 0);
  }

  // The ring keeps the most recent CAP samples and the window slides.
  {
    BattLog b;
    for (uint32_t t = 0; t < BattLog::CAP + 50; t++) b.add(t, (uint8_t)(t % 101));
    CHECK(b.size() == BattLog::CAP);
    CHECK(b.minAt(0) == 50);                              // oldest retained
    CHECK(b.minAt(BattLog::CAP - 1) == BattLog::CAP + 49);  // newest
    CHECK(b.latest() == b.pctAt(BattLog::CAP - 1));
  }

  // A forecast still works after the ring has wrapped.
  {
    BattLog b;
    for (uint32_t t = 0; t < BattLog::CAP * 2; t++) {
      int v = 100 - (int)t / 4;
      if (v < 0) v = 0;
      b.add(t, (uint8_t)v);
    }
    CHECK(b.size() == BattLog::CAP);
    CHECK(b.slopePctPerMin() < 0.0f);
  }

  // Degenerate: every sample at the same timestamp must not divide by zero.
  {
    BattLog b;
    for (int i = 0; i < 10; i++) b.add(5, (uint8_t)(90 - i));
    CHECK(b.slopePctPerMin() == 0.0f);
    CHECK(b.minutesToEmpty() == -1);
  }

  // Already empty reports 0, not a negative time.
  {
    BattLog b;
    for (uint32_t t = 0; t <= 50; t++) {
      int v = 50 - (int)t;
      b.add(t, (uint8_t)(v < 0 ? 0 : v));
    }
    CHECK(b.latest() == 0);
    CHECK(b.minutesToEmpty() == 0);
  }

  // clear() resets everything.
  {
    BattLog b;
    for (uint32_t t = 0; t < 20; t++) b.add(t, (uint8_t)(90 - t));
    b.clear();
    CHECK(b.size() == 0);
    CHECK(b.minutesToEmpty() == -1);
  }
}
