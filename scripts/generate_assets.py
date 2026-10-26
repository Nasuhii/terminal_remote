#!/usr/bin/env python3
"""
Generates high-resolution diagrams for MTA Yapay Zeka Merkezi README standards:
1. assets/demo.png - Side-by-side UI simulation of Flipper Zero screens
2. assets/architecture.png - System architecture block diagram
"""

import os
from PIL import Image, ImageDraw, ImageFont

def get_font(size=14, bold=False):
    try:
        if bold:
            return ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", size)
        return ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", size)
    except Exception:
        return ImageFont.load_default()

def generate_demo_image():
    W, H = 1000, 360
    img = Image.new("RGB", (W, H), color="#0f172a")
    draw = ImageDraw.Draw(img)

    title_font = get_font(18, bold=True)
    flipper_font_bold = get_font(12, bold=True)
    flipper_font = get_font(11, bold=False)
    small_font = get_font(10, bold=False)

    # Title
    draw.text((W // 2, 25), "FLIPPER ZERO — TERMINAL REMOTE (UI FLOW)", fill="#f8fafc", font=title_font, anchor="mm")

    screens = [
        ("1. Eşleşme Arama (Waiting)", [
            ("[ BLUETOOTH BAGLANTISI ]", True),
            ("", False),
            ("      ᛒ  BAGLANTI BEKLENIYOR", True),
            ("         Cihaz: Flipper BT", False),
            ("         PC/Telefondan Baglan", False),
            ("", False),
            ("OK: Konsola Gec      BK: Cikis", False)
        ]),
        ("2. Terminal Konsolu (Tab 1)", [
            ("● BLE  [MAIN] [WORK] [LOGS] #42", True),
            ("> pts/1 [main] ~          [Alt+1]", False),
            ("----------------------------------", False),
            ("nasuhi@main:~$", True),
            ("> ls -la █", False),
            ("----------------------------------", False),
            ("<:Tab-  ^:Up  OK:Ret  v:Dn  >:Tab+", False)
        ]),
        ("3. Terminal Hub Menüsü", [
            ("[ TERMINAL HUB ]", True),
            (">_  Terminal Console", True),
            ("BT  Bluetooth: Bagli [OK]", False),
            ("#P  Preset: Terminal", False),
            (">>  Quick Macros", False),
            ("-o- Settings & Remap", False),
            ("▲▼: Gezin         OK: Sec", False)
        ])
    ]

    card_w = 280
    card_h = 220
    start_y = 65

    for idx, (label, lines) in enumerate(screens):
        start_x = 40 + idx * (card_w + 35)

        # Device outer shell (Flipper Orange Style)
        draw.rounded_rectangle([start_x - 10, start_y - 8, start_x + card_w + 10, start_y + card_h + 38], radius=16, fill="#1e293b", outline="#ff8200", width=2)
        
        # Subtitle
        draw.text((start_x + card_w // 2, start_y + card_h + 18), label, fill="#94a3b8", font=small_font, anchor="mm")

        # Flipper Screen (Backlight Orange/Amber LCD effect)
        screen_box = [start_x, start_y, start_x + card_w, start_y + card_h]
        draw.rounded_rectangle(screen_box, radius=8, fill="#ff8200", outline="#000000", width=2)

        # Lines inside LCD
        cur_y = start_y + 12
        for text, is_bold in lines:
            if text == "----------------------------------":
                draw.line([start_x + 8, cur_y + 6, start_x + card_w - 8, cur_y + 6], fill="#000000", width=1)
                cur_y += 14
            elif text.startswith("["):
                # Header Inverted Box
                draw.rectangle([start_x + 4, cur_y - 2, start_x + card_w - 4, cur_y + 16], fill="#000000")
                draw.text((start_x + card_w // 2, cur_y + 7), text, fill="#ff8200", font=flipper_font_bold, anchor="mm")
                cur_y += 22
            else:
                fnt = flipper_font_bold if is_bold else flipper_font
                draw.text((start_x + 12, cur_y), text, fill="#000000", font=fnt)
                cur_y += 20

    os.makedirs("/home/nasuhi/terminal_remote/assets", exist_ok=True)
    img.save("/home/nasuhi/terminal_remote/assets/demo.png")
    print("✓ assets/demo.png generated.")

def generate_architecture_image():
    W, H = 1000, 480
    img = Image.new("RGB", (W, H), color="#0b1329")
    draw = ImageDraw.Draw(img)

    title_font = get_font(20, bold=True)
    box_font_bold = get_font(13, bold=True)
    box_font = get_font(11, bold=False)
    small_font = get_font(10, bold=False)

    # Title
    draw.text((W // 2, 28), "SİSTEM VE PROTOKOL MİMARİSİ (SYSTEM ARCHITECTURE)", fill="#38bdf8", font=title_font, anchor="mm")

    # 1. Left Box: Flipper Zero Device
    draw.rounded_rectangle([40, 70, 360, 430], radius=12, fill="#1e293b", outline="#ff8200", width=2)
    draw.text((200, 95), "FLIPPER ZERO (EMBEDDED FAP)", fill="#ff8200", font=box_font_bold, anchor="mm")
    draw.line([60, 115, 340, 115], fill="#475569", width=1)

    flipper_blocks = [
        ("Kullanıcı Arayüzü & Görünüm Katmanı", "128x64 Monokrom LCD (ViewPort & Canvas)"),
        ("Sahne Yöneticisi (Scene State Machine)", "Connection / Controller / Menu / Settings"),
        ("Girdi Olay Kuyruğu (FuriMessageQueue)", "Kısa/Uzun basış algılama, Haptik titreşim"),
        ("BLE HID Keyboard Profili", "Standard BLE HID Driver, FuriHal Bluetooth"),
        ("SD Kart Kalıcılık Katmanı", "apps_data/terminal_remote/settings.dat")
    ]
    by = 135
    for title, desc in flipper_blocks:
        draw.rounded_rectangle([60, by, 340, by + 46], radius=6, fill="#0f172a", outline="#334155", width=1)
        draw.text((70, by + 12), title, fill="#f8fafc", font=box_font_bold)
        draw.text((70, by + 28), desc, fill="#94a3b8", font=small_font)
        by += 54

    # 2. Center: Wireless Protocol (BLE 5.0)
    draw.rounded_rectangle([390, 200, 610, 300], radius=10, fill="#0f172a", outline="#38bdf8", width=2)
    draw.text((500, 225), "KABLOSUZ İLETİŞİM", fill="#38bdf8", font=box_font_bold, anchor="mm")
    draw.text((500, 245), "Bluetooth 5.0 LE HID", fill="#f8fafc", font=box_font, anchor="mm")
    draw.text((500, 265), "GATT Standart Klavye Raporu", fill="#94a3b8", font=small_font, anchor="mm")
    draw.text((500, 282), "Sıfır Ek Yazılım / Doğrudan Donanım", fill="#4ade80", font=small_font, anchor="mm")

    # Connecting Arrows
    draw.line([360, 250, 390, 250], fill="#38bdf8", width=3)
    draw.line([610, 250, 640, 250], fill="#38bdf8", width=3)

    # 3. Right Box: Host Computer
    draw.rounded_rectangle([640, 70, 960, 430], radius=12, fill="#1e293b", outline="#38bdf8", width=2)
    draw.text((800, 95), "HEDEF BİLGİSAYAR (HOST SYSTEM)", fill="#38bdf8", font=box_font_bold, anchor="mm")
    draw.line([660, 115, 940, 115], fill="#475569", width=1)

    host_blocks = [
        ("Yerel İşletim Sistemi Sürücüsü", "Linux / macOS / Windows BLE HID Stack"),
        ("Çekirdek Girdi Alt Sistemi", "/dev/input/event* (evdev) / HID Keyboard"),
        ("Terminal Emülatörü & Kabuk", "tmux, Alacritty, GNOME Terminal, iTerm2"),
        ("Sekme Yönetim Katmanı", "Alt+1, Alt+2, Alt+3 ile 3 Farklı Çalışma Alanı"),
        ("Süreç Kontrol Makroları", "SIGINT (^C), EOF (^D), Clear (^L), TAB Tamamlama")
    ]
    by = 135
    for title, desc in host_blocks:
        draw.rounded_rectangle([660, by, 940, by + 46], radius=6, fill="#0f172a", outline="#334155", width=1)
        draw.text((670, by + 12), title, fill="#f8fafc", font=box_font_bold)
        draw.text((670, by + 28), desc, fill="#94a3b8", font=small_font)
        by += 54

    img.save("/home/nasuhi/terminal_remote/assets/architecture.png")
    print("✓ assets/architecture.png generated.")

if __name__ == "__main__":
    generate_demo_image()
    generate_architecture_image()
