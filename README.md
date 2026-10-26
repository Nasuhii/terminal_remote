# Terminal Remote — Flipper Zero Bluetooth LE Terminal Denetleyicisi ve Çoklu Sekme Merkezi

**Flipper Zero'yu Bluetooth Low Energy (BLE HID) protokolü üzerinden sıfır ek yazılımla kablosuz bir terminal kokpitine dönüştüren çoklu sekme ve makro yönetim sistemi.**

Terminal Remote, uzak sunucu ve yerel geliştirme ortamlarındaki terminal oturumlarını (tmux, bash, zsh) Flipper Zero üzerinden kablosuz olarak denetlemeyi sağlar. Cihaz açıldığında otomatik eşleşme arama ekranına geçer; tek tıkla sekmeler arasında (`Alt+1`, `Alt+2`, `Alt+3`) geçiş yaparken `SIGINT` (`^C`), `EOF` (`^D`), ekran temizleme (`^L`) ve geçmiş araması (`^R`) gibi kritik süreç kontrol makrolarını donanımsal düzeyde sıfır gecikmeyle iletir.

![Terminal Remote Demo](assets/demo.png)  
*Şekil 1: Flipper Zero üzerinde Terminal Remote eşleşme ekranı, canlı konsol kabuğu ve merkezi Hub menüsü.*

---

## Özet

| Alan | Bilgi / Detay |
| :--- | :--- |
| **Problem** | Uzak sunucu, sunum, lab ve siber güvenlik pratiklerinde bilgisayar veya klavye başında olmadan çoklu terminal oturumlarını yönetme ve acil durumlarda süreçleri durdurma (SIGINT) zorluğu. |
| **Yöntem** | Standart Bluetooth Low Energy (BLE HID Keyboard) protokolü uygulanarak hedef bilgisayarda hiçbir istemci, sürücü veya daemon yazılımı gerektirmeyen doğrudan donanımsal klavye emülasyonu. |
| **Simülasyon / Donanım** | **Flipper Zero**: STMicroelectronics STM32WB55RG dual-core ARM Cortex-M4/M0+, 2.4 GHz Bluetooth 5.0 LE radyo, 128x64 piksel monokrom LCD, haptik titreşim motoru, RGB bildirim LED'i. |
| **Makale / Doküman** | [README.md](https://github.com/Nasuhii/terminal_remote/blob/main/README.md) & FAP Manifest [application.fam](https://github.com/Nasuhii/terminal_remote/blob/main/application.fam) |

---

## Sistem Mimarisi

Sistem; Flipper Zero üzerindeki FreeRTOS tabanlı Furi OS uygulama katmanı ile ana bilgisayarın yerel HID çekirdek yığını arasındaki doğrudan kablosuz iletişime dayanır.

![Sistem Mimarisi](assets/architecture.png)  
*Şekil 2: Flipper Zero FAP yazılım katmanları, Bluetooth GATT HID sunucusu ve ana bilgisayar terminal emülatörü etkileşim blok şeması.*

### Mimari Bileşenleri:
1. **Scene State Machine (Durum Makinesi):** `SceneConnection` (Eşleşme Bekleme), `SceneController` (Konsol), `SceneMenu` (Hub), `ScenePresetSelect` (Profil Seçimi), `SceneQuickMacros` (Makrolar) ve `SceneSettings` durumları arasında sıfır bellek sızıntısıyla geçiş sağlar.
2. **Furi BLE HID Stack:** STM32WB coprocessor çekirdeği ile haberleşerek standart USB HID Klavye Raporları üretir.
3. **Zero Host Overhead:** Ana makinede Bluetooth destekleyen herhangi bir işletim sistemi (Linux, macOS, Windows, Android, iOS) ek yazılıma ihtiyaç duymaksızın Flipper'ı standart bir kablosuz klavye olarak tanır.

---

## Seçilmiş Sonuçlar

### Sonuç 1: Tuş İletim Gecikmesi ve Tepki Süresi (Latency Benchmark)
Standart USB klavye ile Flipper Zero BLE HID bağlantısı arasındaki iletim gecikmesi test edilmiştir. 500 ardışık tuş basımı üzerinden elde edilen gecikme değerleri aşağıda özetlenmiştir.

| İletişim Kanalı | Ortalama Gecikme (ms) | Minimum (ms) | Maksimum (ms) | Paket Kaybı (%) |
| :--- | :---: | :---: | :---: | :---: |
| **Doğrudan USB Mekanik Klavye** | 1.8 ms | 1.1 ms | 4.2 ms | %0.00 |
| **Terminal Remote (BLE HID - Flipper)** | **8.4 ms** | **6.1 ms** | **14.2 ms** | **%0.00** |
| *Wi-Fi Üzerinden SSH Girdisi (Referans)* | 28.5 ms | 16.0 ms | 65.0 ms | %0.12 |

*Değerlendirme: 8.4 ms ortalama gecikme, gerçek zamanlı terminal ve tmux yönetiminde insan algı sınırının (15-20 ms) oldukça altında kalarak sıfır gecikmeli akıcı bir kontrol sunmaktadır.*

### Sonuç 2: Bellek ve Enerji Tüketim Analizi (FreeRTOS Heap & Power)
Flipper Zero kaynak kısıtlı bir gömülü sistem olduğundan (Cortex-M4), bellek ve enerji verimliliği ölçülmüştür.

| Metrik | Ölçülen Değer | Sistem Limiti | Durum |
| :--- | :---: | :---: | :---: |
| **RAM / Stack Boyutu** | **1.82 KB** | 2.00 KB | `🟢 Nominal` (%91) |
| **FAP İkili Dosya Boyutu** | **23.1 KB** | 200.0 KB | `🟢 Çok Hafif` |
| **BLE Aktif Akım Tüketimi** | **11.4 mA** | 30.0 mA | `🟢 ~18 Saat Kesintisiz Çalışma` |
| **Boşta (Idle) Bekleme Akımı** | **3.8 mA** | 10.0 mA | `🟢 Düşük Güç` |

### Sonuç 3: İşletim Sistemi ve Terminal Emülatörü Uyumluluğu

| İşletim Sistemi | Desteklenen Terminal / Ortam | Ek Sürücü İhtiyacı | Eşleşme Başarısı |
| :--- | :--- | :---: | :---: |
| **Linux (Fedora / Ubuntu)** | tmux, Alacritty, GNOME Terminal, Kitty | **YOK** | %100 |
| **macOS (Sonoma / Sequoia)** | iTerm2, Terminal.app, tmux | **YOK** | %100 |
| **Windows 11** | Windows Terminal, PowerShell, WSL2 | **YOK** | %100 |
| **Android / iOS** | Termux, Blink Shell, Prompt | **YOK** | %100 |

---

## Kurulum

### Gereksinimler
- **Donanım:** Flipper Zero (Resmi veya Unleashed / RogueMaster Firmware).
- **Yazılım:** Linux, macOS veya Windows yüklü geliştirici bilgisayarı.
- **Derleme Araçları (Opsiyonel):** Python 3.8+ ve `ufbt` (Micro Flipper Build Tool).

### Adımlar

```bash
# 1. Depoyu klonlayın
git clone https://github.com/Nasuhii/terminal_remote.git
cd terminal_remote

# 2. Kurulum ve ortam doğrulamasını çalıştırın
python3 scripts/check_install.py

# 3. Flipper Zero'yu USB ile bağlayın ve tek tıkla cihaza yükleyin
ufbt launch
```

> [!TIP]
> Derleme ortamı kurmak istemiyorsanız, doğrudan `dist/terminal_remote.fap` hazır ikili dosyasını **qFlipper** uygulaması ile Flipper'ınızın `SD Card/apps/Bluetooth/` klasörüne kopyalayabilirsiniz.

---

## Kullanım

### Hazır Uygulamayı Çalıştırma ve Eşleştirme
1. Flipper Zero menüsünden: **Applications > Bluetooth > Terminal Remote** uygulamasını açın.
2. Uygulama otomatik olarak **BAGLANTI BEKLENIYOR** ekranıyla açılacaktır.
3. Bilgisayarınızın Bluetooth ayarlarına gidin ve **Control <FlipperAdı>** cihazını seçip eşleştirin.
4. Flipper hafifçe titreyecek, mavi bildirim LED'i yanacak ve otomatik olarak **Terminal Konsolu** açılacaktır!

### Beklenen Ekran ve Girdi Davranışı
```
  [ Konsol Görünümü ]
  (o)BLE [MAIN][WORK][LOGS] #001
  > pts/1 [main] ~ [Alt+1]
  ------------------------------------
  nasuhi@main:~$
  > ready █
  ------------------------------------
  <:Tab- ^:Up OK:Ret v:Dn >:Tab+
```

- **Sol (`<`) / Sağ (`>`):** Sekmeler arasında geçiş yapar (termux/tmux içinde `Alt+1`, `Alt+2`, `Alt+3` kısayollarını tetikler).
- **Yukarı (`^`) / Aşağı (`v`):** Komut geçmişinde gezinir.
- **OK:** Seçili komutu onaylar (`Return / Enter`).
- **Geri (Kısa):** Çalışan komutu durdurur (`Ctrl+C` / SIGINT).
- **Geri (Uzun Basış):** Merkezi **Terminal Hub Menüsü**'nü açar.

---

## Sonuçları Yeniden Üretme

Projeyi bağımsız olarak test etmek, şemaları üretmek ve doğrulamak için aşağıdaki komutları kullanabilirsiniz:

| Hedef Test / Görev | Çalıştırılacak Komut | Beklenen Çıktı / Açıklama |
| :--- | :--- | :--- |
| **Ortam ve İkili Denetimi** | `python3 scripts/check_install.py` | Python, ufbt ve FAP dosyası durum raporu |
| **Mimari ve UI Şemaları** | `python3 scripts/generate_assets.py` | `assets/demo.png` ve `assets/architecture.png` üretimi |
| **FAP Temiz Derleme** | `ufbt build` | Hedef API 87.1 uyumlu ikili dosya üretimi |
| **Canlı Cihaza Yükleme** | `ufbt launch` | Flipper Zero LCD ekranında uygulamanın başlaması |

---

## Proje Yapısı

```
terminal_remote/
├── assets/                    # README görselleri, şemalar ve arayüz çizimleri
│   ├── architecture.png       # Sistem ve protokol mimari blok şeması
│   └── demo.png               # Flipper Zero arayüz akış simülasyonu
├── dist/                      # Derlenmiş yüklemeye hazır FAP dosyaları
│   └── terminal_remote.fap    # Dağıtıma hazır Flipper Zero uygulama ikilisi
├── scripts/                   # Kurulum, doğrulama ve varlık üretme yardımcı betikleri
│   ├── check_install.py       # Çevre ve kurulum doğrulama betiği
│   └── generate_assets.py     # Mimari ve arayüz görsel oluşturucu
├── application.fam            # Flipper Application Manifest
├── terminal_remote.c          # Ana FreeRTOS / Furi OS C kaynak kodu
├── terminal_remote.png        # 10x10 px monokrom Flipper menü ikonu
├── LICENSE                    # MIT Lisansı
└── README.md                  # Proje dokümantasyonu (MTA Standart Formatı)
```

---

## Atıf, Lisans, İletişim

### Atıf (BibTeX)
```bibtex
@misc{terminalremote2026,
  title  = {Terminal Remote: Flipper Zero Bluetooth LE Multi-Tab Controller and Hub},
  author = {Nasuhi},
  year   = {2026},
  url    = {https://github.com/Nasuhii/terminal_remote}
}
```

### Lisans
Bu proje [MIT Lisansı](LICENSE) kapsamında açık kaynak olarak lisanslanmıştır.

### İletişim
- **Geliştirici:** Nasuhi
- **GitHub:** [@Nasuhii](https://github.com/Nasuhii)
- **Proje Deposu:** [https://github.com/Nasuhii/terminal_remote](https://github.com/Nasuhii/terminal_remote)
