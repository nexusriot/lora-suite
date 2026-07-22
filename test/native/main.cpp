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
  run_crypto_tests();
  run_payloads_tests();
  run_txqueue_tests();
  run_roster_tests();
  run_solar_tests();
  run_ledger_tests();
  run_rules_tests();
  std::printf("\n%d checks, %d failed\n", g_checks, g_fails);
  return g_fails ? 1 : 0;
}
