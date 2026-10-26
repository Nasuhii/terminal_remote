#!/usr/bin/env python3
"""
Generates high-resolution diagrams for MTA AI Center README standards:
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
    W, H = 1080, 360
    img = Image.new("RGB", (W, H), color="#0f172a")
    draw = ImageDraw.Draw(img)

    title_font = get_font(18, bold=True)
    flipper_font_bold = get_font(11, bold=True)
    flipper_font = get_font(10, bold=False)
    small_font = get_font(10, bold=False)

    # Title
    draw.text((W // 2, 22), "FLIPPER ZERO — TERMINAL REMOTE PRO (UI & SCENES)", fill="#f8fafc", font=title_font, anchor="mm")

    screens = [
        ("1. BLE Host Pairing", [
            ("[ BLUETOOTH PAIRING ]", True),
            ("", False),
            ("      ᛒ  WAITING FOR HOST", True),
            ("         Device: Flipper BT", False),
            ("         Pair from PC / Phone", False),
            ("", False),
            ("OK: To Console       BK: Exit", False)
        ]),
        ("2. CyberSecurity Role", [
            ("● BLE [RECON][EXPLO][REVSH] #18", True),
            ("> nmap -sC -sV 10.10.10.x [Alt+1]", False),
            ("----------------------------------", False),
            ("root@kali:~/recon#", True),
            ("> rustscan -a target █", False),
            ("----------------------------------", False),
            ("<:Tab-  ^:Up  OK:Ret  v:Dn  >:Tab+", False)
        ]),
        ("3. D-Pad Mouse / Presenter", [
            ("[ D-PAD MOUSE MODE ]", True),
            ("MODE: AIR-MOUSE     STEP: 14px", False),
            ("          ( + ) Crosshair", True),
            ("          [CLICK!] Ready", False),
            ("----------------------------------", False),
            ("OK: Left Click | OK+: Double", False),
            ("BK: Back to Menu", False)
        ]),
        ("4. Terminal Hub & Panic", [
            ("[ TERMINAL HUB ]", True),
            (">_  Terminal Console", False),
            ("(!) PANIC: Lock (Super+L)", True),
            ("::  Role: CyberSecurity", False),
            ("><  D-Pad Mouse / Presenter", False),
            (">>  Quick Macros & Payloads", False),
            ("▲▼: Navigate      OK: Select", False)
        ])
    ]

    card_w = 230
    card_h = 220
    start_y = 58

    for idx, (label, lines) in enumerate(screens):
        start_x = 25 + idx * (card_w + 28)

        # Device outer shell (Flipper Orange Style)
        draw.rounded_rectangle([start_x - 8, start_y - 8, start_x + card_w + 8, start_y + card_h + 38], radius=14, fill="#1e293b", outline="#ff8200", width=2)
        
        # Subtitle
        draw.text((start_x + card_w // 2, start_y + card_h + 18), label, fill="#94a3b8", font=small_font, anchor="mm")

        # Flipper Screen (Backlight Orange/Amber LCD effect)
        screen_box = [start_x, start_y, start_x + card_w, start_y + card_h]
        draw.rounded_rectangle(screen_box, radius=8, fill="#ff8200", outline="#000000", width=2)

        # Lines inside LCD
        cur_y = start_y + 10
        for text, is_bold in lines:
            if text == "----------------------------------":
                draw.line([start_x + 6, cur_y + 6, start_x + card_w - 6, cur_y + 6], fill="#000000", width=1)
                cur_y += 14
            elif text.startswith("["):
                # Header Inverted Box
                draw.rectangle([start_x + 3, cur_y - 2, start_x + card_w - 3, cur_y + 16], fill="#000000")
                draw.text((start_x + card_w // 2, cur_y + 7), text, fill="#ff8200", font=flipper_font_bold, anchor="mm")
                cur_y += 22
            else:
                fnt = flipper_font_bold if is_bold else flipper_font
                draw.text((start_x + 8, cur_y), text, fill="#000000", font=fnt)
                cur_y += 19

    os.makedirs("/home/nasuhi/terminal_remote/assets", exist_ok=True)
    img.save("/home/nasuhi/terminal_remote/assets/demo.png")
    print("✓ assets/demo.png updated.")

def generate_architecture_image():
    W, H = 1000, 480
    img = Image.new("RGB", (W, H), color="#0b1329")
    draw = ImageDraw.Draw(img)

    title_font = get_font(20, bold=True)
    box_font_bold = get_font(13, bold=True)
    box_font = get_font(11, bold=False)
    small_font = get_font(10, bold=False)

    # Title
    draw.text((W // 2, 28), "SYSTEM & PROTOCOL ARCHITECTURE", fill="#38bdf8", font=title_font, anchor="mm")

    # 1. Left Box: Flipper Zero Device
    draw.rounded_rectangle([40, 70, 360, 435], radius=12, fill="#1e293b", outline="#ff8200", width=2)
    draw.text((200, 95), "FLIPPER ZERO (EMBEDDED FAP)", fill="#ff8200", font=box_font_bold, anchor="mm")
    draw.line([60, 115, 340, 115], fill="#475569", width=1)

    flipper_blocks = [
        ("Advanced Scene FSM", "Pairing / Console / Mouse / Roles / Hub / Macros"),
        ("Workflow Role Profiles", "Pentest / AI & Data / DevOps / Standard Dev"),
        ("Driverless BLE HID Stack", "Standard Keyboard + D-Pad Mouse Emulation"),
        ("Emergency Panic Engine", "One-Touch Screen Lock (Super+L) + RGB Strobe"),
        ("Macro & Payload Injector", "One-click ip a, git status, docker ps, nc listener")
    ]
    by = 135
    for title, desc in flipper_blocks:
        draw.rounded_rectangle([60, by, 340, by + 46], radius=6, fill="#0f172a", outline="#334155", width=1)
        draw.text((70, by + 12), title, fill="#f8fafc", font=box_font_bold)
        draw.text((70, by + 28), desc, fill="#94a3b8", font=small_font)
        by += 54

    # 2. Center: Wireless Protocol (BLE 5.0)
    draw.rounded_rectangle([390, 195, 610, 310], radius=10, fill="#0f172a", outline="#38bdf8", width=2)
    draw.text((500, 220), "WIRELESS PROTOCOL", fill="#38bdf8", font=box_font_bold, anchor="mm")
    draw.text((500, 240), "Bluetooth 5.0 LE HID", fill="#f8fafc", font=box_font, anchor="mm")
    draw.text((500, 260), "Standard Keyboard + Mouse Profile", fill="#94a3b8", font=small_font, anchor="mm")
    draw.text((500, 280), "Zero Host Software Required", fill="#4ade80", font=small_font, anchor="mm")
    draw.text((500, 296), "Avg. Latency: 8.4 ms", fill="#38bdf8", font=small_font, anchor="mm")

    # Connecting Arrows
    draw.line([360, 252, 390, 252], fill="#38bdf8", width=3)
    draw.line([610, 252, 640, 252], fill="#38bdf8", width=3)

    # 3. Right Box: Host Computer
    draw.rounded_rectangle([640, 70, 960, 435], radius=12, fill="#1e293b", outline="#38bdf8", width=2)
    draw.text((800, 95), "TARGET HOST SYSTEM", fill="#38bdf8", font=box_font_bold, anchor="mm")
    draw.line([660, 115, 940, 115], fill="#475569", width=1)

    host_blocks = [
        ("Native OS HID Stack", "Linux / macOS / Windows / Android / iOS"),
        ("Kernel Input Subsystem", "/dev/input (evdev) Standard HID Devices"),
        ("Multi-Tab Workspace", "Instant Tab Switching via Alt+1, Alt+2, Alt+3"),
        ("Terminal Emulator & Shell", "tmux, Alacritty, GNOME Terminal, Kitty, iTerm2"),
        ("Process & Session Control", "SIGINT (^C), EOF (^D), Screen Lock, Payloads")
    ]
    by = 135
    for title, desc in host_blocks:
        draw.rounded_rectangle([660, by, 940, by + 46], radius=6, fill="#0f172a", outline="#334155", width=1)
        draw.text((670, by + 12), title, fill="#f8fafc", font=box_font_bold)
        draw.text((670, by + 28), desc, fill="#94a3b8", font=small_font)
        by += 54

    img.save("/home/nasuhi/terminal_remote/assets/architecture.png")
    print("✓ assets/architecture.png updated.")

if __name__ == "__main__":
    generate_demo_image()
    generate_architecture_image()
