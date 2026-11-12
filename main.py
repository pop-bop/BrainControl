import sys
import time
import socket
import argparse
import handler

DEFAULT_ROBOT_IP = "192.168.0.119"
DEFAULT_ROBOT_PORT = 8888
DEFAULT_SPEED = 100
HEARTBEAT_INTERVAL = 0.25


def create_udp_socket():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(0.5)
    return sock


def discover_robot(sock, target_ip, port):

    try:
        sock.sendto(b"dist", (target_ip, port))
        data, _ = sock.recvfrom(512)
        if b"ACK:" in data or b"DIST:" in data:
            print(f"[Robot Connected] Responded directly at {target_ip}:{port}")
            return target_ip
    except (socket.timeout, OSError):
        pass

    # 2. Subnet broadcast
    print(f"[Discovery] Probing subnet for TinkerTech robot...")
    broadcast_targets = ["255.255.255.255", "192.168.0.255", "192.168.4.1"]
    for b_ip in broadcast_targets:
        try:
            sock.sendto(b"dist", (b_ip, port))
        except Exception:
            pass

    start = time.time()
    while time.time() - start < 1.0:
        try:
            data, (found_ip, _) = sock.recvfrom(512)
            if b"ACK:" in data or b"DIST:" in data:
                print(f"[Discovery] Found active robot at {found_ip}:{port}!")
                return found_ip
        except (socket.timeout, OSError):
            break
        except Exception:
            break

    print(f"[Notice] Robot not reachable yet (is it powered on?). Using target {target_ip}.")
    return target_ip


def send_robot_command(sock, ip, port, cmd):
    try:
        sock.sendto(cmd.encode("utf-8"), (ip, port))
    except Exception as e:
        print(f"[UDP Error] Failed to send '{cmd}': {e}")


def main():
    parser = argparse.ArgumentParser(description="BrainControl Robot Controller")
    parser.add_argument(
        "--ip",
        type=str,
        default=DEFAULT_ROBOT_IP,
        help=f"Robot IP address (default: {DEFAULT_ROBOT_IP})",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_ROBOT_PORT,
        help=f"Robot UDP port (default: {DEFAULT_ROBOT_PORT})",
    )
    parser.add_argument(
        "--speed",
        type=int,
        default=DEFAULT_SPEED,
        help=f"Motor speed percentage 0..100 (default: {DEFAULT_SPEED})",
    )
    args = parser.parse_args()

    robot_ip = args.ip or DEFAULT_ROBOT_IP
    print(f"\n[Config] Target Robot: {robot_ip}:{args.port} | Speed: {args.speed}%")
    sock = create_udp_socket()
    robot_ip = discover_robot(sock, robot_ip, args.port)

    # Step 1: Initialize EMG streaming & calibration
    print("\n[BrainControl] Connecting to NPG Lite EMG device via BLE...")
    try:
        handler.ensure_streaming(wait_for_calibration=True)
    except Exception as e:
        print(f"[Error] Failed to connect to NPG Lite: {e}")
        print("Make sure NPG device is turned on, charged, and in BLE mode.")
        sys.exit(1)

    print("\n=======================================================")
    print("  CALIBRATION COMPLETE - READY FOR BRAIN CONTROL!     ")
    print("=======================================================")
    print("  Controls:")
    print("    • RIGHT ARM CLENCH -> Turn Right")
    print("    • LEFT ARM CLENCH  -> Turn Left")
    print("    • BOTH ARMS CLENCH -> Forward Straight")
    print("    • BOTH RELAXED     -> Stop")
    print("=======================================================\n")

    last_state = None
    last_sent_time = 0.0

    try:
        while True:
            current_state = handler.pull_state()
            now = time.time()

            state_changed = (current_state != last_state)
            need_heartbeat = (now - last_sent_time >= HEARTBEAT_INTERVAL)

            if state_changed or (current_state != "none" and need_heartbeat):
                if current_state == "both":
                    cmd = f"fwd {args.speed}"
                    display = f">>> [FORWARD] Both arms clenched (speed {args.speed}%)"
                elif current_state == "left":
                    cmd = f"left {args.speed-75}"
                    display = f"<<< [TURN LEFT] Left arm clenched (speed {args.speed-75}%)"
                elif current_state == "right":
                    cmd = f"right {args.speed-75}"
                    display = f">>> [TURN RIGHT] Right arm clenched (speed {args.speed-75}%)"
                else:  # "none"
                    cmd = "stop"
                    display = "--- [STOPPED] Arms relaxed"

                send_robot_command(sock, robot_ip, args.port, cmd)
                last_sent_time = now

                if state_changed:
                    print(f"[{time.strftime('%H:%M:%S')}] {display}")
                    last_state = current_state

            time.sleep(0.02)

    except KeyboardInterrupt:
        print("\n[Exiting] Stopping robot...")
        send_robot_command(sock, robot_ip, args.port, "stop")
        sock.close()
        print("Done. Goodbye!")


if __name__ == "__main__":
    main()
