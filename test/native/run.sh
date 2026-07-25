#!/usr/bin/env bash
# Build and run the portable-core unit tests on the host (no hardware needed).
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
out="${TMPDIR:-/tmp}/lora-suite-tests"

g++ -std=c++17 -O2 -Wall -Wextra \
  "$here"/*.cpp \
  "$root"/src/proto/frame.cpp \
  "$root"/src/proto/airtime.cpp \
  "$root"/src/proto/geo.cpp \
  "$root"/src/proto/nodetable.cpp \
  "$root"/src/proto/meshoverlay.cpp \
  "$root"/src/proto/protobuf_lite.cpp \
  "$root"/src/proto/meshtastic.cpp \
  "$root"/src/proto/payloads.cpp \
  "$root"/src/proto/roster.cpp \
  "$root"/src/proto/solar.cpp \
  "$root"/src/crypto/chacha20.cpp \
  "$root"/src/crypto/aes.cpp \
  "$root"/src/crypto/sha256.cpp \
  "$root"/src/crypto/channel.cpp \
  -o "$out"

"$out"
