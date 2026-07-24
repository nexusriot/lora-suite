#include "lora_service.h"
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "../hal/pins.h"
#include "../hal/spi_bus.h"

namespace ls {

static SPIClass s_spi(FSPI);
static Module   s_mod(pins::LORA_NSS, pins::LORA_DIO1, pins::LORA_RST, pins::LORA_BUSY, s_spi);
static SX1262   s_radio(&s_mod);
static volatile bool s_rxFlag = false;

static void IRAM_ATTR onDio1() { s_rxFlag = true; }

bool LoRaService::begin(const RadioCfg& cfg) {
  SpiBus::begin();
  s_spi.begin(pins::LORA_SCK, pins::LORA_MISO, pins::LORA_MOSI, pins::LORA_NSS);
  cfg_ = cfg;

  SpiBus::lock();
  int st = s_radio.begin(cfg.freqHz / 1e6, cfg.bwHz / 1000.0, cfg.sf, cfg.cr,
                         cfg.syncWord, cfg.power, cfg.preamble);
  SpiBus::unlock();
  if (st != RADIOLIB_ERR_NONE) { err_ = "radio begin failed"; return false; }

  // Most SX1262 modules (incl. the LoRa-1262 stamp) drive the RF switch from
  // DIO2. If the module has a TCXO, also add: s_radio.setTCXO(1.8);
  s_radio.setDio2AsRfSwitch(true);
  s_radio.setDio1Action(onDio1);
  ready_ = true;
  startReceive();
  return true;
}

bool LoRaService::applyConfig(const RadioCfg& cfg) {
  if (!ready_) return begin(cfg);
  cfg_ = cfg;
  SpiBus::Guard g;
  s_radio.setFrequency(cfg.freqHz / 1e6);
  s_radio.setBandwidth(cfg.bwHz / 1000.0);
  s_radio.setSpreadingFactor(cfg.sf);
  s_radio.setCodingRate(cfg.cr);
  s_radio.setOutputPower(cfg.power);
  s_radio.setPreambleLength(cfg.preamble);
  s_radio.setSyncWord(cfg.syncWord);
  return true;
}

bool LoRaService::sendFrame(Frame& f, bool urgent) {
  if (!ready_) return false;
  return queue_.push(f, millis(), urgent);
}

void LoRaService::pump() {
  if (!ready_ || rxOnly_ || queue_.empty()) return;
  uint32_t now = millis();
  if (now < backoffUntil_) return;

  Frame f;
  bool urgent = false;
  if (!queue_.peek(now, f, urgent)) return;
  uint32_t toa = (uint32_t)timeOnAirMs(cfg_, f.len);

  // duty gate (urgent traffic — alerts/distress — bypasses it)
  if (!urgent && !duty_.canSend(now, toa)) {
    uint32_t wait = duty_.timeToNextTxMs(now, toa);
    if (wait == DutyGovernor::NEVER || wait > 2000) wait = 2000;
    backoffUntil_ = now + wait;
    return;
  }
  // listen-before-talk: defer if the channel is busy
  if (!urgent && channelBusy()) {
    backoffUntil_ = now + 20 + (now % 40);
    return;
  }

  uint8_t buf[MAX_FRAME];
  size_t nb = encode(f, buf, sizeof(buf));
  if (!nb) { queue_.removeBest(now); txFails_ = 0; return; }   // un-encodable: drop, don't livelock

  int st;
  {
    SpiBus::Guard g;
    st = s_radio.transmit(buf, nb);
  }
  startReceive();

  if (st == RADIOLIB_ERR_NONE) {
    duty_.record(millis(), toa);       // only charge duty for a real transmission
    ledger_.record(f.airTag, toa);     // attribute the airtime (by type / relay)
    queue_.removeBest(now);
    txFails_ = 0;
  } else if (++txFails_ >= MAX_TX_FAILS) {
    queue_.removeBest(now);            // give up after N to avoid head-of-line livelock
    txFails_ = 0;
  } else {
    backoffUntil_ = now + 40;          // transient: retry the same frame shortly
  }
}

void LoRaService::startReceive() {
  if (!ready_) return;
  SpiBus::Guard g;
  s_radio.startReceive();
}

void LoRaService::loop() {
  if (ready_ && s_rxFlag) {
    s_rxFlag = false;

    uint8_t buf[MAX_FRAME];
    size_t n = 0;
    int st;
    {
      SpiBus::Guard g;
      size_t len = s_radio.getPacketLength();
      if (len > sizeof(buf)) len = sizeof(buf);
      st = s_radio.readData(buf, len);
      n = len;
      rssi_ = (int16_t)s_radio.getRSSI();
      snr_  = s_radio.getSNR();
    }
    startReceive();

    if (st == RADIOLIB_ERR_NONE && n > 0) {
      RxMeta m{rssi_, (int8_t)snr_, millis()};
      if (rawRx_) rawRx_(buf, n, m);        // raw tap (Meshtastic scanner) sees every packet
      Frame f;
      if (decode(buf, n, f) && rx_) rx_(f, m);
    }
  }
  pump();   // Marshal: attempt one queued transmission
}

float LoRaService::channelRssi() {
  if (!ready_) return -200;
  SpiBus::Guard g;
  return s_radio.getRSSI();
}

bool LoRaService::channelBusy() {
  if (!ready_) return false;
  SpiBus::Guard g;
  int st = s_radio.scanChannel();
  startReceive();
  return st == RADIOLIB_LORA_DETECTED;
}

} // namespace ls
