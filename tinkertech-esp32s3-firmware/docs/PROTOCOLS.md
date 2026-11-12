# Communication Protocols & Command Reference

The TinkerTech ESP32-S3 firmware supports three primary control and telemetry channels:
1. **Wi-Fi UDP Server** (Primary high-speed wireless control channel)
2. **USB Serial Console** (Command-line interface and debugging at 115200 baud)
3. **ESP-NOW Peer-to-Peer Protocol** (Ultra-low latency wireless controller link)

---

## 1. Wi-Fi UDP Protocol

### Network Settings
- **Default Port**: `8888`
- **Station Mode (STA)**: Connects to pre-configured Wi-Fi network (IP assigned by router DHCP, displayed on LCD).
- **SoftAP Fallback**: `TinkerBot` (Password: `12345678`, IP: `192.168.4.1`).

### Command Format
All UDP commands are plain ASCII strings terminated by newline or packet boundary. The robot replies to every command with an immediate UDP ACK packet containing live telemetry.

### Motion & Steering Commands
| Command | Format | Example | Description |
|---|---|---|---|
| `steer` | `steer <throttle> <turn>` | `steer 70 -30` | Differential steering. Both parameters range from `-100` to `100`. |
| `drive` | `drive <left> <right>` | `drive 80 40` | Direct individual motor speeds (`-100` to `100`). |
| `fwd` | `fwd [speed]` | `fwd 60` | Drive forward straight at specified speed ($0-100\%$, default 60). |
| `rev` | `rev [speed]` | `rev 60` | Drive backward straight at specified speed ($0-100\%$, default 60). |
| `left` | `left [speed]` | `left 60` | Tank pivot turn left (Left backward, Right forward). |
| `right` | `right [speed]` | `right 60` | Tank pivot turn right (Left forward, Right backward). |
| `stop` | `stop` | `stop` | Immediate motor stop; resets active drive states. |

### Robotic Arm & Servo Commands
| Command | Format | Example | Description |
|---|---|---|---|
| `servo` | `servo <id> <angle>` | `servo 0 120` | Sets specified servo (`0` = Lift, `1` = Claw) to angle ($0° - 180°$). |
| `arm` | `arm <lift> <claw>` | `arm 110 140` | Sets both arm servos simultaneously. |
| `lift` | `lift <angle\|up\|down>` | `lift 120` or `lift up` | Sets lift angle or adjusts by $\pm 10°$. |
| `claw` | `claw <angle\|open\|close>` | `claw open` or `claw 50` | Sets claw angle or selects presets ($140°$ open, $40°$ close). |
| `release` | `release` | `release` | Disables PWM to both servos (prevents heating/humming). |

### Lighting & Strobe Commands
| Command | Format | Example | Description |
|---|---|---|---|
| `police` | `police` or `police on` | `police on` | Enables high-speed emergency cruiser strobe on all LEDs. |
| `police off` | `police off` | `police off` | Disables strobe and turns off all LEDs. |
| `color` | `color <r> <g> <b>` | `color 0 255 0` | Sets static solid RGB color ($0-255$) on all LEDs. |

### Telemetry Request Commands
| Command | Description | Response Example |
|---|---|---|
| `dist` | Reads calibrated ultrasonic distance | `[ULTRASONIC] Distance: 28.5 cm (raw: 285400)` |
| `ip` | Returns STA and SoftAP IP addresses | `STA IP: 192.168.0.119 \| AP IP: 192.168.4.1` |

### Robot ACK Telemetry Response
Every command packet received over UDP triggers an instant acknowledgement packet back to the sender's IP and port:
```
ACK: <cmd> | DIST: <distance_cm> cm | ARM: L:<lift_deg> C:<claw_deg> | ACT: <action_description>
```
*Example:*
```
ACK: steer 50 20 | DIST: 42.1 cm | ARM: L:90 C:140 | ACT: STEER T:50 T:20 (L:70 R:30)
```

---

## 2. Serial Console Protocol (115200 Baud)

When connected via USB CDC or UART at **115200 baud, 8-N-1**:
- Accepts the exact same commands listed above (`steer`, `arm`, `fwd`, `dist`, `police`, etc.).
- Line endings: `\n` (newline) or `\r\n`.
- Replies with `OK: <command>` and prints diagnostic log messages.

---

## 3. Co-Processor I2C Register Map (`0x0F`)

The mainboard STM32/N76 co-processor listens at 7-bit I2C slave address **`0x0F`**:

```
+---------------+------------------------+-------------------------------------------------------------+
| Register      | Name                   | Description & Data Format                                   |
+---------------+------------------------+-------------------------------------------------------------+
| 0x00          | MB_REG_BATTERY_PCT     | uint8_t: Remaining battery capacity (0 - 100%)              |
| 0x02          | MB_REG_BATTERY_VOLT    | uint8_t: Battery voltage * 10 (e.g. 74 = 7.4 V)              |
| 0x04          | MB_REG_CHARGING_STATE  | uint8_t: 1 = Charging, 0 = Discharging                      |
| 0x0A          | MB_REG_LIGHT_COLOR     | uint8_t[3]: Ambient RGB underglow light [R, G, B]           |
| 0x0D          | MB_REG_LIGHT_BRIGHT    | uint8_t: Ambient RGB brightness (0 - 255)                   |
| 0x14          | MB_REG_RTC_TIME        | uint8_t[3]: Hardware RTC [Hours, Minutes, Seconds]          |
| 0x17          | MB_REG_RTC_DATE        | uint8_t[4]: Hardware RTC [DayOfWeek, Month, Day, Year]      |
| 0x1E          | MB_REG_SERVO_0_ANGLE   | uint8_t: Servo 0 (Lift) target angle (0 - 180 degrees)      |
| 0x1F          | MB_REG_SERVO_1_ANGLE   | uint8_t: Servo 1 (Claw) target angle (0 - 180 degrees)      |
| 0x20          | MB_REG_SERVO_0_SPEED   | int8_t: Servo 0 continuous rotation speed (-100 to 100)     |
| 0x21          | MB_REG_SERVO_1_SPEED   | int8_t: Servo 1 continuous rotation speed (-100 to 100)     |
| 0x22          | MB_REG_SERVO_0_RELEASE | uint8_t: Write 0 to detach PWM output from Servo 0          |
| 0x23          | MB_REG_SERVO_1_RELEASE | uint8_t: Write 0 to detach PWM output from Servo 1          |
| 0x28          | MB_REG_VOICE_CMD       | uint8_t: Recognized offline voice command identifier        |
| 0x50          | MB_REG_MOTOR_0         | int32_t: Motor 0 quadrature encoder tick count (big-endian) |
| 0x52          | MB_REG_MOTOR_0_SPEED   | int16_t: Motor 0 actual rotational speed                    |
| 0x6E          | MB_REG_MOTOR_1         | int32_t: Motor 1 quadrature encoder tick count (big-endian) |
| 0x8C          | MB_REG_MOTOR_2         | int32_t: Motor 2 quadrature encoder tick count (big-endian) |
| 0xAA          | MB_REG_MOTOR_3         | int32_t: Motor 3 quadrature encoder tick count (big-endian) |
| 0xC8          | MB_REG_MOTOR_PAIR      | uint8_t: Dual drive command: 0=Stop, 1=Fwd, 2=Rev, 3=L, 4=R|
| 0xC9          | MB_REG_MOTOR_PAIR_CFG  | uint8_t[2]: Motor indices assigned to pair (default: [0, 1])|
| 0xCA          | MB_REG_MOTOR_PAIR_SPD  | uint8_t[4]: [L_speed_hi, L_speed_lo, R_speed_hi, R_speed_lo]|
+---------------+------------------------+-------------------------------------------------------------+
```
