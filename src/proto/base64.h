#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// Minimal RFC 4648 base64, needed to type a Meshtastic channel PSK in the form
// their apps show it ("AQ==", or a 24/44-character key).
//
// Decoding is deliberately strict: any character outside the alphabet (including
// whitespace), a length that is not a multiple of four, or padding anywhere but
// the tail is rejected. A mistyped PSK must fail loudly — silently decoding to a
// *different* key would leave the node transmitting ciphertext nobody can read,
// which looks identical to being out of range.
//
// Both alphabets are accepted: standard ('+', '/') and URL-safe ('-', '_'). That
// is not laxity — the Cardputer keyboard maps '/' to the right-arrow, so a
// standard-base64 key containing '/' cannot be typed on this device at all, and
// the URL-safe spelling is the only way in. The two alphabets do not overlap, so
// accepting both stays unambiguous.
//
// Both functions return 0 for an empty input as well as an error; callers treat
// "no key text" as "use the default", so the two cases need no distinction.
size_t base64Decode(const char* in, uint8_t* out, size_t cap);

// Writes a NUL-terminated string; returns the length written (excluding the NUL).
size_t base64Encode(const uint8_t* in, size_t n, char* out, size_t cap);

} // namespace ls
