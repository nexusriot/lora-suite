#include <M5Cardputer.h>
#include <cstring>

#include "proto/frame.h"
#include "proto/payloads.h"
#include "proto/dedup.h"
#include "proto/defrag.h"
#include "proto/squeeze.h"
#include "shell/context.h"
#include "shell/net.h"
#include "shell/screen_manager.h"
#include "shell/launcher.h"
#include "shell/ble_bridge.h"
#include "services/lora_service.h"
#include "services/gps_service.h"
#include "services/storage.h"
#include "services/clock.h"
#include "services/audio.h"
#include "hal/spi_bus.h"
#include "ui/theme.h"

#include "apps/courier.h"
#include "apps/chat.h"
#include "apps/archive.h"
#include "apps/recall.h"
#include "apps/contacts.h"
#include "apps/fleet.h"
#include "apps/relay.h"
#include "apps/beacon.h"
#include "apps/gps.h"
#include "apps/radar.h"
#include "apps/mesh.h"
#include "apps/pathfinder.h"
#include "apps/breadcrumb.h"
#include "apps/mayday.h"
#include "apps/sweep.h"
#include "apps/mesh_scan.h"
#include "apps/mesh_tx.h"
#include "apps/mesh_chat.h"
#include "apps/mesh_cfg.h"
#include "apps/monitor.h"
#include "apps/ranger.h"
#include "apps/chronos.h"
#include "apps/countdown.h"
#include "apps/console.h"
#include "apps/gateway.h"
#include "apps/probe.h"
#include "apps/wifi_scan.h"
#include "apps/bt.h"
#include "apps/settings.h"
#include "apps/sdutils.h"
#include "apps/ir_blaster.h"
#include "apps/recorder.h"
#include "apps/coulomb.h"
#include "apps/ledger.h"
#include "apps/reflex.h"
#include "apps/reactor.h"
#include "apps/klaxon.h"
#include "apps/telemetry.h"
#include "apps/dropbox.h"

using namespace ls;

static LoRaService loraSvc;
static GpsService  gpsSvc;
static Storage     storage;
static Clock       clk;

static Courier    courier;
static Chat       chat;
static Archive    archiveApp;
static Recall     recallApp;
static Contacts   contacts;
static Fleet      fleet;
static Relay      relay;
static Beacon     beacon;
static Gps        gpsApp;
static Radar      radar;
static Mesh       meshApp;
static Pathfinder pathfinder;
static Breadcrumb breadcrumb;
static Mayday     mayday;
static Sweep      sweep;
static MeshScan   meshScan;
static MeshTX     meshTx;
static MeshChat   meshChat;
static MeshCfg    meshCfgApp;
static Monitor    monitor;
static Ranger     ranger;
static Chronos     chronos;
static CountdownApp countdownApp;
static Console     console;
static Gateway     gateway;
static Probe       probe;
static WifiScan    wifiScan;
static Bluetooth   bluetooth;
static Settings    settingsApp;
static SdUtils     sdUtils;
static IrBlaster   irBlaster;
static Recorder    recorder;
static Coulomb     coulomb;
static Ledger      ledgerApp;
static Reflex      reflexApp;
static Reactor     reactor;
static Klaxon     klaxon;
static TelemetryApp telemetry;
static Dropbox    dropbox;

static App* apps[] = {
    &courier, &chat, &archiveApp, &recallApp, &contacts, &fleet, &relay, &beacon, &gpsApp, &radar,
    &meshApp, &pathfinder, &breadcrumb, &mayday, &sweep, &meshScan, &meshTx, &meshChat, &meshCfgApp, &monitor, &ranger, &chronos, &countdownApp,
    &console, &gateway, &probe, &wifiScan, &bluetooth, &settingsApp, &sdUtils, &irBlaster, &recorder, &coulomb, &ledgerApp, &reflexApp, &reactor, &klaxon, &telemetry, &dropbox};
static const int APP_COUNT = sizeof(apps) / sizeof(apps[0]);

static ScreenManager sm;
static Launcher launcher(apps, APP_COUNT, &sm);
static Dedup relayDedup;
static Dedup rxDedup;    // separate: dedups archive/unread per message (not the relay gate)
static Defrag defrag;    // reassembles multi-fragment text messages
static M5Canvas canvas(&M5Cardputer.Display);

// Gateway/Uplink: re-encode a heard frame to its on-air bytes and stream it out
// the USB serial console as one JSON line (feeds tools/lorakit dissect + a
// meshobserv fork). Uses printf, not Serial: on the Cardputer-Adv the console is
// the USB-Serial-JTAG the host sees, and Arduino `Serial` (USB CDC) is not wired.
static void emitGateway(const Frame& f, const RxMeta& m) {
  uint8_t buf[MAX_FRAME];
  size_t n = encode(f, buf, sizeof(buf));
  if (!n) return;
  printf("{\"t\":%lu,\"rssi\":%d,\"snr\":%d,\"hex\":\"",
         (unsigned long)m.when, m.rssi, (int)m.snr);
  for (size_t i = 0; i < n; i++) printf("%02x", buf[i]);
  printf("\"}\n");
  ctx.gatewaySent++;
}

// A TEXT frame's body, once decrypted: reassemble it if fragmented, decompress
// it if squeezed, then deliver the whole message. Fragments produce nothing until
// the last piece lands. `wire` is the original frame (for ACK addressing).
static void receiveText(const Frame& wire, const Frame& local, const RxMeta& m) {
  const uint8_t* body = local.payload;
  size_t bodyLen = local.len;                  // reassembled length can exceed a frame

  if (local.flags & FLAG_FRAGMENT) {
    FragHeader h;
    if (!parseFragHeader(body, local.len, h)) return;
    if (!defrag.offer(wire.src, h, body + FRAG_HDR_LEN, (uint8_t)(local.len - FRAG_HDR_LEN), m.when))
      return;                                  // still waiting on other fragments
    body = defrag.data();
    bodyLen = defrag.size();
  }

  char text[TEXT_MAX + 1];
  size_t n;
  if (local.flags & FLAG_SQUEEZE) {
    n = unsqueeze(body, bodyLen, (uint8_t*)text, TEXT_MAX);
    if (n == 0) return;                        // malformed compressed stream: drop
  } else {
    n = bodyLen > TEXT_MAX ? TEXT_MAX : bodyLen;
    memcpy(text, body, n);
  }
  text[n] = 0;

  ctx.unread++;
  char t[9];
  clk.hms(t);
  ctx.archive.add(t, 'I', wire.src, text);        // persist received text (Archive)
  ble::onRadioText(wire.src, text, m.rssi);       // mirror to the phone bridge
  sm.onTextMessage(wire.src, text, (uint16_t)n, m);

  // ACK at the RX choke rather than inside one app, so an addressed message is
  // confirmed no matter which screen is open (and once per message, not per
  // fragment — only the last fragment carries FLAG_ACK_REQ).
  if ((wire.flags & FLAG_ACK_REQ) && wire.dst == ctx.myAddr) {
    Frame a = makeAck(wire.src, wire.msgid);
    netSend(a, true);
  }
}

// Every decoded frame off the air: node table, monitor tap, mesh relay (on the
// still-encrypted frame, skipping blocked contacts), then channel filter,
// decrypt, strip the Pulse health tail, route by type, and hand to the app.
static void onRadioFrame(Frame& f, const RxMeta& m) {
  ctx.rxCount++;
  ctx.lastRxMs = m.when;
  ctx.nodes.heard(f.src, m.rssi, m.snr, m.when);
  sm.onRaw(f, m);
  if (ctx.gatewayOn) emitGateway(f, m);

  if (ctx.relayOn && (f.flags & FLAG_MESH) && f.hop > 1 && f.src != ctx.myAddr &&
      !ctx.roster.isBlocked(f.src)) {
    if (!relayDedup.seen(f.src, f.msgid, m.when)) {
      Frame fwd = f;
      fwd.hop = f.hop - 1;
      fwd.airTag = AIRTAG_RELAY;   // Ledger: forwarded airtime is its own bucket
      // Forward alerts (incl. distress) urgently — type is readable from the
      // plaintext header even when the payload is encrypted.
      if (loraSvc.sendFrame(fwd, fwd.type == MSG_ALERT)) ctx.relayForwarded++;
    }
  }

  if (f.chan != ctx.channel.id()) return;
  Frame local = f;
  // Verify-then-decrypt. A keyed frame that fails authentication is forged or
  // corrupted, so it is dropped here and never reaches an app.
  if (!ctx.channel.open(local)) return;

  if ((local.flags & FLAG_HEALTH) && local.len >= HEALTH_LEN) {
    Health h;
    if (unpackHealth(local.payload + local.len - HEALTH_LEN, HEALTH_LEN, h))
      ctx.nodes.setHealth(f.src, h.battPct, h.uptimeHr, h.dutyPct, h.tempC,
                          (local.flags & FLAG_LOWPWR) != 0, h.presence, m.when);
    local.len -= HEALTH_LEN;
  }

  switch (local.type) {
    case MSG_BEACON: {
      Position p;
      if (unpackPosition(local.payload, local.len, p))
        ctx.nodes.setPos(f.src, p.lat, p.lon, m.when);
      break;
    }
    case MSG_NODEINFO: {
      char nm[12];
      uint8_t n = local.len < 11 ? local.len : 11;
      memcpy(nm, local.payload, n);
      nm[n] = 0;
      ctx.nodes.setName(f.src, nm, m.when);
      ctx.roster.setName(f.src, nm);
      break;
    }
    case MSG_TIMESYNC: {
      TimeSync ts;
      if (unpackTimeSync(local.payload, local.len, ts))
        clk.adopt(ts.unix, 1);   // second-hand -> "mesh" quality
      break;
    }
    case MSG_COUNTDOWN: {
      // self-guard + dedup: relayed echoes must not re-arm a cancelled timer or re-fire
      Countdown cd;
      if (f.src != ctx.myAddr && !rxDedup.seen(f.src, f.msgid, m.when) &&
          unpackCountdown(local.payload, local.len, cd)) {
        ctx.cdTarget = cd.unix;
        ctx.cdCode = cd.code;
        ctx.cdFrom = f.src;
        ctx.cdFired = false;
      }
      break;
    }
    case MSG_TEXT:
      // dedup across relayed copies so a message is archived / counted once
      if (f.src != ctx.myAddr && !rxDedup.seen(f.src, f.msgid, m.when))
        receiveText(f, local, m);
      break;
    case MSG_ALERT:
      if (local.len >= 1 && local.payload[0] == ALERT_DISTRESS) {
        Position p;
        uint8_t batt = 0;
        if (local.len >= 1 + POSITION_LEN) unpackPosition(local.payload + 1, POSITION_LEN, p);
        if (local.len >= 2 + POSITION_LEN) batt = local.payload[1 + POSITION_LEN];
        mayday.receiveDistress(f.src, p, batt);   // ungated: each copy refreshes the alarm
        if (sm.top() != &mayday) sm.push(&mayday);
      }
      if (f.src != ctx.myAddr && !rxDedup.seen(f.src, f.msgid, m.when)) ctx.unread++;
      break;
    default:
      break;
  }
  sm.onPacket(local, m);

  // Reflex: frame-triggered automation rules (self-frames guarded in the engine).
  RuleAction ra;
  if (ctx.rules.onFrame(local.type, local.type == MSG_ALERT, f.src, ctx.myAddr, m.when, ra))
    runRuleAction(ra);
}

// Screenshots are taken right after the frame is pushed, so the file matches
// exactly what was on screen when the key was pressed.
static bool shotPending = false;

static void takeScreenshot() {
  if (!ctx.store || !ctx.store->sdReady()) return;
  char path[32];
  if (!ctx.store->nextShotPath(path, sizeof(path))) return;
  const uint16_t* px = (const uint16_t*)canvas.getBuffer();
  if (!px) return;
  if (ctx.store->writeBmp(path, px, ui::SCREEN_W, ui::SCREEN_H)) audio::tick();
}

// Keys the shell claims before any app sees them. Both keyboard paths funnel
// through here. Everything is gated on consumesText() so these characters stay
// typeable while an app is capturing text, and can't fire by accident.
static bool handleGlobalKey(const KeyEvent& ev) {
  if (!ev.ch || (sm.top() && sm.top()->consumesText())) return false;
  if (ev.ch == '\\') {
    // panic() only arms a confirm prompt; it does not transmit until Enter.
    if (sm.top() != &mayday) sm.push(&mayday);
    mayday.panic();
    return true;
  }
  if (ev.ch == '=') {
    shotPending = true;
    return true;
  }
  return false;
}

static void translateKeys() {
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return;
  auto st = M5Cardputer.Keyboard.keysState();
  KeyEvent ev;
  ev.enter = st.enter;
  ev.del = st.del;
  ev.tab = st.tab;
  // The ; . , / keys double as arrows; ` acts as ESC; \ is a global panic.
  for (char c : st.word) {
    switch (c) {
      case ';': ev.up = true; break;
      case '.': ev.down = true; break;
      case ',': ev.left = true; break;
      case '/': ev.right = true; break;
      case '`': ev.esc = true; break;
      case '\\': ev.ch = '\\'; break;
      default:  ev.ch = c; break;
    }
  }
  if (handleGlobalKey(ev)) return;
  sm.onKey(ev);
}

// The Cardputer-Adv keyboard (TCA8418) interrupt line (GPIO 11) does not fire on
// this unit, so the library's interrupt-driven reader never delivers keys. Poll
// the chip's event FIFO directly over the internal I2C and dispatch, reusing the
// library's getKey() char map. (translateKeys() still covers working-IRQ units.)
static void pollKeyboardAdv() {
  static bool shift = false;   // tracked across events so capitals/symbols type
  const uint8_t ADDR = 0x34;   // TCA8418
  bool any = false;
  for (int i = 0; i < 16; i++) {   // drain the event FIFO directly (0 = empty)
    uint8_t evb = M5.In_I2C.readRegister8(ADDR, 0x04, 400000);
    if (evb == 0) break;
    any = true;
    bool pressed = evb & 0x80;               // bit7 = pressed (vs released)
    int code = (evb & 0x7f) - 1;
    if (code < 0) continue;
    int rawRow = code / 10, rawCol = code % 10;
    Point2D_t pt;
    pt.x = rawRow * 2 + (rawCol > 3 ? 1 : 0); // remap raw (row,col) to the key grid
    pt.y = rawCol % 4;
    KeyValue_t kv = M5Cardputer.Keyboard.getKeyValue(pt);
    if ((uint8_t)kv.value_first == KEY_LEFT_SHIFT) { shift = pressed; continue; }
    if (!pressed) continue;                  // other keys act on key-down only
    // Shift picks value_second — so ';.,/' stay arrows unshifted but type ':>?<' shifted.
    uint8_t c = (uint8_t)(shift ? kv.value_second : kv.value_first);
    if (c == 0) continue;

    KeyEvent ev;
    switch (c) {
      case ';': ev.up = true; break;
      case '.': ev.down = true; break;
      case ',': ev.left = true; break;
      case '/': ev.right = true; break;
      case '`': ev.esc = true; break;
      case KEY_ENTER: ev.enter = true; break;
      case KEY_BACKSPACE: ev.del = true; break;
      case KEY_TAB: ev.tab = true; break;
      case '\\': ev.ch = '\\'; break;
      default:
        if (c >= 0x20 && c < 0x7f) ev.ch = (char)c; else continue;
        break;
    }
    if (handleGlobalKey(ev)) continue;
    sm.onKey(ev);
  }
  if (any) M5.In_I2C.writeRegister8(ADDR, 0x02, 0x01, 400000);   // clear K_INT status
}

void setup() {
  auto m5cfg = M5.config();
  // On ESP32-S3, M5Unified's default fallback_board is AtomS3Lite; if Cardputer-Adv
  // auto-detection is uncertain, the whole device (I2C, keyboard, display) is set up
  // wrong and the TCA8418 keyboard never comes up. Pin the fallback to the Adv.
  m5cfg.fallback_board = m5::board_t::board_M5CardputerADV;
  M5Cardputer.begin(m5cfg, true);
  audio::init();
  audio::beep();                 // boot chirp — proves the speaker is alive
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextSize(1);
  canvas.setColorDepth(16);
  canvas.createSprite(ui::SCREEN_W, ui::SCREEN_H);
  canvas.setFont(&fonts::Font0);
  SpiBus::begin();

  storage.begin();
  { uint8_t b, v; storage.loadSettings(b, v); M5Cardputer.Display.setBrightness(b); audio::setVolume(v); }
  storage.loadIdentity(ctx.myAddr, ctx.callName, sizeof(ctx.callName));
  if (ctx.myAddr == 0 || ctx.myAddr == ADDR_BROADCAST) {
    uint64_t mac = ESP.getEfuseMac();
    ctx.myAddr = (uint16_t)(mac ^ (mac >> 16) ^ (mac >> 32));
    if (ctx.myAddr == 0 || ctx.myAddr == ADDR_BROADCAST) ctx.myAddr = 0x0001;
    snprintf(ctx.callName, sizeof(ctx.callName), "n%04X", ctx.myAddr);
    storage.saveIdentity(ctx.myAddr, ctx.callName);
  }
  storage.loadRoster(ctx.roster);
  storage.loadRules(ctx.rules);
  storage.loadMeshCfg(ctx.meshCfg);   // falls back to the public EU_868 LongFast defaults
  if (!storage.loadIrCodes(ctx.irCodes)) ctx.irCodes.loadDefaults();

  char psk[24] = {0};
  if (storage.loadProfile(storage.activeSlot(), ctx.cfg, psk, sizeof(psk)) && psk[0])
    ctx.channel.setPSK(psk);
  else
    ctx.channel.clear();

  ctx.lora = &loraSvc;
  ctx.gps = &gpsSvc;
  ctx.store = &storage;
  ctx.clock = &clk;
  clk.attach(&gpsSvc);

  gpsSvc.begin();
  storage.sdBegin();
  loraSvc.onReceive(onRadioFrame);
  if (!loraSvc.begin(ctx.cfg)) {
    M5Cardputer.Display.fillScreen(theme::CRIT);
    M5Cardputer.Display.drawString("LoRa init failed", 10, 60);
  }

  sm.begin(&launcher);
}

void loop() {
  M5Cardputer.update();
  loraSvc.loop();          // drains RX + pumps the Marshal TX queue
  gpsSvc.loop();
  clk.loop();
  ble::loop();
  ctx.timeSource = clk.source();
  translateKeys();
  pollKeyboardAdv();

  // Cross-app navigation intent (e.g. Fleet -> Courier "message this node").
  if (ctx.navRequest) {
    for (int i = 0; i < APP_COUNT; i++)
      if (std::strcmp(apps[i]->callsign(), ctx.navRequest) == 0) {
        if (sm.top() != apps[i]) sm.push(apps[i]);
        break;
      }
    ctx.navRequest = nullptr;
    ctx.pendingPeer = ADDR_BROADCAST;   // drop any unconsumed handoff
  }

  defrag.sweep(millis());   // reclaim reassemblies whose sender went away

  for (int i = 0; i < APP_COUNT; i++) apps[i]->background();   // background services

  sm.update();
  sm.draw(canvas);
  canvas.pushSprite(0, 0);
  if (shotPending) { shotPending = false; takeScreenshot(); }
  delay(10);
}
