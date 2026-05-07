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
inline constexpr const char* FIRMWARE_VERSION = "v1.2.0";

// ------- ntfy.sh uzaktan kontrol (istemci POLL, kisa timeout; ekranda kitlenmez) -------
/** Konu adi ornek https://ntfy.sh/{topic}; paylasimli makroda ASCII. */
#ifndef DESKBUDDY_NTFY_TOPIC
#define DESKBUDDY_NTFY_TOPIC "deskbuddy"
#endif
/** Bos ise uzaktan komutlar iptal (guvenlik). Ornek uc kelimelik gizli dize. */
#ifndef DESKBUDDY_NTFY_TOKEN
#define DESKBUDDY_NTFY_TOKEN "erdem"
#endif
/** Istek en fazla ms (_wifi / finans gibi bloklama yapmasin diye sinirla). */
#ifndef DESKBUDDY_NTFY_HTTP_MS
#define DESKBUDDY_NTFY_HTTP_MS 2800
#endif
/** POLL araligi; kisa aralik ekranda degil ana dongude zaman paylasilir. */
#ifndef DESKBUDDY_NTFY_POLL_MS
#define DESKBUDDY_NTFY_POLL_MS (45000UL)
#endif
/** Son bildirim penceresi ntfy "since=s" parametresi. */
#ifndef DESKBUDDY_NTFY_SINCE_WINDOW
#define DESKBUDDY_NTFY_SINCE_WINDOW "180s"
#endif
