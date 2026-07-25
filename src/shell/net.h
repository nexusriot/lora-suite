#pragma once
#include "context.h"
#include "../proto/payloads.h"

namespace ls {

// Stamp src/msgid/chan on f, encrypt if the channel is keyed, and transmit via
// LoRaService. Returns false if the duty governor blocks it. Frames forwarded by
// the mesh relay bypass this (they keep their original src/msgid) and go straight
// to LoRaService::sendFrame.
bool netSend(Frame& f, bool urgent = false);

Frame makeText(uint16_t dst, const char* text, bool wantAck = false);
Frame makeAck(uint16_t dst, uint16_t ackId);
Frame makeBeacon(const Position& p);
Frame makeAlert(uint8_t code, const char* label);
Frame makeNodeInfo();
Frame makePing(uint16_t dst, uint16_t seq);
Frame makePong(uint16_t dst, uint16_t seq, int16_t rssi, int8_t snr);
Frame makeWaypoint(const Waypoint& w);
Frame makeTimeSync(uint32_t unix, uint8_t source);
Frame makeDistress(const Position& p, uint8_t battPct);
Frame makeCountdown(uint32_t unix, uint8_t code);

// Execute a fired Reflex rule action (beep / send canned text / alert / beacon).
void runRuleAction(const RuleAction& a);

// Connect to the stored WiFi network, fetch UTC via SNTP, and adopt it into the
// Clock as an NTP source (a GPS-less time fallback). Blocks up to ~13 s; returns
// true on success. WiFi is turned off again afterward.
bool ntpSyncViaWifi();

} // namespace ls
