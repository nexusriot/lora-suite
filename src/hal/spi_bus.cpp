#include "spi_bus.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace ls {

static SemaphoreHandle_t s_mtx = nullptr;

void SpiBus::begin() {
  if (!s_mtx) s_mtx = xSemaphoreCreateRecursiveMutex();
}

void SpiBus::lock() {
  if (!s_mtx) begin();
  xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY);
}

void SpiBus::unlock() {
  if (s_mtx) xSemaphoreGiveRecursive(s_mtx);
}

} // namespace ls
