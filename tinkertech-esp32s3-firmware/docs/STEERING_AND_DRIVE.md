# Differential Drive Kinematics & Steering System

The robot utilizes a two-wheel differential drive platform with passive rear caster balance, driven by high-torque DC gearmotors with optical quadrature encoders.

---

## 1. Kinematic Equations

Differential drive locomotion relies on varying the relative speeds of the left motor ($v_L$) and right motor ($v_R$).

### Proportional Mixing
The firmware converts **Throttle** ($T \in [-100, 100]$) and **Steering Turn** ($S \in [-100, 100]$) into individual motor speeds:

$$\begin{aligned}
v_L &= \text{clamp}(T + S, -100, 100) \\
v_R &= \text{clamp}(T - S, -100, 100)
\end{aligned}$$

### Operating Modes
- **Straight Forward / Reverse** ($S = 0$): Both wheels rotate at identical velocity ($v_L = v_R = T$).
- **Curvature Arc Steering** ($T \neq 0, S \neq 0$): Both wheels turn in the same direction at different speeds, producing a smooth arc of curvature radius $R = \frac{W}{2} \cdot \frac{v_R + v_L}{v_R - v_L}$ (where $W$ is the track width).
- **Pivot In-Place Turn** ($T = 0, S \neq 0$): Wheels turn in opposite directions ($v_L = S, v_R = -S$), turning the robot around its central vertical axis without translational displacement.

---

## 2. Co-Processor Hardware Implementation

The low-level motor controller at I2C address `0x0F` accepts 4-byte proportional speed buffers at register `MB_REG_MOTOR_PAIR + 0x02` (`0xCA`):

```cpp
uint8_t speedBuf[4] = {
    (uint8_t)((abs(leftSpeed) >> 8) & 0xFF),
    (uint8_t)(abs(leftSpeed) & 0xFF),
    (uint8_t)((abs(rightSpeed) >> 8) & 0xFF),
    (uint8_t)(abs(rightSpeed) & 0xFF)
};
writeReg(0xCA, speedBuf, 4);
```

The direction register `0xC8` is then updated to select the polarity:
- `DIR_STOP` (`0`): Both motors coast / brake.
- `DIR_FORWARD` (`1`): Both motors driven forward.
- `DIR_BACKWARD` (`2`): Both motors driven in reverse.
- `DIR_LEFT` (`3`): Left motor reverse, Right motor forward.
- `DIR_RIGHT` (`4`): Left motor forward, Right motor reverse.

---

## 3. Communication Failsafe Watchdog

To prevent runaway robot conditions caused by Wi-Fi packet drops, router congestion, or crashed client controllers:
- The firmware maintains a timestamp `lastRemoteCmdTime`.
- If no motion command is received for **$600\text{ ms}$**, the watchdog automatically triggers:
  ```cpp
  if (isRemoteActive && (millis() - lastRemoteCmdTime > 600)) {
      TinkerKit.mainboard.stop();
      isRemoteActive = false;
      lastAction = "STOPPED (Failsafe)";
  }
  ```
- Controller applications should transmit commands at approximately **$10\text{ Hz}$** ($100\text{ ms}$ intervals) while active to maintain continuous motion.
