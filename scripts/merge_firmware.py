#!/usr/bin/env python3
"""Package PlatformIO ESP32-C3 build outputs into OTA + merged factory binaries."""

import argparse
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys


def read_fw_version(config_path: str) -> str:
    with open(config_path, "r", encoding="utf-8") as f:
        match = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', f.read())
    if not match:
        raise RuntimeError(f"Could not find FW_VERSION in {config_path}")
    return match.group(1).strip()


def find_file(candidates: list[str], label: str) -> str:
    for pattern in candidates:
        matches = sorted(glob.glob(pattern))
        if matches:
            return matches[0]
    raise FileNotFoundError(f"Could not locate {label}. Checked: {candidates}")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def run_esptool_merge(
    pio_home: str,
    out_path: str,
    bootloader: str,
    partitions: str,
    boot_app0: str,
    app_bin: str,
) -> None:
    esptool_candidates = [
        os.path.join(pio_home, "packages", "tool-esptoolpy", "esptool.py"),
    ]
    esptool_script = next((p for p in esptool_candidates if os.path.isfile(p)), None)

    if esptool_script:
        cmd = [sys.executable, esptool_script]
    else:
        cmd = [sys.executable, "-m", "esptool"]

    cmd.extend(
        [
            "--chip",
            "esp32c3",
            "merge_bin",
            "-o",
            out_path,
            "--flash_mode",
            "dio",
            "--flash_freq",
            "80m",
            "--flash_size",
            "4MB",
            "0x0",
            bootloader,
            "0x8000",
            partitions,
            "0xe000",
            boot_app0,
            "0x10000",
            app_bin,
        ]
    )

    print("Running:", " ".join(cmd))
    subprocess.run(cmd, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build versioned OTA and merged factory firmware binaries."
    )
    parser.add_argument(
        "--project-dir",
        default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        help="Path to the PlatformIO project root.",
    )
    parser.add_argument(
        "--env",
        default="esp32-c3-supermini",
        help="PlatformIO build environment name.",
    )
    parser.add_argument(
        "--version",
        default=None,
        help="Version string (without leading 'v'). Defaults to FW_VERSION in include/config.h.",
    )
    parser.add_argument(
        "--out-dir",
        default="dist",
        help="Output directory for packaged firmware artifacts.",
    )
    args = parser.parse_args()

    project_dir = os.path.abspath(args.project_dir)
    build_dir = os.path.join(project_dir, ".pio", "build", args.env)
    pio_home = os.environ.get("PLATFORMIO_CORE_DIR") or os.path.expanduser("~/.platformio")

    raw_version = args.version or read_fw_version(
        os.path.join(project_dir, "include", "config.h")
    )
    clean_version = raw_version.lstrip("v")
    safe_version = re.sub(r"[^0-9A-Za-z._-]", "_", clean_version)

    bootloader = find_file([os.path.join(build_dir, "bootloader.bin")], "bootloader.bin")
    partitions = find_file([os.path.join(build_dir, "partitions.bin")], "partitions.bin")
    boot_app0 = find_file(
        [
            os.path.join(
                pio_home,
                "packages",
                "framework-arduinoespressif32*",
                "tools",
                "partitions",
                "boot_app0.bin",
            )
        ],
        "boot_app0.bin",
    )
    app_bin = find_file(
        [
            os.path.join(build_dir, "firmware_v*.bin"),
            os.path.join(build_dir, "firmware.bin"),
        ],
        "compiled firmware binary",
    )

    out_dir = os.path.abspath(args.out_dir)
    os.makedirs(out_dir, exist_ok=True)

    ota_name = f"flightclock-v{safe_version}-ota.bin"
    factory_name = f"flightclock-v{safe_version}-factory.bin"
    ota_path = os.path.join(out_dir, ota_name)
    factory_path = os.path.join(out_dir, factory_name)

    shutil.copy2(app_bin, ota_path)
    run_esptool_merge(
        pio_home=pio_home,
        out_path=factory_path,
        bootloader=bootloader,
        partitions=partitions,
        boot_app0=boot_app0,
        app_bin=app_bin,
    )

    manifest = {
        "name": "ESP32-C3 Flight Clock",
        "version": f"v{safe_version}",
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": "ESP32-C3",
                "parts": [{"path": factory_name, "offset": 0}],
            }
        ],
    }
    manifest_path = os.path.join(out_dir, "manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    checksum_lines = [
        f"{sha256_file(factory_path)}  {factory_name}",
        f"{sha256_file(ota_path)}  {ota_name}",
    ]
    checksum_path = os.path.join(out_dir, "SHA256SUMS.txt")
    with open(checksum_path, "w", encoding="utf-8") as f:
        f.write("\n".join(checksum_lines) + "\n")

    print("Packaged firmware artifacts:")
    for line in checksum_lines:
        print(" ", line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
