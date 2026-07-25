# Cardputer Companion

An Android phone companion for the **lora-suite** firmware
(`workspace/my/lora-suite`) running on an M5Stack Cardputer-Adv. It pairs over
**Bluetooth Low Energy** and gives you a phone-sized front-end for the device's
LoRa functions — like the Meshtastic phone app paired to a node.

## How it connects

The ESP32-S3 is **BLE-only** (no Classic Bluetooth / SPP). The firmware exposes a
Nordic UART Service (NUS) GATT peripheral; this app connects to it and exchanges
**newline-delimited JSON**:

- **commands → device:** `{"c":"tx","to":"FFFF","t":"hi"}`, `{"c":"get","w":"nodes|mesh|status|roster"}`, `{"c":"cfg",...}`, `{"c":"pres","p":2}`, `{"c":"beacon"}`, `{"c":"ping","to":"1A2B"}`, `{"c":"alert","label":"HELP"}`, `{"c":"distress"}`, `{"c":"countdown","secs":60}`, `{"c":"gw","on":1}`, `{"c":"meshtx","t":"hi"}`, `{"c":"ntp"}`
- **events → phone:** `{"e":"msg"}`, `{"e":"st"}` (status), `{"e":"nd"}` (node), `{"e":"ct"}` (contact), `{"e":"mn"}` (Meshtastic node), `{"e":"ntp"}`, `{"e":"meshtx"}`

On the device: open the **Bluetooth** (`BT`) app and press Enter to enable the
bridge, then Scan + connect here.

## Screens

Functions are grouped by area — a bottom navigation bar for the day-to-day areas,
plus a Config screen behind the top-bar gear (settings live off the main flow, per
Android convention):

- **Link** — scan for `LoRa-*` devices and connect / disconnect.
- **Messages** — send LoRa text (broadcast `FFFF` or a hex address), canned quick-messages, and the contact roster (tap a contact to address it).
- **Fleet** — live status (GPS / battery / duty / power / channel), the node table with per-node **Ping**, and your own **presence** selector.
- **Mesh** — the foreign Meshtastic nodes the device has scanned (MeshScan), plus send text / your GPS position **into** the Meshtastic public channel.
- **Ops** — one-tap actions: GPS **beacon**, **ping**, **alert** pager, mesh **countdown**, **gateway** toggle, and a confirm-gated **distress / mayday**.
- **Config** (gear) — callsign, address, region, brightness, volume, channel **PSK**, WiFi credentials, and an **NTP** time-sync.

## Build

```bash
export JAVA_HOME=/opt/android-studio/jbr ANDROID_HOME=$HOME/Android/Sdk
./gradlew :app:assembleDebug
# → app/build/outputs/apk/debug/app-debug.apk
```

Kotlin/Compose, AGP 8.5 / Kotlin 2.0, minSdk 26. BLE via the platform
`android.bluetooth` APIs (no third-party BLE lib); JSON via the framework `org.json`.

**Status:** builds to an APK and is logically complete against the device's BLE
protocol, but the actual pairing hasn't been exercised on a physical phone yet —
verify the connect/scan flow on-device.
