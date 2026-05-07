# Deskbuddy Özel Entegrasyon Rehberi

Deskbuddy, şifreli kimlik doğrulama (OAuth 2.0) gerektiren Google Takvim ve Spotify gibi dev platformlara doğrudan bağlanamaz. Çünkü üzerinde şifrenizi girip "İzin Ver" butonuna basabileceğiniz bir web tarayıcısı yoktur. 

Bu sorunu çözmek için **Google Apps Script** platformunu ücretsiz bir "Aracı (Proxy) Sunucu" olarak kullanıyoruz. Kurulumu sadece bir kez yapacaksınız.

---

## 1. Google Takvim Entegrasyonu

Önünüzdeki 24 saat içindeki etkinlikleri Deskbuddy'ye getirmek için aşağıdaki adımları izleyin:

### Adım 1.1: Aracı Kodu Hazırlama
1. Bilgisayarınızdan [script.google.com](https://script.google.com) adresine gidin.
2. Sol üstteki **Yeni Proje (New Project)** butonuna basın.
3. Karşınıza çıkan kod editöründeki mevcut her şeyi silin.
4. **Aşağıdaki kodu kopyalayıp editöre yapıştırın:**

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

  // Gelen etkinlikleri kontrol et ve sırada olan ilk etkinliği yakala
  for (var i = 0; i < events.length; i++) {
    // Tüm gün süren etkinlikleri ve saati şu andan geride olan (geçmiş) etkinlikleri ele
    if (!events[i].isAllDayEvent() && events[i].getStartTime() > now) {
      nextEvent = events[i];
      break; 
    }
  }

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

### Adım 1.2: Kodunuzu İnternete Açın (Dağıtım)
1. Editörün sağ üst köşesindeki **Dağıt (Deploy)** butonuna tıklayın, ardından **Yeni Dağıtım (New deployment)** seçeneğine basın.
2. "Tür seçin" (Select type) yazısının yanındaki **Tekerlek (Ayarlar) İkonuna** tıklayıp **Web Uygulaması (Web app)** seçeneğini işaretleyin.
3. Alt kısımdaki **Erişim (Who has access)** menüsünü kesinlikle **Herkes (Anyone)** olarak ayarlayın (Aksi takdirde saat veriyi okuyamaz).
4. **Dağıt**'a basın. Karşınıza kırmızı bir uyarı çıkacak. Kendi Google hesabınızı seçin, **Gelişmiş (Advanced)** yazısına basıp en alttaki **Projeye Git (Güvenli Değil)** seçeneğine tıklayarak izin verin.
5. İşlem bittiğinde size verilen **Web Uygulaması URL'sini** kopyalayın.

### Adım 1.3: Deskbuddy'ye Bağlama
Deskbuddy'nin web arayüzüne girin (saatin menüsünden IP adresine bakabilirsiniz). "Google Takvim" kısmındaki kutucuğa kopyaladığınız bu uzun linki yapıştırın ve arayüzün en altından kaydedin. Bitti!

---

## 2. Spotify (Şu An Çalan) Entegrasyonu

Spotify daha yüksek güvenlikli bir sistem kullandığı için önce bir Spotify geliştirici uygulamanız olmalı.

### Adım 2.1: Spotify API Şifrenizi Alın
1. Bilgisayarınızdan [developer.spotify.com/dashboard](https://developer.spotify.com/dashboard) adresine gidin ve Spotify hesabınızla giriş yapın.
2. **Create App** butonuna basın.
3. Uygulamanın ismine "Deskbuddy" ve açıklamasına herhangi bir şey yazabilirsiniz.
4. **Redirect URIs** bölümüne tam olarak şu adresi yazın ve ekleyin (Bir harf bile eksik olmamalı):  
   `https://script.google.com/macros/`
5. Web API vb. kutucuklarını rastgele seçebilirsiniz. Formun en altındaki sözleşmeyi kabul edip Save'e basın.
6. Uygulamanız açıldığında, **Settings** (Ayarlar) kısmına girin. Orada yazılı olan **Client ID** kodunu ve **Client Secret** (Gizli olan kodu View diyerek açın) kodunu bir kenara kopyalayın.

### Adım 2.2: Google Aracı Sunucusunu Kurun
1. Takvimde olduğu gibi tekrar [script.google.com](https://script.google.com) adresine gidip yeni bir boş proje açın.
2. Açılan ekranda sol taraftaki menüden (Düzenleyicinin altındaki) **Proje Ayarları (Çark İkonu)** sekmesine gidin.
3. En altlara kaydırın ve **Komut dosyası özellikleri (Script properties)** bölümünde **Komut dosyası özelliği ekle** tuşuna basın.
   - Özellik kutusuna `SPOTIFY_CLIENT_ID` yazın, Değer kutusuna Spotify'dan aldığınız **Client ID** kodunu yapıştırın.
   - Tekrar ekle tuşuna basın. Özellik kutusuna `SPOTIFY_CLIENT_SECRET` yazın, Değer kutusuna Spotify'dan aldığınız **Client Secret** kodunu yapıştırın.
   - O bölümü kaydedin.
4. Sol menüden **Düzenleyiciye (Kod ekranına)** geri dönün. Mevcut kodu tamamen silip aşağıdaki devasa kodu yapıştırın:

```javascript
var CLIENT_ID = PropertiesService.getScriptProperties().getProperty('SPOTIFY_CLIENT_ID');
var CLIENT_SECRET = PropertiesService.getScriptProperties().getProperty('SPOTIFY_CLIENT_SECRET');

function doGet(e) {
  var props = PropertiesService.getUserProperties();
  var refreshToken = props.getProperty("SPOTIFY_REFRESH_TOKEN");
  var redirectUri = ScriptApp.getService().getUrl();
  
  // EĞER CİHAZA İZİN VERİLMEMİŞSE, SİZİ İZİN SAYFASINA YÖNLENDİRECEK ALAN
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
        return ContentService.createTextOutput("Tebrikler Cihaz Eslendi! Bu sekmeyi kapatabilir ve ilk adimda kopyaladiginiz Google URL'sini saatinizin Web paneline yapistirabilirsiniz.");
      } else {
        return ContentService.createTextOutput("Hata olustu. Spotify uygulamasinda Redirect URI icerisine bu scriptin URL'sini (https://script.google.com/macros/) eklediginizden emin olun: " + res.getContentText());
      }
    } else {
      var authUrl = "https://accounts.spotify.com/authorize?client_id=" + CLIENT_ID + "&response_type=code&redirect_uri=" + encodeURIComponent(redirectUri) + "&scope=user-read-currently-playing";
      var html = "<div style='font-family: sans-serif; text-align: center; margin-top: 50px;'><h2>Spotify Yetkilendirmesi Gerekiyor</h2><a href='" + authUrl + "' target='_top' style='padding: 10px 20px; background-color: #1DB954; color: white; text-decoration: none; border-radius: 50px; font-weight: bold; font-size: 16px;'>Spotify'a Baglan</a></div>";
      return HtmlService.createHtmlOutput(html);
    }
  }
  
  // HER ŞEY HAZIRSA, YENİ TOKEN AL VE ÇALAN ŞARKIYI SORGULA EKRANI
  var tokenRes = UrlFetchApp.fetch("https://accounts.spotify.com/api/token", {
    "method": "post",
    "headers": { "Authorization": "Basic " + Utilities.base64Encode(CLIENT_ID + ":" + CLIENT_SECRET) },
    "payload": { "grant_type": "refresh_token", "refresh_token": refreshToken }, "muteHttpExceptions": true
  });
  var accessToken = JSON.parse(tokenRes.getContentText()).access_token;
  
  if (!accessToken) {
    props.deleteProperty("SPOTIFY_REFRESH_TOKEN");
    return ContentService.createTextOutput(JSON.stringify({song: "Hata", artist: "Token gecersiz", playing: false}));
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

### Adım 2.3: Son Dağıtım ve Yetkilendirme (Çok Önemli)
1. Takvimde yaptığınız gibi Sağ üstten **Dağıt (Deploy)** ardından **Yeni dağıtım** deyin.
2. Türünü **Web Uygulaması** ve Erişimi yine **Herkes (Anyone)** yapıp kaydedin (Yine izin ver kısımlarını onaylayın).
3. Ekrana çıkan **Web Uygulaması URL'sini (Linkini)** her şeyden önce tamamen kopyalayın!
4. **Dikkat:** Bu linki tarayıcınızda (Google Chrome) yeni bir sekme açarak normal bir siteye girer gibi adres çubuğuna **yapıştırıp enter'a basın.**
5. Script sizi Spotify yetkilendirme sayfasına yönlendirecek. "Kabul Et" butonuna bastığınızda ekranda **"Tebrikler Cihaz Eslendi!"** tarzında düz beyaz bir sayfa ile karşılaşmanız gerekiyor. Karşılaştıysanız bağlantı kurulmuştur!

### 🚨 Olası Hata Çözümü: `redirect_uri: Not matching configuration`
Eğer açık yeşil butona tıkladığınızda Spotify **"INVALID_CLIENT: Invalid redirect URI"** veya **"Not matching configuration"** hatası veriyorsa, bunun sebebi **Google'da her "Yeni Dağıtım" yaptığınızda URL'nin tamamen değişmesidir.**
1. En son dağıtımdan elde ettiğiniz ve tarayıcınızın üst kısmında o an açık olan o uzun `https://script.google.com/macros/s/..../exec` URL'sini tekrar kopyalayın.
2. [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) uygulamanızın **Ayarlarına (Settings)** geri dönün.
3. **Redirect URIs** bölümünde bulunan önceki eski linki silip (çarpıya basıp), bu son kopyaladığınız en güncel linki yapıştırıp "Add" ve "Save" deyin.
4. Yeniden butona basıp denediğinizde yetkilendirme sorunsuz sağlanacaktır!

### Adım 2.4: Deskbuddy'ye Bağlama
Google Script'ten elde ettiğiniz o uzun "Web Uygulaması URL'sini", Deskbuddy'nin arayüzündeki "Spotify (Google Apps)" alanına yapıştırın ve kaydedin. 

Cihaz masaüstünde 15-20 saniye içerisinde Spotify hesabınıza bağlanıp anlık dinlediğiniz şarkıyı çok şık bir biçimde kayan yazılarla ekrana dökmeye başlayacaktır. İyi eğlenceler!
