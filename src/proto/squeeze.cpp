#include "squeeze.h"
#include <cstring>

namespace ls {

// Dictionary of common English fragments, ordered longest-first within the
// matcher so greedy matching prefers the biggest win. Tuned for terse radio
// traffic (status, movement, coordination) on top of ordinary prose.
static const char* const DICT[] = {
  " the ", "the ", "and ", "ing ", "ion ", " to ", "tion", "that", "here",
  "with", "have", "this", "from", "your", " at ", " on ", " in ", " is ",
  " of ", " we ", " be ", " it ", " no ", " ok", "over", "copy", "roger",
  "north", "south", "east", "west", "position", "status", "moving", "arrived",
  "standby", "confirm", "negative", "affirm", "repeat", "message", "contact",
  "location", "battery", "signal", "return", "base", "camp", "road", "river",
  "bridge", "hill", "ridge", "valley", "point", "meet", "need", "help",
  "come", "back", "went", "will", "wait", "hold", "move", "stop", "left",
  "right", "up", "down", "now", "min", "hour", "day", "km", "get", "got",
  "can", "not", "you", "are", "for", "was", "all", "out", "see", "way",
  "who", "how", "why", "yes", "has", "man", "new", "old", "one",
  "two", "ten", "map", "gps", "sos", "eta", "the", "and", "ent", "ers",
  "ess", "est", "ate", "ter", "ver", "ist", "ain", "ome", "our", "ould",
  "ough", "ight", "tch", "sh", "ch", "th", "wh", "qu", "ck", "ng", "st",
  "nd", "rd", "ll", "ss", "ee", "oo", "ea", "ou", "ai", "ie", "er", "re",
  "an", "in", "on", "at", "en", "es", "or", "ar", "it", "is", "to", "of",
  "as", "be", "by", "he", "we", "do", "go", "no", "so", "us", "me",
  "my", "if", "am", "e ", "t ", "a ", "o ", "i ", "n ", "s ", "r ",
  "d ", "l ", ". ", ", ", "! ", "? ", "; ", ": ", " ", "e", "t", "a", "o",
  "i", "n", "s", "r", "h", "l", "d", "c", "u", "m", "f", "p", "g", "w",
  "y", "b", "v", "k", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
  ".", ",", "-", "/", "'",
};

static const size_t DICT_N = sizeof(DICT) / sizeof(DICT[0]);

// Longest dictionary match at `p`, or DICT_N if none. Entries are scanned by
// descending length so the first hit of the longest length wins.
static size_t bestMatch(const uint8_t* p, size_t avail, size_t& matchLen) {
  size_t bestIdx = DICT_N, bestLen = 0;
  for (size_t i = 0; i < DICT_N; i++) {
    size_t l = std::strlen(DICT[i]);
    if (l <= bestLen || l > avail) continue;
    if (std::memcmp(p, DICT[i], l) == 0) { bestIdx = i; bestLen = l; }
  }
  matchLen = bestLen;
  return bestIdx;
}

size_t squeeze(const uint8_t* in, size_t inLen, uint8_t* out, size_t outCap) {
  if (DICT_N > SQZ_DICT_SIZE) return 0;   // dictionary must stay inside the index space
  size_t o = 0, i = 0;
  uint8_t lit[SQZ_MAX_RUN];
  size_t litN = 0;

  // Emit any buffered literals as one run (or a single-literal escape).
  auto flush = [&]() -> bool {
    while (litN) {
      if (litN == 1) {
        if (o + 2 > outCap) return false;
        out[o++] = SQZ_LIT1;
        out[o++] = lit[0];
        litN = 0;
      } else {
        size_t n = litN;
        if (o + 2 + n > outCap) return false;
        out[o++] = SQZ_LITN;
        out[o++] = (uint8_t)n;
        std::memcpy(out + o, lit, n);
        o += n;
        litN = 0;
      }
    }
    return true;
  };

  while (i < inLen) {
    size_t mlen = 0;
    size_t idx = bestMatch(in + i, inLen - i, mlen);
    // A 1-byte dictionary hit costs the same as a run literal, so only break a
    // literal run for it when no run is in progress.
    if (idx != DICT_N && (mlen > 1 || litN == 0)) {
      if (!flush()) return 0;
      if (o + 1 > outCap) return 0;
      out[o++] = (uint8_t)idx;
      i += mlen;
    } else {
      if (litN == SQZ_MAX_RUN && !flush()) return 0;
      lit[litN++] = in[i++];
    }
  }
  if (!flush()) return 0;
  return o;
}

size_t unsqueeze(const uint8_t* in, size_t inLen, uint8_t* out, size_t outCap) {
  size_t o = 0, i = 0;
  while (i < inLen) {
    uint8_t b = in[i++];
    if (b == SQZ_LIT1) {
      if (i >= inLen || o + 1 > outCap) return 0;
      out[o++] = in[i++];
    } else if (b == SQZ_LITN) {
      if (i >= inLen) return 0;
      uint8_t n = in[i++];
      if (n == 0 || i + n > inLen || o + n > outCap) return 0;
      std::memcpy(out + o, in + i, n);
      o += n;
      i += n;
    } else {
      if (b >= DICT_N) return 0;                 // index past the dictionary
      size_t l = std::strlen(DICT[b]);
      if (o + l > outCap) return 0;
      std::memcpy(out + o, DICT[b], l);
      o += l;
    }
  }
  return o;
}

bool squeezeIfSmaller(const uint8_t* in, size_t inLen, uint8_t* out, size_t outCap, size_t& outLen) {
  size_t n = squeeze(in, inLen, out, outCap);
  if (n == 0 || n >= inLen) return false;
  outLen = n;
  return true;
}

} // namespace ls
