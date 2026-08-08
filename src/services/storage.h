#pragma once
#include <cstdint>
#include "../proto/airtime.h"
#include "../proto/roster.h"
#include "../proto/rules.h"
#include "../proto/meshoverlay.h"
#include "../proto/meshtastic.h"
#include "../proto/ircodes.h"

namespace ls {

// Persists identity + radio profiles in NVS (Preferences) and provides SD access
// for the logging apps. All SD I/O goes through the shared SPI bus guard.
class Storage {
public:
  void begin();

  // Identity / active settings.
  void loadIdentity(uint16_t& addr, char* name, uint8_t nameCap);
  void saveIdentity(uint16_t addr, const char* name);

  // Named radio profiles (slot 0..7). PSK is stored alongside.
  bool loadProfile(uint8_t slot, RadioCfg& cfg, char* psk, uint8_t pskCap);
  void saveProfile(uint8_t slot, const RadioCfg& cfg, const char* psk);
  uint8_t activeSlot();
  void setActiveSlot(uint8_t slot);

  // Contact roster (NVS blob).
  bool loadRoster(Roster& r);
  void saveRoster(const Roster& r);

  // Reflex rules (NVS blob).
  bool loadRules(RuleEngine& r);
  void saveRules(const RuleEngine& r);

  // Foreign-node snapshot from SD (/mesh/import.csv, produced by tools/meshpull).
  // Replaces the overlay's imported entries; live scanned nodes are preserved.
  bool loadMeshImport(MeshOverlay& mesh, uint32_t now);

  // Meshtastic network settings (region/preset/channel/key/identity). Returns
  // false when nothing is stored or the blob predates the current layout, so the
  // caller keeps its defaults rather than adopting a half-read config.
  bool loadMeshCfg(MeshtasticCfg& cfg);
  void saveMeshCfg(const MeshtasticCfg& cfg);

  // UI settings (brightness / speaker volume), NVS-persisted.
  void loadSettings(uint8_t& brightness, uint8_t& volume);
  void saveSettings(uint8_t brightness, uint8_t volume);

  // WiFi credentials for the NTP time fallback.
  void loadWifi(char* ssid, uint8_t ssidCap, char* pass, uint8_t passCap);
  void saveWifi(const char* ssid, const char* pass);

  // User-editable IR remote codes (NVS blob).
  bool loadIrCodes(IrCodeSet& s);
  void saveIrCodes(const IrCodeSet& s);

  // Dump a 16-bit RGB565 framebuffer to SD as a 24-bit BMP. `path` is created or
  // overwritten. Rows are converted and written one at a time, so this needs no
  // large intermediate buffer.
  bool writeBmp(const char* path, const uint16_t* px, uint32_t w, uint32_t h);

  // Next unused /shots/NNN.bmp path; false if the directory is full or absent.
  bool nextShotPath(char* out, size_t cap);

  // microSD.
  bool sdReady() const { return sd_; }
  bool sdBegin();
  // Reformat the card to a fresh FAT/FAT32 filesystem (erases everything). Blocks
  // for seconds; returns true on success and leaves the card remounted.
  bool sdFormat();
  bool appendLine(const char* path, const char* line);
  int  readFile(const char* path, uint8_t* buf, int cap);   // bytes read from start, or -1
  int  readTail(const char* path, uint8_t* buf, int cap);   // last <=cap bytes, or -1

private:
  bool sd_ = false;
};

} // namespace ls
