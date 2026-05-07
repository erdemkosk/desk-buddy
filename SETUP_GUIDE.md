# Deskbuddy Setup Guide

This guide explains how to install and upload the Deskbuddy software to a compatible ESP32 touchscreen board using **Arduino IDE**.


## What You Need

Before you begin, make sure you have:

- A compatible ESP32 touchscreen board
- A USB cable with data support
- Arduino IDE installed
- Access to a WiFi network
- The Deskbuddy source code

## 1. Install ESP32 Board Support

Deskbuddy is built for ESP32, so you first need to install the ESP32 board package in Arduino IDE.

1. Open **Arduino IDE**
2. Go to **Tools > Board > Boards Manager**
3. Search for `ESP32`
4. Install **ESP32 by Espressif Systems**

After installation, select the board that best matches your hardware under:

**Tools > Board**

If you are unsure which profile to use, start with the closest ESP32 option and adjust if needed.

## 2. Install Required Libraries

Open:

**Sketch > Include Library > Manage Libraries**

Install these libraries:

- `TFT_eSPI`
- `ArduinoJson`
- `XPT2046_Touchscreen`

The following are normally included automatically with the ESP32 board package:

- `WiFi`
- `HTTPClient`
- `WiFiClientSecure`
- `WebServer`
- `Preferences`
- `SPI`

## 3. Configure TFT_eSPI

This is the most important step for getting the display to work correctly.

Deskbuddy uses the **TFT_eSPI** library, and you will most likely need to replace or edit the `User_Setup` file inside the TFT_eSPI library folder so it matches your display.

If the TFT_eSPI setup is wrong, you may see problems like:

- A black or white screen
- Wrong colors
- Incorrect rotation
- No visible output
- Touch and display not matching properly

### What to do

Find the TFT_eSPI library folder on your computer and locate:

`User_Setup.h`

Then either:

- Replace it with a working setup for your display
- Or edit the driver and pin settings manually

If you are using a specific ESP32 touchscreen board variant, it is a good idea to keep a backup of your working `User_Setup.h`.

## 4. Open the Deskbuddy Code

Open the Deskbuddy project in Arduino IDE.

For the public version, use:

- [desk_buddy_github.cpp]

If you rename the file or convert it to an `.ino`, that is also fine as long as the project builds correctly in Arduino IDE.

## 5. Add Your WiFi Credentials

Before uploading, update the WiFi values in the code:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
```

Replace those placeholders with your own WiFi network name and password.

## 6. Select the Correct Board and Port

In Arduino IDE:

- Select the correct **ESP32 board**
- Select the correct **COM port**

You can find the port under:

**Tools > Port**

If no port appears:

- Reconnect the board
- Try another USB cable
- Make sure the cable supports data, not only charging

## 7. Upload the Code

Once everything is configured:

1. Connect the ESP32 board by USB
2. Click **Upload**
3. Wait for the sketch to compile and flash

On some ESP32 boards, you may need to hold the **BOOT** button during upload if flashing does not start automatically.

## 8. First Boot

After a successful upload, the device should:

- Power on the display
- Connect to WiFi
- Sync time
- Fetch weather data
- Start the local web interface

You can also open the Serial Monitor to check boot messages:

**Tools > Serial Monitor**

In many cases, the local IP address will be printed there after WiFi connection succeeds.

## 9. Open the Web Interface

Once the ESP32 is connected to your network, open its local IP address in your browser.

From the browser interface, you can adjust things like:

- Accent colors and theme
- Text colors
- Regional time and date format
- Timer presets
- Weather location
- Notes
- Nickname
- Alert behavior

This makes it easy to personalize the device without editing the code every time.

## 10. Troubleshooting

### The display stays black or white

- Check the TFT_eSPI `User_Setup.h`
- Confirm the correct display driver is selected
- Verify that the board is receiving power

### The display works, but colors or rotation are wrong

- Check the TFT_eSPI configuration
- Verify display driver and pin mapping
- Confirm rotation settings in the code

### Touch does not work correctly

- Check touch wiring and controller support
- Verify rotation and calibration values

### Upload fails

- Make sure the correct COM port is selected
- Try another USB cable
- Hold the **BOOT** button during upload if needed

### WiFi does not connect

- Double-check SSID and password
- Make sure the network is in range
- Check the Serial Monitor for connection messages

### Time or weather does not update

- Confirm WiFi is connected
- Check that the location values are valid
- Verify that API requests are not being blocked

## Final Notes

Deskbuddy is designed to be easy to customize, but exact setup details may vary depending on your ESP32 touchscreen board version.

For most users, the **TFT_eSPI `User_Setup.h` configuration is the most important part** of the installation.

Once that is correct, the rest of the setup is usually straightforward.

---

## Appendix: Google Calendar Integration Setup

To show your Google Calendar events on Deskbuddy, you must provide a **Google Apps Script URL** in the Deskbuddy web interface. Since ESP32 cannot handle Google's complex OAuth2 securely on its own, this proxy script fetches the events on its behalf.

### How to get your URL:

1. Go to [script.google.com](https://script.google.com) and click **New Project**.
2. Replace all the code in the editor with this:

```javascript
function doGet(e) {
  // Varsayılan (kendi) takvimini al
  var calendar = CalendarApp.getDefaultCalendar();
  
  // Bugünü ve gün sonunu hesapla
  var now = new Date();
  var endOfDay = new Date();
  endOfDay.setHours(23, 59, 59, 999);
  
  // Bugüne ait etkinlikleri getir
  var events = calendar.getEvents(now, endOfDay);
  var nextEvent = null;

  // Gelen etkinlikleri kontrol et ve sırdaşında olan yaklaşan ilk etkinliği yakala
  for (var i = 0; i < events.length; i++) {
    // Tüm gün süren etkinlikler (mesela doğum günleri) genelde masa saati için elenir
    if (!events[i].isAllDayEvent() && events[i].getStartTime() > now) {
      nextEvent = events[i];
      break; 
    }
  }

  // Çıktı hazırlığı
  var responseData = {};
  
  if (nextEvent) {
    responseData = {
      status: "success",
      title: nextEvent.getTitle(),
      time: Utilities.formatDate(nextEvent.getStartTime(), Session.getScriptTimeZone(), "HH:mm")
    };
  } else {
    responseData = {
      status: "empty",
      title: "Etkinlik Yok",
      time: "--:--"
    };
  }

  // Veriyi metin (JSON) formatında dışarı aktar
  return ContentService.createTextOutput(JSON.stringify(responseData))
    .setMimeType(ContentService.MimeType.JSON);
}
```

---

## Appendix: Spotify Integration Setup (Now Playing)

To show currently playing music from Spotify on Deskbuddy automatically without storing sensitive authorization codes on the ESP32 itself, you will use a Google Apps Script proxy.

### Step 1: Create a Spotify App
1. Go to the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) and log in.
2. Click **Create App**. 
3. Name it "Deskbuddy". Enter any description.
4. For **Redirect URIs**, enter exactly: `https://script.google.com/macros/` (Wait, Google Apps Scripts redirect URIs actually end in `/usercallback` usually, but this script fetches the URL directly. Use `https://script.google.com/macros/` or come back here to paste the final web-app URL later. For safety, just add `https://script.google.com/macros/`).
5. Agree to Terms and click **Save**.
6. Open your App settings and copy your **Client ID** and **Client Secret**.

### Step 2: Create the Google Apps Script
1. Go to [script.google.com](https://script.google.com) and click **New Project**.
2. Go to **Project Settings** (the gear icon on the left panel).
3. Scroll down to **Script Properties** and click **Add script property**.
   - Add property `SPOTIFY_CLIENT_ID` and paste your Spotify Client ID.
   - Add property `SPOTIFY_CLIENT_SECRET` and paste your Spotify Client Secret.
4. Go back to the **Editor** (`< >` icon) and replace `Code.gs` with the following:

```javascript
var CLIENT_ID = PropertiesService.getScriptProperties().getProperty('SPOTIFY_CLIENT_ID');
var CLIENT_SECRET = PropertiesService.getScriptProperties().getProperty('SPOTIFY_CLIENT_SECRET');

function doGet(e) {
  var props = PropertiesService.getUserProperties();
  var refreshToken = props.getProperty("SPOTIFY_REFRESH_TOKEN");
  var redirectUri = ScriptApp.getService().getUrl();
  
  if (!refreshToken) {
    if (e.parameter.code) {
      var payload = {
        "grant_type": "authorization_code", "code": e.parameter.code, "redirect_uri": redirectUri
      };
      
      var res = UrlFetchApp.fetch("https://accounts.spotify.com/api/token", {
        "method": "post",
        "headers": { "Authorization": "Basic " + Utilities.base64Encode(CLIENT_ID + ":" + CLIENT_SECRET) },
        "payload": payload, "muteHttpExceptions": true
      });
      var json = JSON.parse(res.getContentText());
      
      if (json.refresh_token) {
        props.setProperty("SPOTIFY_REFRESH_TOKEN", json.refresh_token);
        return ContentService.createTextOutput("Success! You can now close this tab and paste the Web App URL into Deskbuddy.");
      } else {
        return ContentService.createTextOutput("Error: Did you add this script URL to your Spotify Redirect URIs? " + res.getContentText());
      }
    } else {
      var authUrl = "https://accounts.spotify.com/authorize?client_id=" + CLIENT_ID + "&response_type=code&redirect_uri=" + encodeURIComponent(redirectUri) + "&scope=user-read-currently-playing";
      var html = "<div style='font-family: sans-serif; text-align: center; margin-top: 50px;'><h2>Spotify Yetkilendirmesi Gerekiyor</h2><a href='" + authUrl + "' target='_top' style='padding: 10px 20px; background-color: #1DB954; color: white; text-decoration: none; border-radius: 50px; font-weight: bold; font-size: 16px;'>Spotify'a Baglan</a></div>";
      return HtmlService.createHtmlOutput(html);
    }
  }
  
  var tokenRes = UrlFetchApp.fetch("https://accounts.spotify.com/api/token", {
    "method": "post",
    "headers": { "Authorization": "Basic " + Utilities.base64Encode(CLIENT_ID + ":" + CLIENT_SECRET) },
    "payload": { "grant_type": "refresh_token", "refresh_token": refreshToken }, "muteHttpExceptions": true
  });
  var accessToken = JSON.parse(tokenRes.getContentText()).access_token;
  
  if (!accessToken) {
    props.deleteProperty("SPOTIFY_REFRESH_TOKEN");
    return ContentService.createTextOutput(JSON.stringify({song: "Error", artist: "Token exp.", playing: false}));
  }
  
  var playRes = UrlFetchApp.fetch("https://api.spotify.com/v1/me/player/currently-playing", {
    "method": "get", "headers": { "Authorization": "Bearer " + accessToken }, "muteHttpExceptions": true
  });
  if (playRes.getResponseCode() == 204 || playRes.getContentText() == "") {
    return ContentService.createTextOutput(JSON.stringify({song: "Spotify Uykuda", artist: "", playing: false}));
  }
  
  var playJson = JSON.parse(playRes.getContentText());
  if (playJson && playJson.item && playJson.is_playing) {
    return ContentService.createTextOutput(JSON.stringify({ song: playJson.item.name, artist: playJson.item.artists[0].name, playing: true }));
  } else {
    return ContentService.createTextOutput(JSON.stringify({song: "Durduruldu", artist: "", playing: false}));
  }
}
```

### Step 3: Deploy & Authenticate
1. Click **Deploy > New deployment**.
2. Select **Web app**. Change "Who has access" to **Anyone**. Click Deploy.
3. **CRITICAL:** Copy the **Web App URL**.
4. Go back to your Spotify Developer Dashboard. Edit your app settings, and paste this exact Google Web App URL into the **Redirect URIs** list. Save the Spotify app.
5. Open a new tab in your browser and go to your **Web App URL**.
6. Google will ask for permission, approve it. Then click the large **Connect to Spotify** button. It will ask for permission, approve it. It should say **"Success!"**.

> [!WARNING]
> If Spotify throws a black screen saying **`INVALID_CLIENT: Invalid redirect URI`** or **`Not matching configuration`**:
> This happens because Google generates a brand-new URL hash every time you click "New Deployment". 
> 1. Copy the newest URL from the top of your current browser tab.
> 2. Go back to your Spotify Dashboard Settings.
> 3. Delete the old URL from **Redirect URIs**, paste the newest URL, click **Add**, and **Save**.
> 4. Go back to your tab, click the green button again. It will work perfectly!

7. Keep that final working **Web App URL**, go to Deskbuddy's local IP, and paste the URL into the **Spotify Proxy URL** field. Done!

3. Click **Deploy > New deployment** in the top right corner.
4. Click the gear icon next to "Select type" and choose **Web app**.
5. Set **Who has access** to **Anyone** (this is strictly required so your ESP32 can fetch it without a manual login).
6. Click **Deploy**.
7. Google will ask you to authorize access to your calendar. Click **Authorize access**, select your Google account, click **Advanced**, and then click **Go to [Project Name] (unsafe)**. Allow the required permissions.
8. Copy the **Web app URL** it generates (`https://script.google.com/macros/s/.../exec`).
9. Paste this URL into the **Google Calendar** section in your Deskbuddy's Web Panel.
