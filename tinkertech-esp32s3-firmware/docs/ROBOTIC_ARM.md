# Robotic Arm & Servo Control Guide

The TinkerTech S4S kit supports a dual-axis articulated robotic arm driven by standard $180°$ PWM hobby servos connected to dedicated headers on the mainboard.

---

## 1. Servo Assignments

```
                [ CLAW / GRIPPER ]  <--- Servo 1 (Addr: 0x1F)
                        |                Range: 40° (Grip) to 140° (Open)
                        |
                [ ARM ELEVATION ]   <--- Servo 0 (Addr: 0x1E)
                        |                Range: 0° (Lowest) to 180° (Highest)
                        |
                +---------------+
                |   BASE MCU    |
                +---------------+
```

| Servo Index | Function | Physical Joint | Operating Range | Default / Rest |
|---|---|---|---|---|
| **Servo 0** | **Lift** | Arm Shoulder / Elevation | $0° - 180°$ | $90°$ (Level) |
| **Servo 1** | **Claw** | End-Effector Gripper | $0° - 180°$ | $90°$ (Half-Open) |

---

## 2. Recommended Operating Angles

### Servo 0 (Elevation / Lift)
- **`0°`**: Arm fully lowered / resting against base.
- **`90°`**: Level horizontal posture (ideal cruising stance).
- **`140° - 160°`**: Elevated carry stance.
- **`180°`**: Maximum vertical extension.

### Servo 1 (Gripper Claw)
- **`40°`**: Fully closed / firm pinch grip on small to medium objects.
- **`90°`**: Semi-open neutral stance.
- **`140°`**: Maximum wide opening for approaching objects.

> [!CAUTION]
> Driving the claw beyond its physical mechanical limits (e.g. attempting to force it past $40°$ while gripping a hard object) will cause the servo motor to stall, drawing excessive current and heating the co-processor. Use the `release` command when holding objects statically for extended durations.

---

## 3. Power Safety & The `release` Command

Digital and analog servos draw substantial quiescent current (often $100\text{ mA} - 500\text{ mA}$) even when holding position against zero external load.

To protect the onboard battery and servo gearboxes:
- Send `release` or `arm release` when the arm has completed a motion and does not need active torque holding.
- The firmware writes `0x00` to co-processor registers `0x22` and `0x23`, disabling the PWM drive signals.
- To re-engage the servos, simply issue any new angle command (e.g. `lift 90` or `arm 90 90`).

---

## 4. Real-Time Telemetry & Cockpit Display

The current angles of both servos are dynamically rendered on the onboard ST7789 LCD in the bottom status panel:
```
+-----------------------------------------------------------+
| ROBOTIC ARM (SERVOS 0 & 1):                               |
| LIFT: 110 deg   CLAW: 140 deg                             |
+-----------------------------------------------------------+
```
These values are also included in the telemetry metadata payload returned on every Wi-Fi UDP ACK packet:
```
ACK: lift 110 | DIST: 23.4 cm | ARM: L:110 C:140 | ACT: LIFT: 110 deg
```
This enables the PC Teleop Cockpit to update its graphical angle gauges in real-time.
