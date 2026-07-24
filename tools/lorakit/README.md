# lorakit

A Go port of the lora-suite wire codec (`src/proto/frame.h`) for host-side
tooling — kept byte-compatible with the firmware (the tests cross-check against a
golden frame produced by the C encoder). Foundation for self-hosting a map of
your own fleet (a meshobserv fork that ingests these frames) and for debugging.

## Package

```go
import "lorakit"

f, err := lorakit.Decode(buf)          // 13-byte header + CRC-16/CCITT-FALSE
out, err := lorakit.Encode(f)          // reproduces the exact on-air bytes
lorakit.TypeName(f.Type)               // "TEXT", "BEACON", ...
lorakit.FlagString(f.Flags)            // "MESH|HEALTH"
```

## dissect

Decodes frames from stdin — raw hex or the Gateway app's JSON lines — and prints
one human-readable row each. Undecodable input is reported (so foreign/corrupt
traffic is visible too).

```bash
# a hex capture
cat capture.hex | go run ./cmd/dissect

# live from a device running the Gateway app (uplink ON)
pio device monitor | go run ./cmd/dissect
```

Gateway JSON line format: `{"t":<ms>,"rssi":<int>,"snr":<int>,"hex":"<hex>"}`.

Encrypted payloads are shown as `<N bytes encrypted>` — the header is always
cleartext, so type/addressing/routing are visible regardless. (Decrypting a keyed
channel would need the PSK and a Go port of the channel cipher — not yet done.)

## Test

```bash
go test ./...
```
