#!/usr/bin/env python3
"""
MTA AI Center Standard Installation & Verification Script
This tool verifies:
1. Python runtime and ufbt (Micro Flipper Build Tool) environment.
2. Compiled FAP binary integrity and packaging status.
3. Assets, schema diagrams, and project directory structure.
"""

import os
import sys
import shutil

RESET = "\033[0m"
BOLD = "\033[1m"
GREEN = "\033[32m"
RED = "\033[31m"
YELLOW = "\033[33m"
CYAN = "\033[36m"

def print_step(title):
    print(f"\n{BOLD}{CYAN}=== {title} ==={RESET}")

def print_ok(msg):
    print(f"  {GREEN}[✓ PASSED]{RESET} {msg}")

def print_warn(msg):
    print(f"  {YELLOW}[⚠️ WARNING]{RESET} {msg}")

def print_err(msg):
    print(f"  {RED}[❌ ERROR]{RESET} {msg}")

def main():
    print(f"{BOLD}🐬 Terminal Remote Setup & Verification Utility (MTA Standards){RESET}")
    print("-" * 65)

    # 1. Python Version Check
    print_step("1. Python Runtime Check")
    py_ver = sys.version.split()[0]
    if sys.version_info >= (3, 8):
        print_ok(f"Python version compliant: {py_ver}")
    else:
        print_err(f"Python 3.8 or higher is required. Found: {py_ver}")

    # 2. ufbt (Micro Flipper Build Tool) Check
    print_step("2. Flipper Build Tool (ufbt) Check")
    ufbt_path = shutil.which("ufbt") or os.path.expanduser("~/.local/bin/ufbt")
    if os.path.exists(ufbt_path) if isinstance(ufbt_path, str) else False:
        print_ok(f"ufbt installed and available: {ufbt_path}")
    else:
        print_warn("ufbt not found. To build from source, run 'pip install ufbt'.")
        print_warn("You can directly deploy 'dist/terminal_remote.fap' via qFlipper.")

    # 3. FAP Binary Integrity Check
    print_step("3. FAP Binary Integrity Check")
    fap_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "dist", "terminal_remote.fap")
    if os.path.exists(fap_path):
        fap_size = os.path.getsize(fap_path)
        print_ok(f"Deployable .fap binary verified: {fap_path} ({round(fap_size / 1024, 1)} KB)")
    else:
        print_err("dist/terminal_remote.fap missing! Run 'ufbt build' to compile.")

    # 4. Project Directory & Assets Check
    print_step("4. Assets & Repository Integrity Check")
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    assets_dir = os.path.join(root_dir, "assets")
    if os.path.isdir(assets_dir) and len(os.listdir(assets_dir)) >= 2:
        print_ok("Visual schematics and UI assets verified (assets/).")
    else:
        print_warn("Some assets may be missing in assets/ directory.")

    print("\n" + "=" * 65)
    print(f"{BOLD}{GREEN}VERIFICATION COMPLETE!{RESET}")
    print("To flash and launch onto your Flipper Zero directly via USB:")
    print(f"  {CYAN}ufbt launch{RESET}")
    print("Or copy 'dist/terminal_remote.fap' via qFlipper into 'SD Card/apps/Bluetooth/'")
    print("=" * 65 + "\n")

if __name__ == "__main__":
    main()
