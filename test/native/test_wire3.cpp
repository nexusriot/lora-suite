#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/crypto/channel.h"
#include "../../src/proto/squeeze.h"
#include "../../src/proto/defrag.h"

using namespace ls;

// Mirrors the device pipeline (net.cpp netSendText -> main.cpp receiveText) so the
// three v3 features are proven to compose, not just work in isolation.
struct Wire {
  Channel ch;
  Defrag  defrag;
  uint16_t msgid = 1;
  uint8_t  group = 0;

  // Returns the fully recovered message, or "" if nothing was delivered.
  // `corruptFrame` optionally flips a byte in one fragment to model an attack.
  bool send(const char* text, uint16_t src, char* out, size_t outCap,
            int corruptFrame = -1, uint32_t now = 1000) {
    size_t n = std::strlen(text);
    uint8_t body[TEXT_MAX];
    size_t bodyLen = n;
    bool squeezed = false;
    uint8_t comp[TEXT_MAX];
    size_t cn = 0;
    if (squeezeIfSmaller((const uint8_t*)text, n, comp, sizeof(comp), cn)) {
      std::memcpy(body, comp, cn); bodyLen = cn; squeezed = true;
    } else {
      std::memcpy(body, text, n);
    }

    size_t budget = MAX_PAYLOAD;
    if (ch.encrypted()) budget -= MAC_LEN;
    uint8_t base = FLAG_MESH | (squeezed ? FLAG_SQUEEZE : 0);

    size_t total = 1, per = bodyLen;
    bool frag = bodyLen > budget;
    if (frag) {
      per = budget - FRAG_HDR_LEN;
      if (per > FRAG_BODY_MAX) per = FRAG_BODY_MAX;   // reassembler slot is the limit
      total = (bodyLen + per - 1) / per;
      if (total > MAX_FRAGMENTS) return false;
      group++;
    }

    bool delivered = false;
    for (size_t i = 0; i < total; i++) {
      Frame f;
      f.type = MSG_TEXT;
      f.src = src;
      f.dst = ADDR_BROADCAST;
      f.msgid = msgid++;
      f.chan = ch.id();
      f.flags = base | (frag ? FLAG_FRAGMENT : 0);
      if (frag) {
        size_t off = i * per, chunk = (bodyLen - off < per) ? (bodyLen - off) : per;
        uint8_t p[MAX_PAYLOAD];
        p[0] = group; p[1] = (uint8_t)i; p[2] = (uint8_t)total;
        std::memcpy(p + FRAG_HDR_LEN, body + off, chunk);
        f.setPayload(p, (uint8_t)(FRAG_HDR_LEN + chunk));
      } else {
        f.setPayload(body, (uint8_t)bodyLen);
      }
      if (!ch.seal(f)) return false;

      if ((int)i == corruptFrame) f.payload[2] ^= 0x40;     // in-flight tamper

      // ---- receiver ----
      Frame local = f;
      if (!ch.open(local)) continue;                        // dropped: bad MAC

      const uint8_t* rbody = local.payload;
      size_t rlen = local.len;
      if (local.flags & FLAG_FRAGMENT) {
        FragHeader h;
        if (!parseFragHeader(rbody, (uint8_t)rlen, h)) continue;
        if (!defrag.offer(f.src, h, rbody + FRAG_HDR_LEN,
                          (uint8_t)(rlen - FRAG_HDR_LEN), now + (uint32_t)i)) continue;
        rbody = defrag.data();
        rlen = defrag.size();
      }
      size_t got;
      if (local.flags & FLAG_SQUEEZE) {
        got = unsqueeze(rbody, rlen, (uint8_t*)out, outCap - 1);
        if (got == 0) continue;
      } else {
        got = rlen > outCap - 1 ? outCap - 1 : rlen;
        std::memcpy(out, rbody, got);
      }
      out[got] = 0;
      delivered = true;
    }
    return delivered;
  }
};

void run_wire3_tests() {
  std::printf("[wire3]\n");

  const char* shortMsg = "moving north to the ridge";
  // A long, compressible message: exercises compress + (maybe) fragment + MAC.
  static char longMsg[TEXT_MAX];
  {
    size_t o = 0;
    const char* unit = "we are moving north to the ridge and will confirm position when we arrive. ";
    while (o + std::strlen(unit) < TEXT_MAX - 1) { std::strcpy(longMsg + o, unit); o += std::strlen(unit); }
    longMsg[o] = 0;
  }
  // An incompressible long message: forces real fragmentation.
  static char noisy[300];
  {
    for (size_t i = 0; i < sizeof(noisy) - 1; i++) {
      uint8_t v = (uint8_t)((i * 97 + 13) % 94 + 33);
      if (v == '"') v = '#';
      noisy[i] = (char)v;
    }
    noisy[sizeof(noisy) - 1] = 0;
  }

  // Public (cleartext) channel: short and long both round-trip.
  {
    Wire w;
    char out[TEXT_MAX + 1] = {0};
    CHECK(w.send(shortMsg, 0x11, out, sizeof(out)));
    CHECK(std::strcmp(out, shortMsg) == 0);

    out[0] = 0;
    CHECK(w.send(longMsg, 0x11, out, sizeof(out)));
    CHECK(std::strcmp(out, longMsg) == 0);

    out[0] = 0;
    CHECK(w.send(noisy, 0x11, out, sizeof(out)));
    CHECK(std::strcmp(out, noisy) == 0);
  }

  // Keyed channel: same three, now encrypted + authenticated + tag-shrunk budget.
  {
    Wire w;
    w.ch.setPSK("field team alpha");
    char out[TEXT_MAX + 1] = {0};
    CHECK(w.send(shortMsg, 0x22, out, sizeof(out)));
    CHECK(std::strcmp(out, shortMsg) == 0);

    out[0] = 0;
    CHECK(w.send(longMsg, 0x22, out, sizeof(out)));
    CHECK(std::strcmp(out, longMsg) == 0);

    out[0] = 0;
    CHECK(w.send(noisy, 0x22, out, sizeof(out)));
    CHECK(std::strcmp(out, noisy) == 0);
  }

  // Tampering with one fragment of a keyed multi-fragment message must prevent
  // delivery: the corrupted piece fails its MAC and the message never completes.
  {
    Wire w;
    w.ch.setPSK("field team alpha");
    char out[TEXT_MAX + 1] = {0};
    CHECK(!w.send(noisy, 0x33, out, sizeof(out), 0));   // corrupt fragment 0
  }

  // The whole point of compression: a long message must cost fewer frames.
  {
    size_t raw = std::strlen(longMsg);
    uint8_t comp[TEXT_MAX];
    size_t cn = squeeze((const uint8_t*)longMsg, raw, comp, sizeof(comp));
    CHECK(cn > 0);
    size_t budget = MAX_PAYLOAD - MAC_LEN - FRAG_HDR_LEN;
    size_t framesRaw  = (raw + budget - 1) / budget;
    size_t framesComp = (cn + budget - 1) / budget;
    CHECK(framesComp < framesRaw);
  }
}
