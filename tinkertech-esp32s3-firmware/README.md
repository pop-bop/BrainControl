# TinkerTech S4S ESP32-S3 Robot Firmware & Teleop Suite

A native C++ firmware suite, reverse-engineered peripheral driver library, and interactive PC teleoperation system for the **TinkerTek / S4S ESP32-S3 STEM Robot Kit**.

---

## Key Features

- **High-Performance Architecture**: Native C++ Arduino firmware running on the dual-core **ESP32-S3** at $240\text{ MHz}$ with direct hardware register integration.
- **Proportional Differential Steering**: Smooth curvature arc steering and sharp zero-radius pivot turns via `steer <throttle> <turn>` and `drive <left> <right>`.
- **Dual-Servo Robotic Arm Support**: Full control over elevation ($0° - 180°$) and claw gripper opening/closing, featuring power-saving `release` mode to protect servo motors.
- **Police Cruiser Emergency Strobes**: Multi-pulse non-blocking alternating Red/Blue emergency beacon synchronized across mainboard ambient underglow and ultrasonic eye RGB LEDs.
- **Real-Time Cockpit LCD Telemetry**: On-board ST7789 $320 \times 240$ color display renders live battery status, Wi-Fi IP, obstacle distance radar, IMU orientation, and robotic arm angles.
- **PC Cockpit Teleoperation (`teleop.py`)**: Real-time Pygame HUD supporting automatic hotplug USB/Bluetooth gamepads (Xbox, PlayStation, Logitech) and simultaneous keyboard driving.
- **Tri-Mode Communications**: Wi-Fi UDP server (Port `8888`), USB Serial CLI (`115200` baud), and ultra-low-latency ESP-NOW peer-to-peer link.

---

## Repository Structure

```
tinkertech-esp32s3-firmware/
├── README.md                          <- Main project overview & quickstart (this file)
├── LICENSE                            <- MIT Open-Source License
├── binaries/                          <- Ready-to-flash factory binaries
│   ├── bootloader.bin                 <- ESP32-S3 second-stage bootloader (0x0000)
│   ├── partitions.bin                 <- Partition table (0x8000)
│   ├── firmware.bin                   <- Core application binary (0x10000)
│   ├── flash_firmware.py              <- Cross-platform Python flashing utility
│   └── flash_firmware.bat             <- 1-click Windows flashing batch file
├── firmware/                          <- Complete C++ source code & PlatformIO project
│   ├── platformio.ini                 <- PlatformIO configuration for ESP32-S3 N8
│   ├── src/                           <- Core implementation & entry point (main.cpp)
│   ├── include/                       <- Library headers (TinkerKit SDK)
│   └── lib/LovyanGFX/                 <- High-speed display driver library
├── tools/                             <- PC Remote Control & Telemetry tools
│   ├── teleop.py                      <- Gamepad & Keyboard Cockpit application
│   ├── run_teleop.bat                 <- Windows launcher for teleop
│   └── requirements.txt               <- Python dependencies (pygame-ce)
└── docs/                              <- In-depth technical guides
    ├── ARCHITECTURE.md                <- System block diagrams & pinout mapping
    ├── PORTS_AND_INTERFACES.md        <- Hardware Ports, Serial COM Identification & Reverse-Engineering
    ├── PROTOCOLS.md                   <- Complete Wi-Fi UDP, Serial & I2C register reference
    ├── ROBOTIC_ARM.md                 <- Articulated arm kinematics, calibration & safety
    ├── STEERING_AND_DRIVE.md          <- Differential drive equations & failsafe watchdog
    ├── TELEOP_GUIDE.md                <- PC Cockpit Teleoperation & Gamepad/Keyboard guide
    └── FLASHING_GUIDE.md              <- Step-by-step build & flashing instructions
```

---

## Quick Start in 3 Steps

### Step 1: Flash the Firmware
Connect the robot to your computer via USB-C and run the flasher:

```bash
# Windows 1-Click
binaries\flash_firmware.bat

# Or from any terminal with Python
python binaries/flash_firmware.py
```
*(For custom builds using PlatformIO, see [docs/FLASHING_GUIDE.md](docs/FLASHING_GUIDE.md)).*

### Step 2: Power On & Check LCD Dashboard
When the robot boots, the LCD screen will display:
- **STA IP**: Wi-Fi IP address assigned by your router (or fallback AP `TinkerBot:8888`).
- **BATTERY**: Live pack voltage and remaining capacity percentage.
- **ARM SERVOS**: Current elevation angle and claw opening status.
- **POLICE BADGE**: Flashing beacon confirmation.

### Step 3: Launch the PC Teleop Cockpit
Install the controller dependencies and launch the cockpit:

```bash
cd tools
pip install -r requirements.txt
python teleop.py [ROBOT_IP]
```
*(Or double-click `tools\run_teleop.bat` on Windows).*

---

## Controls Summary

### With Gamepad / Controller
| Input | Action |
|---|---|
| **Left Analog Stick** | Proportional Throttle (Up/Down) & Smooth Steering (Left/Right) |
| **D-Pad Up / Down** | Raise / Lower Robotic Arm Elevation |
| **Left / Right Bumpers (LB/RB)** | Open / Close Claw Gripper |
| **Button A / Cross** | Emergency Brake / STOP |
| **Button X / Square** | Toggle Police Cruiser Emergency Strobes |

### With Keyboard
| Key | Action |
|---|---|
| **`W` / `S`** | Forward / Backward Throttle |
| **`A` / `D`** | Turn Left / Turn Right |
| **`Q` / `E`** | Smooth Wide Arc Turning |
| **`I` / `K`** | Arm Lift Up / Down ($\pm 4°$ per step) |
| **`J` / `L`** | Claw Grip / Open ($\pm 4°$ per step) |
| **`O` / `C`** | Preset: Instant Open Claw ($140°$) / Firm Grip ($40°$) |
| **`R`** | Release Servo Power (prevents motor heating/stalling) |
| **`Space`** | Immediate Emergency Stop |
| **`P`** | Toggle Police Strobes |
| **`1` – `9`** | Set Maximum Speed Limit ($10\% - 90\%$) |

---

## Wi-Fi UDP Command Cheat Sheet

Send standard ASCII strings to the robot at port **`8888`**:

```bash
steer 70 20     # Drive forward with slight right curvature
steer -50 0     # Drive straight in reverse at 50% speed
stop            # Cut motor power immediately
lift 120        # Set arm elevation to 120 degrees
claw open       # Open claw gripper to 140 degrees
claw close      # Close claw grip to 40 degrees
arm 90 90       # Set both servos to neutral 90 degrees
release         # Cut power to arm servos to save battery
police on       # Activate emergency police strobes
police off      # Deactivate police strobes
dist            # Query ultrasonic distance sensor
```

Every command receives an instantaneous JSON/ASCII telemetry acknowledgement packet containing ultrasonic distance, servo angles, and active motor status.

---

## Hardware Pinout Reference

| Function | Pin Number | Bus / Interface |
|---|---|---|
| **LCD SPI MOSI / MISO / SCK** | `GPIO 40`, `38`, `39` | SPI Bus 1 |
| **LCD Control DC / CS / RST** | `GPIO 5`, `4`, `6` | SPI Bus 1 |
| **LCD Backlight (PWM)** | `GPIO 15` | Active HIGH |
| **SD Card CS** | `GPIO 3` | SPI Bus 1 |
| **Internal I2C SCL / SDA** | `GPIO 41`, `42` | I2C Bus 0 (`0x0F` Co-Processor, `0x6B` IMU) |
| **External I2C SCL / SDA** | `GPIO 2`, `1` | I2C Bus 1 (`0x57` Ultrasonic, `0x6F` Line Tracker) |

---

## Documentation Links

For comprehensive specifications, see the guides in the `docs/` folder:
- **[System Architecture & Pinouts](docs/ARCHITECTURE.md)**
- **[Hardware Ports, Serial COM Identification & Reverse-Engineering](docs/PORTS_AND_INTERFACES.md)**
- **[Communication Protocols & I2C Registers](docs/PROTOCOLS.md)**
- **[Robotic Arm Kinematics & Operation](docs/ROBOTIC_ARM.md)**
- **[Differential Drive Steering & Kinematics](docs/STEERING_AND_DRIVE.md)**
- **[PC Cockpit Teleoperation Guide](docs/TELEOP_GUIDE.md)**
- **[Flashing & Compilation Guide](docs/FLASHING_GUIDE.md)**

---

## License

This project is licensed under the [MIT License](LICENSE).
