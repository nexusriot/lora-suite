#pragma once
#include <cstdint>
#include <cstdio>
#include <cstddef>

namespace ls {

// A small RAM FIFO of formatted message lines awaiting persistence. Capture
// happens on the hot paths (onRadioFrame for RX, Courier for TX); the Archive
// app drains this to SD from its background() tick so SD writes never stall the
// radio. Oldest lines are dropped if the FIFO overflows before a flush.
class ArchiveLog {
public:
  static const int CAP = 16;
  static const int LINELEN = 72;

  void add(const char* timeStr, char dir, uint16_t peer, const char* text) {
    char* slot = q_[(head_ + count_) % CAP];
    char clean[48];
    size_t j = 0;
    for (size_t i = 0; text[i] && j < sizeof(clean) - 1; i++) {
      char c = text[i];
      if (c == ',' || c == '\n' || c == '\r') c = ' ';   // keep the CSV clean
      clean[j++] = c;
    }
    clean[j] = 0;
    std::snprintf(slot, LINELEN, "%s,%c,%04X,%s", timeStr, dir, peer, clean);
    if (count_ < CAP) count_++;
    else head_ = (head_ + 1) % CAP;   // overwrite oldest
  }

  bool pending() const { return count_ > 0; }
  const char* front() const { return q_[head_]; }
  void popFront() { if (count_) { head_ = (head_ + 1) % CAP; count_--; } }

private:
  char q_[CAP][LINELEN] = {};
  int head_ = 0;
  int count_ = 0;
};

} // namespace ls
