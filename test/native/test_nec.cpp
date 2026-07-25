#include <cstdio>
#include "check.h"
#include "../../src/proto/nec.h"

using namespace ls;

void run_nec_tests() {
  std::printf("[nec]\n");

  // Byte layout is LSB-first: addr, ~addr, cmd, ~cmd.
  // addr=0x00 cmd=0x0C -> 00 FF 0C F3 -> 0xF30CFF00
  CHECK(necWord(0x00, 0x0C) == 0xF30CFF00u);
  // addr=0x04 cmd=0x08 -> 04 FB 08 F7 -> 0xF708FB04
  CHECK(necWord(0x04, 0x08) == 0xF708FB04u);
  // addr=0xFF cmd=0xFF -> FF 00 FF 00 -> 0x00FF00FF
  CHECK(necWord(0xFF, 0xFF) == 0x00FF00FFu);

  // Each byte and its complement must XOR to 0xFF, and addr/cmd land in the
  // right byte positions — checked over a representative grid.
  const uint8_t vals[] = {0x00, 0x01, 0x55, 0xAA, 0xFF};
  for (uint8_t a : vals) {
    for (uint8_t c : vals) {
      uint32_t w = necWord(a, c);
      CHECK(a == (uint8_t)(w & 0xFF));
      CHECK(c == (uint8_t)((w >> 16) & 0xFF));
      CHECK(((w & 0xFF) ^ ((w >> 8) & 0xFF)) == 0xFF);
      CHECK((((w >> 16) & 0xFF) ^ ((w >> 24) & 0xFF)) == 0xFF);
    }
  }
}
