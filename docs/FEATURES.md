# LoRa Suite — Feature Guide

What every app and subsystem is **for**, and how to use it. This is the
explanatory companion to the terse app table in [../README.md](../README.md) and
the architecture in [../DESIGN.md](../DESIGN.md).

The device is an **M5Stack Cardputer-Adv** with the **Cap LoRa868** module
(SX1262 radio + GPS). 32 keyboard-driven apps share one radio, one 13-byte wire
protocol, one duty-cycle budget, and the module's GPS.

**Keys everywhere:** the `;` `.` `,` `/` cluster is ↑ ↓ ← → · **Enter** confirms /
opens · **`` ` ``** is back/ESC · **`\`** arms the global distress prompt (ignored
while typing). Each app shows its own extra keys.

---

## Messaging & contacts

| App | For what | How |
|---|---|---|
| **Courier** (CR) | Your main text chat — talk to one node or the whole mesh, with delivery ACKs and an optional encrypted channel. | Type a message + Enter to broadcast; `@name msg` sends to a saved contact. |
| **Chat** (CHAT) | A dead-simple **broadcast** messenger (in the spirit of the Cardputer demo's LoRa Chat) — everyone on the channel shares one scrolling feed, no addressing or ACKs. | Type + Enter broadcasts to all; incoming text scrolls in with the sender. |
| **Archive** (HIST) | A durable log of everything sent/received, so history survives a reboot (RAM chat scrolls away). | Opens the on-SD history; scroll with ↑↓, `f` filters by substring. |
| **Recall** (UNDO) | "Unsend" — pull back a message that's still queued (waiting on the duty budget) before it actually airs. | Lists your queued frames; Enter cancels the selected one. |
| **Contacts** (CB) | A durable name↔address book so you address people by name instead of hex, and can block/favourite nodes. | Add aliases, block, favourite, or import heard nodes; persists to flash. |
| **Klaxon** (ALRT) | A LoRa pager — fire a canned alert that flashes the screen and beeps on every receiver. | Pick a canned message + send; incoming alerts interrupt with sound + flash. |

## Fleet & mesh control

| App | For what | How |
|---|---|---|
| **Fleet** (FLT) | A squad dashboard — see every teammate's battery, availability, signal and staleness at a glance, worst-battery first. | `p` cycles your own Presence (Available/Busy/En-route/Resting); Enter hands a node to Courier. |
| **Relay** (RLY) | Control + status for the always-on mesh forwarding, and a table of every node heard. | Toggle relaying; forwarding runs in the background of every screen regardless. |

## Location & safety

| App | For what | How |
|---|---|---|
| **GPS** (GPS) | See your own GPS state — fix, latitude/longitude, altitude, speed/course, satellite count, HDOP and UTC time (the module's GPS is also the fleet's clock). | Read-only status. |
| **Beacon** (BCN) | Periodically broadcast your GPS position so the mesh can see where you are; interval auto-throttles to stay legal. | Toggle on; it self-paces against the duty budget. |
| **Radar** (RDR) | A live polar plot of who's around you, by bearing + distance from your fix. Foreign Meshtastic nodes show as hollow blue markers. | ↑↓ change the display range. |
| **Pathfinder** (PF) | Navigate to a point — share/capture a waypoint and home in on it with a bearing arrow, range and closing speed. | Capture/select a waypoint; follow the arrow. |
| **Breadcrumb** (TRK) | Log your track + heard nodes to the SD card (GPS-timestamped CSV) for later review. | Toggle logging; writes to SD. |
| **Mayday** (SOS) | Emergency — a confirmed distress broadcast plus a dead-man switch (auto-fires if you go still), and a homing view for received distress. | Global `\` arms it (confirm with Enter before it transmits). |

## Meshtastic interoperability

lora-suite is **not** Meshtastic, but shares the 868 band, so it can *observe* the
local Meshtastic world three ways. See the dedicated section in
[../README.md](../README.md#meshtastic-interoperability) for commands.

| App | For what | How |
|---|---|---|
| **Mesh** (MESH) | See nearby **Meshtastic** nodes (imported from meshmap.net, and/or heard by MeshScan) as read-only situational awareness — you can't message them, but you can see where they are and how fresh they are. | `r` reloads the SD import; ↑↓ scroll; **Enter** opens a full node card (name, hardware, battery/voltage, position, last-heard). Rows dim as they age. |
| **MeshScan** (SCAN) | Passively **receive** the local Meshtastic mesh over the air — decrypts the public channel and drops decoded **positions, names, device telemetry (battery/voltage) and text messages** into Mesh with live signal strength. **Sweeps the LongFast / MediumFast / ShortFast presets** so it catches nodes whatever preset they use (LongFast is the global default). | Open it and it auto-cycles presets every ~15 s (`n` = next now); Enter jumps to the Mesh list, `c` clears counters. **One radio: while open you can't hear your own mesh.** |
| **MeshTX** (MTX) | **Send** into the local Meshtastic public channel — broadcast a short text, or your GPS position, that real Meshtastic nodes (and meshmap.net) can see. | Type + Enter sends text; **Tab** broadcasts your current GPS position. Retunes to the Meshtastic preset for the send, then restores. Airs on a shared public channel — be a good neighbour. |
| **MeshChat** (MCHT) | A real **two-way conversation** on the Meshtastic public channel — incoming + outgoing text in one feed, like chatting with actual Meshtastic users. The culmination of the interop work. | Type + Enter to send; incoming text scrolls in; **Tab** cycles the preset (LongFast/MediumFast/ShortFast). One radio: deaf to your own mesh while open. |
| **Gateway** (GW) | Uplink **your own** mesh to a computer — streams every frame you hear out USB serial as JSON, to feed the `dissect` tool or a self-hosted map of your fleet. | Enter toggles uplink on/off; runs in the background so you can use other apps. |

## RF & diagnostics

| App | For what | How |
|---|---|---|
| **Sweep** (SWP) | See what's on the air — a live RSSI waterfall across a band, to spot activity or interference. | ↑↓ change step; **`m`** jumps to the EU_868 Meshtastic band (869.525 MHz). |
| **Monitor** (MON) | Watch raw frames off the air (type / address / RSSI / SNR) — a promiscuous sniffer for our protocol. | Just open it; frames scroll as they arrive. |
| **Ranger** (RNG) | Test a link to another node — ping/echo giving RSSI, SNR, round-trip time, loss and distance. | Aim at a peer; reads out link quality. |
| **Console** (CFG) | Tune the radio and understand the trade-offs — edit frequency/SF/BW/CR/power and see live airtime, duty budget and link budget. | ↑↓ pick a field, ←→ change it; `r` cycles region; **`m`** applies the Meshtastic preset; s/l save/load a profile. |
| **Ledger** (LOG) | See what's eating your 1% duty budget — per-message-type airtime accounting + a daily SD summary. | Read-only breakdown. |
| **Probe** (PRB) | Hardware self-test — scans the I2C bus and reports radio / GPS / SD / keyboard / board status on one screen. The fastest way to spot a wiring or detection fault (e.g. a missing keyboard chip). | `r` re-scans. |
| **WiFi** (WIFI) | Scan nearby WiFi access points (SSID / signal / channel / secured) using the otherwise-unused 2.4 GHz radio — a quick site survey. | `r` re-scans, `↑`/`↓` scroll. |

## Time

| App | For what | How |
|---|---|---|
| **Chronos** (TIME) | Give the RTC-less fleet a shared clock — distribute GPS-derived UTC over the mesh; also a daylight-length almanac. | Toggle time-sync broadcast. |
| **Countdown** (CDWN) | A mesh-wide synchronized timer — every node counts down to the same absolute UTC instant and fires together. | Set a fire time; it broadcasts to the mesh. |

## Automation, power, sensors, transfer

| App | For what | How |
|---|---|---|
| **Reflex** (RULE) | On-device IFTTT — automate reactions (e.g. beep/alert/beacon) to events (a message type arrives, an alert, low battery, a timer). | Build event→action rules; persists to flash. |
| **Reactor** (PWR) | Extend battery life — a power state machine that degrades CPU/LCD with hysteresis and drops to a survival low-power beacon. | Automatic; shows the current power state. |
| **Telemetry** (TLM) | Stream live motion (IMU) + battery to a paired unit — a template for any sensor feed. | Enter toggles transmit; a paired unit shows the values. |
| **Dropbox** (DROP) | Send a note/file in chunks over the link (stretch feature). | Chunked transfer. |

---

## Cross-cutting subsystems (not apps)

- **Status bar** — every screen's footer: duty %, seconds-to-next-permitted-TX,
  TX-queue depth, unread badge, channel, battery, GPS fix.
- **Pulse + Presence** — a tiny health TLV (battery/uptime/heap/duty/temp/presence)
  rides along on Beacon/Telemetry/Pong frames, so peers' vitals show up in Fleet
  almost for free.
- **Marshal** — the transmit scheduler: a priority queue with listen-before-talk
  and the 1% duty governor. Alerts/distress jump the queue; everything else waits
  its turn so the band stays legal.
- **Mesh relay** — always-on background forwarding of `MESH`-flagged frames
  (hop-limited, de-duplicated), independent of which app is open.
- **Message capture / Archive** — received/sent text is de-duplicated and buffered
  in RAM, then flushed to SD off the radio path.
- **Airtime attribution / Ledger** — every transmission is charged to a budget
  bucket so you can see what's using airtime.

## Wire protocol & channels

13-byte header + payload + CRC-16. Channel 0 is cleartext "public"; set a PSK for
a keyed channel (payloads are then ChaCha20-encrypted). The 1% duty governor meters
a rolling hour; non-urgent transmits are refused once the budget is spent.

## Host tools (`tools/`)

- **meshpull** — trims the ~3.7 MB [meshmap.net](https://meshmap.net/)
  `nodes.json` feed down to a small CSV of nearby Meshtastic nodes, which you copy
  to the SD card for the **Mesh** app to import.
- **lorakit** — a Go port of the wire codec plus a **`dissect`** CLI that decodes
  frames from a hex capture or the **Gateway** app's JSON, for debugging and for
  building a self-hosted map of your own fleet.

## Audio

The ES8311 codec + 1 W speaker are driven through a shared `services/audio.h`
(`audio::beep/alert/tone`), initialised at boot (a short boot chirp confirms it
works). **Klaxon**, **Mayday**, **Countdown** and **Reflex** beeps now actually
sound. (Still unused: the ES8311 microphone and the IR transmitter.)

## Hardware note — Cardputer-Adv keyboard

The Adv's keyboard is a **TCA8418** I2C matrix chip. Its interrupt line does not
fire reliably on all units, so the firmware **polls the chip's event FIFO** each
loop (`pollKeyboardAdv` in `main.cpp`) rather than relying on the interrupt, and
pins the M5Unified board to `board_M5CardputerADV` so the internal I2C comes up
correctly. The poll tracks the SHIFT key across events so capitals and shifted
symbols type. This is why keys work here where a stock interrupt-only build may
not — and the **Probe** app surfaces exactly this kind of fault at a glance.
