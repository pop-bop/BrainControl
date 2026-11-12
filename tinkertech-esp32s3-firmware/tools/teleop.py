"""
TinkerTech Steerable Robot & Robotic Arm Teleop Controller
Interactive Game Controller (Gamepad/Joystick) + Keyboard Teleop over WiFi UDP.

Supports:
- Proportional Steering & Differential Drive
- Dual-Servo Robotic Arm (Servo 0: Lift, Servo 1: Claw Gripper)
- Police Emergency Flashing Strobes
- Live Telemetry (Ultrasonic distance, Arm Angles, Battery, IMU)
- USB / Bluetooth Gamepads (Xbox, PlayStation, Generic HID) with Hotplug
- Simultaneous Keyboard Controls
"""

import sys
import os
import time
import math
import socket
import threading

DEFAULT_ROBOT_IP = "192.168.0.119"
DEFAULT_ROBOT_PORT = 8888

# Check for headless CLI flag
RUN_CLI_ONLY = "--cli" in sys.argv

try:
    import pygame
    PYGAME_AVAILABLE = True
except ImportError:
    PYGAME_AVAILABLE = False


class RobotTeleop:
    def __init__(self, ip=DEFAULT_ROBOT_IP, port=DEFAULT_ROBOT_PORT):
        self.ip = ip
        self.port = port
        self.running = True

        # State
        self.speed_limit = 70       # Max throttle % (0..100)
        self.throttle = 0           # -100..100
        self.turn = 0               # -100..100
        self.lift_angle = 90        # 0..180 deg (Servo 0)
        self.claw_angle = 90        # 0..180 deg (Servo 1)
        self.police_lights = True
        self.last_sent_cmd = "stop"
        self.last_telemetry = "Connecting..."
        self.distance_cm = 0.0
        self.controller_name = "None (Keyboard Active)"

        # UDP Socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(0.5)

    def send_cmd(self, cmd):
        try:
            self.sock.sendto(cmd.encode("utf-8"), (self.ip, self.port))
            self.last_sent_cmd = cmd
        except Exception:
            pass

    def telemetry_listener(self):
        while self.running:
            try:
                data, _ = self.sock.recvfrom(512)
                if data:
                    text = data.decode("utf-8", errors="ignore").strip()
                    self.last_telemetry = text
                    # Parse distance if present: DIST: 23.7 cm
                    if "DIST:" in text:
                        parts = text.split("DIST:")[1].split("cm")[0].strip()
                        try:
                            self.distance_cm = float(parts)
                        except ValueError:
                            pass
            except socket.timeout:
                continue
            except Exception:
                break

    def set_arm_lift(self, angle):
        self.lift_angle = max(0, min(180, angle))
        self.send_cmd(f"servo 0 {self.lift_angle}")

    def set_arm_claw(self, angle):
        self.claw_angle = max(0, min(180, angle))
        self.send_cmd(f"servo 1 {self.claw_angle}")

    def toggle_police(self):
        self.police_lights = not self.police_lights
        self.send_cmd("police on" if self.police_lights else "police off")

    def run_gui(self):
        pygame.init()
        pygame.joystick.init()
        pygame.font.init()

        width, height = 640, 480
        screen = pygame.display.set_mode((width, height))
        pygame.display.set_caption("TinkerTech Steerable Robot Cockpit")
        clock = pygame.time.Clock()

        # Fonts
        font_large = pygame.font.SysFont("Consolas", 22, bold=True)
        font_med = pygame.font.SysFont("Consolas", 16, bold=True)
        font_small = pygame.font.SysFont("Consolas", 13)

        joysticks = {}
        for i in range(pygame.joystick.get_count()):
            j = pygame.joystick.Joystick(i)
            j.init()
            joysticks[j.get_instance_id()] = j
            self.controller_name = j.get_name()

        # Telemetry receiver
        threading.Thread(target=self.telemetry_listener, daemon=True).start()

        # Initial ping
        self.send_cmd("police on")
        self.send_cmd("arm 90 90")

        last_net_send = 0
        last_arm_step = 0

        while self.running:
            dt = clock.tick(30) / 1000.0
            now = time.time()

            # Handle Events
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running = False

                elif event.type == pygame.JOYDEVICEADDED:
                    j = pygame.joystick.Joystick(event.device_index)
                    j.init()
                    joysticks[j.get_instance_id()] = j
                    self.controller_name = j.get_name()
                    print(f"[JOYSTICK CONNECTED] {self.controller_name}")

                elif event.type == pygame.JOYDEVICEREMOVED:
                    if event.instance_id in joysticks:
                        del joysticks[event.instance_id]
                    self.controller_name = list(joysticks.values())[0].get_name() if joysticks else "None (Keyboard Active)"
                    print(f"[JOYSTICK REMOVED] Remaining: {len(joysticks)}")

                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        self.running = False
                    elif event.key == pygame.K_SPACE:
                        self.throttle = 0
                        self.turn = 0
                        self.send_cmd("stop")
                    elif event.key == pygame.K_p:
                        self.toggle_police()
                    elif event.key == pygame.K_o: # Open claw
                        self.set_arm_claw(140)
                    elif event.key == pygame.K_c: # Close claw
                        self.set_arm_claw(40)
                    elif event.key == pygame.K_r: # Release servos
                        self.send_cmd("release")
                    elif event.key in (pygame.K_1, pygame.K_2, pygame.K_3, pygame.K_4, pygame.K_5, pygame.K_6, pygame.K_7, pygame.K_8, pygame.K_9):
                        self.speed_limit = (event.key - pygame.K_0) * 10

                elif event.type == pygame.JOYBUTTONDOWN:
                    # Button 0 (A/Cross): Stop
                    if event.button == 0:
                        self.throttle = 0
                        self.turn = 0
                        self.send_cmd("stop")
                    # Button 1 or 3: Police strobe toggle
                    elif event.button in (2, 3):
                        self.toggle_police()
                    # Bumpers for claw: LB=Open, RB=Close
                    elif event.button == 4: # Left Bumper
                        self.set_arm_claw(self.claw_angle + 15)
                    elif event.button == 5: # Right Bumper
                        self.set_arm_claw(self.claw_angle - 15)

            # Polling inputs: Gamepad + Keyboard
            target_throttle = 0.0
            target_turn = 0.0

            # 1. Gamepad input
            if joysticks:
                j = list(joysticks.values())[0]
                # Left stick: Y axis inverted (push forward = -1.0)
                ay = -j.get_axis(1)
                ax = j.get_axis(0)

                # Deadband
                if abs(ay) < 0.12: ay = 0.0
                if abs(ax) < 0.12: ax = 0.0

                target_throttle += ay * self.speed_limit
                target_turn += ax * self.speed_limit

                # D-Pad / Hat for Robotic Arm
                if j.get_numhats() > 0:
                    hat = j.get_hat(0)
                    if now - last_arm_step > 0.08:
                        if hat[1] > 0:  # Up -> Lift Up
                            self.set_arm_lift(self.lift_angle + 3)
                            last_arm_step = now
                        elif hat[1] < 0: # Down -> Lift Down
                            self.set_arm_lift(self.lift_angle - 3)
                            last_arm_step = now
                        if hat[0] > 0:  # Right -> Claw Open
                            self.set_arm_claw(self.claw_angle + 3)
                            last_arm_step = now
                        elif hat[0] < 0: # Left -> Claw Close
                            self.set_arm_claw(self.claw_angle - 3)
                            last_arm_step = now

                # Right stick vertical for Lift (axis 3 or 4 depending on OS)
                if j.get_numaxes() >= 4:
                    ry = -j.get_axis(3 if j.get_numaxes() == 4 else 4)
                    if abs(ry) > 0.2 and (now - last_arm_step > 0.06):
                        self.set_arm_lift(self.lift_angle + int(ry * 4))
                        last_arm_step = now

            # 2. Keyboard input
            keys = pygame.key.get_pressed()
            kb_fwd = keys[pygame.K_w] or keys[pygame.K_UP]
            kb_rev = keys[pygame.K_s] or keys[pygame.K_DOWN]
            kb_left = keys[pygame.K_a] or keys[pygame.K_LEFT]
            kb_right = keys[pygame.K_d] or keys[pygame.K_RIGHT]

            if kb_fwd: target_throttle += self.speed_limit
            if kb_rev: target_throttle -= self.speed_limit
            if kb_left: target_turn -= self.speed_limit
            if kb_right: target_turn += self.speed_limit

            # Wide curved arc keys (Q and E)
            if keys[pygame.K_q]:
                target_throttle = self.speed_limit * 0.7
                target_turn = -self.speed_limit * 0.4
            elif keys[pygame.K_e]:
                target_throttle = self.speed_limit * 0.7
                target_turn = self.speed_limit * 0.4

            # Continuous Arm Keys (I/K for Lift, J/L for Claw)
            if now - last_arm_step > 0.08:
                if keys[pygame.K_i]:
                    self.set_arm_lift(self.lift_angle + 4)
                    last_arm_step = now
                elif keys[pygame.K_k]:
                    self.set_arm_lift(self.lift_angle - 4)
                    last_arm_step = now
                if keys[pygame.K_j]:
                    self.set_arm_claw(self.claw_angle - 4)
                    last_arm_step = now
                elif keys[pygame.K_l]:
                    self.set_arm_claw(self.claw_angle + 4)
                    last_arm_step = now

            self.throttle = int(max(-100, min(100, target_throttle)))
            self.turn = int(max(-100, min(100, target_turn)))

            # Network send logic (~10 Hz or immediately on stop/motion change)
            if now - last_net_send > 0.09:
                last_net_send = now
                if self.throttle == 0 and self.turn == 0:
                    if self.last_sent_cmd != "stop":
                        self.send_cmd("stop")
                else:
                    cmd = f"steer {self.throttle} {self.turn}"
                    self.send_cmd(cmd)

            # RENDER HUD
            screen.fill((15, 20, 28))

            # Header Banner
            pygame.draw.rect(screen, (22, 32, 45), (10, 10, width - 20, 45), border_radius=6)
            title = font_large.render("TINKERTECH POLICE BOT TELEOP", True, (0, 220, 255))
            screen.blit(title, (24, 20))

            target_info = font_small.render(f"Target: {self.ip}:{self.port} | Max Speed: {self.speed_limit}%", True, (160, 180, 200))
            screen.blit(target_info, (width - 240, 24))

            # Controller & Status Card
            pygame.draw.rect(screen, (20, 26, 36), (10, 65, width - 20, 40), border_radius=4)
            ctrl_text = font_med.render(f"INPUT DEVICE: {self.controller_name}", True, (100, 255, 150) if joysticks else (255, 200, 80))
            screen.blit(ctrl_text, (20, 75))

            # Police Beacon Badge
            if self.police_lights:
                is_red = (int(now * 5) % 2 == 0)
                badge_col = (255, 40, 40) if is_red else (40, 120, 255)
                pygame.draw.rect(screen, badge_col, (width - 170, 70, 150, 30), border_radius=4)
                strobe_lbl = font_med.render("POLICE ACTIVE", True, (255, 255, 255))
                screen.blit(strobe_lbl, (width - 155, 75))

            # --- LEFT PANEL: STEERING & THROTTLE GAUGES ---
            pygame.draw.rect(screen, (20, 26, 36), (10, 115, 305, 210), border_radius=6)
            screen.blit(font_med.render("DIFFERENTIAL STEERING", True, (0, 220, 255)), (20, 125))

            # Steering Crosshair Grid
            cx, cy = 162, 220
            radius = 65
            pygame.draw.circle(screen, (40, 50, 70), (cx, cy), radius, 2)
            pygame.draw.line(screen, (40, 50, 70), (cx - radius, cy), (cx + radius, cy), 1)
            pygame.draw.line(screen, (40, 50, 70), (cx, cy - radius), (cx, cy + radius), 1)

            # Stick Indicator Dot
            dot_x = cx + int((self.turn / 100.0) * (radius - 5))
            dot_y = cy - int((self.throttle / 100.0) * (radius - 5))
            pygame.draw.circle(screen, (0, 255, 120), (dot_x, dot_y), 9)

            # Left/Right motor calculated outputs
            l_motor = max(-100, min(100, self.throttle + self.turn))
            r_motor = max(-100, min(100, self.throttle - self.turn))
            mot_text = font_small.render(f"Motor L: {l_motor:+4d}% | Motor R: {r_motor:+4d}%", True, (200, 220, 240))
            screen.blit(mot_text, (25, 295))

            # --- RIGHT PANEL: ROBOTIC ARM SERVOS ---
            pygame.draw.rect(screen, (20, 26, 36), (325, 115, 305, 210), border_radius=6)
            screen.blit(font_med.render("ROBOTIC ARM CONTROLS", True, (255, 200, 50)), (335, 125))

            # Lift Angle Bar
            screen.blit(font_small.render(f"Servo 0 (Lift): {self.lift_angle}°", True, (240, 240, 240)), (335, 160))
            pygame.draw.rect(screen, (40, 48, 60), (335, 180, 280, 18), border_radius=3)
            lift_fill = int((self.lift_angle / 180.0) * 280)
            pygame.draw.rect(screen, (255, 180, 0), (335, 180, lift_fill, 18), border_radius=3)

            # Claw Angle Bar
            screen.blit(font_small.render(f"Servo 1 (Claw): {self.claw_angle}°", True, (240, 240, 240)), (335, 215))
            pygame.draw.rect(screen, (40, 48, 60), (335, 235, 280, 18), border_radius=3)
            claw_fill = int((self.claw_angle / 180.0) * 280)
            pygame.draw.rect(screen, (0, 200, 255), (335, 235, claw_fill, 18), border_radius=3)

            # Quick Claw Presets
            open_txt = font_small.render("[O] Open Claw (140°)", True, (150, 180, 200))
            close_txt = font_small.render("[C] Close Grip (40°)", True, (150, 180, 200))
            rel_txt = font_small.render("[R] Release Motor", True, (150, 180, 200))
            screen.blit(open_txt, (335, 265))
            screen.blit(close_txt, (335, 282))
            screen.blit(rel_txt, (335, 299))

            # --- BOTTOM PANEL: TELEMETRY & CONTROLS GUIDE ---
            pygame.draw.rect(screen, (20, 26, 36), (10, 335, width - 20, 135), border_radius=6)
            screen.blit(font_med.render("LIVE TELEMETRY & GUIDE", True, (0, 220, 255)), (20, 345))

            dist_col = (255, 60, 60) if (0 < self.distance_cm < 15) else (100, 255, 150)
            dist_lbl = font_large.render(f"OBSTACLE DISTANCE: {self.distance_cm:.1f} cm", True, dist_col)
            screen.blit(dist_lbl, (20, 370))

            raw_telem = font_small.render(f"Raw: {self.last_telemetry[:60]}", True, (130, 150, 170))
            screen.blit(raw_telem, (20, 400))

            guide_1 = font_small.render("Gamepad: Left Stick=Drive/Steer | D-Pad/Right Stick=Lift | Bumpers=Claw | A=Stop | X=Police", True, (180, 200, 220))
            guide_2 = font_small.render("Keyboard: W/S/A/D=Steer | I/K=Lift | J/L=Claw | Space=Stop | P=Police | 1-9=Speed Limit", True, (180, 200, 220))
            screen.blit(guide_1, (20, 420))
            screen.blit(guide_2, (20, 438))

            pygame.display.flip()

        pygame.quit()
        self.send_cmd("stop")
        print("\nTeleop closed. Robot stopped safely.")

    def run_cli(self):
        import msvcrt
        threading.Thread(target=self.telemetry_listener, daemon=True).start()
        self.send_cmd("police on")
        self.send_cmd("arm 90 90")

        print("=================================================================")
        print(f"  TINKERTECH POLICE BOT TELEOP (CLI) | Target: {self.ip}:{self.port}")
        print("=================================================================")
        print("  Controls:")
        print("    [W] Forward     [S] Backward     [A] Turn Left   [D] Turn Right")
        print("    [I] Lift Up     [K] Lift Down    [J] Claw Close  [L] Claw Open")
        print("    [O] Claw Preset Open             [C] Claw Preset Close")
        print("    [Space] STOP    [P] Police Lights                [1-9] Speed Limit")
        print("    [Q] Quit")
        print("=================================================================")

        try:
            while self.running:
                if msvcrt.kbhit():
                    ch = msvcrt.getch().decode("utf-8", errors="ignore").lower()
                    if ch == 'w': self.send_cmd(f"steer {self.speed_limit} 0")
                    elif ch == 's': self.send_cmd(f"steer -{self.speed_limit} 0")
                    elif ch == 'a': self.send_cmd(f"steer 0 -{self.speed_limit}")
                    elif ch == 'd': self.send_cmd(f"steer 0 {self.speed_limit}")
                    elif ch in (' ', 'x'): self.send_cmd("stop")
                    elif ch == 'i': self.set_arm_lift(self.lift_angle + 10)
                    elif ch == 'k': self.set_arm_lift(self.lift_angle - 10)
                    elif ch == 'j': self.set_arm_claw(self.claw_angle - 10)
                    elif ch == 'l': self.set_arm_claw(self.claw_angle + 10)
                    elif ch == 'o': self.set_arm_claw(140)
                    elif ch == 'c': self.set_arm_claw(40)
                    elif ch == 'r': self.send_cmd("release")
                    elif ch == 'p': self.toggle_police()
                    elif ch in '123456789': self.speed_limit = int(ch) * 10
                    elif ch == 'q': break
                    print(f"\rCmd: {self.last_sent_cmd:25s} | Telem: {self.last_telemetry[:40]}", end="", flush=True)
                time.sleep(0.02)
        finally:
            self.send_cmd("stop")
            print("\nRobot stopped.")

    def run(self):
        if PYGAME_AVAILABLE and not RUN_CLI_ONLY:
            try:
                self.run_gui()
            except Exception as e:
                print(f"[GUI Notice] Falling back to CLI mode: {e}")
                self.run_cli()
        else:
            self.run_cli()


if __name__ == "__main__":
    ip = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else DEFAULT_ROBOT_IP
    teleop = RobotTeleop(ip=ip)
    teleop.run()
