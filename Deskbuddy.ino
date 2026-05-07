// Deskbuddy V.8
// Nav: Home / Weather / Notes / Status
// Full version
// - Home: Doviz (10 sn donusumlu), Buddy animasyonlu yuz, web slot secimi
// - Home + Hava (ust sol): Sicaklik basligi, deger + hava ikonu (aynı sprite cizimi)
// - KP dots replaced with Low / Medium / High / Extreme text
// - KP level text uses same small font as wind direction and stays inside the box
// - Wind + direction added to Weather page
// - Wind direction uses Accent color
// - Weather sun event field automatically shows Sunrise or Sunset, whichever is next
// - Uptime added to Status page
// - Wi-Fi: NVS keys wifiSsid / wifiPass; first boot opens AP + WIFI: QR (telefon ile ag katilimi) + 192.168.4.1
// - Ust bar wifi unut: NVS sil, onay (Evet/Hayir) sonra kurulum AP

// Arduino: otomatik fonksiyon prototipleri son #include'dan sonra eklenir;
// parametre tipleri (HomeWidgetType, WxKind) include'lardan once tanimlanir.
enum HomeWidgetType {
  HOME_WIDGET_HUMIDITY = 0,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_OUTDOOR,
  HOME_WIDGET_KP,
  HOME_WIDGET_UV,
  HOME_WIDGET_WIND,
  HOME_WIDGET_SUN,
  HOME_WIDGET_FINANCE,
  HOME_WIDGET_BUDDY
};
const int HOME_SLOT_COUNT = 4;

/** Open-Meteo WMO weather_code */
enum WxKind {
  WX_UNK,
  WX_CLEAR_DAY,
  WX_CLEAR_NIGHT,
  WX_PARTLY,
  WX_CLOUD,
  WX_FOG,
  WX_DRIZZLE,
  WX_RAIN,
  WX_SNOW,
  WX_SHOWER,
  WX_THUNDER
};

#include <WiFi.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>
#include <stdio.h>
#include "qrcode.h"

// =========================================================
// WIFI
// =========================================================
// NVS anahtarlari: wifiSsid, wifiPass (kurulum portalindan yazilir).
// Ilk kullanimda bos ise "Deskbuddy-Setup" AP + tarayici 192.168.4.1
//
// Isteg bagli tek seferlik tasma (bir kez NVS'e yazilir, sonra bu satirlari bos birakin):
#ifndef DESKBUDDY_WIFI_FALLBACK_SSID
#define DESKBUDDY_WIFI_FALLBACK_SSID ""
#endif
#ifndef DESKBUDDY_WIFI_FALLBACK_PASS
#define DESKBUDDY_WIFI_FALLBACK_PASS ""
#endif

static const char* WIFI_AP_SSID = "Deskbuddy-Setup";

// =========================================================
// DISPLAY / TOUCH
// =========================================================
TFT_eSPI tft;

const int ROT = 2;
const bool INV = false;

#define TOUCH_CS  33
#define TOUCH_IRQ 36

static const int T_SCK  = 25;
static const int T_MISO = 39;
static const int T_MOSI = 32;

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS);

static const int TOUCH_X_MIN = 562;
static const int TOUCH_X_MAX = 3604;
static const int TOUCH_Y_MIN = 544;
static const int TOUCH_Y_MAX = 3720;

static const bool TOUCH_SWAP_XY = false;
static const bool TOUCH_FLIP_X  = false;
static const bool TOUCH_FLIP_Y  = false;

// =========================================================
// WEB / STORAGE
// =========================================================
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

// =========================================================
// SPRITES
// =========================================================
TFT_eSprite sprClock = TFT_eSprite(&tft);
TFT_eSprite sprSmall = TFT_eSprite(&tft);
TFT_eSprite sprNotes = TFT_eSprite(&tft);
static bool sprNotesViewportReady = false;

// =========================================================
// LOCATION
// =========================================================
float LAT = 38.4705f;
float LNG = 27.1155f;
String locationName = "İzmir";

// =========================================================
// THEME
// =========================================================
uint16_t COL_BG        = 0x08A3;
uint16_t COL_PANEL     = 0x1106;
uint16_t COL_PANEL_ALT = 0x18C7;
uint16_t COL_STROKE    = 0x31EC;
uint16_t COL_TEXT      = 0xEF7D;
uint16_t COL_DIM       = 0x94B2;
uint16_t COL_ACCENT    = 0x5EFA;

const uint16_t COL_GREEN  = TFT_GREEN;
const uint16_t COL_YELLOW = 0xFFE0;
const uint16_t COL_RED    = TFT_RED;
const uint16_t COL_BLUE   = 0x041F;

String textColorKey = "standard";
String unitKey = "metric"; // metric = C/mm, imperial = F/in
String regionFormatKey = "europe"; // europe = 24h + dd.mm.yyyy, us = 12h + mm/dd/yyyy

// =========================================================
// LAYOUT
// =========================================================
const int SCREEN_W = 240;
const int SCREEN_H = 320;
const int TOPBAR_H = 34;
const int NAV_H    = 44;

/** Ust bar sag: wifi unut (sol), daralt, tam uyku / ay (sag). */
const int TOPBAR_BTN_SZ = 23;
const int TOPBAR_BTN_GAP = 11;
const int TOPBAR_BTN_MR = 5;

static int topBarMoonBtnX() { return SCREEN_W - TOPBAR_BTN_SZ - TOPBAR_BTN_MR; }
static int topBarDimBtnX() { return topBarMoonBtnX() - TOPBAR_BTN_SZ - TOPBAR_BTN_GAP; }
static int topBarWifiForgetBtnX() { return topBarDimBtnX() - TOPBAR_BTN_SZ - TOPBAR_BTN_GAP; }

/** Firmware semver; baslik yaninda gosterilir (drawTopBar). */
static const char* FIRMWARE_VERSION = "v1.2.0";

const int HOME_GRID_Y1 = 120;
const int HOME_GRID_Y2 = 198;
const int HOME_WIDGET_H = 70;

const int HOME_TIMER_X = 124;
const int HOME_TIMER_Y = HOME_GRID_Y1;
const int HOME_TIMER_W = 108;
const int HOME_TIMER_H = HOME_WIDGET_H;
const int TIMER_MENU_X = 20;
const int TIMER_MENU_Y = 68;
const int TIMER_MENU_W = 200;
const int TIMER_MENU_H = 194;
const int TIMER_DONE_X = 26;
const int TIMER_DONE_Y = 92;
const int TIMER_DONE_W = 188;
const int TIMER_DONE_H = 108;
/** Wi-Fi agini unut onay kutusu */
const int WIFI_FORGET_DLG_X = 16;
const int WIFI_FORGET_DLG_Y = 88;
const int WIFI_FORGET_DLG_W = 208;
const int WIFI_FORGET_DLG_H = 144;
const int PAGE_ROW1_Y = 42;
const int PAGE_ROW2_Y = 120;
const int PAGE_ROW3_Y = 198;
const int PAGE_WIDGET_H = HOME_WIDGET_H;

// =========================================================
// NOTES
// =========================================================
String notesText = "Henuz not yok.";
bool notesDirty = true;

/** Notlar alanı: uzun metinde dikey kaydırma + sağ kenarda scrollbar */
static const int NOTES_MAX_LINES = 96;
static const int NOTES_VIEW_X = 18;
static const int NOTES_VIEW_Y = 54;
static const int NOTES_VIEW_W = 204;
static const int NOTES_VIEW_H = 196;
static const int NOTES_SCROLLBAR_W = 6;
static const int NOTES_TEXT_MAX_W = 190;

static String notesWrappedLines[NOTES_MAX_LINES];
static int notesWrappedLineCount = 0;
static int notesTotalContentPx = 0;
static int notesScrollY = 0;
static bool notesViewportDirty = true;
static bool notesFingerDown = false;
static int notesDragLastY = 0;
String buddyNickname = "";

HomeWidgetType homeWidgetSlots[HOME_SLOT_COUNT] = {
  HOME_WIDGET_HUMIDITY,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_OUTDOOR
};

String cacheHomeSlots[HOME_SLOT_COUNT];

// =========================================================
// STATE
// =========================================================
enum Page {
  PAGE_HOME = 0,
  PAGE_WEATHER = 1,
  PAGE_NOTES = 2,
  PAGE_STATUS = 3
};

Page currentPage = PAGE_HOME;
Page lastDrawnPage = (Page)-1;

unsigned long lastClockTick = 0;
unsigned long lastDataTick  = 0;

const unsigned long CLOCK_TICK_MS = 1000;
const unsigned long DATA_TICK_MS  = 30UL * 1000UL;

bool pageDirty = true;
bool dataDirty = true;

// cache
String cacheClock = "";
String cacheTemp = "";
String cacheRain = "";
String cacheHomeEmpty1 = "";
String cacheHomeEmpty2 = "";
String cacheFocusTimer = "";
String cacheTimerMenu = "";
String cacheTimerDone = "";
String cacheTimerDoneCountdown = "";
String cacheTimerDoneFlash = "";
String cacheWifiForgetDlg = "";

String lastWifiText = "";
String lastSignalText = "";
String lastIpText = "";
String lastUptimeText = "";
String lastTempText = "";
String lastRainText = "";
String lastUvText = "";
String lastUvLevelText = "";
String lastKpText = "";
String lastKpLevelText = "";
String lastWindText = "";
String lastWindDirText = "";
String lastNextSunLabel = "";
String lastNextSunTime = "";
String lastNotesText = "";
String lastNetworkToggleText = "";

const char* homeWidgetKey(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_HUMIDITY: return "humidity";
    case HOME_WIDGET_TIMER:   return "timer";
    case HOME_WIDGET_RAIN:    return "rain";
    case HOME_WIDGET_OUTDOOR: return "outdoor";
    case HOME_WIDGET_KP:      return "kp";
    case HOME_WIDGET_UV:      return "uv";
    case HOME_WIDGET_WIND:    return "wind";
    case HOME_WIDGET_SUN:     return "sun";
    case HOME_WIDGET_FINANCE: return "finance";
    case HOME_WIDGET_BUDDY:   return "buddy";
    default:                  return "humidity";
  }
}

const char* homeWidgetLabel(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_HUMIDITY: return "Nem";
    case HOME_WIDGET_TIMER:   return "Sayac";
    case HOME_WIDGET_RAIN:    return "Yagmur";
    case HOME_WIDGET_OUTDOOR: return "Sicaklik";
    case HOME_WIDGET_KP:      return "Kp";
    case HOME_WIDGET_UV:      return "UV";
    case HOME_WIDGET_WIND:    return "Ruzgar";
    case HOME_WIDGET_SUN:     return "Gunes";
    case HOME_WIDGET_FINANCE: return "Doviz";
    case HOME_WIDGET_BUDDY:   return "Buddy";
    default:                  return "Nem";
  }
}

HomeWidgetType homeWidgetFromKey(const String& key) {
  if (key == "humidity" || key == "week") return HOME_WIDGET_HUMIDITY;
  if (key == "timer") return HOME_WIDGET_TIMER;
  if (key == "rain") return HOME_WIDGET_RAIN;
  if (key == "outdoor") return HOME_WIDGET_OUTDOOR;
  if (key == "kp") return HOME_WIDGET_KP;
  if (key == "uv") return HOME_WIDGET_UV;
  if (key == "wind") return HOME_WIDGET_WIND;
  if (key == "sun") return HOME_WIDGET_SUN;
  if (key == "finance") return HOME_WIDGET_FINANCE;
  if (key == "buddy") return HOME_WIDGET_BUDDY;
  return HOME_WIDGET_HUMIDITY;
}

const char* homeSlotLabel(int slot) {
  switch (slot) {
    case 0: return "Ust sol";
    case 1: return "Ust sag";
    case 2: return "Alt sol";
    case 3: return "Alt sag";
    default: return "Yuva";
  }
}

void getHomeSlotRect(int slot, int& x, int& y, int& w, int& h) {
  const int xs[HOME_SLOT_COUNT] = {8, 124, 8, 124};
  const int ys[HOME_SLOT_COUNT] = {HOME_GRID_Y1, HOME_GRID_Y1, HOME_GRID_Y2, HOME_GRID_Y2};
  x = xs[slot];
  y = ys[slot];
  w = 108;
  h = HOME_WIDGET_H;
}

void appendHomeWidgetOptions(String& page, const String& selectedKey) {
  const HomeWidgetType types[] = {
    HOME_WIDGET_HUMIDITY,
    HOME_WIDGET_TIMER,
    HOME_WIDGET_RAIN,
    HOME_WIDGET_OUTDOOR,
    HOME_WIDGET_KP,
    HOME_WIDGET_UV,
    HOME_WIDGET_WIND,
    HOME_WIDGET_SUN,
    HOME_WIDGET_FINANCE,
    HOME_WIDGET_BUDDY
  };

  for (HomeWidgetType type : types) {
    const char* key = homeWidgetKey(type);
    page += "<option value='";
    page += key;
    page += "'";
    if (selectedKey == key) page += " selected";
    page += ">";
    page += homeWidgetLabel(type);
    page += "</option>";
  }
}

void clearHomeSlotCaches() {
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }
}

// Focus timer
bool focusMenuOpen = false;
bool focusTimerRunning = false;
bool focusTimerFinished = false;
unsigned long focusEndMs = 0;
unsigned long focusDurationSec = 0;
unsigned long focusRemainingSec = 0;
bool timerDoneDialogOpen = false;
unsigned long timerDoneDialogStartedMs = 0;
/** Wi-Fi NVS kaydini silmeden once onay kutusu */
bool wifiForgetConfirmOpen = false;
const unsigned long TIMER_DONE_DIALOG_MS = 60UL * 1000UL;
bool flashModeEnabled = false;
int timerPresetMin[6] = {1, 5, 10, 15, 25, 30};
bool wifiEnabled = true;
bool wifiConnectInProgress = false;
unsigned long wifiConnectStartedMs = 0;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL;

// Weather
static float tempC = NAN;
static float tempMinC = NAN;
static float tempMaxC = NAN;
static float precipMm = NAN;
static float windSpeedMs = NAN;
static float windDirectionDeg = NAN;
static float uvIndex = NAN;
static float humidityPct = NAN;
static int weatherCode = -1;
static int weatherIsDay = 1;
static time_t lastWeatherFetch = 0;
static const uint32_t WEATHER_INTERVAL_SEC = 10 * 60;

// Kur / gram altin (TL): finance.truncgil.com API (USD + Rates.GRA)
static float financeUsdTry = NAN;
static float financeGoldTryGram = NAN;
static time_t lastFinanceFetch = 0;
static const uint32_t FINANCE_INTERVAL_SEC = 15 * 60; // Truncgil kur / altin yenileme
static const unsigned long FINANCE_ROTATE_MS = 10000UL; // Ana sayfada altin <-> doviz gosterimi

// KP-index
static float kpIndex = NAN;
static time_t lastKpFetch = 0;
static const uint32_t KP_INTERVAL_SEC = 10 * 60;

// Sunrise / Sunset
static int sunriseMin = -1;
static int sunsetMin  = -1;
static int lastSunYmd = -1;
static time_t lastSyncTime = 0;

// =========================================================
// SLEEP / BACKLIGHT
// =========================================================
const int BACKLIGHT_PIN = 21;

bool sleepDimmed = false;
bool sleepOff = false;
bool manualDimMode = false;

unsigned long lastInteractionMs = 0;

int sleepIntervalMin = 10;

const int BL_FULL = 255;
const int BL_DIM  = 18;
const int BL_OFF  = 0;
const int FLASH_BL_LOW = 20;
const int FLASH_BL_HIGH = 255;

void touchResetGate();

int sanitizeTimerMinutes(int value);

// =========================================================
// HELPERS
// =========================================================
static int ymdFromLocal(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return (tmLocal.tm_year + 1900) * 10000 + (tmLocal.tm_mon + 1) * 100 + tmLocal.tm_mday;
}

static int minutesFromLocalEpoch(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return tmLocal.tm_hour * 60 + tmLocal.tm_min;
}

static int minutesNowLocal() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  return tmNow.tm_hour * 60 + tmNow.tm_min;
}

static String wifiStatusText() {
  if (!wifiEnabled) return "Kapali";
  return WiFi.status() == WL_CONNECTED ? "Bagli" : "Net yok";
}

static String signalText() {
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) return "-- dBm";
  return String(WiFi.RSSI()) + " dBm";
}

static String ipText() {
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) return "-";
  return WiFi.localIP().toString();
}

static bool useUsRegionFormat() {
  return regionFormatKey == "us";
}

static String formatClockParts(const struct tm& tmValue, bool withSeconds) {
  char buf[20];
  const char* pattern = useUsRegionFormat()
    ? (withSeconds ? "%I:%M:%S %p" : "%I:%M %p")
    : (withSeconds ? "%H:%M:%S" : "%H:%M");
  strftime(buf, sizeof(buf), pattern, &tmValue);
  return String(buf);
}

/** Gun adi: TFT fontunda Turkce ozel harf yok; sadece ASCII kisaltma (Car = Carsamba). */
static const char* weekdayShortGfx(int tm_wday) {
  static const char* names[] = {"Paz", "Pzt", "Sal", "Car", "Per", "Cum", "Cmt"};
  if (tm_wday < 0 || tm_wday > 6) return "";
  return names[tm_wday];
}

static String formatDateParts(const struct tm& tmValue) {
  char buf[40];
  const char* wd = weekdayShortGfx(tmValue.tm_wday);
  if (useUsRegionFormat()) {
    snprintf(buf, sizeof(buf), "%s %02d/%02d/%04d", wd,
             tmValue.tm_mon + 1, tmValue.tm_mday, tmValue.tm_year + 1900);
  } else {
    snprintf(buf, sizeof(buf), "%s %02d.%02d.%04d", wd,
             tmValue.tm_mday, tmValue.tm_mon + 1, tmValue.tm_year + 1900);
  }
  return String(buf);
}

static String formatMinuteOfDay(int minOfDay) {
  if (minOfDay < 0) return "--:--";
  if (useUsRegionFormat()) {
    int hour24 = minOfDay / 60;
    int minute = minOfDay % 60;
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    char buf[12];
    snprintf(buf, sizeof(buf), "%d:%02d %s", hour12, minute, hour24 >= 12 ? "PM" : "AM");
    return String(buf);
  }
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", minOfDay / 60, minOfDay % 60);
  return String(buf);
}

static String tempText() {
  if (isnan(tempC)) return unitKey == "imperial" ? "--.-F" : "--.-C";

  if (unitKey == "imperial") {
    float f = tempC * 9.0f / 5.0f + 32.0f;
    return String(f, 1) + "F";
  }

  return String(tempC, 1) + "C";
}

static String formatDisplayTemp(float value) {
  if (isnan(value)) return "--";

  if (unitKey == "imperial") {
    float f = value * 9.0f / 5.0f + 32.0f;
    return String((int)roundf(f)) + "F";
  }

  return String((int)roundf(value)) + "C";
}

/** Tek satir: Y: 21C A: 8C */
static String tempRangeInline() {
  return String("Y:") + formatDisplayTemp(tempMaxC) + " A:" + formatDisplayTemp(tempMinC);
}

/** Sicaklik kutusu: 108x70 kart; origin = kart sol üst (Hava: 8+PAGE_ROW1_Y, Ana: sprite 0,0). */
static const int DISARI_CARD_LEFT   = 8;
static const int DISARI_TX         = 10;
static const int DISARI_INNER_TOP  = 6;
static const int DISARI_FILL_W     = 92;
static const int DISARI_FILL_H     = 62;
static const int DISARI_LAB_Y      = 8;
static const int DISARI_TMP_Y      = 32;
static const int DISARI_RANGE_Y    = 58;
static const int DISARI_ICON_L     = 78;
static const int DISARI_ICON_TOP   = 8;
static const int DISARI_ICON_W     = 26;

static String rainText() {
  if (isnan(precipMm)) return unitKey == "imperial" ? "--.--in" : "--.-mm";

  if (unitKey == "imperial") {
    float inches = precipMm / 25.4f;
    return String(inches, 2) + "in";
  }

  return String(precipMm, 1) + "mm";
}

static String humidityText() {
  if (isnan(humidityPct)) return "--%";
  return String((int)roundf(humidityPct)) + "%";
}

static String windText() {
  if (isnan(windSpeedMs)) return unitKey == "imperial" ? "--.-mph" : "--.-m/s";

  if (unitKey == "imperial") {
    float mph = windSpeedMs * 2.236936f;
    return String(mph, 1) + "mph";
  }

  return String(windSpeedMs, 1) + "m/s";
}

static String windDirectionText() {
  if (isnan(windDirectionDeg)) return "--";

  const char* dirs[] = {"K", "KD", "D", "GD", "G", "GB", "B", "KB"};
  int idx = (int)roundf(windDirectionDeg / 45.0f) % 8;
  return String(dirs[idx]) + " " + String((int)roundf(windDirectionDeg)) + "deg";
}

static String kpText() {
  return isnan(kpIndex) ? "Kp --" : "Kp " + String(kpIndex, 1);
}

static String kpLevelText() {
  if (isnan(kpIndex)) return "--";
  if (kpIndex < 3.0f) return "Dusuk";
  if (kpIndex < 5.0f) return "Orta";
  if (kpIndex < 7.0f) return "Yuksek";
  return "Siddetli";
}

static String uvText() {
  return isnan(uvIndex) ? "UV --" : "UV " + String(uvIndex, 1);
}

static String uvLevelText() {
  if (isnan(uvIndex)) return "--";
  if (uvIndex < 3.0f) return "Dusuk";
  if (uvIndex < 6.0f) return "Orta";
  if (uvIndex < 8.0f) return "Yuksek";
  if (uvIndex < 11.0f) return "Cok yuksek";
  return "Asiri";
}

/** Ana sayfa slotu dar (108px); birim icin rakamin yaninda kucuk "TL" yazisi. */
static String financeUsdMainLine() {
  if (isnan(financeUsdTry)) return "--";
  return "$" + String(financeUsdTry, 2);
}

/** Widget icin: iki ondalik TL ($ rozeti var); punto dahil genislik font seciminde altin ile birlikte hesaplanir. */
static String financeUsdWidgetTryDecimals() {
  if (isnan(financeUsdTry)) return "--";
  return String(financeUsdTry, 2);
}

/** Gram fiyat rakami; gram rozeti sag ustte oldugu icin "g" oneki yok. */
static String financeGoldMainLine() {
  if (isnan(financeGoldTryGram)) return "--";
  return String(financeGoldTryGram, 0);
}

/** Guncelleme zamani: mutlak tarih yerine "X dk once" (ASCII). */
static String financeUpdatedFooter() {
  if (lastFinanceFetch <= 0) return "";
  time_t now = time(nullptr);
  if (now <= 0) return "";
  if (lastFinanceFetch > now) return "Az once";
  long delta = (long)(now - lastFinanceFetch);
  if (delta < 45) return "Az once";
  long mins = delta / 60;
  if (mins < 60) return String((unsigned long)mins) + " dk once";
  long hrs = mins / 60;
  if (hrs < 48) return String((unsigned long)hrs) + " sa once";
  long days = hrs / 24;
  return String((unsigned long)days) + " gun once";
}

/** Rakamin yaninda kucuk "TL" metni (font 1); font hesabi icin yatay pay. */
static int financeTlTextSuffixReserve(TFT_eSprite& spr) {
  const int gap = 4;
  return gap + spr.textWidth("TL", 1);
}

/** Iki degerden genis olanina gore font; doviz/altin donusumunde boyut ziplamaz. */
static int financeHomeSharedValueFont(TFT_eSprite& spr, int maxContentW, const String& usdVal, const String& goldVal) {
  for (int f = 4; f >= 2; f--) {
    int wMax = 0;
    if (usdVal != "--") {
      int w = spr.textWidth(usdVal, f);
      if (w > wMax) wMax = w;
    }
    if (goldVal != "--") {
      int w = spr.textWidth(goldVal, f);
      if (w > wMax) wMax = w;
    }
    if (wMax <= maxContentW) return f;
  }
  return 2;
}

static bool financeHomeGoldPhaseNow() {
  return ((millis() / FINANCE_ROTATE_MS) % 2UL) == 0UL;
}

static void drawFinanceGoldBadge(TFT_eSprite& spr, int cx, int cy) {
  spr.fillCircle(cx, cy, 6, COL_YELLOW);
  spr.drawCircle(cx, cy, 6, COL_ACCENT);
  spr.fillCircle(cx - 2, cy - 2, 2, 0xFFE0);
}

static void drawFinanceUsdBadge(TFT_eSprite& spr, int cx, int cy) {
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(COL_GREEN, COL_PANEL);
  spr.drawString("$", cx, cy, 2);
  spr.setTextDatum(TL_DATUM);
}

static uint16_t statusColor() {
  if (textColorKey != "standard") return COL_TEXT;
  if (!wifiEnabled) return COL_YELLOW;
  return WiFi.status() == WL_CONNECTED ? COL_GREEN : COL_RED;
}

static String uptimeText() {
  unsigned long seconds = millis() / 1000UL;
  unsigned long days = seconds / 86400UL;
  seconds %= 86400UL;
  unsigned long hours = seconds / 3600UL;
  seconds %= 3600UL;
  unsigned long minutes = seconds / 60UL;

  if (days > 0) return String(days) + "g " + String(hours) + "sa";
  if (hours > 0) return String(hours) + "sa " + String(minutes) + "dk";
  return String(minutes) + "dk";
}

static String nextSunLabel() {
  int nowMin = minutesNowLocal();
  if (sunriseMin < 0 || sunsetMin < 0) return "Gunes";
  if (nowMin < sunriseMin) return "Dogus";
  if (nowMin < sunsetMin) return "Batim";
  return "Dogus";
}

static String nextSunTimeText() {
  int nowMin = minutesNowLocal();
  if (sunriseMin < 0 || sunsetMin < 0) return "--:--";
  if (nowMin < sunriseMin) return formatMinuteOfDay(sunriseMin);
  if (nowMin < sunsetMin) return formatMinuteOfDay(sunsetMin);
  return formatMinuteOfDay(sunriseMin);
}

static String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

static String cssColorFrom565(uint16_t color) {
  uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (color & 0x1F) * 255 / 31;
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return String(buf);
}

static String accentPreviewCss(const String& key) {
  if (key == "standard") return cssColorFrom565(0xEF7D);
  if (key == "ice")      return cssColorFrom565(0xEFFF);
  if (key == "white")    return cssColorFrom565(TFT_WHITE);
  if (key == "cyan")     return cssColorFrom565(0x5EFA);
  if (key == "mint")     return cssColorFrom565(0x07F0);
  if (key == "green")    return cssColorFrom565(TFT_GREEN);
  if (key == "blue")     return cssColorFrom565(0x3D9F);
  if (key == "purple")   return cssColorFrom565(0xA2F5);
  if (key == "pink")     return cssColorFrom565(0xF97F);
  if (key == "orange")   return cssColorFrom565(0xFD20);
  if (key == "amber")    return cssColorFrom565(0xFEA0);
  if (key == "red")      return cssColorFrom565(TFT_RED);
  return cssColorFrom565(0xEF7D);
}

static String themePreviewCss(const String& key) {
  if (key == "slate")    return cssColorFrom565(0x08A3);
  if (key == "deep")     return cssColorFrom565(0x0000);
  if (key == "nordic")   return cssColorFrom565(0x0864);
  if (key == "forest")   return cssColorFrom565(0x0208);
  if (key == "coffee")   return cssColorFrom565(0x18A3);
  if (key == "soft")     return cssColorFrom565(0x10A2);
  if (key == "midnight") return cssColorFrom565(0x0008);
  if (key == "graphite") return cssColorFrom565(0x1082);
  if (key == "garnet")   return cssColorFrom565(0x1004);
  if (key == "ochre")    return cssColorFrom565(0x20E1);
  return cssColorFrom565(0x08A3);
}

static String formatTimerClock(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  unsigned long seconds = totalSec % 60UL;

  char buf[10];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", minutes, seconds);
  return String(buf);
}

static String focusHintText() {
  if (focusTimerRunning) return "";
  return "Dokun";
}

static String formatElapsedText(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  if (minutes == 0) return "<1 dk";
  if (minutes == 1) return "1 dk";
  return String(minutes) + " dk";
}

static String lastSyncText() {
  if (lastSyncTime <= 0) return "Senk --:--";

  struct tm tmSync;
  localtime_r(&lastSyncTime, &tmSync);
  return "Senk " + formatClockParts(tmSync, false);
}

static String timerDoneCountdownText() {
  if (!timerDoneDialogOpen) return "";

  unsigned long elapsedMs = millis() - timerDoneDialogStartedMs;
  unsigned long remainingMs = (elapsedMs >= TIMER_DONE_DIALOG_MS) ? 0 : (TIMER_DONE_DIALOG_MS - elapsedMs);
  unsigned long remainingSec = (remainingMs + 999UL) / 1000UL;
  return String(remainingSec) + "sn kapanir";
}

static String homeTitleText() {
  return buddyNickname.length() > 0 ? buddyNickname : "Deskbuddy";
}

int sanitizeTimerMinutes(int value) {
  return constrain(value, 1, 180);
}

void resetFocusTimer() {
  focusTimerRunning = false;
  focusTimerFinished = false;
  focusMenuOpen = false;
  timerDoneDialogOpen = false;
  focusEndMs = 0;
  focusDurationSec = 0;
  focusRemainingSec = 0;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
}

void startFocusTimer(unsigned long minutes) {
  focusDurationSec = minutes * 60UL;
  focusRemainingSec = focusDurationSec;
  focusEndMs = millis() + (focusDurationSec * 1000UL);
  focusTimerRunning = true;
  focusTimerFinished = false;
  focusMenuOpen = false;
  timerDoneDialogOpen = false;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
}

void dismissTimerDoneDialog() {
  if (!timerDoneDialogOpen) return;
  timerDoneDialogOpen = false;
  timerDoneDialogStartedMs = 0;
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
  restoreSleepAwareBacklight();
  pageDirty = true;
}

void openTimerDoneDialog() {
  timerDoneDialogOpen = true;
  timerDoneDialogStartedMs = millis();
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  wakeDisplay(true);
}

void updateFocusTimerState() {
  if (!focusTimerRunning) return;

  unsigned long now = millis();
  if ((long)(focusEndMs - now) <= 0) {
    focusRemainingSec = 0;
    focusTimerRunning = false;
    focusTimerFinished = true;
    focusMenuOpen = false;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
    cacheTimerMenu = "";
    openTimerDoneDialog();
    return;
  }

  unsigned long remainingMs = focusEndMs - now;
  unsigned long nextRemainingSec = (remainingMs + 999UL) / 1000UL;
  if (nextRemainingSec != focusRemainingSec) {
    focusRemainingSec = nextRemainingSec;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
  }
}

void updateTimerDoneDialogState() {
  if (!timerDoneDialogOpen) return;
  if (millis() - timerDoneDialogStartedMs >= TIMER_DONE_DIALOG_MS) {
    dismissTimerDoneDialog();
  }
}

void setBacklight(int value) {
  value = constrain(value, 0, 255);
  analogWrite(BACKLIGHT_PIN, value);
}

/** Mevcut uyku/kisim durumuna gore backlight (tam uyku haric widget mantigi dokunulmaz). */
void restoreSleepAwareBacklight() {
  if (sleepOff) setBacklight(BL_OFF);
  else if (sleepDimmed) setBacklight(BL_DIM);
  else setBacklight(BL_FULL);
}

void wakeDisplay(bool clearManualMode) {
  sleepDimmed = false;
  sleepOff = false;
  if (clearManualMode) manualDimMode = false;
  lastInteractionMs = millis();
  setBacklight(BL_FULL);
  pageDirty = true;
  touchResetGate();
}

void enterSleepDim() {
  if (sleepDimmed || sleepOff || manualDimMode) return;
  sleepDimmed = true;
  setBacklight(BL_DIM);
}

void enterSleepOff() {
  if (sleepOff) return;
  sleepOff = true;
  sleepDimmed = true;
  setBacklight(BL_OFF);
  tft.fillScreen(TFT_BLACK);
  pageDirty = false;
  touchResetGate();
}

/** Elle parlaklik kis: ust bar sol dugme; tekrar basininca tam acilir. */
void toggleManualDimBar() {
  if (sleepOff) return;
  if (manualDimMode && sleepDimmed && !sleepOff) {
    wakeDisplay(true);
    return;
  }

  manualDimMode = true;
  sleepDimmed = true;
  sleepOff = false;
  setBacklight(BL_DIM);
  pageDirty = true;
}

/** Elle tam uyku: siyah ekran + backlight kapali; dokununca wakeDisplay. */
void enterManualSleepFull() {
  if (sleepOff) return;
  manualDimMode = false;
  enterSleepOff();
}


void handleAutoSleep() {
  if (focusMenuOpen || timerDoneDialogOpen || wifiForgetConfirmOpen) return;
  if (sleepIntervalMin <= 0) return;

  unsigned long now = millis();
  unsigned long dimAfterMs = (unsigned long)sleepIntervalMin * 60UL * 1000UL;

  if (!sleepDimmed && !sleepOff && now - lastInteractionMs > dimAfterMs) {
    enterSleepDim();
  }

  // Otomatik modda sadece parlaklik kisilir; tam siyah uyku yalnizca ay dugmesiyle.
  // Eski: idle sonrasi enterSleepOff → dokunmatik bazen tepki vermiyordu / donmus gibi.
}

// =========================================================
// THEME / SETTINGS
// =========================================================
void applyThemeByKey(const String& accentKey, const String& bgKey) {
  if (accentKey == "standard")    COL_ACCENT = 0xEF7D;
  else if (accentKey == "cyan")   COL_ACCENT = 0x5EFA;
  else if (accentKey == "ice")    COL_ACCENT = 0xEFFF;
  else if (accentKey == "white")  COL_ACCENT = TFT_WHITE;
  else if (accentKey == "mint")   COL_ACCENT = 0x07F0;
  else if (accentKey == "green")  COL_ACCENT = TFT_GREEN;
  else if (accentKey == "blue")   COL_ACCENT = 0x3D9F;
  else if (accentKey == "purple") COL_ACCENT = 0xA2F5;
  else if (accentKey == "pink")   COL_ACCENT = 0xF97F;
  else if (accentKey == "orange") COL_ACCENT = 0xFD20;
  else if (accentKey == "amber")  COL_ACCENT = 0xFEA0;
  else if (accentKey == "red")    COL_ACCENT = TFT_RED;
  else                            COL_ACCENT = 0x5EFA;

  if (bgKey == "slate") {
    COL_BG = 0x08A3; COL_PANEL = 0x1106; COL_PANEL_ALT = 0x18C7; COL_STROKE = 0x31EC;
  } else if (bgKey == "deep") {
    COL_BG = 0x0000; COL_PANEL = 0x0841; COL_PANEL_ALT = 0x1082; COL_STROKE = 0x2945;
  } else if (bgKey == "nordic") {
    COL_BG = 0x0864; COL_PANEL = 0x10C6; COL_PANEL_ALT = 0x1908; COL_STROKE = 0x3A2D;
  } else if (bgKey == "forest") {
    COL_BG = 0x0208; COL_PANEL = 0x0ACB; COL_PANEL_ALT = 0x134D; COL_STROKE = 0x2D72;
  } else if (bgKey == "coffee") {
    COL_BG = 0x18A3; COL_PANEL = 0x2945; COL_PANEL_ALT = 0x39C7; COL_STROKE = 0x5A89;
  } else if (bgKey == "soft") {
    COL_BG = 0x10A2; COL_PANEL = 0x1924; COL_PANEL_ALT = 0x2145; COL_STROKE = 0x3A49;
  } else if (bgKey == "midnight") {
    COL_BG = 0x0008; COL_PANEL = 0x0011; COL_PANEL_ALT = 0x0018; COL_STROKE = 0x3A7F;
  } else if (bgKey == "graphite") {
    COL_BG = 0x1082; COL_PANEL = 0x18C3; COL_PANEL_ALT = 0x2104; COL_STROKE = 0x4208;
  } else if (bgKey == "garnet") {
    COL_BG = 0x1004; COL_PANEL = 0x1886; COL_PANEL_ALT = 0x20E8; COL_STROKE = 0x41AC;
  } else if (bgKey == "ochre") {
    COL_BG = 0x20E1; COL_PANEL = 0x3184; COL_PANEL_ALT = 0x4226; COL_STROKE = 0x632B;
  } else {
    COL_BG = 0x08A3; COL_PANEL = 0x1106; COL_PANEL_ALT = 0x18C7; COL_STROKE = 0x31EC;
  }
}

void applyTextColorByKey(const String& key) {
  textColorKey = key;

  if (key == "standard") {
    COL_TEXT = 0xEF7D; COL_DIM  = 0x94B2;
  } else if (key == "white") {
    COL_TEXT = TFT_WHITE; COL_DIM = 0xBDF7;
  } else if (key == "ice") {
    COL_TEXT = 0xEFFF; COL_DIM = 0x9D7F;
  } else if (key == "mint") {
    COL_TEXT = 0x07F0; COL_DIM = 0x05EC;
  } else if (key == "orange") {
    COL_TEXT = 0xFD20; COL_DIM = 0xBA26;
  } else if (key == "amber") {
    COL_TEXT = 0xFEA0; COL_DIM = 0xBCE0;
  } else if (key == "green") {
    COL_TEXT = TFT_GREEN; COL_DIM = 0x86E8;
  } else if (key == "cyan") {
    COL_TEXT = 0x5EFA; COL_DIM = 0x3D96;
  } else if (key == "blue") {
    COL_TEXT = 0x3D9F; COL_DIM = 0x22B1;
  } else if (key == "purple") {
    COL_TEXT = 0xA2F5; COL_DIM = 0x79ED;
  } else if (key == "red") {
    COL_TEXT = TFT_RED; COL_DIM = 0xB9E7;
  } else if (key == "pink") {
    COL_TEXT = 0xF97F; COL_DIM = 0xC2F1;
  } else {
    COL_TEXT = 0xEF7D;
    COL_DIM  = 0x94B2;
    textColorKey = "standard";
  }
}

void loadStoredSettings() {
  prefs.begin("deskbuddy", false);

  String accent = prefs.getString("accent", "cyan");
  String bg     = prefs.getString("bg", "slate");
  String txt    = prefs.getString("text", "standard");

  notesText        = prefs.getString("notes", "Henuz not yok.");
  buddyNickname    = prefs.getString("nickname", "");
  locationName     = prefs.getString("locname", "Berlin");
  LAT              = prefs.getFloat("lat", 52.5200f);
  LNG              = prefs.getFloat("lng", 13.4050f);
  sleepIntervalMin = prefs.getInt("sleepMin", 10);
  unitKey          = prefs.getString("units", "metric");
  regionFormatKey  = prefs.getString("region", "europe");
  flashModeEnabled = prefs.getBool("flashMode", false);
  wifiEnabled      = prefs.getBool("wifiEnabled", true);

  if (prefs.getString("wifiSsid", "").length() == 0 && strlen(DESKBUDDY_WIFI_FALLBACK_SSID) > 0) {
    prefs.putString("wifiSsid", String(DESKBUDDY_WIFI_FALLBACK_SSID));
    prefs.putString("wifiPass", String(DESKBUDDY_WIFI_FALLBACK_PASS));
  }

  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    homeWidgetSlots[i] = homeWidgetFromKey(prefs.getString(key.c_str(), homeWidgetKey(homeWidgetSlots[i])));
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    timerPresetMin[i] = sanitizeTimerMinutes(prefs.getInt(key.c_str(), timerPresetMin[i]));
  }

  if (unitKey != "metric" && unitKey != "imperial") unitKey = "metric";
  if (regionFormatKey != "europe" && regionFormatKey != "us") regionFormatKey = "europe";
  buddyNickname.trim();
  applyThemeByKey(accent, bg);
  applyTextColorByKey(txt);
}

void resetDataCaches() {
  tempC = NAN;
  precipMm = NAN;
  windSpeedMs = NAN;
  windDirectionDeg = NAN;
  humidityPct = NAN;
  weatherCode = -1;
  weatherIsDay = 1;
  kpIndex = NAN;
  sunriseMin = -1;
  sunsetMin = -1;
  lastSunYmd = -1;
  lastWeatherFetch = 0;
  lastKpFetch = 0;
  financeUsdTry = NAN;
  financeGoldTryGram = NAN;
  lastFinanceFetch = 0;
  dataDirty = true;
  pageDirty = true;
}

// =========================================================
// TOUCH
// =========================================================
static bool touchGateWasDown = false;
static unsigned long touchGateLastFireMs = 0;

void touchResetGate() {
  touchGateWasDown = false;
}

bool readTouchXY(int& sx, int& sy) {
  if (!ts.touched()) return false;

  TS_Point p = ts.getPoint();
  if (p.z < 80 || p.z > 4000) return false;

  int x = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W);
  int y = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H);

  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);

  if (TOUCH_SWAP_XY) { int tmp = x; x = y; y = tmp; }
  if (TOUCH_FLIP_X)  x = (SCREEN_W - 1) - x;
  if (TOUCH_FLIP_Y)  y = (SCREEN_H - 1) - y;

  sx = x;
  sy = y;
  return true;
}

bool touchNewPress(int& tx, int& ty) {
  bool down = false;
  int x = 0, y = 0;

  if (readTouchXY(x, y)) down = true;

  bool fire = false;
  unsigned long now = millis();

  if (down && !touchGateWasDown && (now - touchGateLastFireMs > 220)) {
    fire = true;
    touchGateLastFireMs = now;
    tx = x;
    ty = y;
  }

  touchGateWasDown = down;
  return fire;
}

// =========================================================
// API
// =========================================================
bool fetchSunriseSunset() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.sunrise-sunset.org/json?lat=") + String(LAT, 4) +
               "&lng=" + String(LNG, 4) + "&formatted=0";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, body)) return false;

  const char* sunriseStr = doc["results"]["sunrise"];
  const char* sunsetStr  = doc["results"]["sunset"];
  if (!sunriseStr || !sunsetStr) return false;

  auto parseIsoToEpochUTC = [](const char* iso) -> time_t {
    int Y, M, D, h, m, s;
    if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6) return (time_t)-1;

    struct tm t{};
    t.tm_year = Y - 1900;
    t.tm_mon  = M - 1;
    t.tm_mday = D;
    t.tm_hour = h;
    t.tm_min  = m;
    t.tm_sec  = s;

    char* oldTz = getenv("TZ");
    String old = oldTz ? String(oldTz) : String("");

    setenv("TZ", "UTC0", 1);
    tzset();
    time_t epoch = mktime(&t);

    if (old.length()) setenv("TZ", old.c_str(), 1);
    else unsetenv("TZ");
    tzset();

    return epoch;
  };

  time_t srEpoch = parseIsoToEpochUTC(sunriseStr);
  time_t ssEpoch = parseIsoToEpochUTC(sunsetStr);
  if (srEpoch < 0 || ssEpoch < 0) return false;

  sunriseMin = minutesFromLocalEpoch(srEpoch);
  sunsetMin  = minutesFromLocalEpoch(ssEpoch);
  lastSunYmd = ymdFromLocal(time(nullptr));
  lastSyncTime = time(nullptr);
  return true;
}

void ensureSunTimesForToday() {
  time_t nowT = time(nullptr);
  int ymd = ymdFromLocal(nowT);

  if ((sunriseMin < 0 || sunsetMin < 0 || ymd != lastSunYmd) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchSunriseSunset()) dataDirty = true;
  }
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(LAT, 4) +
               "&longitude=" + String(LNG, 4) +
               "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,wind_direction_10m,uv_index,weather_code,is_day" +
               "&hourly=precipitation" +
               "&daily=temperature_2m_max,temperature_2m_min" +
               "&forecast_days=1&timezone=auto&wind_speed_unit=ms";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<4096> doc;
  if (deserializeJson(doc, body)) return false;

  tempC = doc["current"]["temperature_2m"] | NAN;
  humidityPct = doc["current"]["relative_humidity_2m"] | NAN;
  windSpeedMs = doc["current"]["wind_speed_10m"] | NAN;
  windDirectionDeg = doc["current"]["wind_direction_10m"] | NAN;
  uvIndex = doc["current"]["uv_index"] | NAN;
  weatherCode = doc["current"]["weather_code"] | -1;
  weatherIsDay = doc["current"]["is_day"] | 1;
  if (weatherIsDay != 0) weatherIsDay = 1;
  tempMaxC = NAN;
  tempMinC = NAN;

  JsonArray maxTemps = doc["daily"]["temperature_2m_max"];
  JsonArray minTemps = doc["daily"]["temperature_2m_min"];
  if (maxTemps && !maxTemps.isNull() && maxTemps.size() > 0) tempMaxC = maxTemps[0] | NAN;
  if (minTemps && !minTemps.isNull() && minTemps.size() > 0) tempMinC = minTemps[0] | NAN;

  JsonArray times = doc["hourly"]["time"];
  JsonArray precs = doc["hourly"]["precipitation"];

  if (times && precs) {
    time_t nowT = time(nullptr);
    struct tm tmNow;
    localtime_r(&nowT, &tmNow);

    char key[20];
    strftime(key, sizeof(key), "%Y-%m-%dT%H:00", &tmNow);

    int idx = -1;
    for (int i = 0; i < (int)times.size(); i++) {
      const char* t = times[i];
      if (t && String(t).startsWith(key)) {
        idx = i;
        break;
      }
    }
    if (idx < 0) idx = 0;
    precipMm = precs[idx] | NAN;
  }

  lastWeatherFetch = time(nullptr);
  lastSyncTime = lastWeatherFetch;
  return true;
}

void ensureWeather() {
  time_t nowT = time(nullptr);
  if ((isnan(tempC) || isnan(tempMinC) || isnan(tempMaxC) || isnan(precipMm) || isnan(windSpeedMs) ||
       isnan(windDirectionDeg) || isnan(uvIndex) || isnan(humidityPct) || weatherCode < 0 ||
       (nowT - lastWeatherFetch) > WEATHER_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchWeather()) dataDirty = true;
  }
}

bool fetchKpIndex() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json")) {
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  int lastRow = body.lastIndexOf('[');
  if (lastRow < 0) return false;

  int firstComma = body.indexOf(',', lastRow);
  if (firstComma < 0) return false;

  int q1 = body.indexOf('"', firstComma);
  if (q1 < 0) return false;
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0) return false;

  String kpStrLocal = body.substring(q1 + 1, q2);
  kpIndex = kpStrLocal.toFloat();

  lastKpFetch = time(nullptr);
  lastSyncTime = lastKpFetch;
  return true;
}

void ensureKpIndex() {
  time_t nowT = time(nullptr);
  if ((isnan(kpIndex) || (nowT - lastKpFetch) > KP_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchKpIndex()) dataDirty = true;
  }
}

static bool fetchFinanceTruncgil() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(20000);
  const char* ua = "Mozilla/5.0 (compatible; Deskbuddy/1.0)";

  bool got = false;

  if (http.begin(client, "https://finance.truncgil.com/api/currency-rates/USD")) {
    http.addHeader("User-Agent", ua);
    if (http.GET() == 200) {
      String body = http.getString();
      DynamicJsonDocument doc(512);
      if (!deserializeJson(doc, body)) {
        JsonObject usd = doc["USD"];
        if (!usd.isNull()) {
          if (usd.containsKey("Selling")) {
            financeUsdTry = usd["Selling"].as<float>();
            got = true;
          } else if (usd.containsKey("Buying")) {
            financeUsdTry = usd["Buying"].as<float>();
            got = true;
          }
        }
      }
    }
    http.end();
  }

  if (http.begin(client, "https://finance.truncgil.com/api/gold-rates")) {
    http.addHeader("User-Agent", ua);
    if (http.GET() == 200) {
      String body = http.getString();
      DynamicJsonDocument doc(8192);
      if (!deserializeJson(doc, body)) {
        JsonObject rates = doc["Rates"];
        if (!rates.isNull()) {
          JsonObject gra = rates["GRA"];
          if (!gra.isNull()) {
            if (gra.containsKey("Selling")) {
              financeGoldTryGram = gra["Selling"].as<float>();
              got = true;
            } else if (gra.containsKey("Buying")) {
              financeGoldTryGram = gra["Buying"].as<float>();
              got = true;
            }
          }
        }
      }
    }
    http.end();
  }

  if (got) {
    lastFinanceFetch = time(nullptr);
    lastSyncTime = lastFinanceFetch;
  }
  return got;
}

void ensureFinance() {
  if (WiFi.status() != WL_CONNECTED) return;

  time_t nowT = time(nullptr);
  bool stale = (lastFinanceFetch == 0) ||
               ((nowT - lastFinanceFetch) > (time_t)FINANCE_INTERVAL_SEC);
  if (!stale && !isnan(financeUsdTry) && !isnan(financeGoldTryGram)) return;

  if (fetchFinanceTruncgil()) dataDirty = true;
}

// =========================================================
// DRAW HELPERS
// =========================================================
void drawCard(int x, int y, int w, int h, bool accent = false) {
  tft.fillRoundRect(x, y, w, h, 10, COL_PANEL);
  tft.drawRoundRect(x, y, w, h, 10, accent ? COL_ACCENT : COL_STROKE);
}

/** Parlaklik / kisma: genislikleri azalan uc yatay cubuk; bbox dikey olarak (cx,cy) merkezli. */
static void drawTopBarDimBrightnessIcon(TFT_eSPI& g, int cx, int cy, uint16_t fg) {
  const int t = 3;
  g.fillRect(cx - 8, cy - 5, 16, t, fg);
  g.fillRect(cx - 6, cy - 1, 12, t, fg);
  g.fillRect(cx - 4, cy + 3, 8, t, fg);
}

/** Hilal / uyku (maskBg = dugme arka plani ile kesik). */
static void drawTopBarMoonSleepIcon(TFT_eSPI& g, int cx, int cy, uint16_t moonCol, uint16_t maskBg) {
  g.fillCircle(cx - 1, cy, 7, moonCol);
  g.fillCircle(cx + 4, cy - 2, 7, maskBg);
}

/** Kucuk "wifi sil / unut" isareti */
static void drawTopBarWifiForgetIcon(TFT_eSPI& g, int cx, int cy, uint16_t fg) {
  g.drawFastHLine(cx - 7, cy - 4, 5, fg);
  g.drawFastHLine(cx - 5, cy - 1, 9, fg);
  g.drawFastHLine(cx - 7, cy + 2, 14, fg);
  g.drawLine(cx + 3, cy - 6, cx + 7, cy + 6, fg);
}

static void performWifiForgetAndRestart() {
  prefs.remove("wifiSsid");
  prefs.remove("wifiPass");
  prefs.putBool("wifiEnabled", true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(180);
  ESP.restart();
}

static void dismissWifiForgetConfirm() {
  wifiForgetConfirmOpen = false;
  cacheWifiForgetDlg = "";
  pageDirty = true;
}

void openWifiForgetConfirm() {
  focusMenuOpen = false;
  cacheTimerMenu = "";
  wifiForgetConfirmOpen = true;
  pageDirty = true;
}

void drawWifiForgetConfirmOverlay(bool force = false) {
  if (!wifiForgetConfirmOpen) return;

  const String combined = String((int)COL_PANEL_ALT) + "|" + String((int)COL_ACCENT) + "|" + String((int)COL_TEXT);
  if (!force && combined == cacheWifiForgetDlg) return;
  cacheWifiForgetDlg = combined;

  tft.fillScreen(COL_BG);
  tft.fillRoundRect(WIFI_FORGET_DLG_X, WIFI_FORGET_DLG_Y, WIFI_FORGET_DLG_W, WIFI_FORGET_DLG_H, 12, COL_PANEL_ALT);
  tft.drawRoundRect(WIFI_FORGET_DLG_X, WIFI_FORGET_DLG_Y, WIFI_FORGET_DLG_W, WIFI_FORGET_DLG_H, 12, COL_ACCENT);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString("Emin misin?", WIFI_FORGET_DLG_X + WIFI_FORGET_DLG_W / 2, WIFI_FORGET_DLG_Y + 28, 2);
  tft.setTextColor(COL_DIM, COL_PANEL_ALT);
  tft.drawString("Kayitli ag silinir.", WIFI_FORGET_DLG_X + WIFI_FORGET_DLG_W / 2, WIFI_FORGET_DLG_Y + 54, 2);
  tft.drawString("Deskbuddy-Setup ile", WIFI_FORGET_DLG_X + WIFI_FORGET_DLG_W / 2, WIFI_FORGET_DLG_Y + 74, 1);
  tft.drawString("yeniden kurarsin.", WIFI_FORGET_DLG_X + WIFI_FORGET_DLG_W / 2, WIFI_FORGET_DLG_Y + 88, 1);

  const int by = WIFI_FORGET_DLG_Y + WIFI_FORGET_DLG_H - 40;
  const int bw = 78;
  const int bx0 = WIFI_FORGET_DLG_X + 18;
  const int bx1 = WIFI_FORGET_DLG_X + WIFI_FORGET_DLG_W - 18 - bw;

  tft.fillRoundRect(bx0, by, bw, 28, 8, COL_RED);
  tft.drawRoundRect(bx0, by, bw, 28, 8, COL_RED);
  tft.setTextColor(TFT_WHITE, COL_RED);
  tft.drawString("Evet", bx0 + bw / 2, by + 15, 2);

  tft.fillRoundRect(bx1, by, bw, 28, 8, COL_PANEL);
  tft.drawRoundRect(bx1, by, bw, 28, 8, COL_STROKE);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString("Hayir", bx1 + bw / 2, by + 15, 2);
  tft.setTextDatum(TL_DATUM);
}

bool handleWifiForgetConfirmTouch(int x, int y) {
  if (!wifiForgetConfirmOpen) return false;

  const int dx = WIFI_FORGET_DLG_X;
  const int dy = WIFI_FORGET_DLG_Y;
  const int dw = WIFI_FORGET_DLG_W;
  const int dh = WIFI_FORGET_DLG_H;
  const int by = dy + dh - 40;
  const int bw = 78;
  const int bx0 = dx + 18;
  const int bx1 = dx + dw - 18 - bw;

  if (x >= bx0 && x < bx0 + bw && y >= by && y < by + 28) {
    performWifiForgetAndRestart();
    return true;
  }
  if (x >= bx1 && x < bx1 + bw && y >= by && y < by + 28) {
    dismissWifiForgetConfirm();
    return true;
  }

  dismissWifiForgetConfirm();
  return true;
}

void drawTopBar(const String& title) {
  tft.fillRect(0, 0, SCREEN_W, TOPBAR_H, COL_PANEL_ALT);
  tft.drawFastHLine(0, TOPBAR_H - 1, SCREEN_W, COL_STROKE);

  const int topBarMidY = TOPBAR_H / 2;

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  const int tx0 = 10;
  tft.drawString(title, tx0, topBarMidY, 2);
  const int suffixX = tx0 + tft.textWidth(title, 2);

  tft.setTextColor(COL_ACCENT, COL_PANEL_ALT);
  tft.drawString(String(" - ") + FIRMWARE_VERSION, suffixX, topBarMidY, 1);
  tft.setTextDatum(TL_DATUM);

  const int bs = TOPBAR_BTN_SZ;
  const int by = (TOPBAR_H - bs) / 2;
  const int bxWifi = topBarWifiForgetBtnX();
  const int bxDim = topBarDimBtnX();
  const int bxMoon = topBarMoonBtnX();

  uint16_t bgWF = COL_PANEL;
  uint16_t fgWF = COL_TEXT;
  tft.fillRoundRect(bxWifi, by, bs, bs, 7, bgWF);
  tft.drawRoundRect(bxWifi, by, bs, bs, 7, COL_ACCENT);
  drawTopBarWifiForgetIcon(tft, bxWifi + bs / 2, by + bs / 2, fgWF);

  bool dimSel = manualDimMode && sleepDimmed && !sleepOff;
  bool moonSel = sleepOff;

  uint16_t bgDim = dimSel ? COL_ACCENT : COL_PANEL;
  uint16_t fgDim = dimSel ? TFT_BLACK : COL_TEXT;
  tft.fillRoundRect(bxDim, by, bs, bs, 7, bgDim);
  tft.drawRoundRect(bxDim, by, bs, bs, 7, COL_ACCENT);
  drawTopBarDimBrightnessIcon(tft, bxDim + bs / 2, by + bs / 2, fgDim);

  uint16_t bgMoon = moonSel ? COL_ACCENT : COL_PANEL;
  uint16_t fgMoon = moonSel ? TFT_BLACK : COL_TEXT;
  tft.fillRoundRect(bxMoon, by, bs, bs, 7, bgMoon);
  tft.drawRoundRect(bxMoon, by, bs, bs, 7, COL_ACCENT);
  drawTopBarMoonSleepIcon(tft, bxMoon + bs / 2, by + bs / 2, fgMoon, bgMoon);
}

void drawNavBar() {
  const int y = SCREEN_H - NAV_H;
  tft.fillRect(0, y, SCREEN_W, NAV_H, COL_PANEL_ALT);
  tft.drawFastHLine(0, y, SCREEN_W, COL_STROKE);

  const int btnW = SCREEN_W / 4;
  const char* names[4] = {"Ana", "Hava", "Notlar", "Durum"};

  for (int i = 0; i < 4; i++) {
    int bx = i * btnW;
    bool active = ((int)currentPage == i);

    uint16_t bg = active ? COL_ACCENT : COL_PANEL;
    uint16_t fg = active ? TFT_BLACK : COL_TEXT;

    tft.fillRoundRect(bx + 4, y + 6, btnW - 8, NAV_H - 12, 8, bg);
    tft.drawRoundRect(bx + 4, y + 6, btnW - 8, NAV_H - 12, 8, active ? COL_ACCENT : COL_STROKE);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(names[i], bx + btnW / 2, y + NAV_H / 2, 1);
  }

  tft.setTextDatum(TL_DATUM);
}

void makeSpriteCard(TFT_eSprite& spr, int w, int h, bool accent = false) {
  spr.setColorDepth(16);
  spr.createSprite(w, h);
  spr.fillSprite(COL_BG);
  spr.fillRoundRect(0, 0, w, h, 10, COL_PANEL);
  spr.drawRoundRect(0, 0, w, h, 10, accent ? COL_ACCENT : COL_STROKE);
}

void pushSpriteAndDelete(TFT_eSprite& spr, int x, int y) {
  spr.pushSprite(x, y, COL_BG);
  spr.deleteSprite();
}

void drawFinanceHomeWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  bool goldPhase = financeHomeGoldPhaseNow();
  String goldStr = financeGoldMainLine();
  String usdStr = financeUsdWidgetTryDecimals();
  String foot = financeUpdatedFooter();
  const char* title = goldPhase ? "Altin" : "Doviz";
  String value = goldPhase ? goldStr : usdStr;

  String combined = String(goldPhase ? "G|" : "U|") + value + "|" + foot + "|" +
                    String(COL_PANEL) + "|" + String(COL_STROKE) + "|" + String(COL_TEXT) + "|" + String(COL_ACCENT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(title, 10, 2, 2);

  const int badgeCx = w - 14;
  const int badgeCy = 13;
  if (goldPhase) {
    drawFinanceGoldBadge(sprSmall, badgeCx, badgeCy);
  } else {
    drawFinanceUsdBadge(sprSmall, badgeCx, badgeCy);
  }

  const int maxW = w - 20;
  const bool anyVal = (usdStr != "--" || goldStr != "--");
  const int tlReserveFont = anyVal ? financeTlTextSuffixReserve(sprSmall) : 0;
  int vf = financeHomeSharedValueFont(sprSmall, maxW - tlReserveFont, usdStr, goldStr);
  int yVal = (vf >= 4) ? 26 : ((vf == 3) ? 28 : 30);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  sprSmall.drawString(value, 10, yVal, vf);

  if (value != "--") {
    const int tlF = 1;
    int tw = sprSmall.textWidth(value, vf);
    int fh = sprSmall.fontHeight(vf);
    int tlX = 10 + tw + 4;
    int tlY = yVal + fh - sprSmall.fontHeight(tlF);
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString("TL", tlX, tlY, tlF);
  }

  if (foot.length() > 0) {
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(foot, 10, h - 12, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

static void buddyDrawMouthShape(TFT_eSprite& spr, int cx, int faceCy, uint8_t mouthKind, uint8_t grin, uint16_t col) {
  int mouthY = faceCy + 11 + (int)grin;
  if (mouthKind == 1) {
    spr.drawFastHLine(cx - 9, mouthY + 3, 18, col);
  } else if (mouthKind == 2) {
    spr.drawLine(cx - 11, mouthY - 1, cx - 3, mouthY + 6, col);
    spr.drawLine(cx - 3, mouthY + 6, cx + 3, mouthY + 6, col);
    spr.drawLine(cx + 3, mouthY + 6, cx + 11, mouthY - 1, col);
  } else {
    spr.drawLine(cx - 10, mouthY, cx - 3, mouthY + 5, col);
    spr.drawLine(cx - 3, mouthY + 5, cx + 3, mouthY + 5, col);
    spr.drawLine(cx + 3, mouthY + 5, cx + 10, mouthY, col);
  }
}

static void buddyDrawOpenEyes(TFT_eSprite& spr, int eyeLX, int eyeRX, int eyeY, int look, bool winkR,
                             uint16_t scleraStroke, uint16_t pupilCol, uint16_t shineCol) {
  int px = constrain(look, -2, 2);
  spr.fillCircle(eyeLX, eyeY, 6, COL_PANEL_ALT);
  spr.drawCircle(eyeLX, eyeY, 6, scleraStroke);
  spr.fillCircle(eyeLX + px, eyeY + 1, 3, pupilCol);
  spr.fillCircle(eyeLX + px - 1, eyeY, 1, shineCol);
  if (winkR) {
    spr.drawFastHLine(eyeRX - 6, eyeY, 13, pupilCol);
    spr.drawFastHLine(eyeRX - 6, eyeY + 1, 13, pupilCol);
  } else {
    spr.fillCircle(eyeRX, eyeY, 6, COL_PANEL_ALT);
    spr.drawCircle(eyeRX, eyeY, 6, scleraStroke);
    spr.fillCircle(eyeRX + px, eyeY + 1, 3, pupilCol);
    spr.fillCircle(eyeRX + px - 1, eyeY, 1, shineCol);
  }
}

/** Anime tarzi >_< X gozler (kirpmada kullanilmaz). */
static void buddyDrawXEyes(TFT_eSprite& spr, int eyeLX, int eyeRX, int eyeY, uint16_t col) {
  const int r = 5;
  for (int ox = -1; ox <= 0; ox++) {
    spr.drawLine(eyeLX - r + ox, eyeY - r, eyeLX + r + ox, eyeY + r, col);
    spr.drawLine(eyeLX - r + ox, eyeY + r, eyeLX + r + ox, eyeY - r, col);
    spr.drawLine(eyeRX - r + ox, eyeY - r, eyeRX + r + ox, eyeY + r, col);
    spr.drawLine(eyeRX - r + ox, eyeY + r, eyeRX + r + ox, eyeY - r, col);
  }
}

/** Ana sayfa Buddy karti: kirpma; 3 goz ifadesi + X; sway/bob; agiz; wink (isim ust barda). */
void drawBuddyHomeWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  unsigned long nowMs = millis();

  unsigned long c = nowMs % 4600UL;
  unsigned long c2 = nowMs % 5800UL;
  uint8_t blink = 0;
  if (c >= 4360 && c < 4445) blink = 2;
  else if (c >= 4348 && c < 4360) blink = 1;
  else if (c2 >= 5635 && c2 < 5710) blink = 2;
  else if (c2 >= 5625 && c2 < 5635) blink = 1;

  unsigned long cw = nowMs % 9400UL;
  bool winkR = (blink == 0 && cw >= 9100 && cw < 9180);

  int look = (int)((nowMs / 380UL) % 7U) - 3;
  uint8_t hop = (uint8_t)((nowMs / 260UL) % 2U);
  int bob = (int)((nowMs / 440UL) % 3U) - 1;
  int sway = (int)((nowMs / 620UL) % 5U) - 2;

  uint8_t grin = (uint8_t)((nowMs / 5500UL) % 2U);
  uint8_t mouthKind = (uint8_t)((nowMs / 4200UL) % 3U);
  uint8_t eyeExpr = (uint8_t)((nowMs / 3600UL) % 3U);
  bool xEyes = (blink == 0 && eyeExpr == 2);
  bool winkActive = winkR && !xEyes;

  String combined = String(blink) + "|" + String(look) + "|" + String(hop) + "|" + String(bob) + "|" + String(sway) + "|" +
                    String(grin) + "|" + String(mouthKind) + "|" + String(winkActive ? 1 : 0) + "|" + String(eyeExpr) + "|" +
                    String(COL_PANEL) + "|" + String(COL_ACCENT) + "|" + String(COL_STROKE);
  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  const int cx = w / 2 + sway;
  const int faceCy = 27 + (int)hop + bob;

  sprSmall.fillCircle(cx, faceCy, 21, COL_PANEL_ALT);
  sprSmall.drawCircle(cx, faceCy, 21, COL_STROKE);

  const int eyeY = faceCy - 5;
  const int eyeLX = cx - 13;
  const int eyeRX = cx + 13;

  if (blink >= 2) {
    sprSmall.drawFastHLine(eyeLX - 6, eyeY, 13, COL_TEXT);
    sprSmall.drawFastHLine(eyeLX - 6, eyeY + 1, 13, COL_TEXT);
    sprSmall.drawFastHLine(eyeRX - 6, eyeY, 13, COL_TEXT);
    sprSmall.drawFastHLine(eyeRX - 6, eyeY + 1, 13, COL_TEXT);
    buddyDrawMouthShape(sprSmall, cx, faceCy, mouthKind, grin, COL_TEXT);
  } else if (blink == 1) {
    sprSmall.drawFastHLine(eyeLX - 5, eyeY, 11, COL_TEXT);
    sprSmall.drawFastHLine(eyeRX - 5, eyeY, 11, COL_TEXT);
    buddyDrawMouthShape(sprSmall, cx, faceCy, mouthKind, grin, COL_TEXT);
  } else if (xEyes) {
    buddyDrawXEyes(sprSmall, eyeLX, eyeRX, eyeY, COL_TEXT);
    buddyDrawMouthShape(sprSmall, cx, faceCy, mouthKind, grin, COL_TEXT);
  } else {
    buddyDrawOpenEyes(sprSmall, eyeLX, eyeRX, eyeY, look, winkActive, COL_STROKE, COL_TEXT, TFT_WHITE);
    buddyDrawMouthShape(sprSmall, cx, faceCy, mouthKind, grin, COL_TEXT);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawCleanSunIcon(TFT_eSprite& spr, int cx, int cy, uint16_t c) {
  spr.fillCircle(cx, cy, 4, c);
  spr.drawLine(cx, cy - 9, cx, cy - 7, c);
  spr.drawLine(cx, cy + 7, cx, cy + 9, c);
  spr.drawLine(cx - 9, cy, cx - 7, cy, c);
  spr.drawLine(cx + 7, cy, cx + 9, cy, c);
  spr.drawLine(cx - 6, cy - 6, cx - 5, cy - 5, c);
  spr.drawLine(cx + 5, cy - 5, cx + 6, cy - 6, c);
  spr.drawLine(cx - 6, cy + 6, cx - 5, cy + 5, c);
  spr.drawLine(cx + 5, cy + 5, cx + 6, cy + 6, c);
}

void drawMoonIcon(TFT_eSprite& spr, int cx, int cy, uint16_t c) {
  spr.fillCircle(cx, cy, 6, c);
  spr.fillCircle(cx + 4, cy - 2, 6, COL_PANEL);
}

// ---- Open-Meteo WMO weather_code -> compact icon (sprite or full TFT) ----
static WxKind wxKindFromCode(int code, int isDay) {
  if (code < 0) return WX_UNK;
  if (code == 0) return isDay ? WX_CLEAR_DAY : WX_CLEAR_NIGHT;
  if (code == 1) return isDay ? WX_PARTLY : WX_CLOUD;
  if (code == 2) return WX_PARTLY;
  if (code == 3) return WX_CLOUD;
  if (code == 45 || code == 48) return WX_FOG;
  if (code >= 51 && code <= 57) return WX_DRIZZLE;
  if (code >= 61 && code <= 67) return WX_RAIN;
  if (code >= 71 && code <= 77) return WX_SNOW;
  if (code >= 80 && code <= 82) return WX_SHOWER;
  if (code >= 85 && code <= 86) return WX_SNOW;
  if (code >= 95) return WX_THUNDER;
  return WX_CLOUD;
}

void drawWxCloudBlob(TFT_eSPI& g, int cx, int cy, uint16_t c) {
  g.fillCircle(cx - 8, cy + 2, 7, c);
  g.fillCircle(cx + 2, cy, 8, c);
  g.fillCircle(cx + 12, cy + 3, 6, c);
}

/** Ortak palet: accent ana renk, dim ikincil. Merkez (cx, cy), ~28px. */
void drawWxConditionIcon(TFT_eSPI& g, int cx, int cy, WxKind k, uint16_t accent, uint16_t dim, uint16_t panelBg) {
  switch (k) {
    case WX_CLEAR_DAY:
      g.fillCircle(cx, cy, 4, accent);
      g.drawLine(cx, cy - 9, cx, cy - 7, accent);
      g.drawLine(cx, cy + 7, cx, cy + 9, accent);
      g.drawLine(cx - 9, cy, cx - 7, cy, accent);
      g.drawLine(cx + 7, cy, cx + 9, cy, accent);
      g.drawLine(cx - 6, cy - 6, cx - 5, cy - 5, accent);
      g.drawLine(cx + 5, cy - 5, cx + 6, cy - 6, accent);
      g.drawLine(cx - 6, cy + 6, cx - 5, cy + 5, accent);
      g.drawLine(cx + 5, cy + 5, cx + 6, cy + 6, accent);
      break;
    case WX_CLEAR_NIGHT:
      g.fillCircle(cx, cy, 5, accent);
      g.fillCircle(cx + 3, cy - 2, 5, panelBg);
      break;
    case WX_PARTLY: {
      drawWxCloudBlob(g, cx + 4, cy + 2, dim);
      g.fillCircle(cx - 6, cy - 6, 4, accent);
      g.drawLine(cx - 6, cy - 13, cx - 6, cy - 11, accent);
      g.drawLine(cx - 13, cy - 6, cx - 11, cy - 6, accent);
      break;
    }
    case WX_CLOUD:
      drawWxCloudBlob(g, cx, cy, accent);
      break;
    case WX_FOG:
      drawWxCloudBlob(g, cx, cy - 3, dim);
      g.drawFastHLine(cx - 14, cy + 8, 10, accent);
      g.drawFastHLine(cx - 2, cy + 11, 14, accent);
      g.drawFastHLine(cx - 10, cy + 14, 12, accent);
      break;
    case WX_DRIZZLE:
    case WX_RAIN:
    case WX_SHOWER:
      drawWxCloudBlob(g, cx, cy - 4, dim);
      g.drawFastVLine(cx - 6, cy + 6, 8, accent);
      g.drawFastVLine(cx + 2, cy + 5, 10, accent);
      g.drawFastVLine(cx + 8, cy + 7, 7, accent);
      break;
    case WX_SNOW:
      drawWxCloudBlob(g, cx, cy - 4, dim);
      g.drawCircle(cx - 6, cy + 8, 2, accent);
      g.drawCircle(cx + 2, cy + 10, 2, accent);
      g.drawCircle(cx + 8, cy + 7, 2, accent);
      break;
    case WX_THUNDER:
      drawWxCloudBlob(g, cx, cy - 4, dim);
      g.drawLine(cx - 1, cy + 2, cx - 6, cy + 12, accent);
      g.drawLine(cx - 6, cy + 12, cx + 2, cy + 12, accent);
      g.drawLine(cx + 2, cy + 12, cx - 4, cy + 20, accent);
      break;
    default: {
      g.setTextDatum(MC_DATUM);
      g.setTextColor(dim, panelBg);
      g.drawString("?", cx, cy, 2);
      g.setTextDatum(TL_DATUM);
      break;
    }
  }
}

static void drawDisariTileAt(TFT_eSPI& g, int ox, int oy, const char* title,
                             const String& tempStr, WxKind wk) {
  const int tx = ox + DISARI_TX;
  const int ix = ox + DISARI_ICON_L;
  const int iy0 = oy + DISARI_ICON_TOP;

  g.setTextDatum(TL_DATUM);
  g.fillRect(tx, oy + DISARI_INNER_TOP, DISARI_FILL_W, DISARI_FILL_H, COL_PANEL);

  g.setTextColor(COL_DIM, COL_PANEL);
  g.drawString(title, tx, oy + DISARI_LAB_Y, 2);

  g.fillRect(ix, iy0, DISARI_ICON_W, DISARI_ICON_W, COL_PANEL);
  drawWxConditionIcon(g, ix + DISARI_ICON_W / 2, iy0 + DISARI_ICON_W / 2, wk,
                      COL_ACCENT, COL_DIM, COL_PANEL);

  g.setTextColor(COL_TEXT, COL_PANEL);
  g.drawString(tempStr, tx, oy + DISARI_TMP_Y, 4);

  g.setTextColor(COL_ACCENT, COL_PANEL);
  g.drawString(tempRangeInline(), tx, oy + DISARI_RANGE_Y, 1);
}

static String outdoorTileCacheKey() {
  WxKind wk = wxKindFromCode(weatherCode, weatherIsDay);
  return tempText() + "|" + tempRangeInline() + "|" + String((int)wk) + "|" +
         String(weatherCode) + "|" + String(weatherIsDay) + "|" +
         String(COL_ACCENT) + "|" + String(COL_DIM) + "|" + String(COL_PANEL);
}

void drawOutdoorHomeWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String combined = outdoorTileCacheKey();

  if (!force && combined == cache) return;
  cache = combined;

  WxKind wk = wxKindFromCode(weatherCode, weatherIsDay);

  makeSpriteCard(sprSmall, w, h, true);
  drawDisariTileAt(sprSmall, 0, 0, homeWidgetLabel(HOME_WIDGET_OUTDOOR), tempText(), wk);
  pushSpriteAndDelete(sprSmall, x, y);
}

/** Wrapping ile tum satirlari diziye yazar (eski drawWrappedTextLimited ile ayni mantik). */
static int wrapTextToLines(const String& text, int maxW, int font, String* linesOut, int maxLinesOut) {
  String line = "";
  String word = "";
  int lineCount = 0;

  auto flushLine = [&]() {
    if (lineCount >= maxLinesOut) return;
    linesOut[lineCount++] = line;
    line = "";
  };

  auto placeWordOnEmptyLine = [&]() {
    if (word.length() == 0 || lineCount >= maxLinesOut) return;

    while (tft.textWidth(word, font) > maxW && word.length() > 1) {
      int cut = word.length();
      while (cut > 1 && tft.textWidth(word.substring(0, cut), font) > maxW) cut--;
      if (lineCount >= maxLinesOut) return;
      linesOut[lineCount++] = word.substring(0, cut);
      word = word.substring(cut);
    }

    if (lineCount < maxLinesOut) {
      line = word;
      word = "";
    }
  };

  auto flushWord = [&]() {
    if (word.length() == 0 || lineCount >= maxLinesOut) return;

    if (line.length() == 0) {
      placeWordOnEmptyLine();
      return;
    }

    String candidate = line + " " + word;
    if (tft.textWidth(candidate, font) <= maxW) {
      line = candidate;
      word = "";
      return;
    }

    flushLine();
    placeWordOnEmptyLine();
  };

  for (int i = 0; i < (int)text.length(); i++) {
    if (lineCount >= maxLinesOut) break;
    char c = text[i];

    if (c == '\n') {
      flushWord();
      flushLine();
      continue;
    }

    if (c == ' ') {
      flushWord();
      continue;
    }

    word += c;
  }

  if (lineCount < maxLinesOut) {
    flushWord();
    if (line.length() > 0) flushLine();
  }

  return lineCount;
}

static int notesLineHeightPx() {
  return tft.fontHeight(2) + 2;
}

static void rebuildNotesWrappedLines() {
  notesWrappedLineCount =
      wrapTextToLines(notesText, NOTES_TEXT_MAX_W, 2, notesWrappedLines, NOTES_MAX_LINES);
  int lh = notesLineHeightPx();
  notesTotalContentPx = notesWrappedLineCount * lh;
  int maxScr = (notesTotalContentPx > NOTES_VIEW_H) ? (notesTotalContentPx - NOTES_VIEW_H) : 0;
  notesScrollY = constrain(notesScrollY, 0, maxScr);
}

static void paintNotesViewport() {
  tft.fillRect(NOTES_VIEW_X, NOTES_VIEW_Y, NOTES_VIEW_W, NOTES_VIEW_H, COL_PANEL);

  const int lh = notesLineHeightPx();
  const int sprW = NOTES_VIEW_W - NOTES_SCROLLBAR_W;

  if (!sprNotesViewportReady) {
    sprNotes.setColorDepth(16);
    sprNotes.createSprite(sprW, NOTES_VIEW_H);
    sprNotesViewportReady = true;
  }

  sprNotes.fillSprite(COL_PANEL);
  sprNotes.setTextDatum(TL_DATUM);
  sprNotes.setTextColor(COL_TEXT, COL_PANEL);

  const int padX = 2;
  for (int i = 0; i < notesWrappedLineCount; i++) {
    int ly = i * lh - notesScrollY;
    if (ly + lh <= 0 || ly >= NOTES_VIEW_H) continue;
    sprNotes.drawString(notesWrappedLines[i], padX, ly, 2);
  }
  sprNotes.pushSprite(NOTES_VIEW_X, NOTES_VIEW_Y);

  const int sbX = NOTES_VIEW_X + sprW;
  if (notesTotalContentPx > NOTES_VIEW_H) {
    tft.fillRect(sbX, NOTES_VIEW_Y, NOTES_SCROLLBAR_W, NOTES_VIEW_H, COL_PANEL_ALT);
    int thumbH = (int)((long)NOTES_VIEW_H * NOTES_VIEW_H / notesTotalContentPx);
    if (thumbH < 16) thumbH = 16;
    const int innerPad = 6;
    int innerH = NOTES_VIEW_H - innerPad;
    if (thumbH > innerH) thumbH = innerH;
    int maxScr = notesTotalContentPx - NOTES_VIEW_H;
    int travel = innerH - thumbH;
    int thumbY = NOTES_VIEW_Y + innerPad / 2 +
                 (maxScr > 0 && travel > 0 ? (int)((long)notesScrollY * travel / maxScr) : 0);
    tft.fillRoundRect(sbX + 1, thumbY, NOTES_SCROLLBAR_W - 2, thumbH, 2, COL_DIM);
  } else {
    tft.fillRect(sbX, NOTES_VIEW_Y, NOTES_SCROLLBAR_W, NOTES_VIEW_H, COL_PANEL);
  }
}

static void pollNotesScrollTouch() {
  if (currentPage != PAGE_NOTES) {
    notesFingerDown = false;
    return;
  }

  int x = 0, y = 0;
  bool down = readTouchXY(x, y);

  if (down) {
    bool inViewport =
        x >= NOTES_VIEW_X && x < NOTES_VIEW_X + NOTES_VIEW_W && y >= NOTES_VIEW_Y && y < NOTES_VIEW_Y + NOTES_VIEW_H;
    if (!notesFingerDown && inViewport) {
      notesFingerDown = true;
      notesDragLastY = y;
    }
    if (notesFingerDown) {
      int dy = y - notesDragLastY;
      notesDragLastY = y;
      if (dy != 0 && notesTotalContentPx > NOTES_VIEW_H) {
        int prev = notesScrollY;
        notesScrollY -= dy;
        int maxScr = notesTotalContentPx - NOTES_VIEW_H;
        notesScrollY = constrain(notesScrollY, 0, maxScr);
        if (notesScrollY != prev) notesViewportDirty = true;
      }
    }
  } else {
    notesFingerDown = false;
  }
}

// =========================================================
// HOME SPRITES
// =========================================================
void drawClockCardSprite(bool force = false) {
  const int x = 8, y = PAGE_ROW1_Y, w = 224, h = HOME_WIDGET_H;

  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);

  String timeBuf = formatClockParts(tmNow, true);
  String dateBuf = formatDateParts(tmNow);

  String sr = formatMinuteOfDay(sunriseMin);
  String ss = formatMinuteOfDay(sunsetMin);

  String combined = timeBuf + "|" + dateBuf + "|" + sr + "|" + ss + "|" +
                    String(COL_ACCENT) + "|" + String(COL_TEXT);

  if (!force && combined == cacheClock) return;
  cacheClock = combined;

  makeSpriteCard(sprClock, w, h, true);

  sprClock.setTextDatum(TL_DATUM);

  sprClock.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = timeBuf.lastIndexOf(' ');
    String clockMain = splitAt > 0 ? timeBuf.substring(0, splitAt) : timeBuf;
    String clockSuffix = splitAt > 0 ? timeBuf.substring(splitAt + 1) : "";
    sprClock.drawString(clockMain, 10, 11, 4);
    if (clockSuffix.length() > 0) {
      int suffixX = 10 + sprClock.textWidth(clockMain, 4) + 4;
      sprClock.drawString(clockSuffix, suffixX, 18, 2);
    }
  } else {
    sprClock.drawString(timeBuf, 10, 11, 4);
  }

  sprClock.setTextColor(COL_DIM, COL_PANEL);
  sprClock.drawString(dateBuf, 10, 45, 2);

  drawCleanSunIcon(sprClock, 151, 22, COL_ACCENT);
  drawMoonIcon(sprClock, 151, 50, COL_ACCENT);

  sprClock.setTextColor(COL_ACCENT, COL_PANEL);
  sprClock.drawString(sr, 165, 15, 2);
  sprClock.drawString(ss, 165, 43, 2);

  pushSpriteAndDelete(sprClock, x, y);
}

void drawMetricSprite(int x, int y, int w, int h, const char* label, const String& value, String& cache, bool force = false, const String& detail = "") {
  String combined = String(label) + "|" + value + "|" + detail + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  sprSmall.drawString(value, 10, 31, 4);

  if (detail.length() > 0) {
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(detail, 10, 55, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawWeatherStyleMetricSprite(int x, int y, int w, int h, const char* label, const String& value, String& cache, bool force = false, const String& detail = "") {
  String combined = String(label) + "|" + value + "|" + detail + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  sprSmall.drawString(value, 10, 28, 4);

  if (detail.length() > 0) {
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(detail, 10, 52, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawSunEventWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String label = nextSunLabel();
  String value = nextSunTimeText();
  String combined = label + "|" + value + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT) + "|" + String(COL_ACCENT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = value.lastIndexOf(' ');
    String mainValue = splitAt > 0 ? value.substring(0, splitAt) : value;
    String suffix = splitAt > 0 ? value.substring(splitAt + 1) : "";
    sprSmall.drawString(mainValue, 10, 30, 4);
    if (suffix.length() > 0) {
      int suffixX = 10 + sprSmall.textWidth(mainValue, 4) + 3;
      sprSmall.drawString(suffix, suffixX, 35, 2);
    }
  } else {
    sprSmall.drawString(value, 10, 30, 4);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawPlaceholderSprite(int x, int y, int w, int h, const char* label, String& cache, bool force = false) {
  String combined = String(label) + "|" + String(COL_PANEL) + "|" + String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_STROKE, COL_PANEL);
  sprSmall.drawString("Bos", 10, 31, 2);

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawFocusTimerWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String value = formatTimerClock(focusRemainingSec);
  String hint = focusHintText();
  String combined = value + "|" + hint + "|" + String(focusMenuOpen ? 1 : 0) +
                    "|" + String(COL_PANEL) + "|" + String(COL_ACCENT) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString("Sayac", 10, 8, 2);

  if (focusMenuOpen) {
    sprSmall.setTextColor(COL_TEXT, COL_PANEL);
    sprSmall.drawString("Sure", 10, 22, 4);
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString("secin", 10, 44, 2);
  } else {
    sprSmall.setTextColor(focusTimerFinished ? COL_GREEN : COL_TEXT, COL_PANEL);
    sprSmall.drawString(value, 10, 24, 4);
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString(hint, 10, 49, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawHomeSlotWidget(int slot, bool force = false) {
  int x, y, w, h;
  getHomeSlotRect(slot, x, y, w, h);

  switch (homeWidgetSlots[slot]) {
    case HOME_WIDGET_HUMIDITY:
      drawWeatherStyleMetricSprite(x, y, w, h, homeWidgetLabel(HOME_WIDGET_HUMIDITY), humidityText(), cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_TIMER:
      drawFocusTimerWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_RAIN:
      drawWeatherStyleMetricSprite(x, y, w, h, homeWidgetLabel(HOME_WIDGET_RAIN), rainText(), cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_OUTDOOR:
      drawOutdoorHomeWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_KP:
      drawWeatherStyleMetricSprite(x, y, w, h, homeWidgetLabel(HOME_WIDGET_KP), kpText(), cacheHomeSlots[slot], force, kpLevelText());
      break;
    case HOME_WIDGET_UV:
      drawWeatherStyleMetricSprite(x, y, w, h, homeWidgetLabel(HOME_WIDGET_UV), uvText(), cacheHomeSlots[slot], force, uvLevelText());
      break;
    case HOME_WIDGET_WIND:
      drawWeatherStyleMetricSprite(x, y, w, h, homeWidgetLabel(HOME_WIDGET_WIND), windText(), cacheHomeSlots[slot], force, windDirectionText());
      break;
    case HOME_WIDGET_SUN:
      drawSunEventWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_FINANCE:
      drawFinanceHomeWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_BUDDY:
      drawBuddyHomeWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
  }
}

void drawFocusMenuOverlay(bool force = false) {
  String combined = String(focusTimerRunning ? 1 : 0) + "|" + String(focusTimerFinished ? 1 : 0) +
                    "|" + String(COL_PANEL_ALT) + "|" + String(COL_PANEL) + "|" + String(COL_ACCENT);
  if (!force && combined == cacheTimerMenu) return;
  cacheTimerMenu = combined;

  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, COL_BG);
  tft.fillRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_PANEL_ALT);
  tft.drawRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_ACCENT);

  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString("Sayac", TIMER_MENU_X + 14, TIMER_MENU_Y + 12, 2);
  tft.setTextColor(COL_DIM, COL_PANEL_ALT);
  tft.drawString("Sure sec", TIMER_MENU_X + 14, TIMER_MENU_Y + 34, 1);

  const int btnW = 74;
  const int btnH = 28;
  const int col1X = TIMER_MENU_X + 14;
  const int col2X = TIMER_MENU_X + 112;
  const int row1Y = TIMER_MENU_Y + 54;
  const int row2Y = TIMER_MENU_Y + 88;
  const int row3Y = TIMER_MENU_Y + 122;

  String labels[6];
  const int xs[] = {col1X, col2X, col1X, col2X, col1X, col2X};
  const int ys[] = {row1Y, row1Y, row2Y, row2Y, row3Y, row3Y};
  for (int i = 0; i < 6; i++) {
    labels[i] = String(timerPresetMin[i]) + " dk";
  }

  for (int i = 0; i < 6; i++) {
    tft.fillRoundRect(xs[i], ys[i], btnW, btnH, 8, COL_PANEL);
    tft.drawRoundRect(xs[i], ys[i], btnW, btnH, 8, COL_STROKE);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawCentreString(labels[i].c_str(), xs[i] + btnW / 2, ys[i] + 7, 2);
  }

  const int actionY = TIMER_MENU_Y + 160;
  const int actionW = 152;
  const int actionX = TIMER_MENU_X + 24;
  const char* actionLabel = focusTimerRunning ? "Durdur" : (focusTimerFinished ? "Sifirla" : nullptr);
  uint16_t actionColor = focusTimerRunning ? COL_RED : COL_ACCENT;

  if (actionLabel) {
    tft.fillRoundRect(actionX, actionY - 4, actionW, 26, 8, COL_PANEL);
    tft.drawRoundRect(actionX, actionY - 4, actionW, 26, 8, actionColor);
    tft.setTextColor(actionColor, COL_PANEL);
    tft.drawCentreString(actionLabel, actionX + actionW / 2, actionY + 4, 2);
  } else {
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString("Disari dokun", TIMER_MENU_X + TIMER_MENU_W / 2, TIMER_MENU_Y + TIMER_MENU_H - 13, 1);
  }
}

void drawTimerDoneOverlay(bool force = false) {
  if (!timerDoneDialogOpen) return;

  String elapsed = formatElapsedText(focusDurationSec);
  String countdown = timerDoneCountdownText();
  bool flashOn = flashModeEnabled && ((millis() / 300UL) % 2UL == 0);
  if (flashModeEnabled)
    setBacklight(flashOn ? FLASH_BL_HIGH : FLASH_BL_LOW);
  else
    restoreSleepAwareBacklight();
  String combined = elapsed + "|" + String(COL_PANEL_ALT) + "|" + String(COL_ACCENT) + "|" + String(COL_TEXT);
  String flashKey = String(flashOn ? 1 : 0);
  if (force || combined != cacheTimerDone || flashKey != cacheTimerDoneFlash) {
    cacheTimerDone = combined;
    cacheTimerDoneFlash = flashKey;
    cacheTimerDoneCountdown = "";

    uint16_t backdrop = flashOn ? COL_ACCENT : COL_BG;
    uint16_t panelBorder = flashOn ? TFT_WHITE : COL_ACCENT;
    tft.fillRect(0, 0, SCREEN_W, SCREEN_H, backdrop);
    tft.fillRoundRect(TIMER_DONE_X, TIMER_DONE_Y, TIMER_DONE_W, TIMER_DONE_H, 12, COL_PANEL_ALT);
    tft.drawRoundRect(TIMER_DONE_X, TIMER_DONE_Y, TIMER_DONE_W, TIMER_DONE_H, 12, panelBorder);

    tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
    tft.drawCentreString("Sure bitti", TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 14, 2);
    tft.setTextColor(COL_ACCENT, COL_PANEL_ALT);
    tft.drawCentreString(elapsed, TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 42, 2);
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString("Dokun", TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 68, 1);
  }

  if (force || countdown != cacheTimerDoneCountdown) {
    cacheTimerDoneCountdown = countdown;
    tft.fillRect(TIMER_DONE_X + 28, TIMER_DONE_Y + 82, TIMER_DONE_W - 56, 12, COL_PANEL_ALT);
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString(countdown.c_str(), TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 82, 1);
  }
}

// =========================================================
// PAGES
// =========================================================
void drawHomePageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar(homeTitleText());
  drawNavBar();

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  pageDirty = false;
  lastDrawnPage = PAGE_HOME;

  drawClockCardSprite(true);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    drawHomeSlotWidget(i, true);
  }
  if (focusMenuOpen) drawFocusMenuOverlay(true);
  if (timerDoneDialogOpen) drawTimerDoneOverlay(true);
}

void updateHomeDynamic() {
  if (timerDoneDialogOpen) {
    drawTimerDoneOverlay(false);
    return;
  }

  if (focusMenuOpen) {
    drawFocusMenuOverlay(false);
    return;
  }

  drawClockCardSprite(false);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    drawHomeSlotWidget(i, false);
  }
}

void drawWeatherPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Hava");
  drawNavBar();

  drawCard(8, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW2_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW2_Y, 108, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);

  pageDirty = false;
  lastDrawnPage = PAGE_WEATHER;

  lastTempText = "";
  lastRainText = "";
  lastUvText = "";
  lastUvLevelText = "";
  lastKpText = "";
  lastKpLevelText = "";
  lastWindText = "";
  lastWindDirText = "";
  lastNextSunLabel = "";
  lastNextSunTime = "";
}

void updateWeatherDynamic() {
  WxKind wk = wxKindFromCode(weatherCode, weatherIsDay);
  String key = outdoorTileCacheKey();

  if (key != lastTempText || dataDirty) {
    makeSpriteCard(sprSmall, 108, PAGE_WIDGET_H, true);
    drawDisariTileAt(sprSmall, 0, 0, homeWidgetLabel(HOME_WIDGET_OUTDOOR), tempText(), wk);
    pushSpriteAndDelete(sprSmall, DISARI_CARD_LEFT, PAGE_ROW1_Y);
    lastTempText = key;
  }

  String r = rainText();
  if (r != lastRainText) {
    tft.fillRect(134, PAGE_ROW1_Y + 30, 88, 24, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Yagmur", 134, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(r, 134, PAGE_ROW1_Y + 30, 4);
    lastRainText = r;
  }

  String u = uvText();
  String ul = uvLevelText();
  if (u != lastUvText || ul != lastUvLevelText || dataDirty) {
    tft.fillRect(18, PAGE_ROW2_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("UV", 18, PAGE_ROW2_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(u, 18, PAGE_ROW2_Y + 28, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(ul, 18, PAGE_ROW2_Y + 52, 1);
    lastUvText = u;
    lastUvLevelText = ul;
  }

  String w = windText();
  String wd = windDirectionText();
  if (w != lastWindText || wd != lastWindDirText) {
    tft.fillRect(134, PAGE_ROW2_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Ruzgar", 134, PAGE_ROW2_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(w, 134, PAGE_ROW2_Y + 28, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(wd, 134, PAGE_ROW2_Y + 52, 1);
    lastWindText = w;
    lastWindDirText = wd;
  }

  String nl = nextSunLabel();
  String nt = nextSunTimeText();
  if (nl != lastNextSunLabel || nt != lastNextSunTime) {
    tft.fillRect(18, PAGE_ROW3_Y + 24, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(nl, 18, PAGE_ROW3_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    if (useUsRegionFormat()) {
      int splitAt = nt.lastIndexOf(' ');
      String sunMain = splitAt > 0 ? nt.substring(0, splitAt) : nt;
      String sunSuffix = splitAt > 0 ? nt.substring(splitAt + 1) : "";
      tft.drawString(sunMain, 18, PAGE_ROW3_Y + 26, 4);
      if (sunSuffix.length() > 0) {
        int suffixX = 18 + tft.textWidth(sunMain, 4) + 3;
        tft.drawString(sunSuffix, suffixX, PAGE_ROW3_Y + 31, 2);
      }
    } else {
      tft.drawString(nt, 18, PAGE_ROW3_Y + 26, 4);
    }
    lastNextSunLabel = nl;
    lastNextSunTime = nt;
  }

  String k = kpText();
  String kl = kpLevelText();
  if (k != lastKpText || kl != lastKpLevelText || dataDirty) {
    tft.fillRect(134, PAGE_ROW3_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Kp", 134, PAGE_ROW3_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(k, 134, PAGE_ROW3_Y + 28, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(kl, 134, PAGE_ROW3_Y + 52, 1);
    lastKpText = k;
    lastKpLevelText = kl;
  }
}

void drawNotesPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Notlar");
  drawNavBar();

  drawCard(8, 42, 224, 226, true);

  pageDirty = false;
  lastDrawnPage = PAGE_NOTES;
  lastNotesText = "";
  notesFingerDown = false;
  notesViewportDirty = true;
}

void updateNotesDynamic() {
  bool textChanged = (notesText != lastNotesText || notesDirty);
  if (textChanged) {
    rebuildNotesWrappedLines();
    notesScrollY = 0;
    lastNotesText = notesText;
    notesDirty = false;
    notesViewportDirty = true;
  }

  pollNotesScrollTouch();

  if (notesViewportDirty) {
    paintNotesViewport();
    notesViewportDirty = false;
  }
}

void drawStatusPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Durum");
  drawNavBar();

  drawCard(8, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW2_Y, 224, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);

  pageDirty = false;
  lastDrawnPage = PAGE_STATUS;

  lastWifiText = "";
  lastSignalText = "";
  lastIpText = "";
  lastUptimeText = "";
  lastNetworkToggleText = "";
}

void updateStatusDynamic() {
  String w = wifiStatusText();
  if (w != lastWifiText) {
    tft.fillRect(18, PAGE_ROW1_Y + 24, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("WiFi", 18, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(statusColor(), COL_PANEL);
    tft.drawString(w, 18, PAGE_ROW1_Y + 32, 2);
    lastWifiText = w;
  }

  String s = signalText();
  if (s != lastSignalText) {
    tft.fillRect(134, PAGE_ROW1_Y + 30, 88, 24, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Sinyal", 134, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(s, 134, PAGE_ROW1_Y + 30, 4);
    lastSignalText = s;
  }

  String ip = ipText();
  if (ip != lastIpText) {
    tft.fillRect(18, PAGE_ROW2_Y + 30, 200, 18, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("IP adresi", 18, PAGE_ROW2_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(ip, 18, PAGE_ROW2_Y + 30, 2);
    lastIpText = ip;
  }

  String up = uptimeText();
  String upCombined = up + "|" + lastSyncText();
  if (upCombined != lastUptimeText) {
    tft.fillRect(18, PAGE_ROW3_Y + 26, 88, 26, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Calisma", 18, PAGE_ROW3_Y + 10, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(up, 18, PAGE_ROW3_Y + 26, 2);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(lastSyncText(), 18, PAGE_ROW3_Y + 42, 1);
    lastUptimeText = upCombined;
  }

  String networkLabel = wifiEnabled ? "Acik" : "Kapali";
  if (networkLabel != lastNetworkToggleText) {
    uint16_t btnBg = wifiEnabled ? COL_ACCENT : COL_PANEL_ALT;
    uint16_t btnFg = wifiEnabled ? TFT_BLACK : COL_TEXT;
    uint16_t btnStroke = wifiEnabled ? COL_ACCENT : COL_STROKE;

    tft.fillRect(132, PAGE_ROW3_Y + 8, 92, 42, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Ag", 134, PAGE_ROW3_Y + 10, 2);
    tft.fillRoundRect(134, PAGE_ROW3_Y + 30, 88, 22, 8, btnBg);
    tft.drawRoundRect(134, PAGE_ROW3_Y + 30, 88, 22, 8, btnStroke);
    tft.setTextColor(btnFg, btnBg);
    tft.drawCentreString(networkLabel.c_str(), 178, PAGE_ROW3_Y + 32, 2);
    lastNetworkToggleText = networkLabel;
  }
}

void drawCurrentPageFull() {
  switch (currentPage) {
    case PAGE_HOME:    drawHomePageFull(); break;
    case PAGE_WEATHER: drawWeatherPageFull(); break;
    case PAGE_NOTES:   drawNotesPageFull(); break;
    case PAGE_STATUS:  drawStatusPageFull(); break;
  }

  if (focusMenuOpen && currentPage == PAGE_HOME) drawFocusMenuOverlay(true);
  if (timerDoneDialogOpen) drawTimerDoneOverlay(true);
  if (wifiForgetConfirmOpen) drawWifiForgetConfirmOverlay(true);
}

void updateCurrentPageDynamic() {
  if (wifiForgetConfirmOpen) {
    drawWifiForgetConfirmOverlay(false);
    return;
  }

  if (timerDoneDialogOpen) {
    drawTimerDoneOverlay(false);
    return;
  }

  if (focusMenuOpen && currentPage == PAGE_HOME) {
    drawFocusMenuOverlay(false);
    return;
  }

  switch (currentPage) {
    case PAGE_HOME:    updateHomeDynamic(); break;
    case PAGE_WEATHER: updateWeatherDynamic(); break;
    case PAGE_NOTES:   updateNotesDynamic(); break;
    case PAGE_STATUS:  updateStatusDynamic(); break;
  }
}

bool handleFocusMenuTouch(int x, int y) {
  if (!focusMenuOpen) return false;

  if (x < TIMER_MENU_X || x >= TIMER_MENU_X + TIMER_MENU_W || y < TIMER_MENU_Y || y >= TIMER_MENU_Y + TIMER_MENU_H) {
    focusMenuOpen = false;
    cacheTimerMenu = "";
    pageDirty = true;
    return true;
  }

  struct ButtonHit {
    int x;
    int y;
    int w;
    int h;
    int minutes;
  };

  const ButtonHit buttons[] = {
    {34, 124, 74, 28, timerPresetMin[0]},
    {132, 124, 74, 28, timerPresetMin[1]},
    {34, 158, 74, 28, timerPresetMin[2]},
    {132, 158, 74, 28, timerPresetMin[3]},
    {34, 192, 74, 28, timerPresetMin[4]},
    {132, 192, 74, 28, timerPresetMin[5]}
  };

  for (const ButtonHit& btn : buttons) {
    if (x >= btn.x && x < btn.x + btn.w && y >= btn.y && y < btn.y + btn.h) {
      startFocusTimer(btn.minutes);
      pageDirty = true;
      return true;
    }
  }

  if ((focusTimerRunning || focusTimerFinished) && x >= 44 && x < 196 && y >= 224 && y < 250) {
    if (focusTimerRunning || focusTimerFinished) {
      resetFocusTimer();
    } else {
      focusMenuOpen = false;
      cacheTimerMenu = "";
    }
    pageDirty = true;
    return true;
  }

  return true;
}

bool handleHomeTouch(int x, int y) {
  if (currentPage != PAGE_HOME) return false;

  if (focusMenuOpen) return handleFocusMenuTouch(x, y);

  for (int slot = 0; slot < HOME_SLOT_COUNT; slot++) {
    if (homeWidgetSlots[slot] != HOME_WIDGET_TIMER) continue;

    int slotX, slotY, slotW, slotH;
    getHomeSlotRect(slot, slotX, slotY, slotW, slotH);

    if (x >= slotX && x < slotX + slotW && y >= slotY && y < slotY + slotH) {
      if (focusTimerFinished) {
        resetFocusTimer();
      } else {
        focusMenuOpen = true;
        cacheFocusTimer = "";
        clearHomeSlotCaches();
        cacheTimerMenu = "";
      }
      pageDirty = true;
      return true;
    }
  }

  return false;
}

bool handleTimerDoneDialogTouch(int x, int y) {
  (void)x;
  (void)y;
  if (!timerDoneDialogOpen) return false;
  dismissTimerDoneDialog();
  return true;
}

bool handleStatusTouch(int x, int y) {
  if (currentPage != PAGE_STATUS) return false;

  if (x >= 124 && x < 232 && y >= 198 && y < 268) {
    setWifiEnabled(!wifiEnabled);
    return true;
  }

  return false;
}

// =========================================================
// NAVIGATION
// =========================================================
void handleNavTouch(int x, int y) {
  if (y < SCREEN_H - NAV_H) return;

  int btnW = SCREEN_W / 4;
  int idx = x / btnW;
  if (idx < 0 || idx > 3) return;

  Page newPage = (Page)idx;
  if (newPage != currentPage) {
    currentPage = newPage;
    pageDirty = true;
  }
}

// =========================================================
// WIFI PROVISIONING (captive portal, NVS: wifiSsid / wifiPass)
// =========================================================
static void handleProvisionCaptiveRedirect() {
  server.sendHeader("Location", "http://192.168.4.1/");
  server.send(302, "text/plain", "");
}

static void handleProvisionRoot() {
  String h;
  h.reserve(1800);
  h += "<!doctype html><html><head><meta charset='utf-8'>";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<title>Deskbuddy Wi-Fi</title>";
  h += "<style>body{margin:0;background:#0f172a;color:#e2e8f0;font-family:system-ui,sans-serif;padding:24px;}";
  h += "h1{font-size:22px;margin:0 0 8px;}p{color:#94a3b8;font-size:14px;line-height:1.45;margin:0 0 16px;}";
  h += "label{display:block;font-size:13px;margin:12px 0 6px;color:#cbd5e1;font-weight:600;}";
  h += "input{width:100%;max-width:360px;padding:12px;border-radius:10px;border:1px solid #334155;background:#0b1220;color:#f1f5f9;box-sizing:border-box;font:inherit;}";
  h += "button{margin-top:18px;background:#38bdf8;border:none;color:#0c1220;padding:12px 20px;border-radius:10px;font-weight:800;cursor:pointer;font:inherit;}</style></head><body>";
  h += "<h1>Wi-Fi kurulumu</h1>";
  h += "<p class='muted'>Once cihaz ekranindaki QR ile katilin; sonra bu formu kullanabilirsiniz.</p>";
  h += "<p>Ev aginizin adini ve sifresini girin. Kaydettikten sonra cihaz yeniden baslar ve bu aga baglanir.</p>";
  h += "<form method='POST' action='/savewifi'>";
  h += "<label>Ag adi (SSID)</label><input name='ssid' maxlength='32' required autocomplete='off'>";
  h += "<label>Sifre (acik ag ise bos)</label><input name='pass' maxlength='64' type='password' autocomplete='new-password'>";
  h += "<button type='submit'>Kaydet ve yeniden baslat</button></form></body></html>";
  server.send(200, "text/html; charset=utf-8", h);
}

static void handleProvisionSave() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "ssid gerekli");
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  ssid.trim();
  pass.trim();
  if (ssid.length() == 0) {
    server.send(400, "text/plain", "SSID bos olamaz");
    return;
  }
  prefs.putString("wifiSsid", ssid);
  prefs.putString("wifiPass", pass);
  server.send(200, "text/html; charset=utf-8",
               "<!doctype html><meta charset='utf-8'><p style='font-family:sans-serif'>Kaydedildi. Yeniden basliyor...</p>");
  delay(400);
  ESP.restart();
}

/** Kurulum ekraninda WIFI:T:nopass ile ag katilimi QR (ricmoo/qrcode MIT). Alt metin ile dönüş y ekseni (px). */
static int drawProvisionWifiJoinQr(int topY) {
  char payload[128];
  snprintf(payload, sizeof(payload), "WIFI:T:nopass;S:%s;P:;H:false;;", WIFI_AP_SSID);

  static uint8_t qrWorkspace[1024];
  QRCode qr;
  int8_t ok = -1;

  for (uint8_t ver = 4; ver <= 10; ver++) {
    uint16_t need = qrcode_getBufferSize(ver);
    if (need > sizeof(qrWorkspace)) break;
    ok = qrcode_initText(&qr, qrWorkspace, ver, ECC_LOW, payload);
    if (ok == 0) break;
  }

  if (ok != 0) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString("QR olusmadi,", 10, topY + 4, 1);
    tft.drawString("elle ag secin:", 10, topY + 18, 1);
    return topY + 36;
  }

  const uint8_t ms = qr.size;
  int px = 4;
  const int footerReserve = 104;
  while (px >= 2) {
    int qw = (int)ms * px;
    int margin = px * 2;
    if (topY + qw + margin * 2 + footerReserve <= SCREEN_H) break;
    px--;
  }

  const int qw = (int)ms * px;
  const int margin = px * 2;
  const int ox = (SCREEN_W - qw) / 2;
  const int oy = topY;

  tft.fillRect(ox - margin, oy - margin, qw + 2 * margin, qw + 2 * margin, TFT_WHITE);
  for (uint8_t y = 0; y < ms; y++) {
    for (uint8_t x = 0; x < ms; x++) {
      uint16_t c = qrcode_getModule(&qr, x, y) ? TFT_BLACK : TFT_WHITE;
      tft.fillRect(ox + (int)x * px, oy + (int)y * px, px, px, c);
    }
  }

  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_DIM, COL_BG);
  int capBaseline = oy + qw + margin + 2;
  tft.drawString("Kamera ile QR okut", SCREEN_W / 2, capBaseline, 1);
  tft.setTextDatum(TL_DATUM);

  return capBaseline + 12;
}

/** Bos NVS + WiFi acik: AP acar, DNS yonlendirir, / formu. Sonunda ESP.restart. */
void runWifiProvisioningIfNeeded() {
  if (!wifiEnabled) return;
  if (prefs.getString("wifiSsid", "").length() > 0) return;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID);
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  dnsServer.start(53, "*", apIP);

  server.on("/", HTTP_GET, handleProvisionRoot);
  server.on("/savewifi", HTTP_POST, handleProvisionSave);
  server.on("/generate_204", HTTP_GET, handleProvisionCaptiveRedirect);
  server.on("/hotspot-detect.html", HTTP_GET, handleProvisionCaptiveRedirect);
  server.on("/canonical.html", HTTP_GET, handleProvisionCaptiveRedirect);
  server.onNotFound([]() { handleProvisionCaptiveRedirect(); });
  server.begin();

  tft.fillScreen(COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString("Wi-Fi kurulum", 10, 4, 2);
  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.drawString("QR -> ag katilimi", 10, 22, 1);
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString(WIFI_AP_SSID, 10, 34, 1);

  int nextY = drawProvisionWifiJoinQr(82);
  if (nextY + 50 > SCREEN_H) nextY = SCREEN_H - 52;
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString("Tarayici:", 10, nextY, 1);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString("192.168.4.1", 10, nextY + 14, 2);
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString("Kayit -> otomatik reset", 10, nextY + 40, 1);

  for (;;) {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(4);
  }
}

// =========================================================
// WEB SERVER
// =========================================================
void handleRoot() {
  String accent = prefs.getString("accent", "cyan");
  String bg     = prefs.getString("bg", "slate");
  String txt    = prefs.getString("text", "standard");
  String units  = prefs.getString("units", "metric");
  String region = prefs.getString("region", "europe");
  String nickname = prefs.getString("nickname", "");
  bool flashMode = prefs.getBool("flashMode", false);
  String homeSlotKeys[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeSlotKeys[i] = prefs.getString((String("homeSlot") + String(i)).c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }

  String page;
  page.reserve(21000);

  page += "<!doctype html><html><head>";
  page += "<meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>Deskbuddy</title>";
  page += "<style>";
  page += ":root{color-scheme:dark;}";
  page += "body{margin:0;background:linear-gradient(180deg,#0b1018 0%,#111827 100%);color:#edf2f7;font-family:system-ui,sans-serif;}";
  page += ".wrap{max-width:980px;margin:0 auto;padding:28px 16px 36px;}";
  page += ".hero{margin-bottom:18px;padding:18px 20px;border:1px solid #243244;border-radius:20px;background:linear-gradient(135deg,#111927 0%,#172235 100%);box-shadow:0 10px 30px rgba(0,0,0,.22);}";
  page += ".hero h1{font-size:30px;margin:0 0 8px 0;}";
  page += ".hero p{margin:0;color:#a9b7c9;font-size:14px;}";
  page += ".ip{display:inline-block;margin-top:14px;padding:8px 12px;border-radius:999px;background:#0b1220;border:1px solid #334155;color:#dbe7f5;font-size:13px;}";
  page += ".layout{display:grid;grid-template-columns:1.15fr .85fr;gap:16px;align-items:start;}";
  page += ".stack{display:grid;gap:16px;}";
  page += ".panel{background:#171b22;border:1px solid #2d3748;border-radius:18px;padding:18px;margin:0;}";
  page += ".panel-toggle{width:100%;display:flex;align-items:center;justify-content:space-between;gap:12px;background:none;border:none;color:#edf2f7;padding:0;margin:0;cursor:pointer;text-align:left;}";
  page += ".panel-toggle:hover{color:#ffffff;}";
  page += ".panel-toggle h2{flex:1;}";
  page += ".panel-chevron{font-size:18px;color:#8ea3ba;transition:transform .18s ease;}";
  page += ".panel.collapsed .panel-chevron{transform:rotate(-90deg);}";
  page += ".panel-body{margin-top:12px;}";
  page += ".panel.collapsed .panel-body{display:none;}";
  page += ".panel h2{margin:0 0 6px 0;font-size:18px;}";
  page += ".panel p{margin:0 0 14px 0;color:#94a3b8;font-size:13px;line-height:1.45;}";
  page += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;}";
  page += ".grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:14px;}";
  page += ".label{display:block;font-size:13px;margin:0 0 8px 0;color:#a0aec0;font-weight:600;}";
  page += "textarea,input,select{width:100%;border-radius:12px;border:1px solid #334155;background:#0b1220;color:#edf2f7;padding:12px;box-sizing:border-box;font:inherit;}";
  page += "textarea{min-height:170px;resize:vertical;}";
  page += "button{margin-top:18px;background:#38bdf8;border:none;color:#001018;padding:13px 18px;border-radius:12px;font-weight:800;cursor:pointer;font:inherit;}";
  page += ".muted{font-size:13px;color:#94a3b8;line-height:1.45;}";
  page += ".footer-note{margin-top:10px;font-size:12px;color:#7f92a8;}";
  page += ".settings-block{margin-top:18px;padding-top:16px;border-top:1px solid #2b3545;}";
  page += ".settings-block:first-of-type{margin-top:0;padding-top:0;border-top:none;}";
  page += ".settings-title{display:block;margin:0 0 6px 0;font-size:14px;font-weight:700;color:#edf2f7;letter-spacing:.02em;}";
  page += ".settings-desc{margin:0 0 12px 0;font-size:12px;color:#8ea3ba;line-height:1.45;}";
  page += ".color-stack{display:grid;gap:12px;}";
  page += ".color-row{display:grid;grid-template-columns:120px 1fr;gap:12px;align-items:center;}";
  page += ".color-meta{display:flex;align-items:center;justify-content:space-between;gap:10px;}";
  page += ".color-meta .label{margin:0;color:#dbe7f5;}";
  page += ".color-value{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += ".swatch-row{display:flex;flex-wrap:wrap;gap:8px;}";
  page += ".swatch{width:22px;height:22px;border-radius:999px;border:1px solid rgba(255,255,255,.18);cursor:pointer;position:relative;box-sizing:border-box;}";
  page += ".swatch input{display:none;}";
  page += ".swatch.active{box-shadow:0 0 0 2px #67e8f9, 0 0 0 5px rgba(103,232,249,.18);}";
  page += ".swatch.active::after{content:'';position:absolute;inset:5px;border-radius:999px;border:1px solid rgba(0,16,24,.45);}";
  page += ".timer-slot-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-top:14px;}";
  page += ".timer-slot{border:1px solid #334155;border-radius:12px;background:#0b1220;padding:10px 10px 12px 10px;}";
  page += ".timer-slot-head{font-size:12px;color:#8ea3ba;margin-bottom:8px;font-weight:600;}";
  page += ".timer-slot-input{display:flex;align-items:center;gap:8px;}";
  page += ".timer-slot input{padding:10px 12px;text-align:center;font-weight:700;}";
  page += ".timer-unit{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += "@media(max-width:820px){.layout{grid-template-columns:1fr;}.grid,.grid-3,.timer-slot-grid{grid-template-columns:1fr;}.color-row{grid-template-columns:1fr;}}";
  page += "</style></head><body><div class='wrap'>";
  page += "<div class='hero'>";
  page += "<h1>Deskbuddy</h1>";
  page += "<p>Adjust notes, colors, system settings, and location from your browser.</p>";
  page += "<div class='ip'>ESP IP: ";
  page += WiFi.localIP().toString();
  page += "</div>";
  page += "<div class='footer-note' style='margin-top:12px'>Firmware ";
  page += FIRMWARE_VERSION;
  page += "</div></div>";

  page += "<form method='POST' action='/save'>";
  page += "<div class='layout'><div class='stack'>";

  page += "<div class='panel' data-panel='notes'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Notes</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Short notes synced to the device.</p>";
  page += "<label class='label'>Notes</label>";
  page += "<textarea name='notes' maxlength='700'>";
  page += htmlEscape(notesText);
  page += "</textarea>";
  page += "<div class='muted'>Saved notes show up right away.</div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='theme'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Theme and color</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Colors and visual style for the display.</p>";
  page += "<div class='grid'>";

  page += "<div style='grid-column:1 / -1;' class='color-stack'>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Accent</label><span class='color-value' id='accent-value'>";
  page += accent;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(accent=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='accent' value='standard'" + String(accent=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='accent' value='ice'" + String(accent=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='accent' value='white'" + String(accent=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='accent' value='cyan'" + String(accent=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='accent' value='mint'" + String(accent=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='accent' value='green'" + String(accent=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='accent' value='blue'" + String(accent=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='accent' value='purple'" + String(accent=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='accent' value='pink'" + String(accent=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='accent' value='orange'" + String(accent=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='accent' value='amber'" + String(accent=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='accent' value='red'" + String(accent=="red"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Text</label><span class='color-value' id='text-value'>";
  page += txt;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(txt=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='text' value='standard'" + String(txt=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='text' value='ice'" + String(txt=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='text' value='white'" + String(txt=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='text' value='cyan'" + String(txt=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='text' value='mint'" + String(txt=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='text' value='green'" + String(txt=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='text' value='blue'" + String(txt=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='text' value='purple'" + String(txt=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='text' value='pink'" + String(txt=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='text' value='orange'" + String(txt=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='text' value='amber'" + String(txt=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='text' value='red'" + String(txt=="red"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Theme</label><span class='color-value' id='bg-value'>";
  page += bg;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(bg=="slate"?" active":"") + "' style='background:" + themePreviewCss("slate") + ";'><input type='radio' name='bg' value='slate'" + String(bg=="slate"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="deep"?" active":"") + "' style='background:" + themePreviewCss("deep") + ";'><input type='radio' name='bg' value='deep'" + String(bg=="deep"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="nordic"?" active":"") + "' style='background:" + themePreviewCss("nordic") + ";'><input type='radio' name='bg' value='nordic'" + String(bg=="nordic"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="forest"?" active":"") + "' style='background:" + themePreviewCss("forest") + ";'><input type='radio' name='bg' value='forest'" + String(bg=="forest"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="coffee"?" active":"") + "' style='background:" + themePreviewCss("coffee") + ";'><input type='radio' name='bg' value='coffee'" + String(bg=="coffee"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="soft"?" active":"") + "' style='background:" + themePreviewCss("soft") + ";'><input type='radio' name='bg' value='soft'" + String(bg=="soft"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="midnight"?" active":"") + "' style='background:" + themePreviewCss("midnight") + ";'><input type='radio' name='bg' value='midnight'" + String(bg=="midnight"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="graphite"?" active":"") + "' style='background:" + themePreviewCss("graphite") + ";'><input type='radio' name='bg' value='graphite'" + String(bg=="graphite"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="garnet"?" active":"") + "' style='background:" + themePreviewCss("garnet") + ";'><input type='radio' name='bg' value='garnet'" + String(bg=="garnet"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="ochre"?" active":"") + "' style='background:" + themePreviewCss("ochre") + ";'><input type='radio' name='bg' value='ochre'" + String(bg=="ochre"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "</div>";

  page += "</div></div></div>";

  page += "<div class='panel' data-panel='settings'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Settings</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Core behavior and timer setup.</p>";
  page += "<div class='settings-block'>";
  page += "<span class='settings-title'>General</span>";
  page += "<div class='grid'>";
  page += "<div><label class='label'>Buddy nickname</label><input name='nickname' maxlength='24' value='" + htmlEscape(nickname) + "'></div>";
  page += "<div><label class='label'>Auto sleep interval</label><select name='sleepMin'>";
  page += "<option value='0'"  + String(sleepIntervalMin==0?" selected":"")  + ">Never</option>";
  page += "<option value='1'"  + String(sleepIntervalMin==1?" selected":"")  + ">1 minute</option>";
  page += "<option value='5'"  + String(sleepIntervalMin==5?" selected":"")  + ">5 minutes</option>";
  page += "<option value='10'" + String(sleepIntervalMin==10?" selected":"") + ">10 minutes</option>";
  page += "<option value='30'" + String(sleepIntervalMin==30?" selected":"") + ">30 minutes</option>";
  page += "<option value='60'" + String(sleepIntervalMin==60?" selected":"") + ">1 hour</option>";
  page += "</select><div class='muted' style='margin-top:8px;'>Idle dims backlight only. Full black sleep: moon button on device.</div></div>";
  page += "<div><label class='label'>Measurement system</label><select name='units'>";
  page += "<option value='metric'"   + String(units=="metric"?" selected":"")   + ">Celsius / mm</option>";
  page += "<option value='imperial'" + String(units=="imperial"?" selected":"") + ">Fahrenheit / inches</option>";
  page += "</select></div>";
  page += "<div><label class='label'>Date format</label><select name='region'>";
  page += "<option value='europe'" + String(region=="europe"?" selected":"") + ">European: dd.mm.yyyy</option>";
  page += "<option value='us'" + String(region=="us"?" selected":"") + ">US: mm/dd/yyyy</option>";
  page += "</select></div>";
  page += "</div>";
  page += "</div>";
  page += "<div class='settings-block'><span class='settings-title'>Timer</span><div class='settings-desc'>Choose the six quick timers shown in the popup menu.</div><div class='timer-slot-grid'>";
  for (int i = 0; i < 6; i++) {
    page += "<div class='timer-slot'><div class='timer-slot-head'>Slot " + String(i + 1) + "</div><div class='timer-slot-input'><input type='number' min='1' max='180' name='timer" + String(i) + "' value='" + String(timerPresetMin[i]) + "'><span class='timer-unit'>min</span></div></div>";
  }
  page += "</div>";
  page += "<div style='margin-top:14px;'><span class='settings-title'>Alert behavior</span><label style='display:flex;align-items:center;gap:10px;color:#edf2f7;'><input type='checkbox' name='flashMode' value='1'" + String(flashMode ? " checked" : "") + " style='width:auto;'>Flash screen when timer ends</label></div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Location</span><div class='settings-desc'>Used for weather data and sun times.</div><div class='grid-3'>";
  page += "<div><label class='label'>Location name</label><input name='locname' value='" + htmlEscape(locationName) + "'></div>";
  page += "<div><label class='label'>Latitude</label><input name='lat' value='" + String(LAT, 6) + "'></div>";
  page += "<div><label class='label'>Longitude</label><input name='lng' value='" + String(LNG, 6) + "'></div>";
  page += "</div><div class='footer-note'>Example Berlin: latitude 52.5200, longitude 13.4050.</div></div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='widgets'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Widget Customization</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Choose which widgets appear in the four Home slots below the clock card.</p>";
  page += "<div class='grid'>";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    page += "<div><label class='label'>";
    page += homeSlotLabel(i);
    page += "</label><select name='homeSlot";
    page += String(i);
    page += "'>";
    appendHomeWidgetOptions(page, homeSlotKeys[i]);
    page += "</select></div>";
  }
  page += "</div>";
  page += "</div></div>";

  page += "</div><div class='stack'>";

  page += "<button type='submit'>Save to Deskbuddy</button>";
  page += "</div></div></form>";
  page += "<script>";
  page += "var colorNames={accent:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},text:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},bg:{slate:'Slate',deep:'Deep black',nordic:'Nordic blue',forest:'Forest',coffee:'Coffee',soft:'Soft dark',midnight:'Midnight',graphite:'Graphite',garnet:'Garnet',ochre:'Ochre'}};";
  page += "var panelStorageKey='deskbuddy-panel-state-v1';";
  page += "document.querySelectorAll('.swatch input').forEach(function(input){";
  page += "input.addEventListener('change',function(){";
  page += "document.querySelectorAll('.swatch input[name=\"'+input.name+'\"]').forEach(function(peer){";
  page += "peer.closest('.swatch').classList.toggle('active', peer.checked);";
  page += "});";
  page += "var valueEl=document.getElementById(input.name+'-value');";
  page += "if(valueEl&&colorNames[input.name]&&colorNames[input.name][input.value]){valueEl.textContent=colorNames[input.name][input.value];}";
  page += "});";
  page += "});";
  page += "function readPanelState(){try{return JSON.parse(localStorage.getItem(panelStorageKey)||'{}');}catch(e){return {};}}";
  page += "function writePanelState(state){localStorage.setItem(panelStorageKey,JSON.stringify(state));}";
  page += "function applyPanelState(panel,collapsed){panel.classList.toggle('collapsed',collapsed);var btn=panel.querySelector('.panel-toggle');if(btn){btn.setAttribute('aria-expanded',collapsed?'false':'true');}}";
  page += "var savedPanelState=readPanelState();";
  page += "document.querySelectorAll('.panel[data-panel]').forEach(function(panel){";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(Object.prototype.hasOwnProperty.call(savedPanelState,panelId)){applyPanelState(panel,!!savedPanelState[panelId]);}";
  page += "});";
  page += "document.querySelectorAll('.panel-toggle').forEach(function(btn){";
  page += "btn.addEventListener('click',function(){";
  page += "var panel=btn.closest('.panel');";
  page += "var collapsed=!panel.classList.contains('collapsed');";
  page += "applyPanelState(panel,collapsed);";
  page += "var state=readPanelState();";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(panelId){state[panelId]=collapsed;writePanelState(state);}";
  page += "});";
  page += "});";
  page += "</script>";
  page += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", page);
}

void handleSave() {
  String newNotes  = server.hasArg("notes") ? server.arg("notes") : notesText;
  String newAccent = server.hasArg("accent") ? server.arg("accent") : "cyan";
  String newBg     = server.hasArg("bg") ? server.arg("bg") : "slate";
  String newText   = server.hasArg("text") ? server.arg("text") : "standard";
  String newUnits  = server.hasArg("units") ? server.arg("units") : "metric";
  String newRegion = server.hasArg("region") ? server.arg("region") : "europe";
  String newLoc    = server.hasArg("locname") ? server.arg("locname") : locationName;
  String newNickname = server.hasArg("nickname") ? server.arg("nickname") : buddyNickname;
  HomeWidgetType newHomeSlots[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    String currentKey = homeWidgetKey(homeWidgetSlots[i]);
    newHomeSlots[i] = homeWidgetFromKey(server.hasArg(key) ? server.arg(key) : currentKey);
  }

  float newLat = server.hasArg("lat") ? server.arg("lat").toFloat() : LAT;
  float newLng = server.hasArg("lng") ? server.arg("lng").toFloat() : LNG;

  newNotes.trim();
  newLoc.trim();
  newNickname.trim();

  if (newNotes.length() == 0) newNotes = "Henuz not yok.";
  if (newNotes.length() > 700) newNotes = newNotes.substring(0, 700);
  if (newLoc.length() == 0) newLoc = "Unknown";
  if (newNickname.length() > 24) newNickname = newNickname.substring(0, 24);
  if (newUnits != "metric" && newUnits != "imperial") newUnits = "metric";
  if (newRegion != "europe" && newRegion != "us") newRegion = "europe";

  int newSleepMin = server.hasArg("sleepMin") ? server.arg("sleepMin").toInt() : sleepIntervalMin;
  sleepIntervalMin = constrain(newSleepMin, 0, 120);
  bool newFlashMode = server.hasArg("flashMode");

  bool locationChanged =
    (fabsf(newLat - LAT) > 0.0001f) ||
    (fabsf(newLng - LNG) > 0.0001f) ||
    (newLoc != locationName);

  notesText = newNotes;
  buddyNickname = newNickname;
  locationName = newLoc;
  LAT = newLat;
  LNG = newLng;
  unitKey = newUnits;
  regionFormatKey = newRegion;
  flashModeEnabled = newFlashMode;
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeWidgetSlots[i] = newHomeSlots[i];
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    int currentValue = timerPresetMin[i];
    int nextValue = server.hasArg(key) ? server.arg(key).toInt() : currentValue;
    timerPresetMin[i] = sanitizeTimerMinutes(nextValue);
  }

  prefs.putString("notes", notesText);
  prefs.putString("accent", newAccent);
  prefs.putString("bg", newBg);
  prefs.putString("text", newText);
  prefs.putString("units", unitKey);
  prefs.putString("region", regionFormatKey);
  prefs.putString("nickname", buddyNickname);
  prefs.putString("locname", locationName);
  prefs.putFloat("lat", LAT);
  prefs.putFloat("lng", LNG);
  prefs.putInt("sleepMin", sleepIntervalMin);
  prefs.putBool("flashMode", flashModeEnabled);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    prefs.putString(key.c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }
  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    prefs.putInt(key.c_str(), timerPresetMin[i]);
  }

  applyThemeByKey(newAccent, newBg);
  applyTextColorByKey(newText);
  restoreSleepAwareBacklight();

  notesDirty = true;
  pageDirty = true;
  dataDirty = true;

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  cacheTimerMenu = "";
  cacheTimerDone = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  lastTempText = "";
  lastRainText = "";
  lastKpText = "";
  lastKpLevelText = "";
  lastWindText = "";
  lastWindDirText = "";
  lastNextSunLabel = "";
  lastNextSunTime = "";
  lastUptimeText = "";

  if (locationChanged) resetDataCaches();

  server.sendHeader("Location", "/");
  server.send(303);
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

// =========================================================
// SETUP / LOOP
// =========================================================
void waitForNtpTime() {
  time_t now = time(nullptr);
  unsigned long t0 = millis();
  while (now < 1700000000 && millis() - t0 < 10000) {
    delay(200);
    now = time(nullptr);
  }
}

void beginWiFiConnect() {
  if (!wifiEnabled) {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    wifiConnectInProgress = false;
    return;
  }

  String ws = prefs.getString("wifiSsid", "");
  String wp = prefs.getString("wifiPass", "");
  if (ws.length() == 0) {
    wifiConnectInProgress = false;
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ws.c_str(), wp.c_str());
  wifiConnectInProgress = true;
  wifiConnectStartedMs = millis();
}

void connectWiFi(bool waitForConnection = true) {
  beginWiFiConnect();
  if (!waitForConnection) return;

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(200);
  }
  wifiConnectInProgress = false;
}

void updateWiFiConnectionState() {
  if (!wifiEnabled || !wifiConnectInProgress) return;

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    wifiConnectInProgress = false;
    ensureSunTimesForToday();
    ensureWeather();
    ensureKpIndex();
    ensureFinance();
    dataDirty = true;
    pageDirty = true;
    return;
  }

  if (millis() - wifiConnectStartedMs >= WIFI_CONNECT_TIMEOUT_MS) {
    wifiConnectInProgress = false;
    pageDirty = true;
  }
}

void setWifiEnabled(bool enabled) {
  wifiEnabled = enabled;
  prefs.putBool("wifiEnabled", wifiEnabled);

  if (!wifiEnabled) {
    wifiConnectInProgress = false;
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
  } else {
    if (prefs.getString("wifiSsid", "").length() == 0) {
      wifiConnectInProgress = false;
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      delay(120);
      ESP.restart();
      return;
    }
    beginWiFiConnect();
  }

  dataDirty = true;
  pageDirty = true;
}

void setup() {
  Serial.begin(115200);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  analogWrite(BACKLIGHT_PIN, BL_FULL);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  loadStoredSettings();
  setBacklight(BL_FULL);
  lastInteractionMs = millis();

  delay(200);

  tft.init();
  tft.setRotation(ROT);
  tft.invertDisplay(INV);
  tft.setSwapBytes(true);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Aciliyor...", 10, 10, 2);

  touchSPI.begin(T_SCK, T_MISO, T_MOSI);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  ts.begin(touchSPI);
  ts.setRotation(ROT);

  runWifiProvisioningIfNeeded();

  tft.drawString("Wi-Fi baglan...", 10, 34, 2);
  connectWiFi(true);

  tft.drawString("Saat ayari...", 10, 58, 2);
  // Turkey (TRT): permanent UTC+3, no DST — CET/CEST is wrong here (often 1h behind TR in summer).
  configTzTime("<+03>-3", "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  waitForNtpTime();

  ensureSunTimesForToday();
  ensureWeather();
  ensureKpIndex();
  ensureFinance();

  setupWebServer();

  pageDirty = true;
  dataDirty = true;
  notesDirty = true;

  drawCurrentPageFull();
  updateCurrentPageDynamic();

  lastClockTick = millis();
  lastDataTick = millis();

  Serial.print("Deskbuddy web: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
  updateWiFiConnectionState();
  updateFocusTimerState();
  updateTimerDoneDialogState();
  handleAutoSleep();

  int tx = 0, ty = 0;
  if (touchNewPress(tx, ty)) {
    lastInteractionMs = millis();

    if (sleepOff) {
      if (manualDimMode) {
        sleepOff = false;
        sleepDimmed = true;
        setBacklight(BL_DIM);
        pageDirty = true;
        touchResetGate();
      } else {
        wakeDisplay(true);
      }
      return;
    }

    if (handleWifiForgetConfirmTouch(tx, ty)) {
      return;
    }

    if (handleTimerDoneDialogTouch(tx, ty)) {
      return;
    }

    const int bxWifi = topBarWifiForgetBtnX();
    const int bxDim = topBarDimBtnX();
    const int bxMoon = topBarMoonBtnX();
    const int bs = TOPBAR_BTN_SZ;
    if (ty <= TOPBAR_H && ty >= 0) {
      if (tx >= bxMoon && tx < bxMoon + bs)
        enterManualSleepFull();
      else if (tx >= bxDim && tx < bxDim + bs)
        toggleManualDimBar();
      else if (tx >= bxWifi && tx < bxWifi + bs)
        openWifiForgetConfirm();
    } else {
      if (sleepDimmed) {
        if (!manualDimMode) {
          wakeDisplay(true);
        } else {
          if (!handleHomeTouch(tx, ty) && !handleStatusTouch(tx, ty)) {
            handleNavTouch(tx, ty);
          }
        }
      } else {
        if (!handleHomeTouch(tx, ty) && !handleStatusTouch(tx, ty)) {
          handleNavTouch(tx, ty);
        }
      }
    }
  }

  if (millis() - lastDataTick >= DATA_TICK_MS) {
    lastDataTick = millis();
    ensureSunTimesForToday();
    ensureWeather();
    ensureKpIndex();
    ensureFinance();
  }

  if (millis() - lastClockTick >= CLOCK_TICK_MS) {
    lastClockTick = millis();
    updateCurrentPageDynamic();
    dataDirty = false;
  }

  if (pageDirty || lastDrawnPage != currentPage) {
    drawCurrentPageFull();
    updateCurrentPageDynamic();
    pageDirty = false;
    dataDirty = false;
  }

  if (currentPage == PAGE_HOME && !focusMenuOpen && !timerDoneDialogOpen && !wifiForgetConfirmOpen && !sleepOff) {
    static unsigned long lastBuddyAnimMs = 0;
    bool anyBuddy = false;
    for (int s = 0; s < HOME_SLOT_COUNT; s++) {
      if (homeWidgetSlots[s] == HOME_WIDGET_BUDDY) {
        anyBuddy = true;
        break;
      }
    }
    if (anyBuddy && millis() - lastBuddyAnimMs >= 70UL) {
      lastBuddyAnimMs = millis();
      for (int s = 0; s < HOME_SLOT_COUNT; s++) {
        if (homeWidgetSlots[s] == HOME_WIDGET_BUDDY)
          drawHomeSlotWidget(s, false);
      }
    }
  }

  if (currentPage == PAGE_NOTES && !sleepOff) {
    updateNotesDynamic();
  }

  delay(10);
}
