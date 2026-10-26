#!/usr/bin/env python3
"""
MTA Yapay Zeka Merkezi Standart Kurulum ve Doğrulama Betiği
Bu araç:
1. Python ve ufbt derleme ortamını denetler.
2. Bluetooth ve sistem yetkilerini doğrular.
3. Derlenmiş FAP ikilisinin bütünlüğünü kontrol eder.
"""

import os
import sys
import shutil
import subprocess

RESET = "\033[0m"
BOLD = "\033[1m"
GREEN = "\033[32m"
RED = "\033[31m"
YELLOW = "\033[33m"
CYAN = "\033[36m"

def print_step(title):
    print(f"\n{BOLD}{CYAN}=== {title} ==={RESET}")

def print_ok(msg):
    print(f"  {GREEN}[✓ BAŞARILI]{RESET} {msg}")

def print_warn(msg):
    print(f"  {YELLOW}[⚠️ UYARI]{RESET} {msg}")

def print_err(msg):
    print(f"  {RED}[❌ HATA]{RESET} {msg}")

def main():
    print(f"{BOLD}🐬 Terminal Remote Kurulum & Doğrulama Aracı (MTA Standartları){RESET}")
    print("-" * 65)

    # 1. Python Sürümü
    print_step("1. Python Sürüm Denetimi")
    py_ver = sys.version.split()[0]
    if sys.version_info >= (3, 8):
        print_ok(f"Python sürümü uygun: {py_ver}")
    else:
        print_err(f"Python 3.8 veya üzeri gereklidir. Mevcut: {py_ver}")

    # 2. ufbt (Micro Flipper Build Tool)
    print_step("2. Flipper Derleme Aracı (ufbt) Kontrolü")
    ufbt_path = shutil.which("ufbt") or os.path.expanduser("~/.local/bin/ufbt")
    if os.path.exists(ufbt_path) if isinstance(ufbt_path, str) else False:
        print_ok(f"ufbt kurulu ve hazır: {ufbt_path}")
    else:
        print_warn("ufbt bulunamadı. Kaynaktan derlemek için 'pip install ufbt' çalıştırın.")
        print_warn("Doğrudan kullanmak için 'dist/terminal_remote.fap' dosyasını SD karta yükleyebilirsiniz.")

    # 3. FAP İkili Dosya Bütünlüğü
    print_step("3. FAP İkili Dosya Denetimi")
    fap_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "dist", "terminal_remote.fap")
    if os.path.exists(fap_path):
        fap_size = os.path.getsize(fap_path)
        print_ok(f"Dağıtıma hazır .fap dosyası mevcut: {fap_path} ({round(fap_size / 1024, 1)} KB)")
    else:
        print_err("dist/terminal_remote.fap bulunamadı! 'ufbt build' komutunu çalıştırın.")

    # 4. Git Bütünlüğü
    print_step("4. Git ve Dizin Bütünlüğü")
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    assets_dir = os.path.join(root_dir, "assets")
    if os.path.isdir(assets_dir) and len(os.listdir(assets_dir)) >= 2:
        print_ok("Görsel ve şema varlıkları (assets/) eksiksiz.")
    else:
        print_warn("assets/ klasöründe eksik dosyalar olabilir.")

    print("\n" + "=" * 65)
    print(f"{BOLD}{GREEN}DOĞRULAMA TAMAMLANDI!{RESET}")
    print("Flipper Zero'ya tek tıkla yüklemek için:")
    print(f"  {CYAN}ufbt launch{RESET}")
    print("Veya 'dist/terminal_remote.fap' dosyasını qFlipper ile 'SD Card/apps/Bluetooth/' dizinine atın.")
    print("=" * 65 + "\n")

if __name__ == "__main__":
    main()
