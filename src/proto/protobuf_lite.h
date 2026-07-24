#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// Minimal protobuf wire reader — just enough to walk Meshtastic's Data/Position/
// User messages without pulling in nanopb. Bounds-checked; every reader returns
// false on a malformed or truncated field so a bad decrypt fails closed.
struct PbReader {
  const uint8_t* p;
  const uint8_t* end;
  PbReader(const uint8_t* buf, size_t n) : p(buf), end(buf + n) {}

  bool eof() const { return p >= end; }

  bool readVarint(uint64_t& out);
  bool readTag(uint32_t& field, uint8_t& wire);         // field number + wire type (tag & 7)
  bool readLengthDelimited(const uint8_t*& data, size_t& len);  // wire type 2
  bool readFixed32(uint32_t& out);                      // wire type 5
  bool readFixed64(uint64_t& out);                      // wire type 1
  bool skip(uint8_t wire);                              // advance past an unwanted field
};

// Minimal protobuf writer — enough to build the Meshtastic Data/Position/Text
// messages for TX. Every put returns false on overflow (bounds-checked).
struct PbWriter {
  uint8_t* buf;
  size_t cap;
  size_t len = 0;
  PbWriter(uint8_t* b, size_t c) : buf(b), cap(c) {}

  bool putByte(uint8_t b);
  bool putVarint(uint64_t v);
  bool putTag(uint32_t field, uint8_t wire);
  bool putVarintField(uint32_t field, uint64_t v);       // wire type 0
  bool putBytesField(uint32_t field, const uint8_t* d, size_t n);  // wire type 2
  bool putFixed32Field(uint32_t field, uint32_t v);      // wire type 5
};

} // namespace ls
