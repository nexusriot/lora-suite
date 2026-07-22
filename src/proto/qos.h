#pragma once
#include <cstdint>
#include "frame.h"

namespace ls {

// Marshal QoS class for a frame. Higher = more urgent; the TX scheduler drains
// high classes first and ages low ones up so they never starve.
enum Priority : uint8_t {
  PRIO_BULK      = 0,   // file chunks
  PRIO_TELEMETRY = 1,   // periodic sensor streams
  PRIO_CONTROL   = 2,   // beacon, ack, ping/pong, nodeinfo, timesync, waypoint
  PRIO_NORMAL    = 3,   // text
  PRIO_ALERT     = 4,   // alerts / distress
};

inline Priority priorityOf(uint8_t type, uint8_t flags) {
  (void)flags;
  switch (type) {
    case MSG_ALERT:     return PRIO_ALERT;
    case MSG_TEXT:      return PRIO_NORMAL;
    case MSG_ACK:
    case MSG_PING:
    case MSG_PONG:
    case MSG_BEACON:
    case MSG_NODEINFO:
    case MSG_TIMESYNC:
    case MSG_WAYPOINT:  return PRIO_CONTROL;
    case MSG_TELEMETRY: return PRIO_TELEMETRY;
    case MSG_FILECHUNK: return PRIO_BULK;
    default:            return PRIO_NORMAL;
  }
}

} // namespace ls
