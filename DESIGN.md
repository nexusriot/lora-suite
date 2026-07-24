# LoRa Suite — Design

A multi-app LoRa toolkit (32 apps) for the **M5Stack Cardputer-Adv** + **Cap LoRa868**
(SX1262 radio + ATGM336H GPS), at `/home/vlad/workspace/my/lora-suite`. This document
is the maintainable companion to the rendered [design brief](docs/design-brief.html);
see [ROADMAP.md](ROADMAP.md) for the feature backlog and [README.md](README.md) for
the user-facing overview.

## Principles

- **Thin apps over shared services.** The hard parts — radio state machine, GPS,
  framing, crypto, mesh dedup, duty accounting — live in shared services. Adding
  an app means writing a screen, not a driver.
- **The portable core is host-tested.** Everything in `src/proto` and `src/crypto`
  is dependency-free C++ with native unit tests (`test/native/run.sh`, 353 checks),
  so protocol/codec/logic bugs are caught without hardware.
- **Respect the band.** 868 MHz is duty-limited (~1%); a governor meters every
  transmission and the Marshal scheduler gates all TX.

## Layers

```
apps/      32 keyboard-driven screens (each an App subclass)
shell/     launcher · screen manager · context (ctx) · net glue · archive FIFO
ui/        theme (RGB565 palette) · widgets (header/footer/status bar)
services/  lora (RadioLib SX1262) · gps (TinyGPSPlus) · storage (NVS+SD) · clock
proto/     frame · airtime · duty · dedup · nodetable · geo · payloads · qos · txqueue
           roster · solar · ledger · rules · meshoverlay · meshtastic · protobuf_lite  (portable, tested)
crypto/    chacha20 · channel · aes                               (portable, tested)
hal/       pins.h · spi_bus (shared radio+SD mutex)
```

### The `App` interface (`shell/app.h`)
Every screen implements: `name/callsign/category`, `onEnter/onExit`, `onKey`,
`onPacket` (channel-matched frames), `onRawPacket` (all frames — Monitor),
`update` (foreground only), **`background`** (every loop, foreground or not — the
hook that makes true background daemons cheap: Beacon, Mayday, Reactor, Chronos,
Countdown, Reflex, Archive-flush all use it), `consumesText` (gates global keys
during text entry), and `drawIcon` (a 20×20 vector glyph for the launcher).

### Shared state — `ctx` (`shell/context.h`)
One global `Context`: `NodeTable`, `MeshOverlay` (foreign Meshtastic nodes),
`Roster`, `ArchiveLog`, `RuleEngine`, active
`Channel`/`RadioCfg`, identity, power/presence, unread counter, a cross-app nav
intent (`navRequest`/`pendingPeer`), and countdown state. Apps read/write it;
services are wired in at boot.

## Wire protocol (`proto/frame.h`)

13-byte header + payload (≤200 B) + CRC-16, little-endian:

```
MAGIC VER TYPE FLAGS CHAN HOP SRC(2) DST(2) MSGID(2) LEN | PAYLOAD | CRC16(2)
```

- **Types:** TEXT, ACK, BEACON, PING, PONG, TELEMETRY, ALERT, NODEINFO,
  FILECHUNK, TIMESYNC, WAYPOINT, COUNTDOWN.
- **Flags:** ACK_REQ, ENCRYPTED, MESH, FRAGMENT, HEALTH, LOWPWR.
- **Version 2** (`PROTO_VERSION`): v2 grew the Pulse health TLV to 7 bytes
  (+presence). `decode()` rejects other versions → mixed-firmware fails closed.
- A non-wire `airTag` byte annotates each frame for Ledger attribution.
- Payloads on a keyed channel are ChaCha20-encrypted, nonce bound to (src, msgid).

## Key subsystems

- **Mesh relay + dedup** (`main.cpp onRadioFrame`) — the single RX choke point.
  Updates the node table, taps Monitor, relays `FLAG_MESH` frames (hop-limit +
  `relayDedup`, on the *still-encrypted* copy, alerts forwarded urgent, blocked
  contacts skipped), then channel-filters, decrypts, strips the health tail,
  routes by type, and feeds Reflex. A separate `rxDedup` gates archive/unread so
  relayed copies count once.
- **Marshal** (`services/lora_service`) — `sendFrame()` enqueues into a priority
  `TxQueue` (QoS by class, age-promotion capped below alerts); `pump()` (each
  loop) does CAD listen-before-talk + the duty gate, transmits one frame, and
  charges the `DutyGovernor` + `AirLedger`. `cancel()` powers Recall.
- **Pulse + Presence** — a 7-byte health TLV (batt/uptime/heap/duty/temp/reboot/
  presence) piggybacks on BEACON/TELEMETRY/PONG behind `FLAG_HEALTH`, stripped
  centrally; Fleet renders it. Presence is one byte of that TLV.
- **Chronos + Countdown** — GPS-UTC disciplined `Clock` with `adopt()` (mesh
  TIMESYNC, source-ranked). Countdown broadcasts an absolute UTC fire-time so
  every node fires on the same second regardless of RX delay.
- **Ledger** — `airTag` (type, or a relay bucket) is charged in `pump()` to a
  pure `AirLedger`; the app shows the breakdown and logs a daily total to SD.
- **Reflex** (`proto/rules.h` + `apps/reflex.h`) — event→action rules. Frame
  events evaluate in `onRadioFrame`; timer/battery events on the app's
  `background()`. Actions route through the duty-gated send path; per-rule
  cooldowns + a self-src guard prevent runaway/loops. Rules persist to NVS.
- **Archive** — text captured on the hot paths into a RAM FIFO (`ctx.archive`),
  drained to `/archive.csv` from `background()` (never on the RX path); a viewer
  reads the file tail.
- **Meshtastic interop** — three ways to observe the foreign Meshtastic world on
  the shared 868 band, all feeding a `MeshOverlay` kept apart from `NodeTable`
  (situational-awareness only — never relayed/ACKed/addressed; keyed by 32-bit
  node numbers; source-tagged `SRC_IMPORT`/`SRC_SCAN`):
  - **Import (A)** — `tools/meshpull` trims the ~3.7 MB meshmap.net feed to an SD
    CSV; the **Mesh** app parses it (comma-tolerant long names, `# generated`
    time, rows dim by last-heard) and **Radar** plots distinct markers.
  - **Scan (B, RX)** — **MeshScan** retunes the SX1262 and sweeps the LongFast/
    MediumFast/ShortFast presets (SF11/9/7 at 869.525 MHz), sets `LoRaService`
    receive-only (so no app airs our frames on their channel), and decodes raw
    packets via `proto/meshtastic` (16-byte header + AES-128-CTR + `protobuf_lite`
    → Text/Position/NodeInfo/Telemetry) into `SRC_SCAN` entries. A raw-RX tap
    (`onRawReceive`) delivers the bytes; `crypto/aes` does the decrypt.
  - **Scan (B, TX)** — **MeshTX** encodes a text or a Position (protobuf writer +
    AES-CTR + per-preset channel hash) and `LoRaService::transmitRaw`s it onto the
    public channel; **MeshChat** is a full RX+TX conversation on it — two-way interop.
  - **Uplink (C2)** — **Gateway** re-encodes every heard frame and streams it out
    the USB serial console as JSON from the RX chokepoint (background, any app
    open); `tools/lorakit` (a byte-compatible Go codec + `dissect` CLI) consumes
    it toward a self-hosted fleet map.

## Constraints & conventions

- **No PSRAM (~512 KB RAM):** fixed-capacity tables (NodeTable 32, Roster 48,
  TxQueue 12, MeshOverlay 96), no heap in the hot paths. `const` icon/data tables
  live in flash.
- **Shared SPI bus:** radio + microSD share SCK/MOSI/MISO; all transfers take
  `SpiBus::Guard` (recursive mutex). SD-heavy apps buffer in RAM and flush from
  `background()` so writes never stall the radio.
- **Keyboard:** `;` `.` `,` `/` are arrows, `` ` `` is ESC/back, `\` is the global
  distress hotkey — apps cannot receive those as characters.
- **No RTC:** timestamps/log names key off GPS-UTC (via Chronos), falling back to
  uptime before a fix.

## Testing & build

- Host: `bash test/native/run.sh` — frame codec, airtime, duty (+next-TX),
  Marshal queue, dedup, node table + geo, roster, solar, ChaCha20/channel crypto,
  **AES-128 (FIPS-197) + CTR**, the **Meshtastic frame decode** (header + AES-CTR
  + protobuf → Text/Position/NodeInfo/Telemetry) + the text/position **encoders** round-trip
  + EU_868 preset, the Meshtastic overlay + CSV parser, ledger, rules, and all
  payload codecs. Pure C++, no board required.
  The Go host tools `tools/meshpull` and `tools/lorakit` have their own `go test`s.
- Device: `pio run -e cardputer-adv` builds the full firmware clean (RadioLib 6.x,
  TinyGPSPlus, M5Cardputer; ~18% RAM / ~20% flash on the StampS3). `-t upload` to
  flash + `pio device monitor`. On-air behaviour still needs the hardware; adjust
  the board id / TCXO / GPS RX-TX per your module (see README caveats).
