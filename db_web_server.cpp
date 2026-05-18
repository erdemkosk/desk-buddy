/**
 * HTTP ayar paneli: GET / HTML formu, POST /save NVS + tema uygula.
 */

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <math.h>

#include "Deskbuddy_config.h"
#include "Deskbuddy_types.h"
#include "db_web_server.h"

// Ana sketch (.ino) içinde tanımlılar; bağlantı zamanında çözülür.
String htmlEscape(const String &s);
String accentPreviewCss(const String &key);
String themePreviewCss(const String &key);
const char *homeSlotLabel(int slot);
void appendHomeWidgetOptions(String &page, const String &selectedKey);
const char *homeWidgetKey(HomeWidgetType type);
HomeWidgetType homeWidgetFromKey(const String &key);
void applyThemeByKey(const String &accentKey, const String &bgKey);
void applyTextColorByKey(const String &key);
void restoreSleepAwareBacklight();
void resetDataCaches();
int sanitizeTimerMinutes(int value);

extern WebServer server;
extern Preferences prefs;

extern String notesText;
extern String locationName;
extern float LAT;
extern float LNG;
extern String buddyNickname;
extern String unitKey;
extern String regionFormatKey;
extern bool flashModeEnabled;
extern int sleepIntervalMin;
extern int timerPresetMin[6];
extern PageLayout pageLayouts[3];
extern String tabNames[4];
extern HomeWidgetType pageWidgetSlots[3][HOME_SLOT_COUNT];

extern String calendarUrl;
extern time_t lastCalendarFetch;

extern String spotifyUrl;
extern time_t lastSpotifyFetch;

extern String githubUser;
extern time_t lastGithubFetch;

extern String steamApiKey;
extern String steamId;
extern time_t lastSteamFetch;
 
extern String qbUrl;
extern String qbUser;
extern String qbPass;
extern String qbSID;
extern time_t lastQbitFetch;
 
extern String octoUrl;
extern String octoKey;
extern time_t lastOctoFetch;

extern String haUrl;
extern String haToken;
extern String haEntityId;
extern String pageHaLabels[3][HOME_SLOT_COUNT];
extern String pageHaEntities[3][HOME_SLOT_COUNT];




extern int waterGoal;

extern bool notesDirty;
extern bool pageDirty;
extern bool dataDirty;

extern String cacheClock;
extern String cacheHomeEmpty1;
extern String cacheHomeEmpty2;
extern String cacheFocusTimer;
extern String cacheTimerMenu;
extern String cacheTimerDone;
extern String cachePageWidgets[3][HOME_SLOT_COUNT];

extern String lastTempText;
extern String lastRainText;
extern String lastKpText;
extern String lastKpLevelText;
extern String lastWindText;
extern String lastWindDirText;
extern String lastNextSunLabel;
extern String lastNextSunTime;
extern String lastUptimeText;

namespace {

/** UTF-8 giris: Turkce harfleri ASCII esdegerlerine cevirir; diger non-ASCII
 * codepointleri atlar. */
static String asciiFoldTurkishUtf8ToAscii(const String &in) {
  String out;
  if (in.length() == 0)
    return out;
  out.reserve(in.length());
  const char *p = in.c_str();
  while (*p) {
    unsigned char c = (unsigned char)*p;
    if (c < 0x80u) {
      out += static_cast<char>(c);
      ++p;
      continue;
    }

    int skip = 1;
    if ((c >> 5) == 6)
      skip = 2;
    else if ((c >> 4) == 14)
      skip = 3;
    else if ((c >> 3) == 30)
      skip = 4;

    if (skip == 2 && p[1]) {
      unsigned char c2 = (unsigned char)p[1];
      char rep = 0;
      if (c == 0xC3) {
        if (c2 == 0xA7)
          rep = 'c';
        else if (c2 == 0x87)
          rep = 'C';
        else if (c2 == 0xB6)
          rep = 'o';
        else if (c2 == 0x96)
          rep = 'O';
        else if (c2 == 0xBC)
          rep = 'u';
        else if (c2 == 0x9C)
          rep = 'U';
      } else if (c == 0xC4) {
        if (c2 == 0x9F)
          rep = 'g';
        else if (c2 == 0x9E)
          rep = 'G';
        else if (c2 == 0xB1)
          rep = 'i';
        else if (c2 == 0xB0)
          rep = 'I';
      } else if (c == 0xC5) {
        if (c2 == 0x9F)
          rep = 's';
        else if (c2 == 0x9E)
          rep = 'S';
      }
      if (rep != 0) {
        out += rep;
        p += 2;
        continue;
      }
      p += 2;
      continue;
    }
    if (skip == 3 && p[1] && p[2]) {
      p += 3;
      continue;
    }
    if (skip == 4 && p[1] && p[2] && p[3]) {
      p += 4;
      continue;
    }
    ++p;
  }
  return out;
}

static void handleRoot() {
  String page = "";
  page.reserve(48000);

  String accent = prefs.getString("accent", "cyan");
  String bg = prefs.getString("bg", "slate");
  String txt = prefs.getString("text", "standard");
  String units = prefs.getString("units", "metric");
  String region = prefs.getString("region", "europe");
  String nickname = prefs.getString("nickname", "");
  bool flashMode = prefs.getBool("flashMode", false);

  page += R"=====(<!doctype html><html><head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Deskbuddy Ayarları</title>
<style>
:root { color-scheme: dark; --accent: #06b6d4; }
body { margin: 0; background: #0b1018; color: #edf2f7; font-family: system-ui, sans-serif; display: flex; flex-direction: column; align-items: center; padding-bottom: 40px; }
.hero { text-align: center; margin: 20px 0; padding: 20px; background: #171b22; border-radius: 16px; border: 1px solid #2d3748; width: 100%; max-width: 900px; box-sizing: border-box;}
.hero h1 { margin: 0 0 5px 0; font-size: 24px; }
.hero p { margin: 0; color: #94a3b8; font-size: 14px; }
.app-container { display: flex; gap: 24px; align-items: flex-start; max-width: 1000px; width: 100%; padding: 0 16px; box-sizing: border-box; }
.preview-pane { position: sticky; top: 24px; width: 260px; flex-shrink: 0; background: #171b22; padding: 10px; border-radius: 20px; border: 1px solid #2d3748; display: flex; flex-direction: column; align-items: center; box-sizing: border-box;}
.preview-pane h3 { margin: 0 0 10px 0; font-size: 14px; color: #a0aec0; text-transform: uppercase; letter-spacing: 1px; }
.sim-screen { width: 240px; height: 320px; background: #0b1220; border-radius: 8px; overflow: hidden; display: flex; flex-direction: column; position: relative; border: 2px solid #000; }
.sim-top { height: 34px; background: #1f2937; border-bottom: 1px solid rgba(255,255,255,0.1); display: flex; justify-content: space-between; padding: 0 10px; align-items: center; font-size: 11px; font-weight: bold; }
.sim-center { flex: 1; padding: 6px; display: grid; gap: 6px; background: #0b1220; }
.sim-center.layout-0 { grid-template-columns: 1fr 1fr; grid-template-rows: 60px 70px 70px; }
.sim-center.layout-3 { grid-template-columns: 1fr 1fr; grid-template-rows: 70px 70px 70px; }
.sim-center.layout-1, .sim-center.layout-2 { display: flex; flex-direction: column; align-items: center; justify-content: center; }
.sim-clock { grid-column: 1 / -1; display: flex; justify-content: center; align-items: center; font-size: 32px; font-weight: bold; color: #fff; }
.sim-widget { background: #1f2937; border-radius: 8px; border: 1px solid rgba(255,255,255,0.05); display: flex; flex-direction: column; align-items: center; justify-content: center; font-size: 12px; font-weight: 500; color: #d1d5db; transition: all 0.2s; position: relative; overflow: hidden;}
.sim-widget::before { content: ''; position: absolute; top:0; left:0; width: 100%; height: 100%; border: 1px solid var(--accent); border-radius: 8px; opacity: 0.5; pointer-events: none;}
.sim-nav { height: 44px; background: #1f2937; border-top: 1px solid rgba(255,255,255,0.1); display: flex; justify-content: space-around; align-items: center; padding: 0 6px; }
.sim-nav-item { width: 44px; height: 30px; background: #111827; border-radius: 6px; display: flex; justify-content: center; align-items: center; font-size: 11px; font-weight: bold; border: 1px solid rgba(255,255,255,0.1); }
.sim-nav-item.active { background: var(--accent); color: #000; border-color: var(--accent); }
.tabs-sidebar { width: 220px; flex-shrink: 0; display: flex; flex-direction: column; gap: 8px; }
.tab-btn { padding: 14px; background: #171b22; border: 1px solid #2d3748; color: #a0aec0; border-radius: 12px; cursor: pointer; text-align: left; font-weight: 600; font-size: 14px; transition: all 0.2s; }
.tab-btn:hover { background: #1f2937; color: #fff; }
.tab-btn.active { background: var(--accent); color: #000; border-color: var(--accent); }
.tab-content-area { flex: 1; min-width: 0; }
.panel { display: none; background: #171b22; border: 1px solid #2d3748; border-radius: 18px; padding: 24px; margin: 0;}
.panel.active { display: block; animation: fadeIn 0.2s ease-in-out; }
@keyframes fadeIn { from { opacity: 0; transform: translateY(5px); } to { opacity: 1; transform: translateY(0); } }
.panel h2 { margin: 0 0 10px 0; font-size: 20px; color: #fff;}
.panel p { margin: 0 0 16px 0; color: #94a3b8; font-size: 14px; line-height: 1.5; }
label.label { display: block; font-size: 13px; margin: 0 0 8px 0; color: #a0aec0; font-weight: 600; }
textarea,input[type=text],input[type=password],input[type=number],select { width: 100%; border-radius: 8px; border: 1px solid #334155; background: #0b1220; color: #edf2f7; padding: 12px; box-sizing: border-box; font-family: inherit; margin-bottom: 16px; }
textarea { min-height: 120px; resize: vertical; }
.grid { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; }
.swatch-row { display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 16px; }
.swatch { width: 28px; height: 28px; border-radius: 50%; border: 2px solid transparent; cursor: pointer; position: relative; }
.swatch input { display: none; }
.swatch.active { border-color: #fff; box-shadow: 0 0 0 2px var(--accent); }
.submit-btn { margin-top: 10px; background: var(--accent); color: #000; padding: 16px 20px; border-radius: 12px; font-weight: bold; border: none; cursor: pointer; width: 100%; font-size: 16px; transition: opacity 0.2s; }
.submit-btn:hover { opacity: 0.9; }
input[type=text].ha-input { font-size: 11px; padding: 6px; margin-bottom: 0px; width: 100%; box-sizing: border-box; }
input[type=text].ha-input.lbl { margin-bottom: 3px; }
.muted { font-size: 12px; color: #64748b; margin-top: -10px; margin-bottom: 16px; display: block; line-height: 1.4;}
hr { border: 0; border-top: 1px solid #2d3748; margin: 20px 0; }
@media(max-width: 800px) {
  .app-container { flex-direction: column; align-items: center; }
  .preview-pane { position: relative; top: 0; width: 100%; max-width: 300px; }
  .tabs-sidebar { flex-direction: row; overflow-x: auto; width: 100%; padding-bottom: 10px; }
  .tab-btn { white-space: nowrap; }
  .tab-content-area { width: 100%; }
}
</style>
</head><body>
<div class="hero">
  <h1>Deskbuddy Ayar Paneli</h1>
  <p>IP: )=====";
  page += WiFi.localIP().toString();
  page += R"=====( | Firmware: )=====";
  page += FIRMWARE_VERSION;
  page += R"=====(</p>
</div>
<form method="POST" action="/save" style="width: 100%; display: flex; justify-content: center;">
<div class="app-container">
  
  <div class="preview-pane">
    <h3>Canlı Önizleme</h3>
    <div class="sim-screen" id="sim-screen">
      <div class="sim-top" id="sim-top">
         <span>WIFI</span><span style="color: var(--accent)">)=====";
  page += FIRMWARE_VERSION;
  page += R"=====(</span>
      </div>
      <div class="sim-center layout-0" id="sim-center">
         <div class="sim-clock" id="sim-clock">14:30</div>
         <div class="sim-widget" id="sim-w0">Slot 1</div>
         <div class="sim-widget" id="sim-w1">Slot 2</div>
         <div class="sim-widget" id="sim-w2">Slot 3</div>
         <div class="sim-widget" id="sim-w3">Slot 4</div>
         <div class="sim-widget" id="sim-w4" style="display:none">Slot 5</div>
         <div class="sim-widget" id="sim-w5" style="display:none">Slot 6</div>
      </div>
      <div class="sim-nav">
         <div class="sim-nav-item active" id="sim-nav-0">T1</div>
         <div class="sim-nav-item" id="sim-nav-1">T2</div>
         <div class="sim-nav-item" id="sim-nav-2">T3</div>
         <div class="sim-nav-item">Ayar</div>
      </div>
    </div>
  </div>

  <div class="tabs-sidebar">
    <button type="button" class="tab-btn active" data-tab="tab-layout">Ekran Düzeni</button>
    <button type="button" class="tab-btn" data-tab="tab-theme">Tema & Renk</button>
    <button type="button" class="tab-btn" data-tab="tab-notes">Notlar</button>
    <button type="button" class="tab-btn" data-tab="tab-settings">Genel Ayarlar</button>
    <button type="button" class="tab-btn" data-tab="tab-api">Bağlantılar (API)</button>
    <button type="submit" class="submit-btn">Cihaza Kaydet</button>
  </div>

  <div class="tab-content-area">
    <!-- Ekran Düzeni Tab -->
    <div class="panel active" id="tab-layout">
      <h2>Ekran Düzeni (Sekmeler)</h2>
      <p>Cihazın alt menüsündeki sekmeleri ve içerdikleri widget'ları tasarla.</p>
)=====";

  for (int p = 0; p < 3; p++) {
    page += "<h3>Sekme " + String(p + 1) + " (" + htmlEscape(tabNames[p]) + ")</h3>";
    page += "<div class='grid'>";
    page += "<div><label class='label'>Sekme İsmi (Max 6 harf)</label>";
    page += "<input type='text' name='t_name" + String(p) + "' id='t_name" + String(p) + "' value='" + htmlEscape(tabNames[p]) + "' maxlength='6'></div>";
    page += "<div><label class='label'>Sayfa Tipi (Layout)</label>";
    page += "<select name='t_lay" + String(p) + "' id='t_lay" + String(p) + "'>";
    page += "<option value='0'" + String(pageLayouts[p] == LAYOUT_GRID ? " selected" : "") + ">Izgara (Saat + 4 Widget)</option>";
    page += "<option value='3'" + String(pageLayouts[p] == LAYOUT_GRID_6 ? " selected" : "") + ">Izgara (Saat Yok + 6 Widget)</option>";
    page += "<option value='1'" + String(pageLayouts[p] == LAYOUT_FULL_WEATHER ? " selected" : "") + ">Tam Ekran: Hava Durumu</option>";
    page += "<option value='2'" + String(pageLayouts[p] == LAYOUT_FULL_NOTES ? " selected" : "") + ">Tam Ekran: Notlar</option>";
    page += "</select></div></div>";

    page += "<div id='grid_p" + String(p) + "' class='grid' style='display:" + String((pageLayouts[p] == LAYOUT_GRID || pageLayouts[p] == LAYOUT_GRID_6) ? "grid" : "none") + "; padding-top: 5px;'>";
    for (int i = 0; i < HOME_SLOT_COUNT; i++) {
      page += "<div id='p" + String(p) + "slot" + String(i) + "_c' style='display:" + String((pageLayouts[p] == LAYOUT_GRID && i >= 4) ? "none" : "block") + "'>";
      page += "<label class='label'>" + String(homeSlotLabel(i)) + "</label>";
      page += "<select name='t" + String(p) + "slot" + String(i) + "' id='p" + String(p) + "slot" + String(i) + "' data-selected='" + homeWidgetKey(pageWidgetSlots[p][i]) + "' onchange='toggleHaFields(" + String(p) + "," + String(i) + ")'></select>";
      
      String isHaSelected = (String(homeWidgetKey(pageWidgetSlots[p][i])) == "ha") ? "block" : "none";
      page += "<div id='p" + String(p) + "slot" + String(i) + "_ha' style='display:" + isHaSelected + "; margin-top:5px;'>";
      page += "<input type='text' class='ha-input lbl' name='t" + String(p) + "slot" + String(i) + "_lbl' placeholder='Buton Etiketi (Örn: Lamba)' value=\"" + htmlEscape(pageHaLabels[p][i]) + "\">";
      page += "<input type='text' class='ha-input' name='t" + String(p) + "slot" + String(i) + "_ent' placeholder='Entity ID(ler)' value=\"" + htmlEscape(pageHaEntities[p][i]) + "\">";
      page += "</div>";
      page += "</div>";
    }
    page += "</div><hr>";
  }

  page += R"=====(
    </div>

    <!-- Tema Tab -->
    <div class="panel" id="tab-theme">
      <h2>Tema ve Renkler</h2>
      <p>Canlı önizlemeden değişimleri anında görebilirsin.</p>
      
      <label class="label">Vurgu Rengi (Accent)</label>
      <div class="swatch-row" id="accent-swatches">
)=====";

  String accents[] = {"standard","ice","white","cyan","mint","green","blue","purple","pink","orange","amber","red"};
  for(const String& a : accents) {
    page += "<label class='swatch" + String(accent == a ? " active" : "") + "' style='background:" + accentPreviewCss(a) + ";' data-color='" + accentPreviewCss(a) + "'><input type='radio' name='accent' value='" + a + "'" + String(accent == a ? " checked" : "") + "></label>";
  }
  
  page += R"=====(
      </div>

      <label class="label">Arka Plan Teması (Background)</label>
      <div class="swatch-row" id="bg-swatches">
)=====";

  String bgs[] = {"slate","deep","nordic","forest","coffee","soft","midnight","graphite","garnet","ochre"};
  for(const String& b : bgs) {
    page += "<label class='swatch" + String(bg == b ? " active" : "") + "' style='background:" + themePreviewCss(b) + ";' data-color='" + themePreviewCss(b) + "'><input type='radio' name='bg' value='" + b + "'" + String(bg == b ? " checked" : "") + "></label>";
  }

  page += R"=====(
      </div>

      <label class="label">Yazı Rengi (Text)</label>
      <div class="swatch-row">
)=====";
  for(const String& t : accents) {
    page += "<label class='swatch" + String(txt == t ? " active" : "") + "' style='background:" + accentPreviewCss(t) + ";'><input type='radio' name='text' value='" + t + "'" + String(txt == t ? " checked" : "") + "></label>";
  }

  page += R"=====(
      </div>
    </div>

    <!-- Notlar Tab -->
    <div class="panel" id="tab-notes">
      <h2>Notlar</h2>
      <p>Cihaz ekranındaki notlar sekmesinde gösterilecek metin.</p>
      <textarea name="notes" maxlength="700">)=====";
  page += htmlEscape(notesText);
  page += R"=====(</textarea>
      <span class="muted">Not: Türkçe harfler (ş, ğ, ç, ö, ü, ı) cihazda desteklenen formatlara otomatik çevrilir.</span>
    </div>

    <!-- Genel Ayarlar Tab -->
    <div class="panel" id="tab-settings">
      <h2>Genel Ayarlar</h2>
      <div class="grid">
        <div>
          <label class="label">Cihaz İsmi (Nickname)</label>
          <input type="text" name="nickname" maxlength="24" value=")=====";
  page += htmlEscape(nickname);
  page += R"=====(">
        </div>
        <div>
          <label class="label">Otomatik Uyku (Ekran Kararma)</label>
          <select name="sleepMin">
            <option value="0")=====" + String(sleepIntervalMin==0?" selected":"") + R"=====(>Hiçbir Zaman</option>
            <option value="1")=====" + String(sleepIntervalMin==1?" selected":"") + R"=====(>1 Dakika</option>
            <option value="5")=====" + String(sleepIntervalMin==5?" selected":"") + R"=====(>5 Dakika</option>
            <option value="10")=====" + String(sleepIntervalMin==10?" selected":"") + R"=====(>10 Dakika</option>
            <option value="30")=====" + String(sleepIntervalMin==30?" selected":"") + R"=====(>30 Dakika</option>
            <option value="60")=====" + String(sleepIntervalMin==60?" selected":"") + R"=====(>1 Saat</option>
          </select>
        </div>
        <div>
          <label class="label">Birim Sistemi</label>
          <select name="units">
            <option value="metric")=====" + String(units=="metric"?" selected":"") + R"=====(>Metrik (°C / mm)</option>
            <option value="imperial")=====" + String(units=="imperial"?" selected":"") + R"=====(>Emperyal (°F / inç)</option>
          </select>
        </div>
        <div>
          <label class="label">Tarih Formatı</label>
          <select name="region">
            <option value="europe")=====" + String(region=="europe"?" selected":"") + R"=====(>Avrupa (GG.AA.YYYY)</option>
            <option value="us")=====" + String(region=="us"?" selected":"") + R"=====(>ABD (AA/GG/YYYY)</option>
          </select>
        </div>
      </div>
      
      <hr>
      <label class="label">Konum (Hava Durumu ve Güneş için)</label>
      <div class="grid">
        <div style="grid-column: 1 / -1;">
          <input type="text" name="locname" value=")=====" + htmlEscape(locationName) + R"=====(" placeholder="Şehir İsmi">
        </div>
        <div>
          <label class="label">Enlem (Latitude)</label>
          <input type="text" name="lat" value=")=====" + String(LAT,6) + R"=====(">
        </div>
        <div>
          <label class="label">Boylam (Longitude)</label>
          <input type="text" name="lng" value=")=====" + String(LNG,6) + R"=====(">
        </div>
      </div>

      <hr>
      <label class="label">Hızlı Sayaç Süreleri (Dakika)</label>
      <div class="grid" style="grid-template-columns: 1fr 1fr 1fr;">
)=====";
  for(int i=0; i<6; i++){
     page += "<div><input type='number' name='timer"+String(i)+"' value='"+String(timerPresetMin[i])+"'></div>";
  }
  page += R"=====(
      </div>
      <label style="display:flex; align-items:center; gap:10px; margin-top:10px; cursor:pointer;">
         <input type="checkbox" name="flashMode" value="1")=====" + String(flashMode?" checked":"") + R"=====( style="width:auto; margin:0;">
         Sayaç bitince ekran yanıp sönsün
      </label>
      
      <hr>
      <label class="label">Su / Alışkanlık Hedefi</label>
      <input type='number' name='waterGoal' value=')=====" + String(waterGoal) + R"=====(' min='1' max='50'>
    </div>

    <!-- API Tab -->
    <div class="panel" id="tab-api">
      <h2>API ve Entegrasyonlar</h2>
      <p style="margin-bottom: 20px; font-size: 14px; color: #9ca3af;">
        Tüm bu bağlantıların (Spotify, Steam, Takvim vb.) nasıl yapılacağını adım adım öğrenmek için 
        <a href="https://github.com/erdemkosk/desk-buddy/blob/main/ENTEGRASYON_REHBERI.md" target="_blank" style="color: #60a5fa; text-decoration: underline;">Deskbuddy Entegrasyon Rehberi</a>'ne tıklayın.
      </p>
      
      <label class="label">Google Takvim URL (Apps Script)</label>
      <input type="text" name="calUrl" value=")=====" + htmlEscape(calendarUrl) + R"=====(">
      <label class="label" style="margin-top:10px">Spotify Proxy URL (Apps Script)</label>
      <input type="text" name="spotifyUrl" value=")=====" + htmlEscape(spotifyUrl) + R"=====(">
      
      <label class="label" style="margin-top:10px">GitHub Kullanıcı Adı</label>
      <input type="text" name="githubUser" value=")=====" + htmlEscape(githubUser) + R"=====(">

      <div class="grid" style="margin-top:10px">
        <div><label class="label">Steam API Key</label><input type="password" name="steamKey" value=")=====" + htmlEscape(steamApiKey) + R"=====("></div>
        <div><label class="label">Steam ID (64-bit)</label><input type="text" name="steamId" value=")=====" + htmlEscape(steamId) + R"=====("></div>
      </div>

      <div class="grid" style="margin-top:10px">
        <div style="grid-column: 1/-1;"><label class="label">qBittorrent URL (IP:Port)</label><input type="text" name="qbUrl" value=")=====" + htmlEscape(qbUrl) + R"=====("></div>
        <div><label class="label">qBit Kullanıcı</label><input type="text" name="qbUser" value=")=====" + htmlEscape(qbUser) + R"=====("></div>
        <div><label class="label">qBit Şifre</label><input type="password" name="qbPass" value=")=====" + htmlEscape(qbPass) + R"=====("></div>
      </div>

      <div class="grid" style="margin-top:10px">
        <div><label class="label">OctoPrint URL</label><input type="text" name="octoUrl" value=")=====" + htmlEscape(octoUrl) + R"=====("></div>
        <div><label class="label">OctoPrint API Key</label><input type="password" name="octoKey" value=")=====" + htmlEscape(octoKey) + R"=====("></div>
      </div>

      <h3 style="color:#a0aec0; margin-top:20px; font-size:14px;">Home Assistant Bağlantı Ayarları</h3>
      <div class="grid">
        <div style="grid-column: 1/-1;"><label class="label">HA URL (Örn: http://192.168.1.50:8123)</label><input type="text" name="haUrl" value=")=====" + htmlEscape(haUrl) + R"=====("></div>
        <div style="grid-column: 1/-1;"><label class="label">HA Token (Long-Lived Access Token)</label><input type="password" name="haToken" value=")=====" + htmlEscape(haToken) + R"=====("></div>
        <div style="grid-column: 1/-1;"><label class="label">Varsayılan Entity ID (OctoPrint Uzun Basma - Örn: switch.evde_3d)</label><input type="text" name="haEntityId" value=")=====" + htmlEscape(haEntityId) + R"=====("></div>
      </div>

    </div>

  </div>
</div>
</form>

<script>
  const WIDGET_OPTIONS = [
    { key: "humidity", label: "Nem" },
    { key: "timer", label: "Sayac" },
    { key: "rain", label: "Yagmur" },
    { key: "outdoor", label: "Sicaklik" },
    { key: "kp", label: "Kp" },
    { key: "uv", label: "UV" },
    { key: "wind", label: "Ruzgar" },
    { key: "sun", label: "Gunes" },
    { key: "finance", label: "Doviz" },
    { key: "buddy", label: "Buddy" },
    { key: "notes", label: "Notlar" },
    { key: "calendar", label: "Takvim" },
    { key: "spotify", label: "Spotify" },
    { key: "github", label: "GitHub" },
    { key: "water", label: "Su" },
    { key: "steam", label: "Steam" },
    { key: "qbit", label: "qBittorrent" },
    { key: "octo", label: "OctoPrint" },
    { key: "ha", label: "HA Butonu" }
  ];

  // Populate all selects
  document.querySelectorAll('select[id^="p"]').forEach(sel => {
    const selectedKey = sel.getAttribute('data-selected');
    WIDGET_OPTIONS.forEach(opt => {
      const option = document.createElement('option');
      option.value = opt.key;
      option.text = opt.label;
      if (opt.key === selectedKey) {
        option.selected = true;
        option.setAttribute('selected', 'selected');
      }
      sel.appendChild(option);
    });
    sel.value = selectedKey;
  });

  function toggleHaFields(p, i) {
    var select = document.getElementById('p' + p + 'slot' + i);
    var div = document.getElementById('p' + p + 'slot' + i + '_ha');
    if (select && div) {
      if (select.value === 'ha') {
        div.style.display = 'block';
      } else {
        div.style.display = 'none';
      }
    }
  }

  // Tab Switching Logic
  const tabs = document.querySelectorAll('.tab-btn');
  const panels = document.querySelectorAll('.panel');
  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      tabs.forEach(t => t.classList.remove('active'));
      panels.forEach(p => p.classList.remove('active'));
      tab.classList.add('active');
      const targetPanel = document.getElementById(tab.dataset.tab);
      if(targetPanel) targetPanel.classList.add('active');
    });
  });

  // Theme Sync
  const root = document.documentElement;
  const simCenter = document.getElementById('sim-center');
  const simScreen = document.getElementById('sim-screen');
  const simTop = document.getElementById('sim-top');
  
  function updateColors() {
     const activeAccent = document.querySelector('#accent-swatches .swatch.active');
     if(activeAccent) root.style.setProperty('--accent', activeAccent.dataset.color);
     
     const activeBg = document.querySelector('#bg-swatches .swatch.active');
     if(activeBg && simCenter && simTop) {
        simCenter.style.background = activeBg.dataset.color;
        simTop.style.background = activeBg.dataset.color;
     }
  }

  document.querySelectorAll('.swatch input').forEach(input => {
    input.addEventListener('change', (e) => {
      const row = e.target.closest('.swatch-row');
      row.querySelectorAll('.swatch').forEach(s => s.classList.remove('active'));
      e.target.closest('.swatch').classList.add('active');
      updateColors();
    });
  });

  // Layout Sync
  let currentTabIdx = 0;
  function updateLayout() {
     const layoutSelect = document.getElementById('t_lay' + currentTabIdx);
     if(!layoutSelect) return;
     const val = layoutSelect.value;
     
     if(simCenter) simCenter.className = 'sim-center layout-' + val;
     
     const isGrid = (val === '0' || val === '3');
     // Hide all grids, show current
     for(let p=0; p<3; p++) {
         const gridArea = document.getElementById('grid_p' + p);
         if(gridArea) gridArea.style.display = 'none';
     }
     const currentGridArea = document.getElementById('grid_p' + currentTabIdx);
     if(currentGridArea) currentGridArea.style.display = isGrid ? 'grid' : 'none';
     
     if (isGrid) {
       if(!document.getElementById('sim-clock') && simCenter) {
          simCenter.innerHTML = '<div class="sim-clock" id="sim-clock">14:30</div><div class="sim-widget" id="sim-w0">Slot 1</div><div class="sim-widget" id="sim-w1">Slot 2</div><div class="sim-widget" id="sim-w2">Slot 3</div><div class="sim-widget" id="sim-w3">Slot 4</div><div class="sim-widget" id="sim-w4" style="display:none">Slot 5</div><div class="sim-widget" id="sim-w5" style="display:none">Slot 6</div>';
       }
       
       const slot4 = document.getElementById('p' + currentTabIdx + 'slot4_c');
       const slot5 = document.getElementById('p' + currentTabIdx + 'slot5_c');
       if(slot4) slot4.style.display = (val === '3') ? 'block' : 'none';
       if(slot5) slot5.style.display = (val === '3') ? 'block' : 'none';
       
       const clock = document.getElementById('sim-clock');
       if(clock) clock.style.display = (val === '0') ? 'flex' : 'none';
       for(let i=0; i<6; i++) {
          const w = document.getElementById('sim-w'+i);
          const sel = document.getElementById('p' + currentTabIdx + 'slot' + i);
          if (w) {
             if (val === '0' && i >= 4) { w.style.display = 'none'; }
             else {
                w.style.display = 'flex';
                let optText = "Slot " + (i + 1);
                if(sel) {
                   const opt = sel.querySelector('option[selected]') || sel.options[0];
                   if(sel.selectedIndex >= 0 && sel.options[sel.selectedIndex]) {
                      optText = sel.options[sel.selectedIndex].text;
                   } else if(opt) {
                      optText = opt.text;
                   }
                }
                w.innerText = optText;
             }
          }
       }
     } else if (val === '1' && simCenter) {
       simCenter.innerHTML = '<div class="sim-clock" style="font-size:20px; color:#a0aec0; text-align:center;">Hava Durumu<br>Tam Ekran</div>';
     } else if (val === '2' && simCenter) {
       simCenter.innerHTML = '<div class="sim-clock" style="font-size:20px; color:#a0aec0; text-align:center;">Notlar<br>Tam Ekran</div>';
     }

     // Sync names
     for(let p=0; p<3; p++){
        const nameInput = document.getElementById('t_name'+p);
        const navItem = document.getElementById('sim-nav-'+p);
        if(nameInput && navItem) {
           navItem.innerText = nameInput.value.substring(0,3).toUpperCase();
        }
     }
  }

  function switchToSimTab(p) {
      if (currentTabIdx === p) return;
      currentTabIdx = p;
      for(let k=0; k<3; k++) {
          const item = document.getElementById('sim-nav-'+k);
          if (item) {
              if (k === p) item.classList.add('active');
              else item.classList.remove('active');
          }
      }
  }

  // Bind change events to all layout and slot selects
  for(let p=0; p<3; p++){
     const laySelect = document.getElementById('t_lay'+p);
     const nameInput = document.getElementById('t_name'+p);
     
     if(laySelect) {
        laySelect.addEventListener('change', () => {
            switchToSimTab(p);
            updateLayout();
        });
        laySelect.addEventListener('focus', () => {
            switchToSimTab(p);
            updateLayout();
        });
     }
     if(nameInput) {
        nameInput.addEventListener('input', () => {
            switchToSimTab(p);
            updateLayout();
        });
        nameInput.addEventListener('focus', () => {
            switchToSimTab(p);
            updateLayout();
        });
     }
     for(let i=0; i<6; i++) {
        const sel = document.getElementById('p'+p+'slot'+i);
        if(sel) {
           sel.addEventListener('change', () => {
               switchToSimTab(p);
               updateLayout();
           });
           sel.addEventListener('focus', () => {
               switchToSimTab(p);
               updateLayout();
           });
           sel.addEventListener('change', () => toggleHaFields(p, i));
        }
        
        const lblInput = document.getElementsByName('t'+p+'slot'+i+'_lbl')[0];
        const entInput = document.getElementsByName('t'+p+'slot'+i+'_ent')[0];
        if(lblInput) {
            lblInput.addEventListener('focus', () => {
                switchToSimTab(p);
                updateLayout();
            });
            lblInput.addEventListener('input', () => {
                switchToSimTab(p);
                updateLayout();
            });
        }
        if(entInput) {
            entInput.addEventListener('focus', () => {
                switchToSimTab(p);
                updateLayout();
            });
            entInput.addEventListener('input', () => {
                switchToSimTab(p);
                updateLayout();
            });
        }
     }
  }
  
  // Tab sync for preview (simulating bottom nav click)
  for(let p=0; p<3; p++){
      const navItem = document.getElementById('sim-nav-'+p);
      if(navItem) {
          navItem.style.cursor = 'pointer';
          navItem.addEventListener('click', () => {
              switchToSimTab(p);
              updateLayout();
          });
      }
  }

  updateColors();
  updateLayout();
  window.addEventListener('DOMContentLoaded', () => {
     updateColors();
     updateLayout();
  });
  window.addEventListener('load', () => {
     updateColors();
     updateLayout();
  });
  setTimeout(updateLayout, 100);
  setTimeout(updateLayout, 500);

</script>
</body></html>)=====";

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(200, "text/html; charset=utf-8", page);
}

static void handleSave() {
  String newNotes = server.hasArg("notes") ? server.arg("notes") : notesText;
  String newAccent = server.hasArg("accent") ? server.arg("accent") : "cyan";
  String newBg = server.hasArg("bg") ? server.arg("bg") : "slate";
  String newText = server.hasArg("text") ? server.arg("text") : "standard";
  String newUnits = server.hasArg("units") ? server.arg("units") : "metric";
  String newRegion = server.hasArg("region") ? server.arg("region") : "europe";
  String newLoc =
      server.hasArg("locname") ? server.arg("locname") : locationName;
  String newNickname =
      server.hasArg("nickname") ? server.arg("nickname") : buddyNickname;
  String newCalUrl =
      server.hasArg("calUrl") ? server.arg("calUrl") : calendarUrl;
  String newQbUrl = server.hasArg("qbUrl") ? server.arg("qbUrl") : qbUrl;
  String newQbUser = server.hasArg("qbUser") ? server.arg("qbUser") : qbUser;
  String newQbPass = server.hasArg("qbPass") ? server.arg("qbPass") : qbPass;
  String newOctoUrl = server.hasArg("octoUrl") ? server.arg("octoUrl") : octoUrl;
  String newOctoKey = server.hasArg("octoKey") ? server.arg("octoKey") : octoKey;
  
  String newHaUrl = server.hasArg("haUrl") ? server.arg("haUrl") : haUrl;
  String newHaToken = server.hasArg("haToken") ? server.arg("haToken") : haToken;
  String newHaEntityId = server.hasArg("haEntityId") ? server.arg("haEntityId") : haEntityId;

  newCalUrl.trim();
  String newSpotifyUrl =
      server.hasArg("spotifyUrl") ? server.arg("spotifyUrl") : spotifyUrl;
  newSpotifyUrl.trim();
  String newGithubUser =
      server.hasArg("githubUser") ? server.arg("githubUser") : githubUser;
  newGithubUser.trim();
  int newWaterGoal = server.hasArg("waterGoal") ? server.arg("waterGoal").toInt() : waterGoal;
  if (newWaterGoal <= 0) newWaterGoal = 8;
  String newSteamKey =
      server.hasArg("steamKey") ? server.arg("steamKey") : steamApiKey;
  newSteamKey.trim();
  String newSteamId =
      server.hasArg("steamId") ? server.arg("steamId") : steamId;
  newSteamId.trim();
  
  String newTabNames[3];
  PageLayout newLayouts[3];
  HomeWidgetType newWidgetSlots[3][HOME_SLOT_COUNT];

  for (int p = 0; p < 3; p++) {
    String nameKey = "t_name" + String(p);
    newTabNames[p] = server.hasArg(nameKey) ? server.arg(nameKey) : tabNames[p];
    newTabNames[p].trim();
    if (newTabNames[p].length() == 0) newTabNames[p] = "Sekme" + String(p+1);
    if (newTabNames[p].length() > 6) newTabNames[p] = newTabNames[p].substring(0, 6);

    String layoutKey = "t_lay" + String(p);
    newLayouts[p] = server.hasArg(layoutKey) ? (PageLayout)server.arg(layoutKey).toInt() : pageLayouts[p];

    for (int i = 0; i < HOME_SLOT_COUNT; i++) {
      String slotKey = "t" + String(p) + "slot" + String(i);
      String currentKey = homeWidgetKey(pageWidgetSlots[p][i]);
      newWidgetSlots[p][i] = homeWidgetFromKey(server.hasArg(slotKey) ? server.arg(slotKey) : currentKey);
      
      String lblKey = slotKey + "_lbl";
      String entKey = slotKey + "_ent";
      if (server.hasArg(lblKey)) pageHaLabels[p][i] = server.arg(lblKey);
      if (server.hasArg(entKey)) pageHaEntities[p][i] = server.arg(entKey);
    }
  }

  float newLat = server.hasArg("lat") ? server.arg("lat").toFloat() : LAT;
  float newLng = server.hasArg("lng") ? server.arg("lng").toFloat() : LNG;

  newNotes = asciiFoldTurkishUtf8ToAscii(newNotes);
  newLoc = asciiFoldTurkishUtf8ToAscii(newLoc);
  newNickname = asciiFoldTurkishUtf8ToAscii(newNickname);

  newNotes.trim();
  newLoc.trim();
  newNickname.trim();

  if (newNotes.length() == 0)
    newNotes = "Henuz not yok.";
  if (newNotes.length() > 700)
    newNotes = newNotes.substring(0, 700);
  if (newLoc.length() == 0)
    newLoc = "Unknown";
  if (newNickname.length() > 24)
    newNickname = newNickname.substring(0, 24);
  if (newUnits != "metric" && newUnits != "imperial")
    newUnits = "metric";
  if (newRegion != "europe" && newRegion != "us")
    newRegion = "europe";

  int newSleepMin = server.hasArg("sleepMin") ? server.arg("sleepMin").toInt()
                                              : sleepIntervalMin;
  sleepIntervalMin = constrain(newSleepMin, 0, 120);
  bool newFlashMode = server.hasArg("flashMode");

  bool locationChanged = (fabsf(newLat - LAT) > 0.0001f) ||
                         (fabsf(newLng - LNG) > 0.0001f) ||
                         (newLoc != locationName);

  notesText = newNotes;
  buddyNickname = newNickname;
  locationName = newLoc;
  LAT = newLat;
  LNG = newLng;
  calendarUrl = newCalUrl;
  spotifyUrl = newSpotifyUrl;
  githubUser = newGithubUser;
  waterGoal = newWaterGoal;
  steamApiKey = newSteamKey;
  steamId = newSteamId;
  unitKey = newUnits;
  regionFormatKey = newRegion;
  flashModeEnabled = newFlashMode;
  qbUrl = newQbUrl;
  qbUser = newQbUser;
  qbPass = newQbPass;
  octoUrl = newOctoUrl;
  octoKey = newOctoKey;
  haUrl = newHaUrl;
  haToken = newHaToken;
  haEntityId = newHaEntityId;

  for (int p = 0; p < 3; p++) {
    tabNames[p] = newTabNames[p];
    pageLayouts[p] = newLayouts[p];
    for (int i = 0; i < HOME_SLOT_COUNT; i++) {
      pageWidgetSlots[p][i] = newWidgetSlots[p][i];
    }
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    int currentValue = timerPresetMin[i];
    int nextValue = server.hasArg(key) ? server.arg(key).toInt() : currentValue;
    timerPresetMin[i] = sanitizeTimerMinutes(nextValue);
  }

  auto putStringIfChanged = [](const char* key, const String& newVal) {
    if (prefs.getString(key, "") != newVal) {
      prefs.putString(key, newVal);
    }
  };
  auto putIntIfChanged = [](const char* key, int newVal) {
    if (prefs.getInt(key, -9999) != newVal) {
      prefs.putInt(key, newVal);
    }
  };
  auto putFloatIfChanged = [](const char* key, float newVal) {
    if (fabsf(prefs.getFloat(key, -9999.0f) - newVal) > 0.0001f) {
      prefs.putFloat(key, newVal);
    }
  };
  auto putBoolIfChanged = [](const char* key, bool newVal) {
    if (prefs.getBool(key, false) != newVal) {
      prefs.putBool(key, newVal);
    }
  };

  putStringIfChanged("notes", notesText);
  putStringIfChanged("accent", newAccent);
  putStringIfChanged("bg", newBg);
  putStringIfChanged("text", newText);
  putStringIfChanged("units", unitKey);
  putStringIfChanged("region", regionFormatKey);
  putStringIfChanged("nickname", buddyNickname);
  putStringIfChanged("locname", locationName);
  putFloatIfChanged("lat", LAT);
  putFloatIfChanged("lng", LNG);
  putIntIfChanged("sleepMin", sleepIntervalMin);
  putBoolIfChanged("flashMode", flashModeEnabled);
  putStringIfChanged("calUrl", calendarUrl);
  putStringIfChanged("spotifyUrl", spotifyUrl);
  putStringIfChanged("githubUser", githubUser);
  putIntIfChanged("w_goal", waterGoal);
  putStringIfChanged("steamKey", steamApiKey);
  putStringIfChanged("steamId", steamId);
  putStringIfChanged("qbUrl", qbUrl);
  putStringIfChanged("qbUser", qbUser);
  putStringIfChanged("qbPass", qbPass);
  putStringIfChanged("octoUrl", octoUrl);
  putStringIfChanged("octoKey", octoKey);
  putStringIfChanged("haUrl", haUrl);
  putStringIfChanged("haToken", haToken);
  putStringIfChanged("haEntityId", haEntityId);

  for (int p = 0; p < 3; p++) {
    putStringIfChanged(("t_name" + String(p)).c_str(), tabNames[p]);
    putIntIfChanged(("t_lay" + String(p)).c_str(), (int)pageLayouts[p]);
    for (int i = 0; i < HOME_SLOT_COUNT; i++) {
      String slotKey = "t" + String(p) + "slot" + String(i);
      putStringIfChanged(slotKey.c_str(), homeWidgetKey(pageWidgetSlots[p][i]));
      putStringIfChanged((slotKey + "_lbl").c_str(), pageHaLabels[p][i]);
      putStringIfChanged((slotKey + "_ent").c_str(), pageHaEntities[p][i]);
    }
  }
  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    putIntIfChanged(key.c_str(), timerPresetMin[i]);
  }

  applyThemeByKey(newAccent, newBg);
  applyTextColorByKey(newText);
  restoreSleepAwareBacklight();

  lastCalendarFetch = 0;
  lastSpotifyFetch = 0;
  lastGithubFetch = 0;
  lastSteamFetch = 0;
  lastQbitFetch = 0;
  qbSID = "";
  lastOctoFetch = 0;



  notesDirty = true;
  pageDirty = true;
  dataDirty = true;

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  cacheTimerMenu = "";
  cacheTimerDone = "";
  for (int p = 0; p < 3; p++) {
    for (int i = 0; i < HOME_SLOT_COUNT; i++) {
      cachePageWidgets[p][i] = "";
    }
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

  if (locationChanged)
    resetDataCaches();

  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

} // namespace

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}
