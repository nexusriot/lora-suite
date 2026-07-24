#pragma once
#include <cstdint>
#include "../proto/frame.h"
#include "../proto/airtime.h"
#include "../proto/duty.h"
#include "../proto/txqueue.h"
#include "../proto/ledger.h"
#include "../shell/app.h"

namespace ls {

// Called for every decoded, CRC-valid frame off the air (raw, pre-channel-filter).
using RxFn = void (*)(Frame&, const RxMeta&);

// Called with the raw radio bytes of every received packet (before our decode).
// Used by the Meshtastic scanner to parse a foreign protocol on the shared band.
using RawRxFn = void (*)(const uint8_t*, size_t, const RxMeta&);

// Thin driver around RadioLib's SX1262: owns the radio state machine, the duty
// governor, and the shared-SPI arbitration. It knows nothing about channels,
// addressing or apps — that glue lives in net.cpp / main.cpp.
class LoRaService {
public:
  bool begin(const RadioCfg& cfg);
  bool applyConfig(const RadioCfg& cfg);
  void onReceive(RxFn fn) { rx_ = fn; }
  void onRawReceive(RawRxFn fn) { rawRx_ = fn; }   // raw-bytes tap (Meshtastic scanner)

  // Receive-only guard: while true, pump() never transmits. The scanner sets this
  // (with the radio retuned to a foreign preset) so no app accidentally airs our
  // frames on someone else's channel.
  void setRxOnly(bool on) { rxOnly_ = on; }

  // Queue f for transmission through the Marshal scheduler (QoS priority + CAD
  // listen-before-talk + duty governor). urgent bypasses the duty gate. Returns
  // false only if the queue is full and f doesn't outrank the lowest slot.
  bool sendFrame(Frame& f, bool urgent = false);

  void loop();          // drain the DIO1 RX flag, dispatch, then pump TX
  void startReceive();
  void pump();          // Marshal: try to send one queued frame this tick

  // Transmit arbitrary bytes immediately under the current radio config (for a
  // foreign protocol, e.g. a Meshtastic frame). Bypasses the Marshal queue but
  // charges the duty governor. Caller retunes + restores the radio around it.
  bool transmitRaw(const uint8_t* buf, size_t n);

  int16_t rssi() const { return rssi_; }
  float   snr() const { return snr_; }
  float   channelRssi();   // instantaneous, for Sweep
  bool    channelBusy();   // CAD, for Sweep
  size_t  queueDepth() const { return queue_.size(); }

  // Recall: cancel a still-queued outbound frame (before it airs).
  bool cancel(uint16_t src, uint16_t msgid) { return queue_.cancel(src, msgid); }
  const TxQueue& queue() const { return queue_; }

  DutyGovernor& duty() { return duty_; }
  AirLedger& ledger() { return ledger_; }
  const RadioCfg& config() const { return cfg_; }
  const char* lastError() const { return err_; }
  bool ready() const { return ready_; }

private:
  static const uint8_t MAX_TX_FAILS = 3;

  RadioCfg cfg_;
  DutyGovernor duty_;
  AirLedger ledger_;
  TxQueue queue_;
  uint32_t backoffUntil_ = 0;
  uint8_t  txFails_ = 0;
  RxFn rx_ = nullptr;
  RawRxFn rawRx_ = nullptr;
  bool rxOnly_ = false;
  int16_t rssi_ = 0;
  float   snr_ = 0;
  const char* err_ = "";
  bool ready_ = false;
};

} // namespace ls
