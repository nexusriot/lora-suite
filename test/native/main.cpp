#include <cstdio>
#include "check.h"
#include "tests.h"

int g_checks = 0;
int g_fails = 0;

int main() {
  std::printf("lora-suite native core tests\n");
  run_frame_tests();
  run_airtime_tests();
  run_duty_tests();
  run_dedup_tests();
  run_nodetable_tests();
  run_meshoverlay_tests();
  run_crypto_tests();
  run_aes_tests();
  run_sha256_tests();
  run_nec_tests();
  run_mac_tests();
  run_squeeze_tests();
  run_defrag_tests();
  run_wire3_tests();
  run_media_tests();
  run_battlog_tests();
  run_ircodes_tests();
  run_base64_tests();
  run_meshtastic_tests();
  run_payloads_tests();
  run_txqueue_tests();
  run_roster_tests();
  run_solar_tests();
  run_ledger_tests();
  run_rules_tests();
  std::printf("\n%d checks, %d failed\n", g_checks, g_fails);
  return g_fails ? 1 : 0;
}
