#!/usr/bin/env python3
"""
TinkerTech S4S ESP32-S3 Flash Utility
Flashes the bootloader, partition table, and firmware to an ESP32-S3 device.
"""

import sys
import os
import subprocess
import argparse
import time

def find_serial_port():
    try:
        import serial.tools.list_ports
        ports = list(serial.tools.list_ports.comports())
        for p in ports:
            # Common ESP32 USB/UART identifiers
            desc = p.description.lower()
            if any(k in desc for k in ["ch340", "ch9102", "cp210", "usb serial", "esp32"]):
                return p.device
        if ports:
            return ports[0].device
    except Exception:
        pass
    return None

def main():
    parser = argparse.ArgumentParser(description="Flash TinkerTech S4S ESP32-S3 Robot Firmware")
    parser.add_argument("--port", "-p", help="Serial port (e.g. COM15, /dev/ttyUSB0)")
    parser.add_argument("--baud", "-b", default="460800", help="Baud rate (default: 460800)")
    parser.add_argument("--retries", "-r", type=int, default=1, help="Number of connection attempts")
    args = parser.parse_args()

    port = args.port or find_serial_port()
    if not port:
        print("[Error] No serial port detected. Please specify via --port <PORT>")
        sys.exit(1)

    here = os.path.dirname(os.path.abspath(__file__))
    bootloader = os.path.join(here, "bootloader.bin")
    partitions = os.path.join(here, "partitions.bin")
    firmware = os.path.join(here, "firmware.bin")

    for f in [bootloader, partitions, firmware]:
        if not os.path.exists(f):
            print(f"[Error] Required binary missing: {f}")
            sys.exit(1)

    print("==================================================")
    print("  TinkerTech ESP32-S3 Firmware Flasher")
    print(f"  Target Port : {port}")
    print(f"  Baud Rate   : {args.baud}")
    print("==================================================")

    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", "esp32s3",
        "--port", port,
        "--baud", args.baud,
        "--before", "default-reset",
        "--after", "hard-reset",
        "write-flash", "-z",
        "--flash-mode", "dio",
        "--flash-freq", "80m",
        "--flash-size", "8MB",
        "0x0000", bootloader,
        "0x8000", partitions,
        "0x10000", firmware
    ]

    print("\nExecuting:", " ".join(cmd))
    for attempt in range(1, args.retries + 1):
        if args.retries > 1:
            print(f"\n--- Attempt {attempt} of {args.retries} ---")
        res = subprocess.run(cmd)
        if res.returncode == 0:
            print("\n[SUCCESS] Firmware flashed successfully! Robot is rebooting...")
            sys.exit(0)
        if attempt < args.retries:
            print("[Info] Retrying in 2 seconds (Hold BOOT button if needed)...")
            time.sleep(2)

    print(f"\n[FAILED] Flash failed with exit code {res.returncode}")
    print("\nTroubleshooting tips:")
    print("1. Power Switch: Make sure the robot's power switch is ON.")
    print("2. Boot Mode: Press and hold the BOOT (IO0) button, press & release RESET (EN), then release BOOT.")
    print("3. Port: Verify COM port is not in use by another application.")
    sys.exit(res.returncode)

if __name__ == "__main__":
    main()
