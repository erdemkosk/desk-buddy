#pragma once

#include <Arduino.h>

/** TLS + JSON icin minimum bos heap; altinda istek atlanir (panic onlenir). */
constexpr uint32_t NET_HEAP_RESERVE_TLS = 30000;
constexpr uint32_t NET_HEAP_RESERVE_HTTP = 16000;
/** Truncgil altin JSON (~8K doc + govde). */
constexpr uint32_t NET_HEAP_RESERVE_FINANCE = 42000;

void netGuardInit();

bool netHeapOk(uint32_t reserveBytes);
bool netHttpBegin(uint32_t reserveBytes, uint32_t lockTimeoutMs = 8000);
void netHttpEnd();

/** RAII: tek seferde yalnizca bir HTTP/TLS oturumu. */
struct NetHttpScope {
  bool active = false;
  explicit NetHttpScope(uint32_t reserveBytes) { active = netHttpBegin(reserveBytes); }
  ~NetHttpScope() {
    if (active)
      netHttpEnd();
  }
  explicit operator bool() const { return active; }
};
