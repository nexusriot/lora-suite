#include <cstdio>
#include "check.h"
#include "../../src/proto/txqueue.h"

using namespace ls;

static Frame mk(uint8_t type) { Frame f; f.type = type; return f; }

void run_txqueue_tests() {
  std::printf("[txqueue]\n");

  // priority order: ALERT > TEXT > FILECHUNK
  TxQueue q;
  CHECK(q.push(mk(MSG_FILECHUNK), 0));
  CHECK(q.push(mk(MSG_TEXT), 0));
  CHECK(q.push(mk(MSG_ALERT), 0));
  Frame out; bool urg;
  CHECK(q.pop(0, out, urg) && out.type == MSG_ALERT);
  CHECK(q.pop(0, out, urg) && out.type == MSG_TEXT);
  CHECK(q.pop(0, out, urg) && out.type == MSG_FILECHUNK);
  CHECK(!q.pop(0, out, urg));

  // urgent jumps the queue
  TxQueue q2;
  q2.push(mk(MSG_TEXT), 0);
  q2.push(mk(MSG_FILECHUNK), 0, true);
  CHECK(q2.pop(0, out, urg) && out.type == MSG_FILECHUNK && urg);

  // aging promotes a starved bulk frame above a fresh normal one
  TxQueue q3;
  q3.push(mk(MSG_FILECHUNK), 0);       // bulk at t=0 (prio 0)
  q3.push(mk(MSG_TEXT), 20000);        // text at t=20000 (prio 3)
  // at t=20000 bulk eff = 0 + 20000/5000 = 4 > text eff 3
  CHECK(q3.pop(20000, out, urg) && out.type == MSG_FILECHUNK);

  // full queue: same-priority newcomer dropped, higher-priority evicts
  TxQueue q4;
  for (size_t i = 0; i < TxQueue::CAP; i++) CHECK(q4.push(mk(MSG_FILECHUNK), 0));
  CHECK(q4.full());
  CHECK(!q4.push(mk(MSG_FILECHUNK), 0));   // no room, equal priority
  CHECK(q4.push(mk(MSG_ALERT), 0));        // alert outranks bulk -> evicts one
  CHECK(q4.pop(0, out, urg) && out.type == MSG_ALERT);

  // admission is by CLASS, not aged priority: a fresh alert must be admitted
  // even when the queue is full of long-aged lower-class frames.
  TxQueue q5;
  for (size_t i = 0; i < TxQueue::CAP; i++) CHECK(q5.push(mk(MSG_TELEMETRY), 0));
  CHECK(q5.push(mk(MSG_ALERT), 20000));    // 20s later, incumbents aged to eff 4
  CHECK(q5.pop(20000, out, urg) && out.type == MSG_ALERT);

  // Recall: cancel a still-queued frame by (src, msgid)
  TxQueue q6;
  Frame a = mk(MSG_TEXT);   a.src = 0x11; a.msgid = 5;
  Frame b = mk(MSG_BEACON); b.src = 0x11; b.msgid = 6;
  q6.push(a, 0);
  q6.push(b, 0);
  CHECK(q6.size() == 2);
  CHECK(!q6.cancel(0x11, 99));   // no match
  CHECK(!q6.cancel(0x22, 5));    // wrong src
  CHECK(q6.cancel(0x11, 5));     // removes the TEXT
  CHECK(q6.size() == 1);
  CHECK(q6.pop(0, out, urg) && out.type == MSG_BEACON && out.msgid == 6);
}
