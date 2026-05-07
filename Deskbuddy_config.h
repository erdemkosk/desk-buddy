#pragma once

// NVS doldurma (bir kez); bos birakin normal kullanimda
#ifndef DESKBUDDY_WIFI_FALLBACK_SSID
#define DESKBUDDY_WIFI_FALLBACK_SSID ""
#endif
#ifndef DESKBUDDY_WIFI_FALLBACK_PASS
#define DESKBUDDY_WIFI_FALLBACK_PASS ""
#endif

/** Kurulum soft-AP SSID (QR + WiFi.softAP). */
inline constexpr const char* DESKBUDDY_SOFTAP_SSID = "Deskbuddy-Setup";

/** Firmware semver; drawTopBar + web */
inline constexpr const char* FIRMWARE_VERSION = "v1.2.1";

/** Varsayilan ntfy.sh public topic (NVS anahtari: ntfyTopic). */
inline constexpr const char* NTFY_DEFAULT_TOPIC = "deskbuddy";
