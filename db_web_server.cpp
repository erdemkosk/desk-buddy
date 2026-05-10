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
  String accent = prefs.getString("accent", "cyan");
  String bg = prefs.getString("bg", "slate");
  String txt = prefs.getString("text", "standard");
  String units = prefs.getString("units", "metric");
  String region = prefs.getString("region", "europe");
  String nickname = prefs.getString("nickname", "");
  bool flashMode = prefs.getBool("flashMode", false);

  String page;
  page.reserve(21000);

  page += "<!doctype html><html><head>";
  page += "<meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>Deskbuddy</title>";
  page += "<style>";
  page += ":root{color-scheme:dark;}";
  page += "body{margin:0;background:linear-gradient(180deg,#0b1018 0%,#111827 "
          "100%);color:#edf2f7;font-family:system-ui,sans-serif;}";
  page += ".wrap{max-width:980px;margin:0 auto;padding:28px 16px 36px;}";
  page += ".hero{margin-bottom:18px;padding:18px 20px;border:1px solid "
          "#243244;border-radius:20px;background:linear-gradient(135deg,#"
          "111927 0%,#172235 100%);box-shadow:0 10px 30px rgba(0,0,0,.22);}";
  page += ".hero h1{font-size:30px;margin:0 0 8px 0;}";
  page += ".hero p{margin:0;color:#a9b7c9;font-size:14px;}";
  page += ".ip{display:inline-block;margin-top:14px;padding:8px "
          "12px;border-radius:999px;background:#0b1220;border:1px solid "
          "#334155;color:#dbe7f5;font-size:13px;}";
  page += ".layout{display:grid;grid-template-columns:1.15fr "
          ".85fr;gap:16px;align-items:start;}";
  page += ".stack{display:grid;gap:16px;}";
  page += ".panel{background:#171b22;border:1px solid "
          "#2d3748;border-radius:18px;padding:18px;margin:0;}";
  page += ".panel-toggle{width:100%;display:flex;align-items:center;justify-"
          "content:space-between;gap:12px;background:none;border:none;color:#"
          "edf2f7;padding:0;margin:0;cursor:pointer;text-align:left;}";
  page += ".panel-toggle:hover{color:#ffffff;}";
  page += ".panel-toggle h2{flex:1;}";
  page += ".panel-chevron{font-size:18px;color:#8ea3ba;transition:transform "
          ".18s ease;}";
  page += ".panel.collapsed .panel-chevron{transform:rotate(-90deg);}";
  page += ".panel-body{margin-top:12px;}";
  page += ".panel.collapsed .panel-body{display:none;}";
  page += ".panel h2{margin:0 0 6px 0;font-size:18px;}";
  page += ".panel p{margin:0 0 14px "
          "0;color:#94a3b8;font-size:13px;line-height:1.45;}";
  page += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;}";
  page += ".grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:14px;}";
  page += ".label{display:block;font-size:13px;margin:0 0 8px "
          "0;color:#a0aec0;font-weight:600;}";
  page +=
      "textarea,input,select{width:100%;border-radius:12px;border:1px solid "
      "#334155;background:#0b1220;color:#edf2f7;padding:12px;box-sizing:border-"
      "box;font:inherit;}";
  page += "textarea{min-height:170px;resize:vertical;}";
  page +=
      "button{margin-top:18px;background:#38bdf8;border:none;color:#001018;"
      "padding:13px "
      "18px;border-radius:12px;font-weight:800;cursor:pointer;font:inherit;}";
  page += ".muted{font-size:13px;color:#94a3b8;line-height:1.45;}";
  page += ".footer-note{margin-top:10px;font-size:12px;color:#7f92a8;}";
  page += ".settings-block{margin-top:18px;padding-top:16px;border-top:1px "
          "solid #2b3545;}";
  page += ".settings-block:first-of-type{margin-top:0;padding-top:0;border-top:"
          "none;}";
  page +=
      ".settings-title{display:block;margin:0 0 6px "
      "0;font-size:14px;font-weight:700;color:#edf2f7;letter-spacing:.02em;}";
  page += ".settings-desc{margin:0 0 12px "
          "0;font-size:12px;color:#8ea3ba;line-height:1.45;}";
  page += ".color-stack{display:grid;gap:12px;}";
  page += ".color-row{display:grid;grid-template-columns:120px "
          "1fr;gap:12px;align-items:center;}";
  page += ".color-meta{display:flex;align-items:center;justify-content:space-"
          "between;gap:10px;}";
  page += ".color-meta .label{margin:0;color:#dbe7f5;}";
  page += ".color-value{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += ".swatch-row{display:flex;flex-wrap:wrap;gap:8px;}";
  page += ".swatch{width:22px;height:22px;border-radius:999px;border:1px solid "
          "rgba(255,255,255,.18);cursor:pointer;position:relative;box-sizing:"
          "border-box;}";
  page += ".swatch input{display:none;}";
  page += ".swatch.active{box-shadow:0 0 0 2px #67e8f9, 0 0 0 5px "
          "rgba(103,232,249,.18);}";
  page += ".swatch.active::after{content:'';position:absolute;inset:5px;border-"
          "radius:999px;border:1px solid rgba(0,16,24,.45);}";
  page += ".timer-slot-grid{display:grid;grid-template-columns:1fr 1fr "
          "1fr;gap:10px;margin-top:14px;}";
  page += ".timer-slot{border:1px solid "
          "#334155;border-radius:12px;background:#0b1220;padding:10px 10px "
          "12px 10px;}";
  page += ".timer-slot-head{font-size:12px;color:#8ea3ba;margin-bottom:8px;"
          "font-weight:600;}";
  page += ".timer-slot-input{display:flex;align-items:center;gap:8px;}";
  page +=
      ".timer-slot input{padding:10px 12px;text-align:center;font-weight:700;}";
  page += ".timer-unit{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += "@media(max-width:820px){.layout{grid-template-columns:1fr;}.grid,."
          "grid-3,.timer-slot-grid{grid-template-columns:1fr;}.color-row{grid-"
          "template-columns:1fr;}}";
  page += "</style></head><body><div class='wrap'>";
  page += "<div class='hero'>";
  page += "<h1>Deskbuddy</h1>";
  page += "<p>Adjust notes, colors, system settings, and location from your "
          "browser.</p>";
  page += "<div class='ip'>ESP IP: ";
  page += WiFi.localIP().toString();
  page += "</div>";
  page += "<div class='footer-note' style='margin-top:12px'>Firmware ";
  page += FIRMWARE_VERSION;
  page += "</div></div>";

  page += "<form method='POST' action='/save'>";
  page += "<div class='layout'><div class='stack'>";

  page += "<div class='panel' data-panel='notes'>";
  page += "<button type='button' class='panel-toggle' "
          "aria-expanded='true'><h2>Notes</h2><span "
          "class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Short notes synced to the device.</p>";
  page += "<label class='label'>Notes</label>";
  page += "<textarea name='notes' maxlength='700'>";
  page += htmlEscape(notesText);
  page += "</textarea>";
  page += "<div class='muted'>Saved notes show up right away. Turkish letters "
          "are saved as plain ASCII for the display (c g i o s u).</div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='theme'>";
  page += "<button type='button' class='panel-toggle' "
          "aria-expanded='true'><h2>Theme and color</h2><span "
          "class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Colors and visual style for the display.</p>";
  page += "<div class='grid'>";

  page += "<div style='grid-column:1 / -1;' class='color-stack'>";

  page += "<div class='color-row'><div class='color-meta'><label "
          "class='label'>Accent</label><span class='color-value' "
          "id='accent-value'>";
  page += accent;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" +
          String(accent == "standard" ? " active" : "") +
          "' style='background:" + accentPreviewCss("standard") +
          ";'><input type='radio' name='accent' value='standard'" +
          String(accent == "standard" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(accent == "ice" ? " active" : "") +
          "' style='background:" + accentPreviewCss("ice") +
          ";'><input type='radio' name='accent' value='ice'" +
          String(accent == "ice" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(accent == "white" ? " active" : "") +
          "' style='background:" + accentPreviewCss("white") +
          ";'><input type='radio' name='accent' value='white'" +
          String(accent == "white" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(accent == "cyan" ? " active" : "") +
          "' style='background:" + accentPreviewCss("cyan") +
          ";'><input type='radio' name='accent' value='cyan'" +
          String(accent == "cyan" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(accent == "mint" ? " active" : "") +
          "' style='background:" + accentPreviewCss("mint") +
          ";'><input type='radio' name='accent' value='mint'" +
          String(accent == "mint" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(accent == "green" ? " active" : "") +
          "' style='background:" + accentPreviewCss("green") +
          ";'><input type='radio' name='accent' value='green'" +
          String(accent == "green" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(accent == "blue" ? " active" : "") +
          "' style='background:" + accentPreviewCss("blue") +
          ";'><input type='radio' name='accent' value='blue'" +
          String(accent == "blue" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(accent == "purple" ? " active" : "") +
          "' style='background:" + accentPreviewCss("purple") +
          ";'><input type='radio' name='accent' value='purple'" +
          String(accent == "purple" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(accent == "pink" ? " active" : "") +
          "' style='background:" + accentPreviewCss("pink") +
          ";'><input type='radio' name='accent' value='pink'" +
          String(accent == "pink" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(accent == "orange" ? " active" : "") +
          "' style='background:" + accentPreviewCss("orange") +
          ";'><input type='radio' name='accent' value='orange'" +
          String(accent == "orange" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(accent == "amber" ? " active" : "") +
          "' style='background:" + accentPreviewCss("amber") +
          ";'><input type='radio' name='accent' value='amber'" +
          String(accent == "amber" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(accent == "red" ? " active" : "") +
          "' style='background:" + accentPreviewCss("red") +
          ";'><input type='radio' name='accent' value='red'" +
          String(accent == "red" ? " checked" : "") + "></label>";
  page += "</div></div>";

  page +=
      "<div class='color-row'><div class='color-meta'><label "
      "class='label'>Text</label><span class='color-value' id='text-value'>";
  page += txt;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(txt == "standard" ? " active" : "") +
          "' style='background:" + accentPreviewCss("standard") +
          ";'><input type='radio' name='text' value='standard'" +
          String(txt == "standard" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(txt == "ice" ? " active" : "") +
          "' style='background:" + accentPreviewCss("ice") +
          ";'><input type='radio' name='text' value='ice'" +
          String(txt == "ice" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(txt == "white" ? " active" : "") +
          "' style='background:" + accentPreviewCss("white") +
          ";'><input type='radio' name='text' value='white'" +
          String(txt == "white" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(txt == "cyan" ? " active" : "") +
          "' style='background:" + accentPreviewCss("cyan") +
          ";'><input type='radio' name='text' value='cyan'" +
          String(txt == "cyan" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(txt == "mint" ? " active" : "") +
          "' style='background:" + accentPreviewCss("mint") +
          ";'><input type='radio' name='text' value='mint'" +
          String(txt == "mint" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(txt == "green" ? " active" : "") +
          "' style='background:" + accentPreviewCss("green") +
          ";'><input type='radio' name='text' value='green'" +
          String(txt == "green" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(txt == "blue" ? " active" : "") +
          "' style='background:" + accentPreviewCss("blue") +
          ";'><input type='radio' name='text' value='blue'" +
          String(txt == "blue" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(txt == "purple" ? " active" : "") +
          "' style='background:" + accentPreviewCss("purple") +
          ";'><input type='radio' name='text' value='purple'" +
          String(txt == "purple" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(txt == "pink" ? " active" : "") +
          "' style='background:" + accentPreviewCss("pink") +
          ";'><input type='radio' name='text' value='pink'" +
          String(txt == "pink" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(txt == "orange" ? " active" : "") +
          "' style='background:" + accentPreviewCss("orange") +
          ";'><input type='radio' name='text' value='orange'" +
          String(txt == "orange" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(txt == "amber" ? " active" : "") +
          "' style='background:" + accentPreviewCss("amber") +
          ";'><input type='radio' name='text' value='amber'" +
          String(txt == "amber" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(txt == "red" ? " active" : "") +
          "' style='background:" + accentPreviewCss("red") +
          ";'><input type='radio' name='text' value='red'" +
          String(txt == "red" ? " checked" : "") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label "
          "class='label'>Theme</label><span class='color-value' id='bg-value'>";
  page += bg;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(bg == "slate" ? " active" : "") +
          "' style='background:" + themePreviewCss("slate") +
          ";'><input type='radio' name='bg' value='slate'" +
          String(bg == "slate" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(bg == "deep" ? " active" : "") +
          "' style='background:" + themePreviewCss("deep") +
          ";'><input type='radio' name='bg' value='deep'" +
          String(bg == "deep" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(bg == "nordic" ? " active" : "") +
          "' style='background:" + themePreviewCss("nordic") +
          ";'><input type='radio' name='bg' value='nordic'" +
          String(bg == "nordic" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(bg == "forest" ? " active" : "") +
          "' style='background:" + themePreviewCss("forest") +
          ";'><input type='radio' name='bg' value='forest'" +
          String(bg == "forest" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(bg == "coffee" ? " active" : "") +
          "' style='background:" + themePreviewCss("coffee") +
          ";'><input type='radio' name='bg' value='coffee'" +
          String(bg == "coffee" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(bg == "soft" ? " active" : "") +
          "' style='background:" + themePreviewCss("soft") +
          ";'><input type='radio' name='bg' value='soft'" +
          String(bg == "soft" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(bg == "midnight" ? " active" : "") +
          "' style='background:" + themePreviewCss("midnight") +
          ";'><input type='radio' name='bg' value='midnight'" +
          String(bg == "midnight" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(bg == "graphite" ? " active" : "") +
          "' style='background:" + themePreviewCss("graphite") +
          ";'><input type='radio' name='bg' value='graphite'" +
          String(bg == "graphite" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(bg == "garnet" ? " active" : "") +
          "' style='background:" + themePreviewCss("garnet") +
          ";'><input type='radio' name='bg' value='garnet'" +
          String(bg == "garnet" ? " checked" : "") + "></label>";
  page += "<label class='swatch" + String(bg == "ochre" ? " active" : "") +
          "' style='background:" + themePreviewCss("ochre") +
          ";'><input type='radio' name='bg' value='ochre'" +
          String(bg == "ochre" ? " checked" : "") + "></label>";
  page += "</div></div>";

  page += "</div>";

  page += "</div></div></div>";

  page += "<div class='panel' data-panel='settings'>";
  page += "<button type='button' class='panel-toggle' "
          "aria-expanded='true'><h2>Settings</h2><span "
          "class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Core behavior and timer setup.</p>";
  page += "<div class='settings-block'>";
  page += "<span class='settings-title'>General</span>";
  page += "<div class='grid'>";
  page += "<div><label class='label'>Buddy nickname</label><input "
          "name='nickname' maxlength='24' value='" +
          htmlEscape(nickname) + "'></div>";
  page += "<div><label class='label'>Auto sleep interval</label><select "
          "name='sleepMin'>";
  page += "<option value='0'" +
          String(sleepIntervalMin == 0 ? " selected" : "") + ">Never</option>";
  page += "<option value='1'" +
          String(sleepIntervalMin == 1 ? " selected" : "") +
          ">1 minute</option>";
  page += "<option value='5'" +
          String(sleepIntervalMin == 5 ? " selected" : "") +
          ">5 minutes</option>";
  page += "<option value='10'" +
          String(sleepIntervalMin == 10 ? " selected" : "") +
          ">10 minutes</option>";
  page += "<option value='30'" +
          String(sleepIntervalMin == 30 ? " selected" : "") +
          ">30 minutes</option>";
  page += "<option value='60'" +
          String(sleepIntervalMin == 60 ? " selected" : "") +
          ">1 hour</option>";
  page +=
      "</select><div class='muted' style='margin-top:8px;'>Idle dims backlight "
      "only. Full black sleep: moon button on device.</div></div>";
  page += "<div><label class='label'>Measurement system</label><select "
          "name='units'>";
  page += "<option value='metric'" +
          String(units == "metric" ? " selected" : "") +
          ">Celsius / mm</option>";
  page += "<option value='imperial'" +
          String(units == "imperial" ? " selected" : "") +
          ">Fahrenheit / inches</option>";
  page += "</select></div>";
  page += "<div><label class='label'>Date format</label><select name='region'>";
  page += "<option value='europe'" +
          String(region == "europe" ? " selected" : "") +
          ">European: dd.mm.yyyy</option>";
  page += "<option value='us'" + String(region == "us" ? " selected" : "") +
          ">US: mm/dd/yyyy</option>";
  page += "</select></div>";
  page += "</div>";
  page += "</div>";
  page += "<div class='settings-block'><span "
          "class='settings-title'>Timer</span><div "
          "class='settings-desc'>Choose the six quick timers shown in the "
          "popup menu.</div><div class='timer-slot-grid'>";
  for (int i = 0; i < 6; i++) {
    page += "<div class='timer-slot'><div class='timer-slot-head'>Slot " +
            String(i + 1) +
            "</div><div class='timer-slot-input'><input type='number' min='1' "
            "max='180' name='timer" +
            String(i) + "' value='" + String(timerPresetMin[i]) +
            "'><span class='timer-unit'>min</span></div></div>";
  }
  page += "</div>";
  page +=
      "<div style='margin-top:14px;'><span class='settings-title'>Alert "
      "behavior</span><label "
      "style='display:flex;align-items:center;gap:10px;color:#edf2f7;'><input "
      "type='checkbox' name='flashMode' value='1'" +
      String(flashMode ? " checked" : "") +
      " style='width:auto;'>Flash screen when timer ends</label></div></div>";
  page +=
      "<div class='settings-block'><span "
      "class='settings-title'>Location</span><div class='settings-desc'>Used "
      "for weather data and sun times.</div><div class='grid-3'>";
  page += "<div><label class='label'>Location name</label><input "
          "name='locname' value='" +
          htmlEscape(locationName) + "'></div>";
  page +=
      "<div><label class='label'>Latitude</label><input name='lat' value='" +
      String(LAT, 6) + "'></div>";
  page +=
      "<div><label class='label'>Longitude</label><input name='lng' value='" +
      String(LNG, 6) + "'></div>";
  page += "</div><div class='footer-note'>Example Berlin: latitude 52.5200, "
          "longitude 13.4050.</div></div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='calendar'>";
  page += "<button type='button' class='panel-toggle' "
          "aria-expanded='true'><h2>Takvim (Google)</h2><span "
          "class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Google Apps Script URL'sini buraya yapıştır.</p>";
  page += "<label class='label'>Google Apps Script URL</label>";
  page += "<input type='text' name='calUrl' value='" + htmlEscape(calendarUrl) +
          "'>";
  page += "<div class='muted'>Örnek: "
          "https://script.google.com/macros/s/.../exec</div>";
  page += "<details "
          "style='margin-top:12px;cursor:pointer;color:#8ea3ba;font-size:13px;'"
          "><summary><b>Nasıl URL Alınır? (Tıkla ve Öğren)</b></summary>";
  page += "<ol style='padding-left:20px;margin-top:8px;line-height:1.6;'>";
  page += "<li><a href='https://script.google.com' target='_blank' "
          "style='color:#38bdf8;'>script.google.com</a> adresine gidin ve "
          "'Yeni Proje' oluşturun.</li>";
  page += "<li>Açılan editöre aşağıdaki <b>Google Apps Script</b> kodunu "
          "yapıştırın:</li>";
  page +=
      "<div style='background:#0b1220;border:1px solid "
      "#334155;padding:10px;border-radius:8px;overflow-x:auto;font-family:"
      "monospace;margin:8px 0;'>"
      "function doGet(e) {<br>"
      "&nbsp;&nbsp;var calendar = CalendarApp.getDefaultCalendar();<br>"
      "&nbsp;&nbsp;var now = new Date();<br>"
      "&nbsp;&nbsp;var endOfDay = new Date();<br>"
      "&nbsp;&nbsp;endOfDay.setHours(23, 59, 59, 999);<br>"
      "&nbsp;&nbsp;var events = calendar.getEvents(now, endOfDay);<br>"
      "&nbsp;&nbsp;var nextEvent = null;<br>"
      "&nbsp;&nbsp;for (var i = 0; i < events.length; i++) {<br>"
      "&nbsp;&nbsp;&nbsp;&nbsp;if (!events[i].isAllDayEvent()) {<br>"
      "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;nextEvent = events[i]; break;<br>"
      "&nbsp;&nbsp;&nbsp;&nbsp;}<br>"
      "&nbsp;&nbsp;}<br>"
      "&nbsp;&nbsp;var responseData = {};<br>"
      "&nbsp;&nbsp;if (nextEvent) {<br>"
      "&nbsp;&nbsp;&nbsp;&nbsp;responseData = { title: nextEvent.getTitle(), "
      "time: Utilities.formatDate(nextEvent.getStartTime(), "
      "Session.getScriptTimeZone(), \"HH:mm\") };<br>"
      "&nbsp;&nbsp;} else {<br>"
      "&nbsp;&nbsp;&nbsp;&nbsp;responseData = { title: \"Etkinlik Yok\", time: "
      "\"--:--\" };<br>"
      "&nbsp;&nbsp;}<br>"
      "&nbsp;&nbsp;return "
      "ContentService.createTextOutput(JSON.stringify(responseData))."
      "setMimeType(ContentService.MimeType.JSON);<br>"
      "}</div>";
  page += "<li>Sağ üstten <b>Dağıt (Deploy)</b> &gt; <b>Yeni dağıtım</b> "
          "seçeneğine tıklayın.</li>";
  page += "<li>Tür olarak <b>Web Uygulaması</b>'nı tıklayın. <b>Erişim: "
          "Herkes</b> (Anyone) olarak seçili olsun!</li>";
  page += "<li>Dağıt'a basın. Karşınıza çıkan uyarıda kendi hesabınızla izin "
          "verin (Advanced &gt; Go to script...).</li>";
  page +=
      "<li>Size verilen <b>Web uygulamasının URL'si</b> yazan linki kopyalayıp "
      "yukarıdaki kutuya yapıştırın ve Deskbuddy'ye kaydedin.</li>";
  page += "</ol></details>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='spotify'>";
  page += "<button type='button' class='panel-toggle' "
          "aria-expanded='true'><h2>Spotify (Google Apps)</h2><span "
          "class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Hangi şarkının çaldığını kayan yazıyla görmek için Proxy URL "
          "ekle.</p>";
  page += "<label class='label'>Spotify Proxy URL</label>";
  page += "<input type='text' name='spotifyUrl' value='" +
          htmlEscape(spotifyUrl) + "'>";
  page += "<details "
          "style='margin-top:12px;cursor:pointer;color:#8ea3ba;font-size:13px;'"
          "><summary><b>Nasıl Kurulur? (Tıkla ve Öğren)</b></summary>";
  page += "<ol style='padding-left:20px;margin-top:8px;line-height:1.6;'>";
  page +=
      "<li>Spotify Developer (geliştirici) sayfasından ücretsiz bir uygulama "
      "açarak <b>Client ID</b> ve <b>Client Secret</b> edinin.</li>";
  page +=
      "<li><b>script.google.com</b> adresinde yeni bir proje oluşturun.</li>";
  page += "<li>Deskbuddy Proje dosyalarındaki <b>SETUP_GUIDE.md</b> belgesinin "
          "en altındaki uzun scripti kopyalayıp editöre yapıştırın.</li>";
  page += "<li>Sol taraftan Proje Ayarları çarkına (Ayarlar) tıklayıp <b>Komut "
          "dosyası özellikleri</b> (Script properties) kısmına bu ID ve Secret "
          "kodlarını ekleyip kaydedin.</li>";
  page += "<li>Dağıt (Deploy) ekranından erişimi <b>Herkes</b> (Anyone) "
          "yaparak onaylayın.</li>";
  page +=
      "<li>Açılan yetkilendirme linklerine tıklayıp Spotify hesabınıza (kendi "
      "kendinize) izin verin. Gelen Web URL'yi bu kutuya yapıştırın!</li>";
  page += "</ol></details>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='github'>";
  page += "<button type='button' class='panel-toggle' "
          "aria-expanded='true'><h2>GitHub Contributions</h2><span "
          "class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>GitHub katkı (commit) geçmişinizi ekranda görmek için kullanıcı adınızı girin.</p>";
  page += "<label class='label'>GitHub Username</label>";
  page += "<input type='text' name='githubUser' value='" +
          htmlEscape(githubUser) + "'>";
  page += "<div class='muted'>Sadece kullanıcı adını girin (Örnek: octocat).</div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='water'>";
  page += "<button type='button' class='panel-toggle' "
          "aria-expanded='true'><h2>Su / Alışkanlık Takipçisi</h2><span "
          "class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Günlük su içme hedefinizi (bardak sayısı) belirleyin. Widget üzerinden dokunarak sayacı artırıp azaltabilirsiniz.</p>";
  page += "<label class='label'>Günlük Su Hedefi (Bardak)</label>";
  page += "<input type='number' name='waterGoal' value='" +
          String(waterGoal) + "' min='1' max='50'>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='steam'>";
  page += "<button type='button' class='panel-toggle' "
          "aria-expanded='true'><h2>Steam</h2><span "
          "class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Son oynanan oyunu ve haftalik sure gostermek icin Steam API Key ve Steam ID girin.</p>";
  page += "<div class='grid'>";
  page += "<div><label class='label'>Steam API Key</label>";
  page += "<input type='text' name='steamKey' value='" + htmlEscape(steamApiKey) + "' placeholder='XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX'></div>";
  page += "<div><label class='label'>Steam ID (64-bit)</label>";
  page += "<input type='text' name='steamId' value='" + htmlEscape(steamId) + "' placeholder='76561198XXXXXXXXX'></div>";
  page += "</div>";
  page += "<div class='muted' style='margin-top:10px;'>";
  page += "API Key icin: <a href='https://steamcommunity.com/dev/apikey' target='_blank' style='color:#38bdf8;'>steamcommunity.com/dev/apikey</a> &nbsp;|&nbsp; ";
  page += "Steam ID icin: <a href='https://steamid.io' target='_blank' style='color:#38bdf8;'>steamid.io</a></div>";
  page += "<div class='muted' style='margin-top:6px;'>Steam profilinizin herkese acik olmasi gerekiyor. Son 2 hafta icinde oynadiginiz oyun gosterilir.</div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='tabs'>";
  page += "<button type='button' class='panel-toggle' "
          "aria-expanded='true'><h2>Sekme Tasarımları (Tabs)</h2><span "
          "class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  
  for (int p = 0; p < 3; p++) {
    page += "<h3>Sekme " + String(p + 1) + "</h3>";
    page += "<div style='margin-bottom: 10px;'>";
    page += "<label class='label'>Sekme İsmi (Max 6 harf)</label>";
    page += "<input type='text' name='t_name" + String(p) + "' value='" + htmlEscape(tabNames[p]) + "' maxlength='6'>";
    page += "</div>";

    page += "<div style='margin-bottom: 10px;'>";
    page += "<label class='label'>Sayfa Tipi (Layout)</label>";
    page += "<select name='t_lay" + String(p) + "' onchange='updateLayout(this.value, " + String(p) + ")'>";
    page += "<option value='0'" + String(pageLayouts[p] == LAYOUT_GRID ? " selected" : "") + ">Izgara (Saat + 4 Widget)</option>";
    page += "<option value='3'" + String(pageLayouts[p] == LAYOUT_GRID_6 ? " selected" : "") + ">Izgara (Saat Yok + 6 Widget)</option>";
    page += "<option value='1'" + String(pageLayouts[p] == LAYOUT_FULL_WEATHER ? " selected" : "") + ">Tam Ekran: Hava Durumu</option>";
    page += "<option value='2'" + String(pageLayouts[p] == LAYOUT_FULL_NOTES ? " selected" : "") + ">Tam Ekran: Notlar</option>";
    page += "</select></div>";

    page += "<div id='grid_p" + String(p) + "' class='grid' style='display:" + String((pageLayouts[p] == LAYOUT_GRID || pageLayouts[p] == LAYOUT_GRID_6) ? "grid" : "none") + "; padding-top: 10px;'>";
    for (int i = 0; i < HOME_SLOT_COUNT; i++) {
      page += "<div id='p" + String(p) + "slot" + String(i) + "_c' style='display:" + String((pageLayouts[p] == LAYOUT_GRID && i >= 4) ? "none" : "block") + "'><label class='label'>";
      page += homeSlotLabel(i);
      page += "</label><select name='t" + String(p) + "slot" + String(i) + "'>";
      appendHomeWidgetOptions(page, homeWidgetKey(pageWidgetSlots[p][i]));
      page += "</select></div>";
    }
    page += "</div><hr style='margin: 20px 0;'>";
  }
  page += "</div></div>";

  page += "</div><div class='stack'>";

  page += "<button type='submit'>Save to Deskbuddy</button>";
  page += "</div></div></form>";
  page += "<script>function updateLayout(val, p){"
          "var g=document.getElementById('grid_p'+p);"
          "var isGrid=(val==='0'||val==='3');"
          "g.style.display=isGrid?'grid':'none';"
          "if(isGrid){"
          "document.getElementById('p'+p+'slot4_c').style.display=(val==='3')?'block':'none';"
          "document.getElementById('p'+p+'slot5_c').style.display=(val==='3')?'block':'none';"
          "}}\n";
  page +=
      "var "
      "colorNames={accent:{standard:'Standard',ice:'Ice',white:'White',cyan:'"
      "Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',"
      "orange:'Orange',amber:'Amber',red:'Red'},text:{standard:'Standard',ice:'"
      "Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',"
      "purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},bg:"
      "{slate:'Slate',deep:'Deep black',nordic:'Nordic "
      "blue',forest:'Forest',coffee:'Coffee',soft:'Soft "
      "dark',midnight:'Midnight',graphite:'Graphite',garnet:'Garnet',ochre:'"
      "Ochre'}};";
  page += "var panelStorageKey='deskbuddy-panel-state-v1';";
  page += "document.querySelectorAll('.swatch input').forEach(function(input){";
  page += "input.addEventListener('change',function(){";
  page += "document.querySelectorAll('.swatch "
          "input[name=\"'+input.name+'\"]').forEach(function(peer){";
  page += "peer.closest('.swatch').classList.toggle('active', peer.checked);";
  page += "});";
  page += "var valueEl=document.getElementById(input.name+'-value');";
  page += "if(valueEl&&colorNames[input.name]&&colorNames[input.name][input."
          "value]){valueEl.textContent=colorNames[input.name][input.value];}";
  page += "});";
  page += "});";
  page += "function readPanelState(){try{return "
          "JSON.parse(localStorage.getItem(panelStorageKey)||'{}');}catch(e){"
          "return {};}}";
  page += "function "
          "writePanelState(state){localStorage.setItem(panelStorageKey,JSON."
          "stringify(state));}";
  page += "function "
          "applyPanelState(panel,collapsed){panel.classList.toggle('collapsed',"
          "collapsed);var "
          "btn=panel.querySelector('.panel-toggle');if(btn){btn.setAttribute('"
          "aria-expanded',collapsed?'false':'true');}}";
  page += "var savedPanelState=readPanelState();";
  page += "document.querySelectorAll('.panel[data-panel]').forEach(function("
          "panel){";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(Object.prototype.hasOwnProperty.call(savedPanelState,panelId)){"
          "applyPanelState(panel,!!savedPanelState[panelId]);}";
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
  prefs.putString("calUrl", calendarUrl);
  prefs.putString("spotifyUrl", spotifyUrl);
  prefs.putString("githubUser", githubUser);
  prefs.putInt("w_goal", waterGoal);
  prefs.putString("steamKey", steamApiKey);
  prefs.putString("steamId", steamId);
  for (int p = 0; p < 3; p++) {
    prefs.putString(("t_name" + String(p)).c_str(), tabNames[p]);
    prefs.putInt(("t_lay" + String(p)).c_str(), (int)pageLayouts[p]);
    for (int i = 0; i < HOME_SLOT_COUNT; i++) {
      String slotKey = "t" + String(p) + "slot" + String(i);
      prefs.putString(slotKey.c_str(), homeWidgetKey(pageWidgetSlots[p][i]));
    }
  }
  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    prefs.putInt(key.c_str(), timerPresetMin[i]);
  }

  applyThemeByKey(newAccent, newBg);
  applyTextColorByKey(newText);
  restoreSleepAwareBacklight();

  lastCalendarFetch = 0;
  lastSpotifyFetch = 0;
  lastGithubFetch = 0;
  lastSteamFetch = 0;

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
  server.send(303);
}

} // namespace

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}
