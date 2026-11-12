# Hardware Ports, Serial Identification & Interface Reverse-Engineering

This document details the reverse-engineered hardware architecture, serial COM port behaviors, I2C bus assignments, network ports, and peripheral registers for the **TinkerTech / S4S ESP32-S3 STEM Robot**.

---

## 1. Serial COM Ports & USB-UART Identification

During hardware diagnostics, multiple COM ports may appear depending on your operating system and hardware revision. Here is how each port behaves:

### Port Types & Hardware Identifiers
```
+----------------------------+-----------------------+-----------------------------------------------------+
| Port Description           | USB VID / PID         | Purpose & Behavior                                  |
+----------------------------+-----------------------+-----------------------------------------------------+
| USB-SERIAL CH340 (COM15)   | VID: 1A86, PID: 7523  | External USB-to-UART bridge on standard board docks.|
|                            |                       | Used for flashing firmware and Serial monitor.      |
+----------------------------+-----------------------+-----------------------------------------------------+
| USB-Enhanced CH9102 (COM13)| VID: 1A86, PID: 55D4  | High-speed WCH USB-UART bridge on newer revisions.  |
|                            |                       | Supports ultra-fast flashing up to 2,000,000 baud.  |
+----------------------------+-----------------------+-----------------------------------------------------+
| USB Serial Device (COM11)  | VID: 303A, PID: 1001  | Native ESP32-S3 USB-JTAG / USB-CDC direct port.     |
|                            |                       | Active when connected directly to the ESP32-S3 USB. |
+----------------------------+-----------------------+-----------------------------------------------------+
```

### Flashing Parameters
- **Chip Architecture**: ESP32-S3 (`esp32s3`), Xtensa LX7 Dual-Core @ 240 MHz.
- **Upload Baud Rate**: `1,152,000` baud (stable and verified across all USB-UART chips).
- **Flash Configuration**: `DIO` mode, `80 MHz` clock, `8MB` flash size.
- **Console Baud Rate**: `115,200` baud (8 data bits, 1 stop bit, no parity).
- **USB CDC Mode**: The firmware is compiled with `-DARDUINO_USB_CDC_ON_BOOT=1` and `-DARDUINO_USB_MODE=1` so serial printing works through both external USB-UART chips and the native USB-C port.

---

## 2. Network Ports & Wireless Architecture

The firmware runs a non-blocking asynchronous network stack on ESP32-S3 Core 0:

### Port 8888 (Wi-Fi UDP Server)
- **Why UDP was chosen**:
  - Connectionless transport eliminates TCP three-way handshakes and head-of-line blocking.
  - Sub-millisecond command dispatch latency ($< 2\text{ ms}$).
  - Broadcast support allows client controllers to auto-discover the robot on the local network (`192.168.0.255:8888`).
- **Concurrent Dual-Mode Wi-Fi (`WIFI_AP_STA`)**:
  1. **Station Mode (STA)**: Attempts connection to the primary router (e.g. `0xMakers_Dev`). When connected, DHCP assigns an IP (e.g. `192.168.0.119`), which is displayed prominently on the robot's LCD screen.
  2. **SoftAP Fallback**: Simultaneously broadcasts an independent Wi-Fi access point:
     - **SSID**: `TinkerBot`
     - **Password**: `12345678`
     - **Static IP**: `192.168.4.1`
     - This guarantees the robot remains fully controllable even in outdoor or field environments with no external router.

### Packet Structure
```
[PC / Controller] ---> "steer 70 20\n" ---> (UDP 8888) ---> [ESP32-S3 Robot]
[PC / Controller] <--- "ACK: steer 70 20 | DIST: 23.4 cm | ARM: L:90 C:140 | ACT: STEER..." <---
```

---

## 3. Hardware I2C Ports & Dual-Bus Topology

The ESP32-S3 has two physical I2C peripheral hardware blocks. The board routes them to two completely isolated physical buses to prevent external sensor noise from locking up the motor controller:

```
                  +-----------------------------------------+
                  |         ESP32-S3 Microcontroller        |
                  +--------------------+--------------------+
                                       |
                   +-------------------+-------------------+
                   |                                       |
                   v I2C Bus 0 (Internal)                  v I2C Bus 1 (External)
                   SCL: GPIO 41, SDA: GPIO 42              SCL: GPIO 2, SDA: GPIO 1
                   Clock: 100 kHz                          Clock: 100 kHz
                   |                                       |
                   +---> Co-Processor MCU (0x0F)           +---> Ultrasonic Distance Sensor (0x57)
                   +---> LSM6DS3 6-DoF IMU (0x6B)          +---> 4-Probe Line Tracker (0x6F)
                   +---> FT6x36 Touch Controller (0x38)    +---> K210 Vision AI Module (0x24)
```

### Why Bus Isolation Matters:
External ultrasonic range sensors and line trackers frequently produce bus capacitance drops and noisy reflections on long cables. By keeping the mainboard co-processor (`0x0F`) on **I2C Bus 0**, motor control and emergency stopping remain 100% reliable even if an external sensor is hot-unplugged.

---

## 4. SPI Display Port (ST7789 Color LCD)

The $320 \times 240$ color IPS display is connected via high-speed SPI Bus 1:

| Signal | GPIO Pin | Hardware Function |
|---|---|---|
| **MOSI** | `GPIO 40` | SPI Master-Out Slave-In (Pixel data stream) |
| **MISO** | `GPIO 38` | SPI Master-In Slave-Out (Status readback) |
| **SCK**  | `GPIO 39` | High-Speed Clock (driven at $40\text{ MHz}$) |
| **DC**   | `GPIO 5`  | Command (`LOW`) vs Data (`HIGH`) selection |
| **CS**   | `GPIO 4`  | Chip Select (Active `LOW`) |
| **RST**  | `GPIO 6`  | Hardware reset pulse |
| **BL**   | `GPIO 15` | Backlight LED PWM control (Active `HIGH`) |
| **SD_CS**| `GPIO 3`  | MicroSD Card SPI Chip Select |

The custom firmware drives the display through LovyanGFX in RGB565 color format, delivering smooth $10\text{ Hz}$ HUD dashboard redraws with zero flicker.

---

## 5. Mainboard Co-Processor Port (`0x0F`) Register Layout

The low-level motor and power controller is an STM32/N76 co-processor listening at 7-bit I2C address `0x0F`:

```
====================================================================================
 Address  | Name                   | Size   | Values / Function
====================================================================================
 0x00     | MB_REG_BATTERY_PCT     | 1 byte | 0 to 100% remaining charge
 0x02     | MB_REG_BATTERY_VOLT    | 1 byte | Volts * 10 (e.g. 74 = 7.4V)
 0x04     | MB_REG_CHARGING_STATE  | 1 byte | 1 = Charging, 0 = Discharging
 0x0A     | MB_REG_LIGHT_COLOR     | 3 bytes| [Red, Green, Blue] underglow (0-255)
 0x0D     | MB_REG_LIGHT_BRIGHT    | 1 byte | Ambient LED brightness (0-255)
 0x14     | MB_REG_RTC_TIME        | 3 bytes| [Hours, Minutes, Seconds]
 0x17     | MB_REG_RTC_DATE        | 4 bytes| [DayOfWeek, Month, Day, Year]
 0x1E     | MB_REG_SERVO_0_ANGLE   | 1 byte | Servo 0 (Lift): 0 to 180 degrees
 0x1F     | MB_REG_SERVO_1_ANGLE   | 1 byte | Servo 1 (Claw): 0 to 180 degrees
 0x22     | MB_REG_SERVO_0_RELEASE | 1 byte | Write 0x00 to detach PWM (power saving)
 0x23     | MB_REG_SERVO_1_RELEASE | 1 byte | Write 0x00 to detach PWM (power saving)
 0x28     | MB_REG_VOICE_CMD       | 1 byte | Recognized offline voice ID (0x01..0xFF)
 0x50     | MB_REG_MOTOR_0         | 4 bytes| 32-bit encoder position (Big-Endian)
 0x52     | MB_REG_MOTOR_0_SPEED   | 2 bytes| Motor 0 speed / RPM
 0x6E     | MB_REG_MOTOR_1         | 4 bytes| 32-bit encoder position (Big-Endian)
 0x8C     | MB_REG_MOTOR_2         | 4 bytes| 32-bit encoder position (Big-Endian)
 0xAA     | MB_REG_MOTOR_3         | 4 bytes| 32-bit encoder position (Big-Endian)
 0xC8     | MB_REG_MOTOR_PAIR      | 1 byte | Dual Drive Action: 0=Stop, 1=Fwd, 2=Rev, 3=Left, 4=Right
 0xC9     | MB_REG_MOTOR_PAIR_CFG  | 2 bytes| Motor indices assigned to pair [LeftId, RightId]
 0xCA     | MB_REG_MOTOR_PAIR_SPD  | 4 bytes| Proportional Dual Speeds [L_hi, L_lo, R_hi, R_lo]
====================================================================================
```

### Sensor Scaling Insights:
- **Ultrasonic Range Sensor (`0x57`)**: The raw 32-bit integer read from the sensor represents tens of micrometers. Dividing by **`10000.0f`** yields true calibrated physical centimeters (`cm`).
