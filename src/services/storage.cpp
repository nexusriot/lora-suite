#include "storage.h"
#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include <cstdlib>
#include <cstring>
#include "ff.h"          // bundled FatFs — f_mkfs() for a real card format
#include "../hal/pins.h"
#include "../hal/spi_bus.h"

namespace ls {

static Preferences s_prefs;

void Storage::begin() {
  s_prefs.begin("lorasuite", false);
}

void Storage::loadIdentity(uint16_t& addr, char* name, uint8_t nameCap) {
  addr = (uint16_t)s_prefs.getUShort("addr", 0);
  String n = s_prefs.getString("name", "node");
  std::strncpy(name, n.c_str(), nameCap - 1);
  name[nameCap - 1] = 0;
}

void Storage::saveIdentity(uint16_t addr, const char* name) {
  s_prefs.putUShort("addr", addr);
  s_prefs.putString("name", name);
}

static void keyFor(char* buf, const char* base, uint8_t slot) {
  snprintf(buf, 16, "%s%u", base, (unsigned)slot);
}

bool Storage::loadProfile(uint8_t slot, RadioCfg& cfg, char* psk, uint8_t pskCap) {
  char k[16];
  keyFor(k, "cfg", slot);
  size_t got = s_prefs.getBytes(k, &cfg, sizeof(cfg));
  keyFor(k, "psk", slot);
  String p = s_prefs.getString(k, "");
  std::strncpy(psk, p.c_str(), pskCap - 1);
  psk[pskCap - 1] = 0;
  return got == sizeof(cfg);
}

void Storage::saveProfile(uint8_t slot, const RadioCfg& cfg, const char* psk) {
  char k[16];
  keyFor(k, "cfg", slot);
  s_prefs.putBytes(k, &cfg, sizeof(cfg));
  keyFor(k, "psk", slot);
  s_prefs.putString(k, psk ? psk : "");
}

uint8_t Storage::activeSlot() { return (uint8_t)s_prefs.getUChar("slot", 0); }
void Storage::setActiveSlot(uint8_t slot) { s_prefs.putUChar("slot", slot); }

bool Storage::loadRoster(Roster& r) {
  size_t sz = s_prefs.getBytesLength("roster");
  if (sz == 0) return false;
  static uint8_t buf[2560];
  if (sz > sizeof(buf)) return false;
  s_prefs.getBytes("roster", buf, sz);
  return r.deserialize(buf, sz);
}

void Storage::saveRoster(const Roster& r) {
  static uint8_t buf[2560];
  size_t n = r.serialize(buf, sizeof(buf));
  if (n) s_prefs.putBytes("roster", buf, n);
}

bool Storage::loadRules(RuleEngine& r) {
  size_t sz = s_prefs.getBytesLength("rules");
  if (sz == 0) return false;
  uint8_t buf[256];
  if (sz > sizeof(buf)) return false;
  s_prefs.getBytes("rules", buf, sz);
  return r.deserialize(buf, sz);
}

void Storage::saveRules(const RuleEngine& r) {
  uint8_t buf[256];
  size_t n = r.serialize(buf, sizeof(buf));
  if (n) s_prefs.putBytes("rules", buf, n);
}

bool Storage::loadMeshImport(MeshOverlay& mesh, uint32_t now) {
  if (!sd_) return false;
  static uint8_t buf[8192];
  int n = readFile("/mesh/import.csv", buf, sizeof(buf) - 1);
  if (n < 0) return false;
  buf[n] = 0;
  mesh.ingestCsv((const char*)buf, (size_t)n, now);
  return true;
}

void Storage::loadSettings(uint8_t& brightness, uint8_t& volume) {
  brightness = (uint8_t)s_prefs.getUChar("bright", 26);   // 10% default
  volume = (uint8_t)s_prefs.getUChar("vol", 200);
}

void Storage::saveSettings(uint8_t brightness, uint8_t volume) {
  s_prefs.putUChar("bright", brightness);
  s_prefs.putUChar("vol", volume);
}

void Storage::loadWifi(char* ssid, uint8_t ssidCap, char* pass, uint8_t passCap) {
  String s = s_prefs.getString("wssid", "");
  std::strncpy(ssid, s.c_str(), ssidCap - 1); ssid[ssidCap - 1] = 0;
  String p = s_prefs.getString("wpass", "");
  std::strncpy(pass, p.c_str(), passCap - 1); pass[passCap - 1] = 0;
}

void Storage::saveWifi(const char* ssid, const char* pass) {
  s_prefs.putString("wssid", ssid ? ssid : "");
  s_prefs.putString("wpass", pass ? pass : "");
}

int Storage::readFile(const char* path, uint8_t* buf, int cap) {
  if (!sd_) return -1;
  SpiBus::Guard g;
  File f = SD.open(path, FILE_READ);
  if (!f) return -1;
  int n = f.read(buf, cap);
  f.close();
  return n;
}

int Storage::readTail(const char* path, uint8_t* buf, int cap) {
  if (!sd_) return -1;
  SpiBus::Guard g;
  File f = SD.open(path, FILE_READ);
  if (!f) return -1;
  size_t sz = f.size();
  if ((int)sz > cap) f.seek(sz - cap);
  int n = f.read(buf, cap);
  f.close();
  return n;
}

bool Storage::sdBegin() {
  SpiBus::Guard g;
  sd_ = SD.begin(pins::SD_CS, SPI, 20000000);
  return sd_;
}

bool Storage::sdFormat() {
  SpiBus::Guard g;
  // f_mkfs drives the card through the SD library's registered FatFs diskio, so the
  // card must be initialized first (SD.begin registers drive "0:" — the only volume
  // this firmware uses). FM_ANY = FAT/FAT32 with an MBR partition (exFAT is disabled
  // in the framework's ffconf, so large cards land on FAT32) — the layout PCs expect.
  if (!sd_) {
    sd_ = SD.begin(pins::SD_CS, SPI, 20000000);
    if (!sd_) return false;
  }
  BYTE* work = (BYTE*)malloc(FF_MAX_SS);
  if (!work) return false;
  FRESULT res = f_mkfs("0:", FM_ANY, 0, work, FF_MAX_SS);
  free(work);

  // The old mount is stale after mkfs — cycle it so the fresh filesystem is live.
  SD.end();
  sd_ = SD.begin(pins::SD_CS, SPI, 20000000);
  return res == FR_OK;
}

bool Storage::appendLine(const char* path, const char* line) {
  if (!sd_) return false;
  SpiBus::Guard g;
  File f = SD.open(path, FILE_APPEND);
  if (!f) return false;
  f.println(line);
  f.close();
  return true;
}

} // namespace ls
