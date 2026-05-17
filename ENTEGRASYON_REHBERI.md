# Deskbuddy Tüm Entegrasyonlar Rehberi 🚀

Deskbuddy'nin web arayüzünde "API ve Entegrasyonlar" sekmesinde gördüğünüz kutucukları nasıl dolduracağınızı bu rehberde adım adım, en basit haliyle anlattık. Hiç yazılımcı olmanıza gerek yok, sadece adımları takip edin!

---

## 📅 1. Google Takvim (Calendar) Entegrasyonu
Deskbuddy direkt olarak Google'a şifrenizle giriş yapamaz (ekranı yok!). Bu yüzden Google'ın ücretsiz bir servisi olan **Apps Script**'i aracı olarak kullanacağız. Bu işlem sadece 2 dakikanızı alacak.

**Nasıl Yapılır?**
1. Bilgisayarınızdan [script.google.com](https://script.google.com) adresine gidin.
2. Sol üstteki **Yeni Proje (New Project)** butonuna basın.
3. Karşınıza çıkan kod ekranındaki her şeyi silin.
4. **Aşağıdaki kodu kopyalayıp o ekrana yapıştırın:**

```javascript
function doGet(e) {
  var calendar = CalendarApp.getDefaultCalendar();
  var now = new Date();
  var end = new Date(now.getTime() + (24 * 60 * 60 * 1000)); // Sonraki 24 saat
  var events = calendar.getEvents(now, end);

  var result = [];
  for (var i = 0; i < events.length && i < 10; i++) {
    result.push({
      title: events[i].getTitle(),
      startTime: events[i].getStartTime().toISOString(),
      endTime: events[i].getEndTime().toISOString()
    });
  }

  return ContentService.createTextOutput(JSON.stringify(result))
    .setMimeType(ContentService.MimeType.JSON);
}
```

5. Üst taraftaki disk 💾 (Kaydet) ikonuna basın.
6. Sağ üstteki mavi **Dağıt (Deploy) -> Yeni dağıtım (New deployment)** seçeneğine tıklayın.
7. Soldaki **"Türü seçin" (Select type)** dişlisine tıklayıp **"Web uygulaması" (Web app)** seçin.
8. **Erişim (Who has access)** kısmını kesinlikle **"Herkes" (Anyone)** olarak ayarlayın!
9. **Dağıt (Deploy)** butonuna basın. (Erişim izni isterse "İzinleri İncele" deyip Google hesabınızı seçin. "Gelişmiş"e tıklayıp "Güvenli değil sekmesine git" diyerek onay verin).
10. Karşınıza uzun bir **"Web uygulaması URL'si"** çıkacak. O linki kopyalayın.
11. Deskbuddy web arayüzüne gidip **Google Takvim URL** kutusuna bu linki yapıştırın! 🎉

---

## 🎵 2. Spotify Entegrasyonu
Spotify ne dinlediğinizi görmek için de tıpkı Takvim'deki gibi bir aracı kullanır. 

**Nasıl Yapılır?**
1. [developer.spotify.com/dashboard](https://developer.spotify.com/dashboard) adresine gidin ve Spotify hesabınızla giriş yapın.
2. **"Create App"** butonuna basın. 
   - App name: `Deskbuddy`
   - Redirect URI: `https://script.google.com/macros/d/BURAYA_APPS_SCRIPT_ID_GELECEK/usercallback` (Bunu şimdilik sallayın, birazdan düzelteceğiz).
   - "Web API" seçeneğini işaretleyip kaydedin.
3. Uygulamanızın sayfasına girin. Oradaki **Client ID** ve **Client Secret** şifrelerini bir yere not edin.
4. Tıpkı Takvim'de yaptığımız gibi [script.google.com](https://script.google.com) adresine gidip Yeni Proje açın.
5. İçini tamamen silin.
6. (Not: Spotify OAuth işlemleri Google Apps Script kütüphanesi gerektirir. GitHub'daki `docs/Spotify_Apps_Script.js` dosyasındaki uzun kodu buraya yapıştırıp, içerisindeki `CLIENT_ID` ve `CLIENT_SECRET` kısımlarına not ettiğiniz şifreleri girmelisiniz).
7. Takvimdeki gibi Dağıt -> Web Uygulaması -> Herkes adımlarını izleyip çıkan URL'yi kopyalayın.
8. Deskbuddy web arayüzünde **Spotify Proxy URL** kutusuna yapıştırın! 🎉

---

## 🐙 3. GitHub Entegrasyonu
Bu en kolayı! Deskbuddy ana ekranında yeşil kareler halinde ne kadar çok kod yazdığınızı (Contribution) gösteren bir alan var.

**Nasıl Yapılır?**
1. Deskbuddy web arayüzündeki **GitHub Kullanıcı Adı** kutusuna, GitHub profil linkinizdeki adınızı yazın. 
   - *Örnek: Profiliniz github.com/erdemkosk ise sadece `erdemkosk` yazın.*

---

## 🎮 4. Steam Entegrasyonu
O an Steam'de hangi oyunu oynadığınızı Deskbuddy ekranında görmek istiyorsanız bunu doldurun.

**Nasıl Yapılır?**
1. **Steam API Key Almak:** [steamcommunity.com/dev/apikey](https://steamcommunity.com/dev/apikey) adresine gidin. Domain kısmına sallama bir şey yazıp (örn: `localhost`) API Anahtarınızı oluşturun ve kopyalayın. Deskbuddy'de **Steam API Key** kutusuna yapıştırın.
2. **Steam ID (64-bit) Almak:** [steamid.io](https://steamid.io) sitesine gidin. Arama kutusuna kendi Steam profil linkinizi yapıştırın. Çıkan sonuçlar arasından **steamID64** yazan uzun numarayı (örn: `765611980...`) kopyalayın. Deskbuddy'de **Steam ID (64-bit)** kutusuna yapıştırın.

---

## 📥 5. qBittorrent Entegrasyonu
Bilgisayarınızda inen Torrent'lerin hızını ve durumunu görmek için kullanılır. Aynı ağda (Aynı Wi-Fi) olmanız gerekir.

**Nasıl Yapılır?**
1. Bilgisayarınızda qBittorrent'i açın.
2. Ayarlar (Çark ikonu) -> **Web Arayüzü (Web UI)** sekmesine gidin.
3. "Web Kullanıcı Arayüzü'nü Etkinleştir" kutusunu işaretleyin.
4. **qBittorrent URL:** Masaüstü bilgisayarınızın yerel IP adresini ve qBittorrent portunu yazın. *Örnek: `http://192.168.1.50:8080`*
5. **qBit Kullanıcı / Şifre:** Yine aynı ayar sayfasında belirlediğiniz kullanıcı adı ve şifreyi yazın (Varsayılan olarak kullanıcı `admin`, şifre `adminadmin`dir).

---

## 🖨️ 6. OctoPrint Entegrasyonu
Eğer 3D Yazıcınızda OctoPrint kullanıyorsanız, baskının yüzde kaçta olduğunu, kalan süreyi ve sıcaklıkları Deskbuddy ekranından takip edebilirsiniz!

**Nasıl Yapılır?**
1. **OctoPrint URL:** OctoPrint'e tarayıcıdan nasıl giriyorsanız o adresi yazın. *Örnek: `http://octopi.local` veya `http://192.168.1.60`*
2. **OctoPrint API Key:** OctoPrint arayüzünde üstteki Ayarlar (İngiliz anahtarı) ikonuna tıklayın. Soldaki menüden **Application Keys** sekmesine gelin. Yeni bir anahtar oluşturup kopyalayın. Deskbuddy arayüzüne yapıştırın!

---

Herhangi bir entegrasyonu yaptıktan sonra Deskbuddy web sayfasının altındaki **"Kaydet"** butonuna basmayı ve cihazın kendini yeniden başlatmasını beklemeyi unutmayın! İyi eğlenceler! 👾
