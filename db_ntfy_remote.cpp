/**
 * ntfy.sh JSON poll — uzaktan parlaklik + ana sayfa slot maskesi.
 * Kisa HTTP timeout (bloklu sure sinirli). Cizim dongusune HTTP koymayin.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <cstring>
#include <TFT_eSPI.h>

#include "Deskbuddy_config.h"
#include "Deskbuddy_types.h"
#include "Deskbuddy_layout.h"
#include "db_ntfy_remote.h"

extern Preferences prefs;
extern TFT_eSPI tft;

extern bool manualDimMode;
extern bool sleepDimmed;
extern bool sleepOff;
extern bool pageDirty;

extern void wakeDisplay(bool clearManualMode);
extern void enterManualSleepFull();
extern void restoreSleepAwareBacklight();
extern void setBacklight(int value);
extern void touchResetGate();

/** Sketch ile ayni parlaklik seviyesi (BL_DIM). */
static constexpr uint8_t kBlDimLvl = 18;

uint8_t deskRemoteHideSlotBits = 0;

constexpr const char kNvsLastId[] = "ntiLst";

struct RevertSnap {
  bool armed = false;
  bool manualDim = false;
  bool sd = false;
  bool soff = false;
  uint8_t hides = 0;
};
static RevertSnap revert;

static String mergedTokenValue() {
  String tok = prefs.getString("ntTok", "");
  tok.trim();
#ifdef DESKBUDDY_NTFY_TOKEN
  if (!tok.length() && strlen(DESKBUDDY_NTFY_TOKEN) > 0) {
    tok = String(DESKBUDDY_NTFY_TOKEN);
    tok.trim();
  }
#else
  (void)0;
#endif
  return tok;
}

static bool bodyTokenMatches(const String& body) {
  String want = mergedTokenValue();
  if (!want.length()) return false;
  int nl = body.indexOf('\n');
  String first = nl >= 0 ? body.substring(0, nl) : body;
  first.trim();
  return first.length() != 0 && first == want;
}

/** Title = DESK:TAIL ; TAIL orijinal (SLOT_HIDE desenleri buyuk kullanilir). */
static bool extractDeskTail(const JsonDocument& msg, String& tail) {
  const char* tp = msg["title"];
  if (!tp) return false;
  String title = String(tp);
  title.trim();
  if (!title.length()) return false;
  String up = title;
  up.toUpperCase();
  static const char kPref[] = "DESK:";
  if (!up.startsWith(kPref)) return false;
  tail = title.substring(strlen(kPref));
  tail.trim();
  return tail.length() > 0;
}

static void revertCaptureOnce() {
  if (revert.armed) return;
  revert.manualDim = manualDimMode;
  revert.sd = sleepDimmed;
  revert.soff = sleepOff;
  revert.hides = deskRemoteHideSlotBits;
  revert.armed = true;
}

static void revertApplyNow() {
  if (!revert.armed) return;
  deskRemoteHideSlotBits = revert.hides;
  manualDimMode = revert.manualDim;
  sleepDimmed = revert.sd;
  sleepOff = revert.soff;
  revert.armed = false;
  if (sleepOff)
    tft.fillScreen(TFT_BLACK);
  restoreSleepAwareBacklight();
  touchResetGate();
  pageDirty = true;
}

/** false = kotu format / tannimadi (id yakilmaz callers'ta mantik ile). true = islendi/islem denendi bilinen */
static bool executeDeskTail(const String& tailRaw) {
  String tu = tailRaw;
  tu.trim();
  tu.toUpperCase();

  if (tu == "REVERT" || tu == "UNDO_REMOTE") {
    revertApplyNow();
    return true;
  }

  if (tu.startsWith("SLOT_HIDE:")) {
    String pattern = tailRaw.substring(tailRaw.indexOf(':') + 1);
    pattern.trim();
    pattern.toUpperCase();
    if (pattern.length() != HOME_SLOT_COUNT) return false;

    uint8_t hideMask = 0;
    for (int i = 0; i < HOME_SLOT_COUNT; i++) {
      char c = pattern[i];
      if (c != '1' && c != '0') return false;
      if (c == '1') hideMask |= (uint8_t)(1 << i);
    }
    revertCaptureOnce();
    deskRemoteHideSlotBits = hideMask;
    touchResetGate();
    pageDirty = true;
    return true;
  }

  if (tu == "MIDRIGHT_HIDE" || tu == "MR_HIDE") {
    revertCaptureOnce();
    deskRemoteHideSlotBits = (uint8_t)((1 << 1) | (1 << 3));
    touchResetGate();
    pageDirty = true;
    return true;
  }

  if (tu == "CLEAR_SLOTS" || tu == "SLOTS_SHOW" || tu == "SLOTS_CLEAR") {
    revertCaptureOnce();
    deskRemoteHideSlotBits = 0;
    touchResetGate();
    pageDirty = true;
    return true;
  }

  if (tu == "DIM") {
    revertCaptureOnce();
    sleepOff = false;
    manualDimMode = false;
    sleepDimmed = true;
    setBacklight(kBlDimLvl);
    touchResetGate();
    pageDirty = true;
    return true;
  }

  if (tu == "BRIGHT" || tu == "WAKE") {
    revertCaptureOnce();
    wakeDisplay(true);
    touchResetGate();
    return true;
  }

  if (tu == "BLACKOUT" || tu == "SLEEP") {
    revertCaptureOnce();
    enterManualSleepFull();
    pageDirty = true;
    touchResetGate();
    return true;
  }

  return false;
}

void deskNtfyPollIfDue() {
  if (WiFi.status() != WL_CONNECTED) return;
  /** Token atanmamis: guvenlik (komut ve HTTP yok). */
  if (!mergedTokenValue().length()) return;

  static unsigned long lastPollMs = 0;
  unsigned long interval = DESKBUDDY_NTFY_POLL_MS;
  if (interval < 12000UL) interval = 12000UL;
  unsigned long now = millis();
  if (lastPollMs != 0 && now - lastPollMs < interval) return;
  lastPollMs = now;

  String url = String("https://ntfy.sh/") + String(DESKBUDDY_NTFY_TOPIC) +
               "/json?since=" + String(DESKBUDDY_NTFY_SINCE_WINDOW);

  WiFiClientSecure cli;
  cli.setInsecure();
  cli.setTimeout(DESKBUDDY_NTFY_HTTP_MS);

  HTTPClient http;
  http.setTimeout((int)DESKBUDDY_NTFY_HTTP_MS);

  if (!http.begin(cli, url)) return;

  int code = http.GET();
  String body = (code == HTTP_CODE_OK) ? http.getString() : String();
  http.end();

  if (body.length() == 0 || body.length() > 16384U) return;

  int ls = 0;
  while (ls < (int)body.length()) {
    int nl = body.indexOf('\n', ls);
    if (nl < 0) nl = body.length();

    String line = body.substring(ls, nl);
    line.trim();
    ls = nl + 1;

    StaticJsonDocument<768> msg;
    if (deserializeJson(msg, line)) continue;

    if (strcasecmp(msg["event"] | "", "message") != 0) continue;

    const char* cid = msg["id"] | "";
    if (!cid || cid[0] == '\0') continue;

    String lastHandled = prefs.getString(kNvsLastId, "");
    if (lastHandled.length() && lastHandled.equals(cid)) continue;

    if (!bodyTokenMatches(String(msg["message"] | ""))) continue;

    String tail;
    if (!extractDeskTail(msg, tail)) continue;

    (void)executeDeskTail(tail);
    prefs.putString(kNvsLastId, cid);
  }
}
