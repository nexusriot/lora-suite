#pragma once
#include <cstdio>
#include <cmath>

extern int g_checks;
extern int g_fails;

#define CHECK(cond) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((double)(a) - (double)(b)) <= (double)(eps))
