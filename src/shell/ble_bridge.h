#pragma once
#include <cstdint>

namespace ls {

// BLE companion bridge — a Nordic-UART-Service (NUS) GATT peripheral that a phone
// app connects to, exchanging newline-delimited JSON: commands in (send text,
// config, get nodes/mesh/status), events out (incoming msg, status, node/mesh
// dumps). ESP32-S3 is BLE-only (no Classic SPP). Only runs while enabled from the
// Bluetooth app. Commands from the BLE task are queued and executed on the main
// loop (radio access must stay single-threaded).
namespace ble {

void begin(const char* name);
void end();
bool enabled();
bool connected();
void loop();                                                  // drain commands + periodic status
void onRadioText(uint16_t from, const char* text, int16_t rssi);  // hook from onRadioFrame

} // namespace ble
} // namespace ls
