#include "ircodes.h"
#include <cstring>

namespace ls {

static void copyLabel(char* dst, const char* src) {
  if (!src) { dst[0] = 0; return; }
  std::strncpy(dst, src, 11);
  dst[11] = 0;
}

bool IrCodeSet::set(size_t i, const char* label, uint8_t addr, uint8_t cmd) {
  if (i >= CAP || i > count_) return false;      // no holes
  copyLabel(items_[i].label, label);
  items_[i].addr = addr;
  items_[i].cmd = cmd;
  items_[i].used = true;
  if (i == count_) count_++;
  return true;
}

bool IrCodeSet::add(const char* label, uint8_t addr, uint8_t cmd) {
  if (count_ >= CAP) return false;
  return set(count_, label, addr, cmd);
}

bool IrCodeSet::remove(size_t i) {
  if (i >= count_) return false;
  for (size_t j = i; j + 1 < count_; j++) items_[j] = items_[j + 1];
  count_--;
  items_[count_] = IrCode{};
  return true;
}

void IrCodeSet::loadDefaults() {
  clear();
  add("Power", 0x00, 0x0C);
  add("Vol +", 0x00, 0x10);
  add("Vol -", 0x00, 0x11);
  add("Mute",  0x00, 0x0D);
  add("Ch +",  0x00, 0x20);
  add("Ch -",  0x00, 0x21);
}

size_t IrCodeSet::serialize(uint8_t* out, size_t cap) const {
  size_t need = 3 + count_ * 14;
  if (cap < need) return 0;
  out[0] = MAGIC;
  out[1] = VERSION;
  out[2] = (uint8_t)count_;
  size_t o = 3;
  for (size_t i = 0; i < count_; i++) {
    std::memcpy(out + o, items_[i].label, 12);
    o += 12;
    out[o++] = items_[i].addr;
    out[o++] = items_[i].cmd;
  }
  return o;
}

bool IrCodeSet::deserialize(const uint8_t* in, size_t n) {
  if (n < 3 || in[0] != MAGIC || in[1] != VERSION) return false;
  size_t c = in[2];
  if (c > CAP) return false;
  if (n < 3 + c * 14) return false;             // truncated blob: refuse it whole

  clear();
  size_t o = 3;
  for (size_t i = 0; i < c; i++) {
    std::memcpy(items_[i].label, in + o, 12);
    items_[i].label[11] = 0;                    // never trust a stored terminator
    o += 12;
    items_[i].addr = in[o++];
    items_[i].cmd = in[o++];
    items_[i].used = true;
  }
  count_ = c;
  return true;
}

} // namespace ls
