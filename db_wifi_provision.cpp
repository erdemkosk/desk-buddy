/**
 * Wi-Fi ilk kurulum: captive portal + TFT'de WIFI: QR kodu (qrcode MIT).
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <stdio.h>
#include "qrcode.h"

#include "Deskbuddy_layout.h"
#include "Deskbuddy_config.h"
#include "db_wifi_provision.h"

extern TFT_eSPI tft;
extern WebServer server;
extern DNSServer dnsServer;
extern Preferences prefs;

extern uint16_t COL_BG;
extern uint16_t COL_TEXT;
extern uint16_t COL_ACCENT;
extern uint16_t COL_DIM;

extern bool wifiEnabled;

namespace {

void handleProvisionCaptiveRedirect() {
  server.sendHeader("Location", "http://192.168.4.1/");
  server.send(302, "text/plain", "");
}

void handleProvisionRoot() {
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

void handleProvisionSave() {
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

int drawProvisionWifiJoinQr(int topY) {
  char payload[128];
  snprintf(payload, sizeof(payload), "WIFI:T:nopass;S:%s;P:;H:false;;", DESKBUDDY_SOFTAP_SSID);

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

}  // namespace

void runWifiProvisioningIfNeeded() {
  if (!wifiEnabled) return;
  if (prefs.getString("wifiSsid", "").length() > 0) return;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(DESKBUDDY_SOFTAP_SSID);
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
  tft.drawString(DESKBUDDY_SOFTAP_SSID, 10, 34, 1);

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
