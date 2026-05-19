#include "db_network_guard.h"

static SemaphoreHandle_t s_netHttpMutex = nullptr;
static uint32_t s_lowHeapLogMs = 0;

void netGuardInit() {
  if (!s_netHttpMutex)
    s_netHttpMutex = xSemaphoreCreateMutex();
}

bool netHeapOk(uint32_t reserveBytes) {
  return ESP.getFreeHeap() >= reserveBytes;
}

bool netHttpBegin(uint32_t reserveBytes, uint32_t lockTimeoutMs) {
  if (!s_netHttpMutex)
    return false;

  if (!netHeapOk(reserveBytes)) {
    const uint32_t now = millis();
    if (now - s_lowHeapLogMs >= 30000) {
      s_lowHeapLogMs = now;
      Serial.printf("[Net] heap low %u (need %u)\n", (unsigned)ESP.getFreeHeap(),
                    (unsigned)reserveBytes);
    }
    return false;
  }

  if (xSemaphoreTake(s_netHttpMutex, pdMS_TO_TICKS(lockTimeoutMs)) != pdTRUE) {
    Serial.println("[Net] http busy, skip");
    return false;
  }

  if (!netHeapOk(reserveBytes)) {
    xSemaphoreGive(s_netHttpMutex);
    return false;
  }
  return true;
}

void netHttpEnd() {
  if (s_netHttpMutex)
    xSemaphoreGive(s_netHttpMutex);
}
