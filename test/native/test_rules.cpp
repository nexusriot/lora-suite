#include <cstdio>
#include "check.h"
#include "../../src/proto/rules.h"
#include "../../src/proto/frame.h"

using namespace ls;

void run_rules_tests() {
  std::printf("[rules]\n");

  RuleEngine E;
  Rule* r = E.add();
  CHECK(r != nullptr);
  r->event = EV_RX_TYPE; r->evParam = MSG_BEACON; r->action = AC_BEEP; r->enabled = true; r->cooldownS = 10;

  RuleAction a;
  CHECK(E.onFrame(MSG_BEACON, false, 0x22, 0x11, 1000, a) && a.type == AC_BEEP);  // match
  CHECK(!E.onFrame(MSG_BEACON, false, 0x22, 0x11, 5000, a));                       // cooldown blocks
  CHECK(E.onFrame(MSG_BEACON, false, 0x22, 0x11, 12000, a));                       // cooldown elapsed
  CHECK(!E.onFrame(MSG_BEACON, false, 0x11, 0x11, 30000, a));                      // never react to self
  CHECK(!E.onFrame(MSG_TEXT, false, 0x22, 0x11, 40000, a));                        // wrong type
  E.at(0).enabled = false;
  CHECK(!E.onFrame(MSG_BEACON, false, 0x22, 0x11, 60000, a));                      // disabled
  E.at(0).enabled = true;

  Rule* r2 = E.add();
  r2->event = EV_ALERT; r2->action = AC_SEND_ALERT; r2->acParam = 1; r2->enabled = true; r2->cooldownS = 0;
  CHECK(E.onFrame(MSG_ALERT, true, 0x33, 0x11, 100000, a) && a.type == AC_SEND_ALERT && a.param == 1);

  // periodic
  RuleEngine T;
  Rule* p = T.add();
  p->event = EV_PERIODIC; p->evArg = 5; p->action = AC_BEACON; p->enabled = true;
  RuleAction b;
  CHECK(!T.tick(1000, 80, b));                       // < 5s since start
  CHECK(T.tick(5000, 80, b) && b.type == AC_BEACON); // fires
  CHECK(!T.tick(7000, 80, b));                       // too soon
  CHECK(T.tick(10000, 80, b));                       // fires again

  // battery edge-trigger + hysteresis
  RuleEngine Bt;
  Rule* bl = Bt.add();
  bl->event = EV_BATT_LOW; bl->evArg = 20; bl->action = AC_BEEP; bl->enabled = true;
  RuleAction c;
  CHECK(!Bt.tick(1000, 50, c));                      // above
  CHECK(Bt.tick(2000, 15, c) && c.type == AC_BEEP);  // crosses below -> fire once
  CHECK(!Bt.tick(3000, 12, c));                      // still below, latched
  CHECK(!Bt.tick(4000, 21, c));                      // 21 < 23 -> not re-armed
  CHECK(!Bt.tick(5000, 25, c));                      // 25 >= 23 -> re-arm (no fire)
  CHECK(Bt.tick(6000, 10, c) && c.type == AC_BEEP);  // fires again
  CHECK(!Bt.tick(7000, -1, c));                      // unknown battery -> skip

  // serialize round-trip (config only)
  uint8_t buf[128];
  size_t sn = E.serialize(buf, sizeof(buf));
  CHECK(sn > 0);
  RuleEngine E2;
  CHECK(E2.deserialize(buf, sn));
  CHECK(E2.count() == E.count());
  CHECK(E2.at(0).event == EV_RX_TYPE && E2.at(0).evParam == MSG_BEACON);
  CHECK(E2.at(0).action == AC_BEEP && E2.at(0).cooldownS == 10);

  // remove
  E.remove(0);
  CHECK(E.count() == 1 && E.at(0).event == EV_ALERT);
}
