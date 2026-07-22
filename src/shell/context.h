#pragma once
#include <cstdint>
#include "../proto/frame.h"
#include "../proto/airtime.h"
#include "../proto/nodetable.h"
#include "../proto/roster.h"
#include "../proto/rules.h"
#include "../crypto/channel.h"
#include "archive_log.h"

namespace ls {

class LoRaService;
class GpsService;
class Storage;
class Clock;

enum PowerState : uint8_t { PWR_PERF = 0, PWR_BALANCED = 1, PWR_ENDURANCE = 2, PWR_SURVIVAL = 3 };
enum PresenceState : uint8_t { PRES_AVAIL = 0, PRES_BUSY = 1, PRES_ENROUTE = 2, PRES_REST = 3 };

// Shared state every app reads and writes. One global instance (ctx) is wired
// up in main.cpp; apps never construct services themselves.
struct Context {
  LoRaService* lora  = nullptr;
  GpsService*  gps   = nullptr;
  Storage*     store = nullptr;
  Clock*       clock = nullptr;

  NodeTable  nodes;
  Roster     roster;      // durable name<->address contacts
  ArchiveLog archive;     // pending message lines to persist (drained by Archive)
  RuleEngine rules;       // Reflex automation rules
  Channel    channel;     // active channel (id + optional key)
  RadioCfg   cfg;         // active radio profile

  PowerState power    = PWR_BALANCED;   // set by Reactor
  uint8_t    presence = PRES_AVAIL;     // own availability, broadcast via the Pulse TLV
  uint8_t    timeSource = 0;            // 0 none, 1 mesh, 2 GPS, 3 NTP
  uint16_t   unread   = 0;              // undismissed incoming messages/alerts

  // Cross-app navigation intent (consumed once in the main loop): an app sets
  // navRequest to a target app's callsign to switch to it, optionally handing
  // off a peer address (e.g. Fleet -> Courier "message this node").
  const char* navRequest = nullptr;
  uint16_t    pendingPeer = ADDR_BROADCAST;

  // Countdown — a Chronos-anchored synchronized timer (absolute UTC fire time).
  uint32_t cdTarget = 0;   // 0 = none
  uint8_t  cdCode   = 0;
  uint16_t cdFrom   = 0;
  bool     cdFired  = false;
  bool       pulseEnabled = true;       // append health TLV to outbound frames
  uint32_t   lastHealthMs = 0;
  uint32_t   healthPeriodMs = 120000;   // min gap between piggybacked health TLVs

  uint16_t myAddr    = 0; // this node's short address
  char     callName[12] = "node";
  uint16_t nextMsgId = 1;
  bool     relayOn   = true;
  uint8_t  relayHops = DEFAULT_HOP;
  uint32_t relayForwarded = 0;
  uint32_t rxCount = 0;
  uint32_t lastRxMs = 0;

  uint16_t allocMsgId() {
    uint16_t id = nextMsgId++;
    if (nextMsgId == 0) nextMsgId = 1;   // never hand out 0
    return id;
  }
};

extern Context ctx;

} // namespace ls
