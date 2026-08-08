# LoRa Suite — Feature Guide

What every app and subsystem is **for**, and how to use it. This is the
explanatory companion to the terse app table in [../README.md](../README.md) and
the architecture in [../DESIGN.md](../DESIGN.md).

The device is an **M5Stack Cardputer-Adv** with the **Cap LoRa868** module
(SX1262 radio + GPS). 39 keyboard-driven apps share one radio, one 13-byte wire
protocol, one duty-cycle budget, and the module's GPS.

**Keys everywhere:** the `;` `.` `,` `/` cluster is ↑ ↓ ← → · **Enter** confirms /
opens · **`` ` ``** is back/ESC · **`\`** arms the global distress prompt and **`=`**
saves a screenshot (both ignored while typing). Each app shows its own extra keys.

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

lora-suite is **not** Meshtastic, but shares the 868 band, so it can both *observe*
and *join* the local Meshtastic world. **Start in MeshCfg** — it decides which
network everything else talks to. See the dedicated section in
[../README.md](../README.md#meshtastic-interoperability) for commands.

| App | For what | How |
|---|---|---|
| **MeshCfg** (MCFG) | **Where you join a Meshtastic network.** Pick the region and modem preset, type a channel name and PSK, and set the name/role you appear under. Everything else — MeshScan, MeshChat, MeshTX, Sweep `m`, Console `m` — follows this. Also the only way to be *visible*: without a NodeInfo announce you show up in everyone's node list as a bare `!xxxxxxxx`. | ↑↓ pick a field, ←→ change an enumerated one, **Enter** to type into a text one. **`a` announces NodeInfo now**, `s` saves to NVS. The right column shows the resulting frequency, SF/BW, channel slot, hash and your `!id`. |
| **Mesh** (MESH) | See nearby **Meshtastic** nodes (imported from meshmap.net, and/or heard by MeshScan) as read-only situational awareness — you can't message them, but you can see where they are and how fresh they are. | `r` reloads the SD import; ↑↓ scroll; **Enter** opens a full node card (name, hardware, battery/voltage, position, last-heard). Rows dim as they age. |
| **MeshScan** (SCAN) | Passively **receive** the local Meshtastic mesh over the air — decrypts the configured channel and drops decoded **positions, names, device telemetry (battery/voltage) and text messages** into Mesh with live signal strength. **Sweeps all nine modem presets**, since the Long/VeryLong ones are narrowband and a receiver on the wrong bandwidth hears nothing at all. | Starts on the preset MeshCfg names, then auto-cycles every ~15 s (`n` = next now); Enter jumps to the Mesh list, `c` clears counters. **One radio: while open you can't hear your own mesh.** |
| **MeshTX** (MTX) | **Send** into the configured Meshtastic channel — broadcast a short text, or your GPS position, that real Meshtastic nodes (and meshmap.net) can see. | Type + Enter sends text; **Tab** broadcasts your current GPS position. Retunes to the Meshtastic preset for the send, then restores. On a public channel — be a good neighbour. |
| **MeshChat** (MCHT) | A real **two-way conversation** on the Meshtastic channel — incoming + outgoing text in one feed, like chatting with actual Meshtastic users. | Type + Enter to send; incoming text scrolls in; **Tab** cycles the preset. One radio: deaf to your own mesh while open. |
| **Gateway** (GW) | Uplink **your own** mesh to a computer — streams every frame you hear out USB serial as JSON, to feed the `dissect` tool or a self-hosted map of your fleet. | Enter toggles uplink on/off; runs in the background so you can use other apps. |

## RF & diagnostics

| App | For what | How |
|---|---|---|
| **Sweep** (SWP) | See what's on the air — a live RSSI waterfall across a band, to spot activity or interference. | ↑↓ change step; **`m`** jumps to the Meshtastic channel configured in MeshCfg. |
| **Monitor** (MON) | Watch raw frames off the air (type / address / RSSI / SNR) — a promiscuous sniffer for our protocol. | Just open it; frames scroll as they arrive. |
| **Ranger** (RNG) | Test a link to another node — ping/echo giving RSSI, SNR, round-trip time, loss and distance. | Aim at a peer; reads out link quality. |
| **Console** (CFG) | Tune the radio and understand the trade-offs — edit frequency/SF/BW/CR/power and see live airtime, duty budget and link budget. | ↑↓ pick a field, ←→ change it; `r` cycles region; **`m`** applies the MeshCfg Meshtastic settings; s/l save/load a profile. |
| **Ledger** (LOG) | See what's eating your 1% duty budget — per-message-type airtime accounting + a daily SD summary. | Read-only breakdown. |
| **Probe** (PRB) | Hardware self-test — scans the I2C bus and reports radio / GPS / SD / keyboard / board status on one screen. The fastest way to spot a wiring or detection fault (e.g. a missing keyboard chip). | `r` re-scans. |
| **WiFi** (WIFI) | Scan nearby WiFi access points (SSID / signal / channel / secured) using the otherwise-unused 2.4 GHz radio — a quick site survey. | `r` re-scans, `↑`/`↓` scroll. |
| **Bluetooth** (BT) | Enable the BLE companion bridge so the phone app can pair and drive the device (messaging / status / mesh / config). | Enter toggles it; shows advertising/connected state. |

## Time

| App | For what | How |
|---|---|---|
| **Chronos** (TIME) | Give the RTC-less fleet a shared clock — distribute GPS-derived UTC over the mesh; also a daylight-length almanac. When there's no GPS fix, **`n`** pulls UTC from the internet over WiFi (SNTP) as a fallback. | Enter broadcasts the sync; **`n`** does a WiFi/NTP sync (needs stored WiFi creds; blocks ~10 s). |
| **Countdown** (CDWN) | A mesh-wide synchronized timer — every node counts down to the same absolute UTC instant and fires together. | Set a fire time; it broadcasts to the mesh. |

## Automation, power, sensors, transfer

| App | For what | How |
|---|---|---|
| **Reflex** (RULE) | On-device IFTTT — automate reactions (e.g. beep/alert/beacon) to events (a message type arrives, an alert, low battery, a timer). | Build event→action rules; persists to flash. |
| **Reactor** (PWR) | Extend battery life — a power state machine that degrades CPU/LCD with hysteresis and drops to a survival low-power beacon. | Automatic; shows the current power state. |
| **Telemetry** (TLM) | Stream live motion (IMU) + battery to a paired unit — a template for any sensor feed. | Enter toggles transmit; a paired unit shows the values. |
| **Dropbox** (DROP) | Send a note/file in chunks over the link (stretch feature). | Chunked transfer. |

## System & utilities

| App | For what | How |
|---|---|---|
| **Settings** (SET) | Adjust screen brightness and speaker volume and have them stick across reboots (saved to NVS). | ↑↓ pick a field, ←→ adjust (±16); applied live and persisted on exit. |
| **SD** (SD) | Check the microSD at a glance — mount status, card type, total / used / free — and wipe or reformat it. | `r` remounts; `e`+**Enter** erases every file (keeps the filesystem); `f`+**Enter** does a real FAT/FAT32 reformat (FatFs `f_mkfs`, an MBR-partitioned layout PCs accept). |
| **IR** (IR) | A programmable universal remote — NEC codes over the IR LED. The table is *your* data: stored in NVS and editable from the phone app, so you can teach it your own gear. | ↑↓ pick, **Enter** transmits, `d` deletes, `r` restores the generic set. Add/edit codes from the companion app's **Remote** tab. **Verify the IR LED GPIO on the Adv.** |
| **Memos** (REC) | Voice notes — records from the built-in microphone (the last unused peripheral) to WAV on the SD card, and plays them back through the speaker. | **Enter** starts/stops recording; ↑↓ pick a memo, `p` plays, `d` deletes, `r` rescans. 8 kHz mono; mic and speaker share one codec so only one runs at a time. |
| **Coulomb** (BATT) | How long the battery has left — logs the level over time, fits the discharge trend, and shows time-to-empty plus a graph of the window. Reactor reacts to the level; this predicts it. | Samples once a minute in the background (also to `/batt.csv`); `c` resets the trend after a charge. |

---

## Cross-cutting subsystems (not apps)

- **Screenshots** — press **`=`** on any screen to save exactly what is displayed
  to `/shots/NNN.bmp` on the SD card (24-bit BMP, written a row at a time so it
  needs no big buffer). Like the distress hotkey it is ignored while an app is
  capturing typed text, so it never fires mid-message.

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
a keyed channel (payloads are then ChaCha20-encrypted). The channel key + id are
derived from the PSK with **HKDF-SHA256** (RFC 5869) — a domain-separated KDF, not
a raw hash — so related channels get independent keys. The 1% duty governor meters
a rolling hour; non-urgent transmits are refused once the budget is spent.

Wire **v3** adds three things to every text message:

- **Authenticated** — keyed frames carry an 8-byte HMAC tag, so a tampered or
  forged frame is dropped instead of decrypting into garbage. (ChaCha20 on its own
  is malleable: flip a ciphertext bit, flip the plaintext bit.)
- **Compressed** — messages are packed with a fixed English dictionary, typically
  to about half size. That is twice the words per 1% duty budget, and often turns a
  two-frame message into one.
- **Longer** — you can now compose up to **480 characters**; anything over one
  frame is split across up to 4 fragments and reassembled at the far end (the old
  limit was a 63-character input box).

## Host tools (`tools/`)

- **meshpull** — trims the ~3.7 MB [meshmap.net](https://meshmap.net/)
  `nodes.json` feed down to a small CSV of nearby Meshtastic nodes, which you copy
  to the SD card for the **Mesh** app to import.
- **lorakit** — a Go port of the wire codec plus a **`dissect`** CLI that decodes
  frames from a hex capture or the **Gateway** app's JSON, for debugging and for
  building a self-hosted map of your own fleet.

## Phone companion (Bluetooth)

The **Bluetooth** app enables a BLE bridge (`shell/ble_bridge`, a Nordic-UART GATT
service) that the **Cardputer Companion** Android app (in [`android/`](../android))
pairs to — a phone front-end like the Meshtastic app. Its functions are grouped into
a bottom-navigation bar plus a Config screen, each mapping to firmware features:

- **Link** — scan / connect / disconnect.
- **Messages** — send/receive LoRa text (broadcast or addressed), canned quick-messages, and the contact roster (tap a contact to address it).
- **Fleet** — live status (GPS / battery / duty / power / channel), the node table with per-node **ping**, and your own **presence** selector.
- **Mesh** — the scanned Meshtastic node feed, plus send text / your GPS position **into** the Meshtastic public channel.
- **Ops** — one-tap actions: GPS **beacon**, **ping**, **alert** (pager), mesh **countdown**, **gateway** toggle, and a confirm-gated **distress / mayday**.
- **Config** — identity (callsign/address), region, brightness, volume, channel **PSK**, WiFi credentials, and an **NTP** time-sync.

The ESP32-S3 is BLE-only (no Classic SPP); commands/events are newline-delimited
JSON. BLE and the WiFi scanner/NTP sync share the 2.4 GHz radio — don't run them at
once. (Some on-device screens — the launcher, Radar's polar plot, Console's live
calculators, Probe — stay device-only by nature.)

## Audio

The ES8311 codec + 1 W speaker are driven through a shared `services/audio.h`
(`audio::beep/alert/tone`), initialised at boot (a short boot chirp confirms it
works). **Klaxon**, **Mayday**, **Countdown** and **Reflex** beeps now actually
sound, and the **Settings** app sets the master volume. (Still unused: the ES8311
microphone — now used by **Memos**.)

## Hardware note — Cardputer-Adv keyboard

The Adv's keyboard is a **TCA8418** I2C matrix chip. Its interrupt line does not
fire reliably on all units, so the firmware **polls the chip's event FIFO** each
loop (`pollKeyboardAdv` in `main.cpp`) rather than relying on the interrupt, and
pins the M5Unified board to `board_M5CardputerADV` so the internal I2C comes up
correctly. The poll tracks the SHIFT key across events so capitals and shifted
symbols type. This is why keys work here where a stock interrupt-only build may
not — and the **Probe** app surfaces exactly this kind of fault at a glance.
