# Firmware Flashing & Build Guide

This guide details two methods for deploying firmware to the TinkerTech S4S ESP32-S3 robot:
1. **Flashing Pre-compiled Binaries** (Fastest, requires no build tools)
2. **Compiling from Source with PlatformIO** (For developers modifying C++ source code)

---

## 1. Quick Flashing via Pre-compiled Binaries

The `binaries/` directory includes tested, pre-compiled binaries ready for flashing.

### Prerequisites
- Python 3.8+ with `esptool`:
  ```bash
  pip install esptool pyserial
  ```
- USB-C cable connecting your computer to the ESP32-S3 programming port.

### Option A: One-Click Windows Batch Script
Simply double-click:
```
binaries\flash_firmware.bat
```

### Option B: Cross-Platform Python Flasher
Run the flasher script directly from any terminal (Windows / macOS / Linux):
```bash
python binaries/flash_firmware.py --port <YOUR_SERIAL_PORT>
```
*(If `--port` is omitted, the script automatically scans for connected CH340, CH9102, or ESP32 USB-UART bridges).*

### Option C: Direct `esptool` Command
You can invoke `esptool` directly using the memory map offsets below:
```bash
esptool.py --chip esp32s3 --port <PORT> --baud 1152000 \
    --before default_reset --after hard_reset write_flash -z \
    --flash_mode dio --flash_freq 80m --flash_size 8MB \
    0x0000 binaries/bootloader.bin \
    0x8000 binaries/partitions.bin \
    0x10000 binaries/firmware.bin
```

---

## 2. Flash Memory Map

| File | Flash Offset | Size | Purpose |
|---|---|---|---|
| `bootloader.bin` | `0x00000` | 15 KB | Second-stage bootloader initializes SPI flash, clocks, and cache |
| `partitions.bin` | `0x08000` | 3 KB | Partition table defining NVS, OTA data, and application slots |
| `firmware.bin`   | `0x10000` | ~816 KB | Core application image containing firmware logic |

---

## 3. Building from Source (PlatformIO)

### Environment Requirements
- [PlatformIO Core (CLI)](https://platformio.org/install/cli) or [VSCode PlatformIO Extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)
- Espressif 32 Platform v6.6.0+
- Arduino framework for ESP32

### Build Commands
From the `firmware/` directory:

```bash
# 1. Compile the firmware
pio run

# 2. Upload to connected board
pio run -t upload --upload-port <PORT>

# 3. Open interactive serial monitor (115200 baud)
pio device monitor -b 115200
```

### `platformio.ini` Configuration
The project is configured for the ESP32-S3-DevKitC-1 with 8MB Flash:
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32 @ 6.6.0
board = esp32-s3-devkitc-1
framework = arduino
board_build.flash_mode = dio
board_build.f_flash = 80000000L
board_upload.flash_size = 8MB
monitor_speed = 115200
upload_speed = 1152000
build_flags = 
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```
