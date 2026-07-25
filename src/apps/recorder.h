#pragma once
#include <M5Unified.h>
#include <SD.h>
#include <cstdio>
#include <cstring>
#include "../shell/app.h"
#include "../shell/context.h"
#include "../services/storage.h"
#include "../services/audio.h"
#include "../hal/spi_bus.h"
#include "../proto/wavfmt.h"
#include "../ui/theme.h"
#include "../ui/widgets.h"

namespace ls {

// Voice memos — the last unused peripheral on the board. Records 8 kHz mono PCM
// from the ES8311 microphone straight to /memos/NNN.wav on SD (16 KB/s is far
// more than RAM could hold, so audio is streamed in double-buffered chunks while
// the other buffer fills), and plays memos back through the speaker.
//
// Mic and speaker share the one ES8311 codec, so they are never open at the same
// time — each mode ends the other. VERIFY the mic actually enumerates on the Adv;
// the app reports plainly if M5Unified brings up no mic for this board.
class Recorder : public App {
public:
  const char* name() const override { return "Memos"; }
  const char* callsign() const override { return "REC"; }
  Cat category() const override { return Cat::Util; }

  void drawIcon(M5Canvas& g, int x, int y, uint16_t c) override {   // microphone
    g.fillRoundRect(x + 7, y + 2, 6, 10, 3, c);
    g.drawFastHLine(x + 5, y + 14, 10, c);
    g.drawFastVLine(x + 10, y + 14, 4, c);
    g.drawLine(x + 5, y + 11, x + 5, y + 13, c);
    g.drawLine(x + 15, y + 11, x + 15, y + 13, c);
  }

  void onEnter() override { scan(); }
  void onExit() override { stopAll(); }        // never leave the codec running

  void onKey(const KeyEvent& k) override {
    if (k.enter) {
      if (rec_) stopRecord();
      else if (play_) stopPlay();
      else startRecord();
      return;
    }
    if (rec_ || play_) return;                 // list keys are idle-only
    if (k.up && sel_ > 0) sel_--;
    else if (k.down && sel_ + 1 < count_) sel_++;
    else if (k.ch == 'p') startPlay();
    else if (k.ch == 'd') erase();
    else if (k.ch == 'r') scan();
  }

  void update() override {
    if (rec_) pumpRecord();
    else if (play_) pumpPlay();
  }

  void draw(M5Canvas& g) override {
    g.fillScreen(theme::BG);
    ui::header(g, *this);
    g.setTextSize(1);
    int y = ui::BODY_Y + 2;
    char s[44];

    if (!micOk_) {
      g.setTextColor(theme::WARN, theme::BG);
      g.drawString("no microphone detected", 6, y);
      y += 12;
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("ES8311 mic not enumerated", 6, y);
      ui::footer(g);
      return;
    }

    if (rec_) {
      g.setTextColor(theme::CRIT, theme::BG);
      std::snprintf(s, sizeof(s), "REC  %lus  %luKB",
                    (unsigned long)(bytes_ / (RATE * 2)), (unsigned long)(bytes_ / 1024));
      g.drawString(s, 6, y);
      y += 14;
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString("Enter = stop", 6, y);
      ui::footer(g);
      return;
    }

    if (play_) {
      g.setTextColor(theme::ACCENT, theme::BG);
      std::snprintf(s, sizeof(s), "PLAY %s", names_[sel_]);
      g.drawString(s, 6, y);
      y += 14;
      g.setTextColor(theme::MUTED, theme::BG);
      std::snprintf(s, sizeof(s), "%lu / %lu KB",
                    (unsigned long)(played_ / 1024), (unsigned long)(total_ / 1024));
      g.drawString(s, 6, y);
      y += 12;
      g.drawString("Enter = stop", 6, y);
      ui::footer(g);
      return;
    }

    if (!count_) {
      g.setTextColor(theme::MUTED, theme::BG);
      g.drawString(sdOk_ ? "no memos yet" : "no SD card", 6, y);
    } else {
      int start = sel_ > 5 ? sel_ - 5 : 0;
      for (int i = start; i < count_ && i < start + 6; i++) {
        bool s0 = (i == sel_);
        std::snprintf(s, sizeof(s), "%c%-12s %luKB", s0 ? '>' : ' ', names_[i],
                      (unsigned long)(sizes_[i] / 1024));
        g.setTextColor(s0 ? theme::ACCENT : theme::TEXT, theme::BG);
        g.drawString(s, 6, y);
        y += 11;
      }
    }

    g.setTextColor(theme::MUTED, theme::BG);
    g.drawString("Enter=rec p=play d=del", 6, ui::SCREEN_H - ui::FOOTER_H - 10);
    ui::footer(g);
  }

private:
  static const uint32_t RATE = 8000;      // speech-grade: 16 KB/s to the card
  static const size_t   CHUNK = 512;      // samples per buffer (~64 ms)
  static const int      MAXN = 24;

  bool rec_ = false, play_ = false, started_ = false;
  bool micOk_ = true, sdOk_ = false;
  int  cur_ = 0;
  uint32_t bytes_ = 0, played_ = 0, total_ = 0;
  int16_t buf_[2][CHUNK];
  File f_;
  char path_[32] = {0};

  char names_[MAXN][14] = {};
  uint32_t sizes_[MAXN] = {0};
  int count_ = 0, sel_ = 0;

  void scan() {
    count_ = 0;
    sdOk_ = ctx.store && ctx.store->sdReady();
    if (!sdOk_) return;
    SpiBus::Guard g;
    SD.mkdir("/memos");
    File dir = SD.open("/memos");
    if (!dir) return;
    while (count_ < MAXN) {
      File e = dir.openNextFile();
      if (!e) break;
      if (!e.isDirectory()) {
        const char* nm = e.name();
        const char* slash = std::strrchr(nm, '/');
        if (slash) nm = slash + 1;
        std::strncpy(names_[count_], nm, sizeof(names_[0]) - 1);
        names_[count_][sizeof(names_[0]) - 1] = 0;
        sizes_[count_] = e.size();
        count_++;
      }
      e.close();
    }
    dir.close();
    if (sel_ >= count_) sel_ = count_ ? count_ - 1 : 0;
  }

  void startRecord() {
    if (!ctx.store || !ctx.store->sdReady()) { sdOk_ = false; return; }
    {
      SpiBus::Guard g;
      SD.mkdir("/memos");
      for (int i = 0; i < 1000; i++) {
        std::snprintf(path_, sizeof(path_), "/memos/%03d.wav", i);
        if (!SD.exists(path_)) break;
      }
      f_ = SD.open(path_, FILE_WRITE);
      if (!f_) return;
      uint8_t hdr[WAV_HEADER_LEN];
      wavHeader(hdr, 0, RATE);          // placeholder; rewritten on stop
      f_.write(hdr, WAV_HEADER_LEN);
    }

    M5.Speaker.end();                   // shared ES8311: only one direction at a time
    if (!M5.Mic.begin()) {
      micOk_ = false;
      SpiBus::Guard g;
      f_.close();
      M5.Speaker.begin();
      return;
    }
    micOk_ = true;
    bytes_ = 0;
    cur_ = 0;
    started_ = false;
    rec_ = true;
  }

  void pumpRecord() {
    if (!started_) {
      M5.Mic.record(buf_[0], CHUNK, RATE);
      M5.Mic.record(buf_[1], CHUNK, RATE);
      started_ = true;
      return;
    }
    // Two slots are queued; when one frees, buf_[cur_] is the finished one.
    if (M5.Mic.isRecording() >= 2) return;
    {
      SpiBus::Guard g;
      f_.write((const uint8_t*)buf_[cur_], CHUNK * sizeof(int16_t));
    }
    bytes_ += CHUNK * sizeof(int16_t);
    M5.Mic.record(buf_[cur_], CHUNK, RATE);
    cur_ ^= 1;
  }

  void stopRecord() {
    rec_ = false;
    started_ = false;
    M5.Mic.end();
    {
      SpiBus::Guard g;
      uint8_t hdr[WAV_HEADER_LEN];
      wavHeader(hdr, bytes_, RATE);     // now we know the real length
      f_.seek(0);
      f_.write(hdr, WAV_HEADER_LEN);
      f_.close();
    }
    M5.Speaker.begin();
    audio::tick();
    scan();
  }

  void startPlay() {
    if (!count_ || !ctx.store || !ctx.store->sdReady()) return;
    char p[40];
    std::snprintf(p, sizeof(p), "/memos/%s", names_[sel_]);
    SpiBus::Guard g;
    f_ = SD.open(p, FILE_READ);
    if (!f_) return;
    total_ = f_.size() > WAV_HEADER_LEN ? f_.size() - WAV_HEADER_LEN : 0;
    f_.seek(WAV_HEADER_LEN);            // skip the header, stream the PCM
    played_ = 0;
    play_ = true;
  }

  void pumpPlay() {
    if (M5.Speaker.isPlaying()) return;
    size_t n;
    {
      SpiBus::Guard g;
      n = f_.read((uint8_t*)buf_[0], CHUNK * sizeof(int16_t));
    }
    if (n < sizeof(int16_t)) { stopPlay(); return; }
    played_ += n;
    M5.Speaker.playRaw(buf_[0], n / sizeof(int16_t), RATE);
  }

  void stopPlay() {
    play_ = false;
    M5.Speaker.stop();
    SpiBus::Guard g;
    f_.close();
  }

  void stopAll() {
    if (rec_) stopRecord();
    if (play_) stopPlay();
  }

  void erase() {
    if (!count_ || !ctx.store || !ctx.store->sdReady()) return;
    char p[40];
    std::snprintf(p, sizeof(p), "/memos/%s", names_[sel_]);
    {
      SpiBus::Guard g;
      SD.remove(p);
    }
    scan();
  }
};

} // namespace ls
