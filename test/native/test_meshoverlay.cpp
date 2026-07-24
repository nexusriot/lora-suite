#include <cstdio>
#include <cstring>
#include "check.h"
#include "../../src/proto/meshoverlay.h"

using namespace ls;

// CSV columns: id,lat,lon,batt,volt,role,seen,hw,short,long
void run_meshoverlay_tests() {
  std::printf("[meshoverlay]\n");

  // --- single well-formed row ---
  {
    MeshOverlay o;
    CHECK(o.ingestCsvLine("1000968721,36.4249088,44.5150000,90,384,2,1784800000,TBEAM,9211,Meshtastic 9211", 1000));
    CHECK(o.size() == 1);
    const MeshNode* m = o.find(1000968721u);
    CHECK(m != nullptr);
    CHECK(m->hasPos);
    CHECK_NEAR(m->lat, 36.4249088, 1e-6);
    CHECK_NEAR(m->lon, 44.5150000, 1e-6);
    CHECK(m->battPct == 90);
    CHECK(m->voltCv == 384);
    CHECK(m->role == MROLE_CLIENT_MUTE);
    CHECK(m->seenEpoch == 1784800000u);
    CHECK(std::strcmp(m->hw, "TBEAM") == 0);
    CHECK(std::strcmp(m->shortName, "9211") == 0);
    CHECK(std::strcmp(m->longName, "Meshtastic 9211") == 0);
    CHECK(m->source == SRC_IMPORT);
    CHECK(m->lastHeard == 1000);
  }

  // --- long name may contain commas; short clamps to 4; hw clamps to 9 ---
  {
    MeshOverlay o;
    CHECK(o.ingestCsvLine("42,10.0,20.0,50,0,3,0,HELTEC_TRACK,TOOLONG,Hello, World, Inc", 1));
    const MeshNode* m = o.find(42);
    CHECK(m != nullptr);
    CHECK(std::strcmp(m->hw, "HELTEC_TR") == 0);              // 9-char clamp
    CHECK(std::strcmp(m->shortName, "TOOL") == 0);            // 4-char clamp
    CHECK(std::strcmp(m->longName, "Hello, World, Inc") == 0); // commas preserved
    CHECK(m->role == MROLE_ROUTER);
  }

  // --- empty hw and empty long name are valid ---
  {
    MeshOverlay o;
    CHECK(o.ingestCsvLine("7,1.0,2.0,0,0,1,0,,AB,", 1));
    const MeshNode* m = o.find(7);
    CHECK(m && m->hw[0] == 0 && m->longName[0] == 0);
    CHECK(std::strcmp(m->shortName, "AB") == 0);
  }

  // --- malformed / non-data rows are rejected and create nothing ---
  {
    MeshOverlay o;
    CHECK(!o.ingestCsvLine("garbage", 1));
    CHECK(!o.ingestCsvLine("1,2", 1));                          // too few fields
    CHECK(!o.ingestCsvLine("1,2.0,3.0,0,0,1,0,HW,SH", 1));      // missing short/long delimiter
    CHECK(!o.ingestCsvLine("0,1.0,2.0,0,0,1,0,,X,zero id", 1)); // id 0 invalid
    CHECK(!o.ingestCsvLine("", 1));
    CHECK(!o.ingestCsvLine("   ", 1));
    CHECK(!o.ingestCsvLine("# just a comment", 1));
    CHECK(o.size() == 0);
  }

  // --- lat/lon 0,0 means position unknown ---
  {
    MeshOverlay o;
    CHECK(o.ingestCsvLine("9,0,0,80,0,1,0,,NOPS,no position", 1));
    const MeshNode* m = o.find(9);
    CHECK(m && !m->hasPos);
  }

  // --- battery >100 (externally powered) kept; unknown role renders "?"; >255 clamps ---
  {
    MeshOverlay o;
    CHECK(o.ingestCsvLine("11,1.0,1.0,101,406,250,0,,EXT,plugged in", 1));
    const MeshNode* m = o.find(11);
    CHECK(m && m->battPct == 101);
    CHECK(m->voltCv == 406);
    CHECK(m->role == 250);                                  // stored verbatim (valid byte)
    CHECK(std::strcmp(meshRoleLabel(m->role), "?") == 0);   // unknown code renders "?"
    CHECK(o.ingestCsvLine("12,1.0,1.0,0,0,999,0,,Y,over", 1));
    CHECK(o.find(12) && o.find(12)->role == MROLE_OTHER);   // out of byte range clamps
  }

  // --- full CSV buffer: header comment + rows + blanks ---
  {
    MeshOverlay o;
    const char* csv =
        "# generated 1784800000\n"
        "# id,lat,lon,batt,volt,role,seen,hw,short,long\n"
        "100,36.1,44.2,60,370,1,1784790000,TBEAM,AAAA,Alpha\n"
        "\n"
        "200,36.3,44.4,70,380,3,1784795000,HELTEC,BBBB,Bravo, the second\n";
    size_t rows = o.ingestCsv(csv, std::strlen(csv), 500);
    CHECK(rows == 2);
    CHECK(o.size() == 2);
    CHECK(o.generatedUnix() == 1784800000u);
    CHECK(o.find(100) && std::strcmp(o.find(100)->longName, "Alpha") == 0);
    CHECK(o.find(200) && std::strcmp(o.find(200)->longName, "Bravo, the second") == 0);
    CHECK(o.find(200) && o.find(200)->seenEpoch == 1784795000u);
  }

  // --- a row without a trailing newline is still parsed ---
  {
    MeshOverlay o;
    const char* csv = "5,1.0,2.0,50,0,1,0,ZZHW,ZZ,no newline";
    CHECK(o.ingestCsv(csv, std::strlen(csv), 1) == 1);
    CHECK(o.find(5) && std::strcmp(o.find(5)->longName, "no newline") == 0);
  }

  // --- ingestCsv drops prior IMPORT nodes but preserves live SCAN nodes ---
  {
    MeshOverlay o;
    o.setPos(0xABCDEF, 12.0, 34.0, 100, SRC_SCAN);       // a live scanned node
    const char* set1 = "1,1.0,1.0,0,0,1,0,,A,one\n2,2.0,2.0,0,0,1,0,,B,two\n";
    o.ingestCsv(set1, std::strlen(set1), 200);
    CHECK(o.size() == 3);
    const char* set2 = "9,9.0,9.0,0,0,1,0,,I,nine\n";      // reload: new import set
    o.ingestCsv(set2, std::strlen(set2), 300);
    CHECK(o.size() == 2);                                 // 1 scan + 1 import
    CHECK(o.find(0xABCDEF) != nullptr);                   // scan survives
    CHECK(o.find(1) == nullptr && o.find(2) == nullptr);  // old imports gone
    CHECK(o.find(9) != nullptr);
  }

  // --- an import row must NOT downgrade a live scanned node with the same id ---
  {
    MeshOverlay o;
    o.setPos(5, 40.0, 44.0, 100, SRC_SCAN);
    o.setRssi(5, -70, 100, SRC_SCAN);
    const char* row = "5,10.0,20.0,80,0,1,0,,IMP,imported\n";  // same id 5 as the scan
    o.ingestCsv(row, std::strlen(row), 200);
    const MeshNode* m = o.find(5);
    CHECK(m && m->source == SRC_SCAN);   // still a live scan, not reclassified to import
    CHECK_NEAR(m->lat, 40.0, 1e-6);      // live position preserved, not the import's 10.0
    CHECK(m->rssi == -70);               // live RSSI preserved
    CHECK(o.size() == 1);                // no duplicate slot for id 5
  }

  // --- live setters upsert and update the same slot ---
  {
    MeshOverlay o;
    o.setUser(77, "Longy", "LG", MROLE_TRACKER, 10, SRC_SCAN);
    o.setPos(77, 51.5, -0.1, 20, SRC_SCAN);
    o.setBattery(77, 45, 30, SRC_SCAN);
    o.setRssi(77, -92, 40, SRC_SCAN);
    CHECK(o.size() == 1);
    const MeshNode* m = o.find(77);
    CHECK(m && std::strcmp(m->shortName, "LG") == 0);
    CHECK(m->role == MROLE_TRACKER);
    CHECK(m->hasPos && m->rssi == -92 && m->battPct == 45);
    CHECK(m->lastHeard == 40);
  }

  // --- prune drops stale nodes only ---
  {
    MeshOverlay o;
    o.setPos(1, 1, 1, 1000, SRC_SCAN);
    o.setPos(2, 2, 2, 5000, SRC_SCAN);
    CHECK(o.prune(6000, 2000) == 1);   // node 1 (age 5000) stale, node 2 (age 1000) kept
    CHECK(o.size() == 1 && o.find(2) != nullptr);
  }

  // --- import never exceeds CAP and never evicts (nearest-first prefix wins) ---
  {
    MeshOverlay o;
    char line[64];
    for (int i = 1; i <= (int)MeshOverlay::CAP + 10; i++) {
      std::snprintf(line, sizeof(line), "%d,1.0,2.0,50,0,1,0,,S,node", i);
      o.ingestCsvLine(line, 1);
    }
    CHECK(o.size() == MeshOverlay::CAP);
    CHECK(o.find(1) != nullptr);                              // first (nearest) kept
    CHECK(o.find((uint32_t)MeshOverlay::CAP + 5) == nullptr); // overflow dropped
  }

  // --- role labels ---
  CHECK(std::strcmp(meshRoleLabel(MROLE_CLIENT_MUTE), "MUTE") == 0);
  CHECK(std::strcmp(meshRoleLabel(MROLE_REPEATER), "RPT") == 0);
  CHECK(std::strcmp(meshRoleLabel(MROLE_OTHER), "?") == 0);
}
