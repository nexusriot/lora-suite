#pragma once
#include <cstdint>
#include <cstddef>

namespace ls {

// Reflex: a small on-device IFTTT engine. Rules are event -> action; frame events
// are evaluated in the RX handler, timer/battery events on a background tick. TX
// actions route through the duty-gated send path and every rule has a cooldown,
// so automation can't runaway or loop. Pure/testable; the executor is device-side.
enum RuleEvent : uint8_t {
  EV_OFF = 0,
  EV_RX_TYPE,     // a frame of MsgType evParam was received (from another node)
  EV_ALERT,       // any ALERT was received
  EV_BATT_LOW,    // own battery dropped below evArg% (edge-triggered)
  EV_PERIODIC,    // every evArg seconds
};

enum RuleActionType : uint8_t {
  AC_NONE = 0,
  AC_BEEP,
  AC_SEND_TEXT,   // canned text index acParam
  AC_SEND_ALERT,  // alert code index acParam
  AC_BEACON,      // transmit own GPS position
};

struct RuleAction {
  uint8_t type = AC_NONE;
  uint8_t param = 0;
};

struct Rule {
  uint8_t  event    = EV_OFF;
  uint8_t  evParam  = 0;      // EV_RX_TYPE: MsgType
  uint16_t evArg    = 0;      // EV_BATT_LOW: percent; EV_PERIODIC: seconds
  uint8_t  action   = AC_NONE;
  uint8_t  acParam  = 0;
  bool     enabled  = false;
  uint16_t cooldownS = 30;
  // runtime (not serialized)
  uint32_t lastFired = 0;
  bool     hasFired = false;  // so the first match isn't blocked by the cooldown window
  bool     armed = true;      // edge-trigger latch for level events
};

class RuleEngine {
public:
  static const int CAP = 8;

  int count() const { return count_; }
  Rule& at(int i) { return rules_[i]; }
  const Rule& at(int i) const { return rules_[i]; }
  Rule* add() { return count_ < CAP ? &(rules_[count_++] = Rule{}) : nullptr; }
  void remove(int i) {
    if (i < 0 || i >= count_) return;
    for (int j = i + 1; j < count_; j++) rules_[j - 1] = rules_[j];
    count_--;
  }

  // Frame-triggered rules. src/myAddr guard against reacting to our own frames.
  bool onFrame(uint8_t type, bool isAlert, uint16_t src, uint16_t myAddr, uint32_t now, RuleAction& out) {
    if (src == myAddr) return false;
    for (int i = 0; i < count_; i++) {
      Rule& r = rules_[i];
      if (!r.enabled) continue;
      bool match = (r.event == EV_RX_TYPE && type == r.evParam) || (r.event == EV_ALERT && isAlert);
      if (match && (!r.hasFired || now - r.lastFired >= (uint32_t)r.cooldownS * 1000)) {
        r.hasFired = true;
        r.lastFired = now;
        out = {r.action, r.acParam};
        return true;
      }
    }
    return false;
  }

  // Timer / battery rules. battPct < 0 means unknown (skip battery rules).
  bool tick(uint32_t now, int battPct, RuleAction& out) {
    for (int i = 0; i < count_; i++) {
      Rule& r = rules_[i];
      if (!r.enabled) continue;
      if (r.event == EV_PERIODIC) {
        if (r.evArg > 0 && now - r.lastFired >= (uint32_t)r.evArg * 1000) {
          r.lastFired = now;
          out = {r.action, r.acParam};
          return true;
        }
      } else if (r.event == EV_BATT_LOW && battPct >= 0) {
        if (battPct < (int)r.evArg && r.armed) {
          r.armed = false;
          r.lastFired = now;
          out = {r.action, r.acParam};
          return true;
        }
        if (battPct >= (int)r.evArg + 3) r.armed = true;   // hysteresis re-arm
      }
    }
    return false;
  }

  size_t serialize(uint8_t* out, size_t cap) const {
    size_t need = 1 + (size_t)count_ * 9;
    if (cap < need) return 0;
    out[0] = (uint8_t)count_;
    size_t o = 1;
    for (int i = 0; i < count_; i++) {
      const Rule& r = rules_[i];
      out[o++] = r.event; out[o++] = r.evParam;
      out[o++] = r.evArg & 0xFF; out[o++] = r.evArg >> 8;
      out[o++] = r.action; out[o++] = r.acParam; out[o++] = r.enabled ? 1 : 0;
      out[o++] = r.cooldownS & 0xFF; out[o++] = r.cooldownS >> 8;
    }
    return need;
  }

  bool deserialize(const uint8_t* in, size_t n) {
    if (n < 1) return false;
    int c = in[0];
    if (c > CAP || n < 1 + (size_t)c * 9) return false;
    count_ = c;
    size_t o = 1;
    for (int i = 0; i < c; i++) {
      Rule& r = (rules_[i] = Rule{});
      r.event = in[o++]; r.evParam = in[o++];
      r.evArg = (uint16_t)(in[o] | (in[o + 1] << 8)); o += 2;
      r.action = in[o++]; r.acParam = in[o++]; r.enabled = in[o++] != 0;
      r.cooldownS = (uint16_t)(in[o] | (in[o + 1] << 8)); o += 2;
    }
    return true;
  }

private:
  Rule rules_[CAP];
  int count_ = 0;
};

} // namespace ls
