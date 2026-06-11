import serial
import time
import re
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from mpl_toolkits.mplot3d import Axes3D
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from math import atan2, sqrt, degrees
import numpy as np
import os
import threading

TTY_PORT = "/dev/ttyUSB0"
BAUD_RATE = 115200

# ICM-20948 device paths (CLASS_NAME="imu", DEVICE_NAME="icm20948")
COMMANDS = [
    "cd ../..",
    "echo 1 > /sys/class/imu/icm20948/init_sensor",
    "cat /dev/icm20948"
]

def wait_for_board():
    print("[INFO] Waiting for board to connect...")
    while not os.path.exists(TTY_PORT):
        time.sleep(1)
    print(f"[INFO] Board connected at {TTY_PORT}")

def parse_line(line):
    # ICM-20948 printk format:
    # "ICM-20948 Accel [%d]  X: %d mg  Y: %d mg  Z: %d mg"
    match = re.search(
        r"ICM-20948 Accel \[\d+\]\s+X:\s*(-?\d+)\s+mg\s+Y:\s*(-?\d+)\s+mg\s+Z:\s*(-?\d+)\s+mg",
        line
    )
    if match:
        return [int(match.group(1)), int(match.group(2)), int(match.group(3))]
    return None

def read_serial_lines(ser, buffer, timeout=2):
    start = time.time()
    while time.time() - start < timeout:
        line = ser.readline().decode(errors='ignore').strip()
        if line:
            buffer.append(line)
            if len(buffer) > 100:
                buffer.pop(0)

def get_orientation(x, y, z):
    # Convert milli-g → g for trig
    x /= 1000.0
    y /= 1000.0
    z /= 1000.0
    pitch = degrees(atan2(-x, sqrt(y * y + z * z)))
    roll  = degrees(atan2(y, z))
    return pitch, roll

def draw_3d_board(ax3d, pitch, roll):
    ax3d.clear()
    size = 1.0

    board = np.array([
        [-size, -size, 0],
        [ size, -size, 0],
        [ size,  size, 0],
        [-size,  size, 0]
    ])

    pitch_rad = np.radians(pitch)
    roll_rad  = np.radians(roll)

    R_pitch = np.array([
        [1,                0,                 0],
        [0,  np.cos(pitch_rad), -np.sin(pitch_rad)],
        [0,  np.sin(pitch_rad),  np.cos(pitch_rad)]
    ])

    R_roll = np.array([
        [ np.cos(roll_rad), 0, np.sin(roll_rad)],
        [0,                 1,               0],
        [-np.sin(roll_rad), 0, np.cos(roll_rad)]
    ])

    R = R_roll @ R_pitch
    rotated_board = board @ R.T

    verts = [rotated_board]
    ax3d.add_collection3d(
        Poly3DCollection(verts, color='lightblue', alpha=0.8, edgecolor='k')
    )

    # Orange line along board X-axis (board-relative)
#    orange_start = R @ np.array([-0.5, -0.3, 0])
#   orange_end   = R @ np.array([ 0.5, -0.3, 0])
#   ax3d.plot(
#       [orange_start[0], orange_end[0]],
#        [orange_start[1], orange_end[1]],
#        [orange_start[2], orange_end[2]],
#        color='orange', linewidth=3
#    )

    # Red line perpendicular to orange (board-relative Y direction)
    red_start = R @ np.array([0.5, -0.3, 0.0])
    red_end   = R @ np.array([0.5,  0.3, 0.0])
    ax3d.plot(
        [red_start[0], red_end[0]],
        [red_start[1], red_end[1]],
        [red_start[2], red_end[2]],
        color='red', linewidth=3
    )

    # World-frame coordinate arrows
    origin = np.array([0, 0, 0])
    ax3d.quiver(*origin, 0, 1, 0, color='r', length=1.5, arrow_length_ratio=0.1)
    ax3d.quiver(*origin, 1, 0, 0, color='g', length=1.5, arrow_length_ratio=0.1)
    ax3d.quiver(*origin, 0, 0, -1, color='b', length=1.5, arrow_length_ratio=0.1)

    ax3d.text(0,   1.6,  0,    'X', color='r', fontsize=10)
    ax3d.text(1.6, 0,    0,    'Y', color='g', fontsize=10)
    ax3d.text(0,   0,   -1.6,  'Z', color='b', fontsize=10)

    ax3d.set_xlim(-2, 2)
    ax3d.set_ylim(-2, 2)
    ax3d.set_zlim(-2, 2)
    ax3d.set_title("ICM-20948 Board Orientation (3D)")


def animate(i, xs, ys, zs, ax, buffer, ax3d):
    for line in reversed(buffer[-10:]):
        vals = parse_line(line)
        if vals:
            x, y, z = vals
            xs.append(x)
            ys.append(y)
            zs.append(z)
            if len(xs) > 50:
                xs.pop(0)
                ys.pop(0)
                zs.pop(0)

            pitch, roll = get_orientation(x, y, z)
            draw_3d_board(ax3d, pitch, roll)

            ax.clear()
            ax.plot(xs, label='X (mg)')
            ax.plot(ys, label='Y (mg)')
            ax.plot(zs, label='Z (mg)')
            ax.set_ylim(-2000, 2000)
            ax.legend()
            ax.set_title("Live ICM-20948 Accelerometer Data")
            ax.set_ylabel("milli-g")

            # Orientation hint messages (same ±1g thresholds as before)
            msg = ""
            if x > 900:
                msg += "Board standing on power-supply side\n"
            elif x < -900:
                msg += "Board standing on opposite side\n"

            if y > 900:
                msg += "Board lying on power-supply side\n"
            elif y < -900:
                msg += "Board lying on opposite side\n"

            if z > 900:
                msg += "Board in reverse (face-down)\n"
            elif z < -900:
                msg += "Board lying flat (face-up)\n"

            if msg:
                ax.text(
                    0.5, 0.95, msg.strip(),
                    transform=ax.transAxes,
                    fontsize=10, color='darkblue',
                    ha='center', va='top',
                    bbox=dict(facecolor='white', alpha=0.8, edgecolor='blue')
                )
            break


def main():
    wait_for_board()

    try:
        ser = serial.Serial(TTY_PORT, BAUD_RATE, timeout=1)
        print(f"[INFO] Opened serial port {TTY_PORT}")

        for cmd in COMMANDS:
            ser.write((cmd + "\n").encode())
            time.sleep(1)

        xs, ys, zs = [], [], []
        buffer = []

        fig = plt.figure(figsize=(12, 6))
        ax   = fig.add_subplot(121)
        ax3d = fig.add_subplot(122, projection='3d')

        # ICM-20948 runs at ~112 Hz (msleep 9 ms); refresh every 100 ms
        ani = animation.FuncAnimation(
            fig, animate,
            fargs=(xs, ys, zs, ax, buffer, ax3d),
            interval=100,
            cache_frame_data=False
        )

        def background_read():
            while True:
                read_serial_lines(ser, buffer)

        thread = threading.Thread(target=background_read, daemon=True)
        thread.start()

        plt.show()
        ser.close()

    except Exception as e:
        print(f"[ERROR] {e}")


if __name__ == "__main__":
    main()
