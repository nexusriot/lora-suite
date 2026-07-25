# Cardputer Companion

An Android phone companion for the **lora-suite** firmware
(`workspace/my/lora-suite`) running on an M5Stack Cardputer-Adv. It pairs over
**Bluetooth Low Energy** and gives you a phone-sized front-end for the device's
LoRa functions — like the Meshtastic phone app paired to a node.

## How it connects

The ESP32-S3 is **BLE-only** (no Classic Bluetooth / SPP). The firmware exposes a
Nordic UART Service (NUS) GATT peripheral; this app connects to it and exchanges
**newline-delimited JSON**:

- **commands → device:** `{"c":"tx","to":"FFFF","t":"hi"}`, `{"c":"get","w":"nodes|mesh|status"}`, `{"c":"cfg",...}`
- **events → phone:** `{"e":"msg",...}`, `{"e":"st",...}` (status), `{"e":"nd",...}` (node), `{"e":"mn",...}` (Meshtastic node)

On the device: open the **Bluetooth** (`BT`) app and press Enter to enable the
bridge, then Scan + connect here.

## Screens

- **Link** — scan for `LoRa-*` devices and connect.
- **Chat** — send LoRa text (broadcast `FFFF` or a hex address) and see incoming messages.
- **Status** — GPS fix, battery, RX/relay counts, channel, TX-queue depth, and the node table.
- **Mesh** — the foreign Meshtastic nodes the device has scanned (from MeshScan).
- **Config** — set the callsign, address, region and screen brightness on the device.

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
