#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/proto/frame.h"

using namespace ls;

void run_frame_tests() {
  std::printf("[frame]\n");

  Frame f;
  f.type = MSG_TEXT;
  f.flags = FLAG_ACK_REQ | FLAG_MESH;
  f.chan = 7;
  f.hop = 3;
  f.src = 0x1234;
  f.dst = 0xABCD;
  f.msgid = 0x0042;
  const char* msg = "hello mesh";
  f.setPayload(msg, (uint8_t)std::strlen(msg));

  uint8_t buf[MAX_FRAME];
  size_t n = encode(f, buf, sizeof(buf));
  CHECK(n == HEADER_LEN + std::strlen(msg) + CRC_LEN);

  Frame g;
  CHECK(decode(buf, n, g));
  CHECK(g.type == MSG_TEXT);
  CHECK(g.flags == (FLAG_ACK_REQ | FLAG_MESH));
  CHECK(g.chan == 7);
  CHECK(g.hop == 3);
  CHECK(g.src == 0x1234);
  CHECK(g.dst == 0xABCD);
  CHECK(g.msgid == 0x0042);
  CHECK(g.len == std::strlen(msg));
  CHECK(std::memcmp(g.payload, msg, g.len) == 0);

  // corrupted CRC must be rejected
  buf[n - 1] ^= 0xFF;
  Frame bad;
  CHECK(!decode(buf, n, bad));
  buf[n - 1] ^= 0xFF; // restore

  // flipped payload byte must be rejected by CRC
  buf[HEADER_LEN] ^= 0x01;
  CHECK(!decode(buf, n, bad));
  buf[HEADER_LEN] ^= 0x01;

  // bad magic / version rejected
  uint8_t save = buf[0]; buf[0] = 0x00;
  CHECK(!decode(buf, n, bad));
  buf[0] = save;
  save = buf[1]; buf[1] = 0x99;
  CHECK(!decode(buf, n, bad));
  buf[1] = save;

  // truncated buffer rejected
  CHECK(!decode(buf, HEADER_LEN + CRC_LEN - 1, bad));
  CHECK(!decode(buf, n - 1, bad));

  // empty-payload broadcast round-trips
  Frame b;
  b.type = MSG_BEACON;
  b.src = 0x0001;
  CHECK(b.isBroadcast());
  uint8_t bb[MAX_FRAME];
  size_t bn = encode(b, bb, sizeof(bb));
  CHECK(bn == HEADER_LEN + CRC_LEN);
  Frame bg;
  CHECK(decode(bb, bn, bg));
  CHECK(bg.len == 0);
  CHECK(bg.dst == ADDR_BROADCAST);

  // max payload round-trips
  Frame m;
  m.type = MSG_FILECHUNK;
  uint8_t big[MAX_PAYLOAD];
  for (size_t i = 0; i < MAX_PAYLOAD; i++) big[i] = (uint8_t)(i * 7 + 1);
  m.setPayload(big, MAX_PAYLOAD);
  uint8_t mb[MAX_FRAME];
  size_t mn = encode(m, mb, sizeof(mb));
  CHECK(mn == MAX_FRAME);
  Frame mg;
  CHECK(decode(mb, mn, mg));
  CHECK(mg.len == MAX_PAYLOAD);
  CHECK(std::memcmp(mg.payload, big, MAX_PAYLOAD) == 0);

  // encode refuses too-small output buffer
  uint8_t tiny[4];
  CHECK(encode(f, tiny, sizeof(tiny)) == 0);
}
