#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// A user-editable set of named IR remote codes.
//
// The IR app shipped with six hardcoded NEC codes, which is only useful if your
// gear happens to match. This holds the codes as data instead: persisted in NVS
// and editable from the phone, so the device becomes *your* remote. Kept portable
// (no Arduino) so the serialization is host-tested — a corrupt NVS blob must not
// be able to walk off the end of the array.
struct IrCode {
  char    label[12] = {0};
  uint8_t addr = 0;
  uint8_t cmd = 0;
  bool    used = false;
};

class IrCodeSet {
public:
  static const size_t CAP = 12;

  size_t size() const { return count_; }
  const IrCode& at(size_t i) const { return items_[i]; }

  // Add or overwrite slot `i` (i < CAP). Slots stay dense: setting the slot just
  // past the end appends. Returns false if `i` would leave a hole.
  bool set(size_t i, const char* label, uint8_t addr, uint8_t cmd);

  // Append; returns false when full.
  bool add(const char* label, uint8_t addr, uint8_t cmd);

  // Remove slot `i`, shifting the rest down.
  bool remove(size_t i);

  void clear() { count_ = 0; }

  // Replace the set with the six generic codes the IR app used to hardcode, so a
  // device with no saved codes still does something useful out of the box.
  void loadDefaults();

  // Flat blob for NVS. Returns bytes written, or 0 if it doesn't fit.
  size_t serialize(uint8_t* out, size_t cap) const;
  bool deserialize(const uint8_t* in, size_t n);

private:
  static const uint8_t MAGIC = 0xC3;
  static const uint8_t VERSION = 1;

  IrCode items_[CAP];
  size_t count_ = 0;
};

} // namespace ls
