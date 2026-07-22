# LoRa Suite — Cardputer-Adv × Cap LoRa868

A multi-function LoRa toolkit for the **M5Stack Cardputer-Adv** with the
**Cap LoRa868** module (SX1262 radio + ATGM336H GPS). Twelve keyboard-driven
apps share one wire protocol, one duty-cycle governor, and the module's GPS.

**Docs:**
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
| HIST | Archive | Persistent message history on SD (survives reboot); tail viewer with scroll + `f` substring filter |
| UNDO | Recall | Cancel a still-queued outbound frame before it airs (within the duty hold-window) |
| CB | Contacts | Durable name↔address roster — aliases, block, favourite, import heard nodes (NVS-persisted) |
| FLT | Fleet | Squad-vitals dashboard (battery/presence/RSSI/age, worst-first) + set your own Presence; Enter → message a node |
| RLY | Relay | Mesh control panel + node table (forwarding runs in the background for every screen) |
| BCN | Beacon | Periodic GPS position, interval auto-throttled to the duty budget |
| RDR | Radar | Polar plot of nodes by bearing + distance from your fix |
| PF | Pathfinder | Capture/share waypoints and home to a target — bearing arrow + range + closing speed |
| TRK | Breadcrumb | Logs track + heard nodes to microSD (CSV), GPS-timestamped |
| SOS | Mayday | Distress + dead-man switch — confirmed panic, IMU-stillness auto-trigger, homing view for received distress |
| SWP | Sweep | RSSI scan across a channel plan, waterfall bars |
| MON | Monitor | Promiscuous frame monitor (type / addr / RSSI / SNR) |
| RNG | Ranger | Ping/echo link test — RSSI, SNR, RTT, loss, distance |
| TIME | Chronos | Mesh time-sync (GPS→TIMESYNC) for the RTC-less fleet + daylight-length almanac |
| CDWN | Countdown | Mesh-wide synchronized timer anchored to absolute UTC — every node fires together at T-0 |
| CFG | Console | Radio profiles + live airtime / duty / link-budget calculator |
| LOG | Ledger | Per-type airtime audit against the 1% duty budget + a daily SD summary |
| RULE | Reflex | On-device IFTTT: event→action rules (RX-type / alert / low-battery / periodic) |
| PWR | Reactor | Battery-aware power state machine (CPU/LCD degrade with hysteresis) + Survival low-power beacon |
| ALRT | Klaxon | LoRa pager — canned alerts, speaker + screen flash on receipt |
| TLM | Telemetry | Streams BMI270 motion + battery; paired unit shows values |
| DROP | Dropbox | Chunked note/file transfer (stretch app) |

**Cross-cutting (not apps):**
- **Status bar** — every screen's footer shows duty %, a *seconds-to-next-permitted-TX* countdown, TX-queue depth, unread badge, channel, battery and GPS fix.
- **Pulse** — a 7-byte health TLV (battery/uptime/heap/duty/temp/reboot/presence) piggybacks on outbound BEACON/TELEMETRY/PONG frames behind `FLAG_HEALTH`; the node table shows every peer's health almost airtime-free.
- **Presence** — your availability (AVAILABLE / BUSY / EN-ROUTE / RESTING) rides one byte of the Pulse TLV; peers' states appear in **Fleet**. Reactor's Survival state auto-sets RESTING.
- **Marshal** — `LoRaService` runs a priority TX queue (QoS classes, age-promotion capped below alerts) with CAD listen-before-talk and the duty governor; alerts/distress send urgent.
- **Launcher icons** — every app draws a single-colour vector glyph (`App::drawIcon`, LovyanGFX primitives, no bitmaps); the home grid is icon tiles, category-coloured, selection lifts the tile background. Preview: [`docs/icons-preview.html`](docs/icons-preview.html).
- **Airtime attribution** — every transmission carries a non-wire `airTag` (its type, or a relay bucket) charged to the **Ledger** (`AirLedger`) at send time, so you can see what's eating the duty budget; a daily total is appended to `/duty.csv`.
- **Message capture** — received/sent text is deduped (a dedicated `rxDedup`, so relayed copies count once) and queued to `ctx.archive`, which **Archive** drains to SD from its background tick (never on the radio RX path).
- **Reflex** — an on-device event→action rule engine: frame events (RX-type, alert) evaluate in the RX handler, timer/battery events on `background()`; actions run through the duty-gated send path with per-rule cooldowns + a self-src guard so automation can't loop.

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

## Layout

```
src/
  proto/     frame · airtime · duty · dedup · nodetable · geo · payloads
             qos · txqueue · roster · solar                               (portable, tested)
  crypto/    chacha20 · channel                                            (portable, tested)
  services/  lora · gps · storage · clock                                  (device)
  shell/     context · net · screen_manager · launcher                     (device)
  ui/        theme · widgets                                               (device)
  apps/      12 apps                                                       (device)
  hal/       pins · spi_bus                                                (device)
  main.cpp
test/native/ host unit tests (g++)
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

Builds and runs the portable-core suite (177 checks): frame codec (round-trip,
CRC rejection, bounds), LoRa airtime math, the duty-cycle governor + next-TX
math, the Marshal priority queue (class-based admission + age-promotion), mesh
dedup, node table + geo, the contact roster (+ serialization), the solar
almanac, ChaCha20 + channel crypto, and the position/telemetry/health/waypoint/
timesync payload codecs.

## Defaults / decisions

- **Region** EU868 by default (`Console` cycles EU868 / US915 / AS923).
- **Encryption** opt-in — channel 0 is cleartext "public"; set a PSK for a keyed
  channel. The skeleton's key-stretch is a placeholder; use a real KDF
  (HKDF/SHA-256) before relying on it.
- **Duty cycle** 1% governor on a 1-hour rolling window; non-urgent TX is
  refused once the budget is spent.
- **Relay** runs always-on in the background of every app (toggle in RLY).
