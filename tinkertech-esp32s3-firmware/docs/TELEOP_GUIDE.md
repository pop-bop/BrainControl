# PC Cockpit Teleoperation Guide (`teleop.py`)

The `tools/teleop.py` script is a cross-platform, multi-threaded interactive teleoperation cockpit engineered to control the TinkerTech S4S robot with either a **USB / Bluetooth Gamepad** or **Keyboard**, while rendering a graphical Head-Up Display (HUD) of real-time telemetry.

---

## 1. System Architecture

```
                    +------------------------------------+
                    |       Interactive Teleop GUI       |
                    |   (Pygame 640x480 Cockpit HUD)     |
                    +--------+------------------+--------+
                             |                  |
           +-----------------+                  +-----------------+
           |                                                      |
           v                                                      v
+-------------------------+                            +-------------------------+
| Foreground Event Loop   |                            | Background Worker Thread|
| - Polling Joysticks     |                            | - UDP Socket Listener   |
| - Polling Keyboard Keys |                            | - Receives Robot ACKs   |
| - 30 FPS HUD Rendering  |                            | - Parses Distance Radar |
| - 10 Hz UDP Transmit    |                            | - Parses Arm Telemetry  |
+-------------------------+                            +-------------------------+
           |                                                      ^
           | (steer / arm / police commands)                      | (ACK packets)
           v                                                      |
+--------------------------------------------------------------------------------+
|                    Wi-Fi UDP Socket (Target: 192.168.0.119:8888)              |
+--------------------------------------------------------------------------------+
```

---

## 2. Gamepad & Controller Integration

The teleoperation system includes **automatic hotplug detection** via `pygame.joystick`:
- When you connect an Xbox, PlayStation (DualShock/DualSense), Logitech, 8BitDo, or generic USB HID game controller, the application detects it on the fly without restarting.
- **Deadband Filtering**: A $\pm 0.12$ deadzone is applied to analog sticks to eliminate stick drift and unwanted creeping when sticks are at resting center.

### Gamepad Input Mapping
| Gamepad Control | Teleop Mapping | Hardware Action on Robot |
|---|---|---|
| **Left Stick Y** (Vertical) | Proportional Throttle | Smooth Forward ($+100\%$) and Reverse ($-100\%$) |
| **Left Stick X** (Horizontal) | Proportional Steering | Smooth Left/Right Differential Curvature |
| **D-Pad Up / Down** | Arm Elevation | Adjusts Servo 0 (Lift) by $\pm 3°$ steps |
| **Right Stick Y** | Continuous Arm Elevation | Smooth elevation proportional to stick deflection |
| **D-Pad Left / Right** | Claw Gripper | Adjusts Servo 1 (Claw) by $\pm 3°$ steps |
| **Left Bumper (LB)** | Claw Open | Widens gripper ($+15°$ per tap) |
| **Right Bumper (RB)** | Claw Close | Tightens gripper ($-15°$ per tap) |
| **Button A / Cross** | Emergency Brake | Immediately transmits `stop` and zeroes throttle |
| **Button X / Square** | Police Beacon Toggle | Toggles alternating emergency strobes |

---

## 3. Keyboard Driving & Arm Controls

Keyboard driving is active **simultaneously** with gamepad controls, functioning either as the primary controller or as a co-pilot interface:

### Driving & Movement
- **`W` / `Up`**: Drive straight Forward.
- **`S` / `Down`**: Drive straight Backward.
- **`A` / `Left`**: Pivot Turn Left.
- **`D` / `Right`**: Pivot Turn Right.
- **`Q`**: Wide smooth curvature arc turn to the Left ($70\%$ throttle, $-40\%$ turn).
- **`E`**: Wide smooth curvature arc turn to the Right ($70\%$ throttle, $+40\%$ turn).
- **`Space`**: Immediate Emergency Stop (`stop`).

### Robotic Arm Operation
- **`I`**: Arm Lift Up ($+4°$ per step, up to $180°$).
- **`K`**: Arm Lift Down ($-4°$ per step, down to $0°$).
- **`J`**: Claw Gripper Close ($-4°$ per step).
- **`L`**: Claw Gripper Open ($+4°$ per step).
- **`O`**: Instant Claw Open Preset ($140°$).
- **`C`**: Instant Claw Close Grip Preset ($40°$).
- **`R`**: Release Servos (cuts PWM power to avoid motor stall and battery heating).

### Speed Limiter Presets
- Keys **`1` through `9`**: Instantly scales maximum throttle limit from $10\%$ to $90\%$.

---

## 4. Head-Up Display (HUD) Elements

When running in GUI mode, `teleop.py` renders a $640 \times 480$ dark cockpit HUD:
1. **Input Device Banner**: Shows the active controller name (e.g. `Xbox Wireless Controller`) in green, or warns when running on keyboard only.
2. **Police Strobe Indicator**: An animated Red/Blue flashing badge reflecting the active cruiser strobe state.
3. **Differential Steering Crosshair**: A circular radar gauge with a dynamic green tracking dot representing stick deflection and calculated left/right motor percentages ($v_L$, $v_R$).
4. **Arm Servo Visual Meters**: Color-coded fill bars showing live Lift ($0° - 180°$, Amber) and Claw ($0° - 180°$, Cyan) angles.
5. **Obstacle Distance Radar**: Prominent distance readout from the ultrasonic sensor. Turns bright red when an obstacle is within $15\text{ cm}$.

---

## 5. Headless / Terminal Fallback (`--cli`)

If running on a headless server or without a graphical display:
```bash
python teleop.py --cli [ROBOT_IP]
```
The script falls back to an interactive console loop using `msvcrt` on Windows, allowing direct single-keystroke control from the terminal prompt.
