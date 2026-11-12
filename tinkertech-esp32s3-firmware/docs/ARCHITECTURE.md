# Hardware Architecture & System Topology

The **TinkerTech / S4S STEM Robot** is powered by an **Espressif ESP32-S3** microcontroller (Dual-Core Xtensa LX7 running at 240 MHz) coupled with a dedicated low-level real-time motion and power co-processor over an internal I2C bus.

---

## 1. System Block Diagram

```
                              +---------------------------------------+
                              |        ESP32-S3 Microcontroller       |
                              |  Dual-Core Xtensa LX7 @ 240 MHz       |
                              |  8MB Flash, 512KB SRAM, Wi-Fi & BLE   |
                              +-------------------+-------------------+
                                                  |
                 +--------------------------------+--------------------------------+
                 |                                |                                |
                 v SPI Bus 1                      v I2C Bus 0 (Internal, 100kHz)   v I2C Bus 1 (External, 100kHz)
           (MOSI: 40, MISO: 38)            (SCL: 41, SDA: 42)               (SCL: 2, SDA: 1)
           (SCK: 39, DC: 5, CS: 4)                |                                |
                 |                                +------------------------+       +------------------------+
                 v                                |                        |       |                        |
+----------------------------------+              v                        v       v                        v
| ST7789 IPS LCD (320 x 240)       |      +---------------+        +-------------+ +---------------+ +---------------+
| High-Speed Direct Framebuffer    |      | Mainboard MCU |        | LSM6DS3 IMU | | Ultrasonic    | | 4-Probe Line  |
| Backlight: GPIO 15 (PWM)         |      | (Addr: 0x0F)  |        | 6-DoF Accel | | Range Sensor  | | Tracker       |
+----------------------------------+      +-------+-------+        | & Gyro      | | (Addr: 0x57)  | | (Addr: 0x6F)  |
| MicroSD Card Slot (CS: GPIO 3)   |              |                | (Addr: 0x6B)| +---------------+ +---------------+
+----------------------------------+              |                +-------------+ | 2x RGB Eye    | | K210 Vision   |
| FT6x36 Capacitive Touch Panel    |              |                                | LEDs          | | AI Module     |
| (Addr: 0x38 on I2C0)             |              |                                +---------------+ | (Addr: 0x24)  |
+----------------------------------+              |                                                  +---------------+
                                                  +----------------------------------+
                                                  |                                  |
                                                  v                                  v
                                         +------------------+              +-------------------+
                                         | 4x DC Motors     |              | 2x 180° Servos    |
                                         | Dual H-Bridge    |              | PWM Output        |
                                         | Encoders Ch 0..3 |              | - Servo 0: Lift   |
                                         +------------------+              | - Servo 1: Claw   |
                                         | RGB Underglow    |              +-------------------+
                                         | Battery Monitor  |
                                         | RTC & Offline VR |
                                         +------------------+
```

---

## 2. Pinout Specification

### SPI Display & MicroSD Interface (SPI Bus 1)
| Signal | ESP32-S3 Pin | Hardware Description |
|---|---|---|
| **LCD MOSI** | `GPIO 40` | Master Out Slave In (Data Line) |
| **LCD MISO** | `GPIO 38` | Master In Slave Out |
| **LCD SCK**  | `GPIO 39` | Serial Clock Line |
| **LCD DC**   | `GPIO 5`  | Data / Command Control Pin |
| **LCD CS**   | `GPIO 4`  | ST7789 Chip Select (Active LOW) |
| **LCD RST**  | `GPIO 6`  | Hardware Reset Pin |
| **LCD BL**   | `GPIO 15` | Backlight PWM Driver (Active HIGH) |
| **SD CS**    | `GPIO 3`  | MicroSD SPI Chip Select |

### Internal I2C Bus 0 (Co-Processor, IMU, Touch)
| Signal | ESP32-S3 Pin | Hardware Description |
|---|---|---|
| **I2C0 SCL** | `GPIO 41` | Dedicated Internal Clock line (100 kHz) |
| **I2C0 SDA** | `GPIO 42` | Dedicated Internal Data line (100 kHz) |

### External Sensor I2C Bus 1
| Signal | ESP32-S3 Pin | Hardware Description |
|---|---|---|
| **I2C1 SCL** | `GPIO 2` | External Sensor Clock line (100 kHz) |
| **I2C1 SDA** | `GPIO 1` | External Sensor Data line (100 kHz) |

### Auxiliary / Expansion Header Pins
The mainboard exposes the following GPIO pins on 2.54mm expansion headers:
`GPIO 8`, `GPIO 9`, `GPIO 17`, `GPIO 18`, `GPIO 19`, `GPIO 20`, `GPIO 45`, `GPIO 46`, `GPIO 48`.

---

## 3. Co-Processor Architecture (I2C `0x0F`)

The mainboard features a secondary real-time microcontroller (at I2C address `0x0F`) that offloads low-level timing-sensitive tasks from the ESP32-S3:
- **Motor Pulse Width Modulation (PWM)**: Independent 4-channel closed-loop H-Bridge drive.
- **Quadrature Encoder Decoding**: Accumulates 32-bit tick positions for Motor 0, 1, 2, and 3.
- **Hardware Servo PWM**: Dedicated pulse generators (50 Hz, 1ms - 2ms pulse width) for Servo 0 and Servo 1.
- **ADC Battery Gauge**: Reads pack voltage and remaining capacity percentage through a voltage divider.
- **Hardware Real-Time Clock (RTC)**: Dedicated timekeeping registers (Hours, Minutes, Seconds, Day, Month, Year).
- **Offline Voice Recognition Processor**: Interfacing at register `0x28`.

---

## 4. Power & Battery Management

- **Power Source**: 2S Li-ion Battery pack (7.4V nominal, 8.4V fully charged).
- **Onboard Buck Converters**:
  - `5.0V Buck`: High-current rail powering DC motors, servos, and external sensors.
  - `3.3V LDO`: High-precision, low-noise rail powering ESP32-S3, LCD, IMU, and logic circuitry.
- **Battery Telemetry**:
  - Register `0x00`: Remaining battery percentage ($0\% - 100\%$).
  - Register `0x02`: Battery voltage scaled by 10 (e.g. `74` represents $7.4\text{ V}$).
  - Register `0x04`: Hardware charging state ($1 = \text{charging}, 0 = \text{discharging}$).
