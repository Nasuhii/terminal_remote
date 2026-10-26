# 🐬 Terminal Remote — Flipper Zero Bluetooth LE Multi-Tab Controller & Hub

[![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-FAP%20v1.1-ff8200?style=for-the-badge&logo=flipperzero&logoColor=white)](https://flipperzero.one/)
[![Bluetooth LE HID](https://img.shields.io/badge/Bluetooth-BLE%20HID%20Keyboard-0082fc?style=for-the-badge&logo=bluetooth&logoColor=white)](https://flipperzero.one/)
[![Zero Host Software](https://img.shields.io/badge/Host-Zero%20Software%20Required-4caf50?style=for-the-badge)](https://github.com/Nasuhii/terminal_remote)
[![Platform Support](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Android%20%7C%20iOS-informational?style=for-the-badge)](https://github.com/Nasuhii/terminal_remote)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](LICENSE)

**Terminal Remote** transforms your **Flipper Zero** into an untethered, wireless pocket cockpit for Linux, macOS, and Windows terminal workflows. It operates as a native standard **Bluetooth LE HID Keyboard**, requiring **zero host drivers or daemon scripts**!

---

## 📸 Ekran Görüntüsü ve Görsel Akış

```
  Eşleşme Bekleniyor              Konsol (Tab 1: MAIN)            Terminal Hub Menüsü
┌──────────────────────┐        ┌──────────────────────┐        ┌──────────────────────┐
│[BLUETOOTH BAGLANTISI]│        │(o)BLE [MAIN][WORK][LOGS] #42 │        │  [ TERMINAL HUB ]    │
├──────────────────────┤        ├──────────────────────┤        ├──────────────────────┤
│ ᛒ BAGLANTI BEKLENIYOR│        │> pts/1 [main] ~ [Alt+1]      │        │ >_ Terminal Console  │
│   Cihaz: Flipper BT  │        │----------------------│        │ BT Bluetooth: Bagli  │
│   PC/Tel'den baglan  │        │nasuhi@main:~$        │        │ #P Preset: Terminal  │
│                      │        │> ls -la █            │        │ >> Quick Macros      │
├──────────────────────┤        ├──────────────────────┤        ├──────────────────────┤
│OK:Konsola Gec BK:Exit│        │<:Tab- ^:Up OK:Ret v:Dn >:Tab+│        │▲▼:Gezin  OK:Sec      │
└──────────────────────┘        └──────────────────────┘        └──────────────────────┘
```

---

## ✨ Öne Çıkan Özellikler

- 📶 **Otomatik Bluetooth Eşleşme Ekranı (`SceneConnection`):**
  - Uygulamayı açtığında Flipper bir cihaza bağlı değilse doğrudan eşleşme arama ekranına geçer.
  - PC, Mac veya telefonundan Flipper Zero'yu eşleştirdiğin anda haptik titreşim ve mavi LED yanarak otomatik olarak **Terminal Konsolu**'na geçiş yapar.
  - İstersen bağlantı olmadan da `[OK]` ile doğrudan konsola geçebilir veya menüden dilediğin an Bluetooth bağlantı durumunu kontrol edebilirsin.
- 💻 **Sıfır Yazılım / Sürücüsüz Çalışma:**
  - Standart BLE HID Keyboard protokolü kullandığı için bilgisayarına **hiçbir köprü (bridge), python scripti veya sürücü kurman gerekmez**. Tıpkı kablosuz bir mekanik klavye gibi çalışır.
- 📑 **3 Sekmeli Çalışma Alanı (MAIN, WORK, LOGS):**
  - Sol (`<`) ve Sağ (`>`) tuşlarıyla sekmeler arasında tek tıkla geçiş yapabilirsin. Her geçişte terminale `Alt+1`, `Alt+2`, `Alt+3` klavye kısayollarını gönderir (tmux, GNOME Terminal, Alacritty, iTerm2, Windows Terminal ile tam uyumlu).
- ⚡ **İnteraktif Terminal Kabuğu:**
  - Ekranda aktif oturum başlığı (`pts/1`), prompt (`nasuhi@main:~$`), son çalıştırılan işlem, yanıp sönen kursor ve anlık tuş sayacı yer alır.
- 🎛️ **Merkezi Terminal Hub Menüsü (Geri Tuşuna Uzun Bas):**
  - Her an `GERİ` tuşuna basılı tutarak Hub Menüsü'ne ulaşabilirsin:
    1. **Terminal Console:** Canlı konsola dönüş.
    2. **Bluetooth Status / Connect:** Bağlantı sinyali ve eşleşme durumu.
    3. **Preset Selector:** Terminal, Anki ve özel tuş profilleri arasında geçiş.
    4. **Quick Macros:** `^C` (SIGINT), `^D` (EOF), `^L` (Clear Screen), `^Z` (Suspend), `^R` (Tersine Arama), `TAB` (Otomatik Tamamlama).
    5. **Settings & Remap:** Titreşim, LED, tuş gecikmesi (ms) ve SD kart ayarları.
    6. **Cheat Sheet:** Hızlı kısayol rehberi.
- 💾 **SD Kart Kalıcılığı:**
  - Tüm ayarlar `apps_data/terminal_remote/settings.dat` dosyasına kaydedilir; Flipper yeniden başlatılsa bile korunur.

---

## 🎮 Varsayılan Tuş Kombinasyonları (Preset: Terminal)

| Tuş | Kısa Basış | Uzun Basış (Hold) |
| :--- | :--- | :--- |
| **YUKARI (▲)** | Komut Geçmişinde Yukarı (`Up Arrow`) | Hızlı Yukarı |
| **AŞAĞI (▼)** | Komut Geçmişinde Aşağı (`Down Arrow`) | Hızlı Aşağı |
| **SOL (◀)** | Önceki Sekmeye Geç (`Tab -` / `Alt+X`) | — |
| **SAĞ (▶)** | Sonraki Sekmeye Geç (`Tab +` / `Alt+X`) | — |
| **OK (Orta)** | Komutu Çalıştır (`Return / Enter`) | — |
| **GERİ (◀)** | Komutu Durdur (`Ctrl+C` / SIGINT) | **🎛️ Hub Menüsünü Aç** |

---

## 🚀 Kurulum

### 1. `ufbt` ile Tek Adımda Derle ve Yükle (Geliştiriciler İçin)
Flipper Zero'yu USB ile bilgisayarına bağla ve çalıştır:
```bash
cd terminal_remote
ufbt launch
```
*(Uygulama otomatik olarak derlenir, Flipper'ın içine yüklenir ve ekranında açılır).*

### 2. Hazır `.fap` Dosyası ile SD Karta Yükleme
1. Proje içindeki derlenmiş hazır dosyayı al: `dist/terminal_remote.fap`
2. **qFlipper** uygulamasını aç -> **File Manager** sekmesine gir.
3. `SD Card/apps/Bluetooth/` klasörüne `terminal_remote.fap` dosyasını sürükleyip bırak.
4. Flipper üzerinden: **Applications > Bluetooth > Terminal Remote** çalıştır.

---

## 📶 Bluetooth ile Eşleştirme

1. Flipper Zero'da **Terminal Remote** uygulamasını aç.
2. Ekranda **BAGLANTI BEKLENIYOR** yazısı görünecektir.
3. Bilgisayarının veya telefonunun Bluetooth ayarlarına gir.
4. **Control <FlipperAdı>** cihazını seç ve Eşle (Pair) butonuna tıkla.
5. Flipper hafifçe titreyecek, mavi ışık yanacak ve otomatik olarak canlı **Terminal Konsolu** açılacaktır!

---

## 📂 Dosya Yapısı

```
terminal_remote/
├── terminal_remote.c        # Ana uygulama kaynak kodu (BLE HID, Scenes, Hub, GUI)
├── terminal_remote.png      # Flipper Menü İkonu (10x10 px)
├── application.fam          # FAP Manifest (v1.1)
├── dist/
│   └── terminal_remote.fap  # Doğrudan yüklenebilir derlenmiş ikili dosya
├── .gitignore               # Git dışlama kuralları
├── LICENSE                  # MIT Lisansı
└── README.md                # GitHub vitrin dokümantasyonu
```

---

## 📄 Lisans

Bu proje **MIT Lisansı** altında lisanslanmıştır. Detaylar için [LICENSE](LICENSE) dosyasına göz atabilirsiniz.
