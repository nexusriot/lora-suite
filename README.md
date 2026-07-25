# LoRa Suite — Cardputer-Adv × Cap LoRa868

A multi-function LoRa toolkit for the **M5Stack Cardputer-Adv** with the
**Cap LoRa868** module (SX1262 radio + ATGM336H GPS). **33** keyboard-driven
apps share one 13-byte wire protocol, one duty-cycle governor, and the module's GPS.

**Location:** `/home/vlad/workspace/my/lora-suite`

**Docs:**
- [`docs/FEATURES.md`](docs/FEATURES.md) — what every app + subsystem is for, and how to use it
- [`DESIGN.md`](DESIGN.md) — architecture, layers, wire protocol, and subsystems
- [`ROADMAP.md`](ROADMAP.md) — what's built + the prioritized feature backlog
- [`docs/design-brief.html`](docs/design-brief.html) — rendered design brief (open in a browser)
- [`docs/icons-preview.html`](docs/icons-preview.html) — faithful render of the launcher icons

## Hardware

| | |
|---|---|
| Host | Cardputer-Adv — ESP32-S3FN8 (StampS3A), 240×135 LCD, 56-key keyboard, BMI270 IMU, ES8311 audio, microSD, 1750 mAh |
| Cap | Cap LoRa868 — SX1262 (868–923 MHz, +20 dBm) + ATGM336H GPS, SMA 3 dBi antenna, Cap-Bus 14-pin |

### Pin map (as wired for the Adv)

| Signal | GPIO | Bus |
|---|---|---|
| LoRa NSS/CS | G5 | SPI |
| LoRa SCK | G40 | SPI — **shared with microSD** |
| LoRa MOSI | G14 | SPI — **shared with microSD** |
| LoRa MISO | G39 | SPI — **shared with microSD** |
| LoRa BUSY | G6 | GPIO |
| LoRa DIO1 (IRQ) | G4 | interrupt |
| LoRa RST | G3 | GPIO |
| GPS (UART1) | G13 / G15 | 115200 8N1 |
| microSD CS | G12 | SPI (shared) |

The radio and the microSD card sit on **one SPI bus** with separate chip-selects.
Every transfer is wrapped in `SpiBus::Guard` (a recursive mutex) so a card write
can never interleave with a radio transaction. See `src/hal/`.

## The apps

| Callsign | App | What it does |
|---|---|---|
| CR | Courier | Addressed + broadcast text chat, ACKs, optional encrypted channel; `@name msg` sends by contact |
| CHAT | Chat | Simple broadcast LoRa messenger (Cardputer-demo style) — one shared scrolling feed, no addressing |
| HIST | Archive | Persistent message history on SD (survives reboot); tail viewer with scroll + `f` substring filter |
| UNDO | Recall | Cancel a still-queued outbound frame before it airs (within the duty hold-window) |
| CB | Contacts | Durable name↔address roster — aliases, block, favourite, import heard nodes (NVS-persisted) |
| FLT | Fleet | Squad-vitals dashboard (battery/presence/RSSI/age, worst-first) + set your own Presence; Enter → message a node |
| RLY | Relay | Mesh control panel + node table (forwarding runs in the background for every screen) |
| GPS | GPS | Own GPS status — fix, lat/lon, altitude, speed/course, satellites, HDOP, UTC |
| BCN | Beacon | Periodic GPS position, interval auto-throttled to the duty budget |
| RDR | Radar | Polar plot of nodes by bearing + distance from your fix (foreign Meshtastic nodes shown as hollow markers) |
| MESH | Mesh | Foreign Meshtastic nodes from a meshmap.net snapshot (SD) — read-only situational awareness, nearest-first, `r` reloads |
| PF | Pathfinder | Capture/share waypoints and home to a target — bearing arrow + range + closing speed |
| TRK | Breadcrumb | Logs track + heard nodes to microSD (CSV), GPS-timestamped |
| SOS | Mayday | Distress + dead-man switch — confirmed panic, IMU-stillness auto-trigger, homing view for received distress |
| SWP | Sweep | RSSI scan across a channel plan, waterfall bars; `m` jumps to the EU_868 Meshtastic band |
| SCAN | MeshScan | Over-the-air Meshtastic receiver — sweeps the LongFast/MediumFast/ShortFast presets, decrypts the public channel, decodes position/name/telemetry/text into Mesh (Direction B) |
| MTX | MeshTX | Send text (Enter) or your GPS position (Tab) INTO the Meshtastic public channel — two-way interop |
| MCHT | MeshChat | Two-way conversation on the Meshtastic public channel — RX + TX text in one feed; Tab cycles preset |
| MON | Monitor | Promiscuous frame monitor (type / addr / RSSI / SNR) |
| RNG | Ranger | Ping/echo link test — RSSI, SNR, RTT, loss, distance |
| TIME | Chronos | Mesh time-sync (GPS→TIMESYNC) for the RTC-less fleet + daylight-length almanac; `n` = NTP-over-WiFi time fallback |
| CDWN | Countdown | Mesh-wide synchronized timer anchored to absolute UTC — every node fires together at T-0 |
| CFG | Console | Radio profiles + live airtime / duty / link-budget calculator; `m` applies the Meshtastic preset |
| GW | Gateway | Uplink heard frames over USB serial as JSON (feeds `tools/lorakit` dissect / a map of your fleet) |
| PRB | Probe | Hardware self-test — I2C scan + radio/GPS/SD/keyboard/board status on one screen |
| WIFI | WiFi | Scan 2.4 GHz WiFi APs (SSID / RSSI / channel / secured) — uses the otherwise-unused WiFi radio |
| BT | Bluetooth | Enable the BLE companion bridge for the [Cardputer Companion](android/) Android app (messaging/status/mesh/config) |
| LOG | Ledger | Per-type airtime audit against the 1% duty budget + a daily SD summary |
| RULE | Reflex | On-device IFTTT: event→action rules (RX-type / alert / low-battery / periodic) |
| PWR | Reactor | Battery-aware power state machine (CPU/LCD degrade with hysteresis) + Survival low-power beacon |
| ALRT | Klaxon | LoRa pager — canned alerts, speaker + screen flash on receipt |
| TLM | Telemetry | Streams BMI270 motion + battery; paired unit shows values |
| DROP | Dropbox | Chunked note/file transfer (stretch app) |
| SET | Settings | Screen brightness + speaker volume, persisted to NVS (applied live) |
| SD | SD | microSD status (type / used / free), remount, and erase-all wipe |
| IR | IR | Infrared NEC remote blaster (Power/Vol/Mute/Ch presets) over the IR LED |

**Cross-cutting (not apps):**
- **Status bar** — every screen's footer shows duty %, a *seconds-to-next-permitted-TX* countdown, TX-queue depth, unread badge, channel, battery and GPS fix.
- **Pulse** — a 7-byte health TLV (battery/uptime/heap/duty/temp/reboot/presence) piggybacks on outbound BEACON/TELEMETRY/PONG frames behind `FLAG_HEALTH`; the node table shows every peer's health almost airtime-free.
- **Presence** — your availability (AVAILABLE / BUSY / EN-ROUTE / RESTING) rides one byte of the Pulse TLV; peers' states appear in **Fleet**. Reactor's Survival state auto-sets RESTING.
- **Marshal** — `LoRaService` runs a priority TX queue (QoS classes, age-promotion capped below alerts) with CAD listen-before-talk and the duty governor; alerts/distress send urgent.
- **Launcher icons** — every app draws a single-colour vector glyph (`App::drawIcon`, LovyanGFX primitives, no bitmaps); the home grid is icon tiles, category-coloured, selection lifts the tile background. Preview: [`docs/icons-preview.html`](docs/icons-preview.html).
- **Airtime attribution** — every transmission carries a non-wire `airTag` (its type, or a relay bucket) charged to the **Ledger** (`AirLedger`) at send time, so you can see what's eating the duty budget; a daily total is appended to `/duty.csv`.
- **Message capture** — received/sent text is deduped (a dedicated `rxDedup`, so relayed copies count once) and queued to `ctx.archive`, which **Archive** drains to SD from its background tick (never on the radio RX path).
- **Reflex** — an on-device event→action rule engine: frame events (RX-type, alert) evaluate in the RX handler, timer/battery events on `background()`; actions run through the duty-gated send path with per-rule cooldowns + a self-src guard so automation can't loop.
- **Meshtastic interop** — foreign Meshtastic nodes (imported from meshmap.net, or heard over the air by **MeshScan**) live in a `MeshOverlay` kept out of the peer table; shown in **Mesh** + on **Radar**. **Gateway** uplinks our own frames to a host. See [Meshtastic interoperability](#meshtastic-interoperability).

Keys: arrows are the `;` `.` `,` `/` cluster, `` ` `` is ESC/back, `Enter`
confirms, `\` arms a global distress confirm (ignored while typing). Each app
shows its own controls.

## Wire protocol

13-byte header + payload + CRC-16, little-endian:

```
MAGIC VER TYPE FLAGS CHAN HOP SRC(2) DST(2) MSGID(2) LEN | PAYLOAD(0..200) | CRC16(2)
```

Types: TEXT ACK BEACON PING PONG TELEMETRY ALERT NODEINFO FILECHUNK TIMESYNC WAYPOINT.
Flags: ACK_REQ ENCRYPTED MESH FRAGMENT HEALTH LOWPWR. Payloads on a keyed channel
are ChaCha20-encrypted with a nonce bound to (src, msgid). See `src/proto/frame.h`.
Wire version **2** (`PROTO_VERSION`) — v2 grew the Pulse health TLV to 7 bytes
(added presence); `decode()` rejects other versions, so mixed-firmware nodes fail
closed rather than mis-parsing.

## Meshtastic interoperability

lora-suite is **not** Meshtastic — different framing, addressing and crypto — but
it shares the SX1262 and the 868 band, so it can *observe* the local Meshtastic
world three ways. Foreign nodes live in a `MeshOverlay` kept out of the peer
`NodeTable` (never messaged or relayed); they show in the **Mesh** app and as
hollow markers on **Radar**.

### Import from meshmap.net (Direction A)

[meshmap.net](https://meshmap.net/) is a live map of Meshtastic nodes seen by the
public MQTT server. Its `/nodes.json` feed is ~3.7 MB — too big for the ESP32 — so
`tools/meshpull` trims it to a small CSV you copy to the SD card:

```bash
cd tools/meshpull
go run . -lat 40.18 -lon 44.51 -radius 30 -topic msh/EU_868 -max 96 -out import.csv
# copy import.csv to the SD card as /mesh/import.csv
```

In **Mesh**: `;`/`.` scroll, `r` reloads, `Enter` opens a detail card (name, `!id`,
role, hardware, battery/voltage, position, distance/bearing, last-heard age). Rows
dim as their meshmap "last heard" ages. Poll meshmap.net at most once a minute
(60 s cache); schedule the tool with cron if you want it fresh.

**CSV format** (`/mesh/import.csv`) — nine comma-separated fixed fields, then the
long name as the rest of the line (which may contain commas):

```
# generated <unix>
id,lat,lon,batt,volt,role,seen,hw,short,long
```

id (uint32), lat/lon (deg), batt (0–255; >100 = externally powered), volt
(centivolts), role (a code — see `meshoverlay.h`), seen (unix, last heard via
MQTT), hw (≤9 chars), short (≤4). `tools/meshpull` and the device parser
(`src/proto/meshoverlay.h`) share this format and are both unit-tested.

### Scan over the air — MeshScan (Direction B)

**MeshScan** (`SCAN`) retunes the radio to the EU_868 / MEDIUM_FAST preset (869.525
MHz), goes receive-only, and decrypts the public channel (the `AQ==` key) — decoded
Position/NodeInfo land in the overlay with live RSSI. `Enter` jumps to the Mesh
list. **One radio: while this screen is open you are deaf to your own mesh.** The
decode chain (AES-128-CTR + a protobuf-lite reader, `src/proto/meshtastic.*`) is
host-tested; the exact key/nonce/modem constants are marked to verify against the
Meshtastic firmware on bring-up. Preview the raw band energy first with **Sweep** → `m`.

### Uplink your fleet — Gateway (Direction C2)

**Gateway** (`GW`) streams every frame heard on *our* channel out USB-CDC as JSON.
Pipe it into the host dissector (or a meshobserv fork) to map your own fleet:

```bash
pio device monitor | (cd tools/lorakit && go run ./cmd/dissect)
```

`tools/lorakit` is a byte-compatible Go port of the wire codec, cross-checked
against a C-encoded golden frame.

## Layout

```
src/
  proto/     frame · airtime · duty · dedup · nodetable · geo · payloads · qos · txqueue
             roster · solar · ledger · rules · meshoverlay · meshtastic · protobuf_lite · nec  (portable, tested)
  crypto/    chacha20 · channel · aes · sha256 (HMAC/HKDF)                  (portable, tested)
  services/  lora · gps · storage · clock · audio · ir                     (device)
  shell/     context · net · screen_manager · launcher                     (device)
  ui/        theme · widgets                                               (device)
  apps/      36 apps                                                       (device)
  hal/       pins · spi_bus                                                (device)
  main.cpp
test/native/    host unit tests (g++)
tools/meshpull/ meshmap.net -> SD import tool (Go, tested)
tools/lorakit/  wire-codec Go port + `dissect` CLI (tested)
android/        Kotlin/Compose BLE companion app (Cardputer Companion)
```

## Build & flash (device)

```
pio run -e cardputer-adv -t upload
pio device monitor
```

Notes:
- Targets RadioLib 6.x, TinyGPSPlus, and the M5Cardputer library. The Adv may
  need the latest M5Cardputer/M5Unified with StampS3A board support; adjust the
  `board` id in `platformio.ini` if an Adv-specific one ships.
- If the radio fails to start, check TCXO voltage / `setDio2AsRfSwitch` for your
  module revision (`src/services/lora_service.cpp`).
- If GPS never fixes, swap RX/TX in `src/hal/pins.h` (the module's pin names are
  from its own perspective).

## Test (host, no hardware)

```
bash test/native/run.sh
```

Builds and runs the portable-core suite (345 checks): frame codec (round-trip,
CRC rejection, bounds), LoRa airtime math, the duty-cycle governor + next-TX
math, the Marshal priority queue (class-based admission + age-promotion), mesh
dedup, node table + geo, the contact roster (+ serialization), the solar
almanac, ChaCha20 + channel crypto, **AES-128 (FIPS-197 vectors) + CTR**, the
**Meshtastic frame decode** (header + AES-CTR + protobuf → Position/NodeInfo),
the Meshtastic overlay + import CSV parser, and the position/telemetry/health/
waypoint/timesync payload codecs.

The Go host tools have their own tests:

```
cd tools/meshpull && go test ./...
cd tools/lorakit  && go test ./...
```

## Defaults / decisions

- **Region** EU868 by default (`Console` cycles EU868 / US915 / AS923).
- **Encryption** opt-in — channel 0 is cleartext "public"; set a PSK for a keyed
  channel (payloads then ChaCha20-encrypted). The channel key + id are derived
  from the PSK with **HKDF-SHA256** (RFC 5869, domain-separated), verified against
  the RFC test vectors in the host test suite.
- **Duty cycle** 1% governor on a 1-hour rolling window; non-urgent TX is
  refused once the budget is spent.
- **Relay** runs always-on in the background of every app (toggle in RLY).
