#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/proto/defrag.h"

using namespace ls;

static FragHeader fh(uint8_t g, uint8_t i, uint8_t t) {
  FragHeader h; h.group = g; h.index = i; h.total = t; return h;
}

void run_defrag_tests() {
  std::printf("[defrag]\n");

  // Header parsing rejects anything that would confuse the reassembler.
  {
    FragHeader h;
    uint8_t good[] = {5, 1, 3};
    CHECK(parseFragHeader(good, 3, h));
    CHECK(h.group == 5 && h.index == 1 && h.total == 3);

    uint8_t shortBuf[] = {5, 1};
    CHECK(!parseFragHeader(shortBuf, 2, h));
    uint8_t zeroTotal[] = {5, 0, 0};
    CHECK(!parseFragHeader(zeroTotal, 3, h));
    uint8_t idxPastTotal[] = {5, 3, 3};
    CHECK(!parseFragHeader(idxPastTotal, 3, h));
    uint8_t tooMany[] = {5, 0, MAX_FRAGMENTS + 1};
    CHECK(!parseFragHeader(tooMany, 3, h));
  }

  // In-order reassembly of a 3-fragment message.
  {
    Defrag d;
    CHECK(!d.offer(0x11, fh(1, 0, 3), (const uint8_t*)"hello ", 6, 1000));
    CHECK(!d.offer(0x11, fh(1, 1, 3), (const uint8_t*)"from the ", 9, 1010));
    CHECK(d.offer(0x11, fh(1, 2, 3), (const uint8_t*)"ridge", 5, 1020));
    CHECK(d.size() == 20);
    CHECK(std::memcmp(d.data(), "hello from the ridge", 20) == 0);
    CHECK(d.activeSlots() == 0);   // slot released on completion
  }

  // Out-of-order arrival still reassembles correctly.
  {
    Defrag d;
    CHECK(!d.offer(0x22, fh(2, 2, 3), (const uint8_t*)"ccc", 3, 100));
    CHECK(!d.offer(0x22, fh(2, 0, 3), (const uint8_t*)"aaa", 3, 110));
    CHECK(d.offer(0x22, fh(2, 1, 3), (const uint8_t*)"bbb", 3, 120));
    CHECK(d.size() == 9);
    CHECK(std::memcmp(d.data(), "aaabbbccc", 9) == 0);
  }

  // Duplicate fragments (relayed copies) must not complete early or corrupt.
  {
    Defrag d;
    CHECK(!d.offer(0x33, fh(3, 0, 2), (const uint8_t*)"one", 3, 10));
    CHECK(!d.offer(0x33, fh(3, 0, 2), (const uint8_t*)"one", 3, 11));   // dupe
    CHECK(!d.offer(0x33, fh(3, 0, 2), (const uint8_t*)"one", 3, 12));   // dupe
    CHECK(d.offer(0x33, fh(3, 1, 2), (const uint8_t*)"two", 3, 13));
    CHECK(d.size() == 6);
    CHECK(std::memcmp(d.data(), "onetwo", 6) == 0);
  }

  // Two senders interleaved: each keeps its own slot.
  {
    Defrag d;
    CHECK(!d.offer(0xAA, fh(1, 0, 2), (const uint8_t*)"AA1", 3, 10));
    CHECK(!d.offer(0xBB, fh(1, 0, 2), (const uint8_t*)"BB1", 3, 11));
    CHECK(d.activeSlots() == 2);
    CHECK(d.offer(0xBB, fh(1, 1, 2), (const uint8_t*)"BB2", 3, 12));
    CHECK(std::memcmp(d.data(), "BB1BB2", 6) == 0);
    CHECK(d.offer(0xAA, fh(1, 1, 2), (const uint8_t*)"AA2", 3, 13));
    CHECK(std::memcmp(d.data(), "AA1AA2", 6) == 0);
  }

  // Same sender, two different groups in flight.
  {
    Defrag d;
    CHECK(!d.offer(0x44, fh(7, 0, 2), (const uint8_t*)"g7a", 3, 10));
    CHECK(!d.offer(0x44, fh(8, 0, 2), (const uint8_t*)"g8a", 3, 11));
    CHECK(d.offer(0x44, fh(8, 1, 2), (const uint8_t*)"g8b", 3, 12));
    CHECK(std::memcmp(d.data(), "g8ag8b", 6) == 0);
    CHECK(d.offer(0x44, fh(7, 1, 2), (const uint8_t*)"g7b", 3, 13));
    CHECK(std::memcmp(d.data(), "g7ag7b", 6) == 0);
  }

  // A stalled reassembly is reclaimed by the timeout sweep.
  {
    Defrag d;
    CHECK(!d.offer(0x55, fh(1, 0, 2), (const uint8_t*)"half", 4, 1000));
    CHECK(d.activeSlots() == 1);
    d.sweep(1000 + Defrag::TIMEOUT_MS - 1);
    CHECK(d.activeSlots() == 1);          // not yet expired
    d.sweep(1000 + Defrag::TIMEOUT_MS + 1);
    CHECK(d.activeSlots() == 0);          // reclaimed
  }

  // A single-fragment message completes immediately.
  {
    Defrag d;
    CHECK(d.offer(0x66, fh(1, 0, 1), (const uint8_t*)"solo", 4, 10));
    CHECK(d.size() == 4);
    CHECK(std::memcmp(d.data(), "solo", 4) == 0);
  }

  // Full-size message: MAX_FRAGMENTS full fragments must fit exactly.
  {
    Defrag d;
    uint8_t chunk[FRAG_BODY_MAX];
    std::memset(chunk, 'x', sizeof(chunk));
    bool done = false;
    for (uint8_t i = 0; i < MAX_FRAGMENTS; i++)
      done = d.offer(0x77, fh(1, i, MAX_FRAGMENTS), chunk, (uint8_t)sizeof(chunk), 10 + i);
    CHECK(done);
    CHECK(d.size() == DEFRAG_MAX);
  }

  // Garbage input is rejected without disturbing state.
  {
    Defrag d;
    CHECK(!d.offer(0x88, fh(1, 0, 0), (const uint8_t*)"x", 1, 10));                  // total 0
    CHECK(!d.offer(0x88, fh(1, 5, 2), (const uint8_t*)"x", 1, 10));                  // index >= total
    CHECK(!d.offer(0x88, fh(1, 0, MAX_FRAGMENTS + 1), (const uint8_t*)"x", 1, 10));  // too many
    CHECK(d.activeSlots() == 0);
  }

  // More concurrent senders than slots: the oldest is evicted, newest still works.
  {
    Defrag d;
    for (uint16_t i = 0; i < Defrag::SLOTS + 2; i++)
      d.offer((uint16_t)(0x100 + i), fh(1, 0, 2), (const uint8_t*)"p", 1, 10 + i);
    CHECK(d.activeSlots() <= Defrag::SLOTS);
    uint16_t last = (uint16_t)(0x100 + Defrag::SLOTS + 1);
    CHECK(d.offer(last, fh(1, 1, 2), (const uint8_t*)"q", 1, 100));
    CHECK(d.size() == 2);
    CHECK(std::memcmp(d.data(), "pq", 2) == 0);
  }
}
