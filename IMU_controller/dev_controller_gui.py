#!/usr/bin/env python3
"""
IVY viewer - shows the Switch Pro Controller sticks and the MPU-6050 tilt
coming from the ESP32.

  pip install pyserial
  python ivy_controller_view.py               # GUI, auto-detects the ESP32 port
  python ivy_controller_view.py --terminal    # live readout (type c + Enter to calibrate)
  python ivy_controller_view.py --port /dev/cu.usbserial-0001

Calibrate: press HOME on the controller, or the Calibrate button in the GUI.
Close the Arduino Serial Monitor first - only one program can hold the port.
"""
import argparse
import math
import queue
import sys
import threading
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit("pyserial is not installed. Run:  pip install pyserial")

BAUD = 115200
TIP_ON_DEG = 10.0      # display only - must match TIP_ON_DEG in the sketch
TILT_RANGE_DEG = 30.0  # tilt at the edge of the lean plot
HINTS = ("cp210", "ch340", "ch910", "usb-serial", "silicon labs", "wch", "usbserial", "uart", "slab")

IMU_TEXT = {
    "NOIMU": "IMU NOT FOUND - check wiring",
    "NOCAL": "Not calibrated - stand rod up, press HOME",
    "CAL":   "Calibrating... hold still",
    "OK":    "UPRIGHT",
    "TIP":   "TIPPING",
}


# ---------------- Serial ----------------
class Link:
    """Owns the serial port in a background thread; lets the UI send commands."""

    def __init__(self, port, q):
        self.port, self.q = port, q
        self.ser = None
        self.stop = threading.Event()
        threading.Thread(target=self._run, daemon=True).start()

    def send(self, text):
        try:
            if self.ser and self.ser.is_open:
                self.ser.write(text.encode())
                return True
        except serial.SerialException:
            pass
        self.q.put(("status", "Not connected to ESP32 - can't send"))
        return False

    def _run(self):
        while not self.stop.is_set():
            try:
                s = serial.Serial()
                s.port, s.baudrate, s.timeout = self.port, BAUD, 0.5
                s.dtr = False   # don't reset the ESP32 when the port opens
                s.rts = False
                s.open()
                self.ser = s
                self.q.put(("status", f"Opened {self.port}"))
                while not self.stop.is_set():
                    line = s.readline().decode(errors="ignore").strip()
                    if line.startswith("$,"):
                        d = parse(line)
                        if d:
                            self.q.put(("data", d))
                    elif line.startswith("#"):
                        self.q.put(("status", line[1:].strip()))
            except (serial.SerialException, OSError) as e:
                self.ser = None
                self.q.put(("status", f"ESP32 not connected ({e.__class__.__name__}) - retrying"))
                self.q.put(("lost", None))
                time.sleep(1)


def find_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        text = f"{p.device} {p.description} {p.manufacturer}".lower()
        if any(h in text for h in HINTS):
            return p.device
    return ports[0].device if len(ports) == 1 else None


def parse(line):
    p = line.split(",")
    if len(p) != 14:
        return None
    try:
        d = dict(zip(("fwd", "turn", "angle", "mag", "lx", "ly", "rx", "ry"), map(float, p[1:9])))
        d["buttons"] = p[9]
        d["conn"] = p[10] == "1"
        d["imu"] = p[11]
        d["tilt"] = float(p[12])
        d["tiltdir"] = float(p[13])
    except ValueError:
        return None
    return d


def direction_words(d):
    if not d["conn"]:
        return "NO CONTROLLER"
    parts = []
    if d["fwd"]:
        parts.append("FORWARD" if d["fwd"] > 0 else "BACK")
    if d["turn"]:
        parts.append("RIGHT" if d["turn"] > 0 else "LEFT")
    return " + ".join(parts) or "STOPPED"


def tilt_words(d):
    t = IMU_TEXT.get(d["imu"], d["imu"])
    if d["imu"] in ("OK", "TIP"):
        t += f" {d['tilt']:4.1f}°"
        if d["imu"] == "TIP":
            t += f" toward {d['tiltdir']:+.0f}°"
    return t


# ---------------- Terminal mode ----------------
def run_terminal(q, link):
    def keys():
        for line in sys.stdin:
            if line.strip().lower() == "c":
                link.send("c")

    threading.Thread(target=keys, daemon=True).start()
    print("Type c + Enter to calibrate. Ctrl+C to quit.")
    try:
        while True:
            kind, val = q.get()
            if kind == "status":
                sys.stdout.write("\r" + " " * 130 + "\r[esp32] " + val + "\n")
            elif kind == "data":
                d = val
                line = (f"FWD {d['fwd']:+6.1f}%  TURN {d['turn']:+6.1f}%  {direction_words(d):<15}"
                        f"| btn: {d['buttons']:<8} | {tilt_words(d)}")
                sys.stdout.write("\r" + line.ljust(130))
            sys.stdout.flush()
    except KeyboardInterrupt:
        print()


# ---------------- GUI mode ----------------
def run_gui(q, link):
    import tkinter as tk

    W, H, R = 980, 470, 95
    BG, RING, DOT, GOOD, BAD, WARN, TXT, DIM = (
        "#14171c", "#3a414d", "#4fc3f7", "#66bb6a", "#ef5350", "#ffb74d", "#e6e6e6", "#9aa4b2")
    LC, RC, TC = (200, 180), (470, 180), (790, 180)

    root = tk.Tk()
    root.title("IVY - Controller + Tilt")
    root.configure(bg=BG)
    c = tk.Canvas(root, width=W, height=H, bg=BG, highlightthickness=0)
    c.pack()

    def ring(center, label):
        cx, cy = center
        c.create_oval(cx - R, cy - R, cx + R, cy + R, outline=RING, width=2)
        c.create_line(cx - R, cy, cx + R, cy, fill=RING)
        c.create_line(cx, cy - R, cx, cy + R, fill=RING)
        c.create_text(cx, cy - R - 20, text=label, fill=TXT, font=("Helvetica", 12, "bold"))

    ring(LC, "LEFT STICK - drive")
    ring(RC, "RIGHT STICK - steer")

    # Tilt panel: top-down view, center = calibrated upright, edge = TILT_RANGE_DEG
    tx, ty = TC
    tilt_outer = c.create_oval(tx - R, ty - R, tx + R, ty + R, outline=RING, width=3)
    r_thr = TIP_ON_DEG / TILT_RANGE_DEG * R
    c.create_oval(tx - r_thr, ty - r_thr, tx + r_thr, ty + r_thr, outline=WARN, dash=(4, 3))
    c.create_line(tx - R, ty, tx + R, ty, fill=RING)
    c.create_line(tx, ty - R, tx, ty + R, fill=RING)
    c.create_text(tx, ty - R - 20, text="ROD TILT (top view)", fill=TXT, font=("Helvetica", 12, "bold"))
    c.create_text(tx, ty - R + 10, text="0°", fill=DIM, font=("Helvetica", 9))
    c.create_text(tx + R + 22, ty, text=f"{TILT_RANGE_DEG:.0f}°", fill=DIM, font=("Helvetica", 9))
    tline = c.create_line(tx, ty, tx, ty, fill=GOOD, width=3)
    tdot = c.create_oval(0, 0, 0, 0, fill=GOOD, outline="")
    tilt_label = c.create_text(tx, ty + R + 26, text="", fill=TXT, font=("Helvetica", 15, "bold"))

    lline = c.create_line(*LC, *LC, fill=DOT, width=2)
    ldot = c.create_oval(0, 0, 0, 0, fill=DOT, outline="")
    rdot = c.create_oval(0, 0, 0, 0, fill=DOT, outline="")

    fx, fy = 70, LC[1]
    c.create_rectangle(fx - 12, fy - R, fx + 12, fy + R, outline=RING)
    c.create_text(fx, fy + R + 14, text="fwd", fill=TXT)
    fbar = c.create_rectangle(0, 0, 0, 0, fill=GOOD, outline="")

    bx, by = RC[0], RC[1] + R + 30
    c.create_rectangle(bx - R, by - 10, bx + R, by + 10, outline=RING)
    c.create_text(bx, by + 22, text="turn", fill=TXT)
    tbar = c.create_rectangle(0, 0, 0, 0, fill=GOOD, outline="")

    banner = c.create_text((LC[0] + RC[0]) // 2, LC[1], text="", fill=BAD,
                           font=("Helvetica", 26, "bold"))
    readout = c.create_text(W // 2, H - 72, text="", fill=TXT, font=("Courier", 14, "bold"))
    buttons = c.create_text(W // 2, H - 46, text="", fill=TXT, font=("Courier", 12))
    status = c.create_text(W // 2, H - 18, text="Waiting for ESP32...", fill=DIM, font=("Helvetica", 11))

    tk.Button(root, text="Calibrate upright (same as HOME)", command=lambda: link.send("c"),
              font=("Helvetica", 12)).pack(pady=(0, 10))

    def clamp(v, lim=1.0):
        return max(-lim, min(lim, v))

    def draw(d):
        lx, ly = clamp(d["lx"] / 512) * R, clamp(d["ly"] / 512) * R
        rx, ry = clamp(d["rx"] / 512) * R, clamp(d["ry"] / 512) * R
        c.coords(ldot, LC[0] + lx - 9, LC[1] + ly - 9, LC[0] + lx + 9, LC[1] + ly + 9)
        c.coords(lline, LC[0], LC[1], LC[0] + lx, LC[1] + ly)
        c.coords(rdot, RC[0] + rx - 9, RC[1] + ry - 9, RC[0] + rx + 9, RC[1] + ry + 9)
        f = d["fwd"] / 100 * R
        c.coords(fbar, fx - 10, fy - max(f, 0), fx + 10, fy - min(f, 0))
        c.itemconfig(fbar, fill=GOOD if f >= 0 else BAD)
        t = d["turn"] / 100 * R
        c.coords(tbar, bx + min(t, 0), by - 8, bx + max(t, 0), by + 8)
        c.itemconfig(banner, text="" if d["conn"] else "NO CONTROLLER")

        # Tilt
        imu = d["imu"]
        if imu in ("OK", "TIP"):
            r = min(d["tilt"] / TILT_RANGE_DEG, 1.0) * R
            a = math.radians(d["tiltdir"])
            px, py = tx - math.sin(a) * r, ty - math.cos(a) * r   # 0° up, viewed from above
            col = BAD if imu == "TIP" else GOOD
        else:
            px, py = tx, ty
            col = WARN if imu in ("NOCAL", "CAL") else BAD
        c.coords(tline, tx, ty, px, py)
        c.coords(tdot, px - 10, py - 10, px + 10, py + 10)
        c.itemconfig(tline, fill=col)
        c.itemconfig(tdot, fill=col)
        c.itemconfig(tilt_outer, outline=BAD if imu == "TIP" else RING)
        c.itemconfig(tilt_label, text=tilt_words(d), fill=col)

        c.itemconfig(readout, text=(f"FWD {d['fwd']:+5.0f}%  TURN {d['turn']:+5.0f}%  "
                                    f"HEADING {d['angle']:+5.0f}°  PUSH {d['mag']:3.0f}%"))
        c.itemconfig(buttons, text=f"{direction_words(d)}    buttons: {d['buttons']}")

    blank = {"fwd": 0, "turn": 0, "angle": 0, "mag": 0, "lx": 0, "ly": 0, "rx": 0, "ry": 0,
             "buttons": "-", "conn": False, "imu": "NOIMU", "tilt": 0, "tiltdir": 0}

    def poll():
        latest = None
        try:
            while True:
                kind, val = q.get_nowait()
                if kind == "status":
                    c.itemconfig(status, text=val)
                elif kind == "data":
                    latest = val
                elif kind == "lost":
                    latest = blank   # ESP32 unplugged: zero everything
        except queue.Empty:
            pass
        if latest:
            draw(latest)
        root.after(16, poll)

    draw(blank)
    root.after(16, poll)
    root.mainloop()


# ---------------- Main ----------------
def main():
    ap = argparse.ArgumentParser(description="IVY controller + tilt viewer")
    ap.add_argument("--port", help="serial port, e.g. /dev/cu.usbserial-0001 or COM5")
    ap.add_argument("--terminal", action="store_true", help="text readout instead of GUI")
    args = ap.parse_args()

    port = args.port or find_port()
    if not port:
        print("Couldn't pick a port automatically. Available ports:")
        for p in serial.tools.list_ports.comports():
            print(f"  {p.device:25s} {p.description}")
        sys.exit("Re-run with --port <one of the above>")

    q = queue.Queue()
    link = Link(port, q)
    try:
        run_terminal(q, link) if args.terminal else run_gui(q, link)
    finally:
        link.stop.set()


if __name__ == "__main__":
    main()
