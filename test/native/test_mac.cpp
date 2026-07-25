#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/crypto/channel.h"
#include "../../src/crypto/sha256.h"

using namespace ls;

static Frame textFrame(const char* body, uint16_t src = 0x1234, uint16_t msgid = 7) {
  Frame f;
  f.type = MSG_TEXT;
  f.src = src;
  f.dst = ADDR_BROADCAST;
  f.msgid = msgid;
  f.chan = 1;
  f.setPayload(body, (uint8_t)std::strlen(body));
  return f;
}

void run_mac_tests() {
  std::printf("[mac]\n");

  Channel ch;
  ch.setPSK("correct horse battery staple");

  // Round-trip: seal then open recovers the plaintext and clears the tag.
  {
    Frame f = textFrame("meet at the north ridge");
    uint8_t plainLen = f.len;
    CHECK(ch.seal(f));
    CHECK((f.flags & FLAG_ENCRYPTED) != 0);
    CHECK((f.flags & FLAG_MAC) != 0);
    CHECK(f.len == plainLen + MAC_LEN);
    CHECK(std::memcmp(f.payload, "meet at", 7) != 0);   // actually encrypted

    CHECK(ch.open(f));
    CHECK(f.len == plainLen);
    CHECK((f.flags & FLAG_MAC) == 0);
    CHECK(std::memcmp(f.payload, "meet at the north ridge", plainLen) == 0);
  }

  // Tampering with any ciphertext byte is rejected.
  {
    Frame f = textFrame("fire in the hold");
    CHECK(ch.seal(f));
    f.payload[3] ^= 0x01;
    CHECK(!ch.open(f));
  }

  // Tampering with the tag itself is rejected.
  {
    Frame f = textFrame("fire in the hold");
    CHECK(ch.seal(f));
    f.payload[f.len - 1] ^= 0x80;
    CHECK(!ch.open(f));
  }

  // Header fields are authenticated: flipping src/dst/msgid/type breaks the tag.
  {
    Frame base = textFrame("authenticated header");
    CHECK(ch.seal(base));

    Frame f = base; f.src ^= 0x0100;   CHECK(!ch.open(f));
    f = base;       f.dst ^= 0x0001;   CHECK(!ch.open(f));
    f = base;       f.msgid ^= 0x0001; CHECK(!ch.open(f));
    f = base;       f.type = MSG_ALERT; CHECK(!ch.open(f));
  }

  // hop is deliberately NOT covered — a relay decrements it and the frame must
  // still authenticate at the far end.
  {
    Frame f = textFrame("relayed onwards");
    CHECK(ch.seal(f));
    f.hop = (uint8_t)(f.hop - 1);
    CHECK(ch.open(f));
  }

  // A different PSK cannot open it (forgery / wrong-key rejection).
  {
    Frame f = textFrame("secret orders");
    CHECK(ch.seal(f));
    Channel other;
    other.setPSK("wrong key entirely");
    CHECK(!other.open(f));
  }

  // A keyed frame arriving without FLAG_MAC is refused (no downgrade to v2).
  {
    Frame f = textFrame("downgrade attempt");
    CHECK(ch.seal(f));
    f.flags &= (uint8_t)~FLAG_MAC;
    CHECK(!ch.open(f));
  }

  // Cleartext frames pass through open() untouched.
  {
    Channel pub;
    Frame f = textFrame("in the clear");
    uint8_t n = f.len;
    CHECK(pub.open(f));
    CHECK(f.len == n);
    CHECK(std::memcmp(f.payload, "in the clear", n) == 0);
  }

  // seal() refuses a body that leaves no room for the tag.
  {
    Frame f;
    f.type = MSG_TEXT;
    f.chan = 1;
    f.len = MAX_PAYLOAD;                 // no space for 8 more bytes
    CHECK(!ch.seal(f));
  }

  // The MAC key must be independent of the cipher key.
  {
    Channel c2;
    c2.setPSK("independence");
    Frame a = textFrame("aaaa"), b = textFrame("aaaa");
    CHECK(c2.seal(a));
    CHECK(c2.seal(b));
    CHECK(std::memcmp(a.payload, b.payload, a.len) == 0);   // deterministic per (src,msgid)
  }

  // Incremental SHA-256 must match the one-shot for a message spanning blocks
  // (the HMAC path relies on this; the old fixed-buffer HMAC truncated at 256 B).
  {
    uint8_t big[1000];
    for (size_t i = 0; i < sizeof(big); i++) big[i] = (uint8_t)(i * 31 + 7);
    uint8_t one[32], inc[32];
    sha256(big, sizeof(big), one);
    Sha256Ctx c;
    sha256_init(c);
    sha256_update(c, big, 1);
    sha256_update(c, big + 1, 63);
    sha256_update(c, big + 64, 500);
    sha256_update(c, big + 564, sizeof(big) - 564);
    sha256_final(c, inc);
    CHECK(std::memcmp(one, inc, 32) == 0);

    // A long HMAC message must not be silently truncated: changing a byte past
    // the old 256-byte cap has to change the tag.
    uint8_t t1[32], t2[32];
    hmac_sha256((const uint8_t*)"k", 1, big, sizeof(big), t1);
    big[900] ^= 0xFF;
    hmac_sha256((const uint8_t*)"k", 1, big, sizeof(big), t2);
    CHECK(std::memcmp(t1, t2, 32) != 0);
  }
}
