#include <cstdio>
#include "check.h"
#include "../../src/proto/duty.h"

using namespace ls;

void run_duty_tests() {
  std::printf("[duty]\n");

  // 1% of a 3600 s window = 36 s = 36000 ms budget.
  DutyGovernor g(3600000UL, 0.01);
  CHECK(g.budgetMs() == 36000);

  uint32_t now = 1000;
  CHECK(g.airtimeMs(now) == 0);
  CHECK(g.canSend(now, 5000));

  // Spend most of the budget.
  g.record(now, 30000);
  CHECK(g.airtimeMs(now) == 30000);
  CHECK(g.canSend(now, 6000));       // exactly at budget is allowed
  CHECK(!g.canSend(now, 6001));      // one ms over is refused
  CHECK_NEAR(g.usedFraction(now), 30000.0 / 36000.0, 1e-9);

  // Airtime older than the window rolls off.
  uint32_t later = now + 3600001UL;
  CHECK(g.airtimeMs(later) == 0);
  CHECK(g.canSend(later, 30000));

  // Multiple records accumulate within the window.
  DutyGovernor h(3600000UL, 0.01);
  h.record(100, 1000);
  h.record(200, 2000);
  h.record(300, 3000);
  CHECK(h.airtimeMs(400) == 6000);

  // time-to-next-permitted-TX
  DutyGovernor t(3600000UL, 0.01);              // budget 36000
  CHECK(t.timeToNextTxMs(1000, 5000) == 0);      // empty -> fits now
  t.record(1000, 30000);
  CHECK(t.timeToNextTxMs(1000, 6000) == 0);      // 30000+6000 == budget, fits
  // 10000 needs 4000 to age out; the lone event exits at 1000+3600000.
  CHECK(t.timeToNextTxMs(1000, 10000) == 3600000);
  // a frame larger than the whole budget can never fit.
  CHECK(t.timeToNextTxMs(1000, 40000) == DutyGovernor::NEVER);

  // multiple events: only enough of the oldest airtime must age out.
  DutyGovernor u(3600000UL, 0.01);
  u.record(1000, 10000);
  u.record(2000, 10000);
  u.record(3000, 10000);                          // 30000 in window
  // send 12000 at now=4000: need 6000; oldest (t=1000) frees 10000, exits 3601000.
  CHECK(u.timeToNextTxMs(4000, 12000) == 3597000);
}
