"""IVY MVP: find a person, lock onto them, keep tracking them, print a steering signal.
Does NOT drive motors. Works with a laptop webcam or any USB (UVC) camera.

  python person_follow.py                      # laptop webcam
  python person_follow.py --source 1           # other camera number
  python person_follow.py --source /dev/video0 --model yolo11n.pt --mjpg      # Pi + USB camera

Keys: SPACE = lock onto most central person   click = lock onto clicked person
      R = release lock      Q = quit
Speed tips: smaller --imgsz is faster; yolo11n.pt (no pose) is faster than yolo11n-pose.pt.
"""
import argparse
import math
import sys
import threading
import time
from dataclasses import dataclass

import cv2
import numpy as np

NOSE, L_SH, R_SH, L_HIP, R_HIP = 0, 5, 6, 11, 12
BODY_POINTS = (NOSE, L_SH, R_SH, L_HIP, R_HIP)
TURN_DEADZONE = 0.10          # |offset| below this counts as CENTERED
PRINT_EVERY_S = 0.25          # throttle terminal output (printing every frame is slow)


# ----------------------------------------------------------------- detections
@dataclass
class Detection:
    id: int
    box: tuple          # x1, y1, x2, y2 in pixels
    kpts: list | None   # 17 x (x, y, conf) from the pose model, else None
    aim: tuple          # (x, y) point we steer toward
    h: float            # box height in pixels


def aim_point(box, kpts):
    """Middle of shoulders/hips (torso) if visible, else the box center."""
    if kpts is not None:
        pts = [kpts[i] for i in (L_SH, R_SH, L_HIP, R_HIP) if kpts[i][2] > 0.5]
        if len(pts) >= 2:
            return (sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts))
    return ((box[0] + box[2]) / 2, (box[1] + box[3]) / 2)


def parse(result):
    """Turn one Ultralytics result into a list of Detection (tracked people only)."""
    b = result.boxes
    if b is None or b.id is None:
        return []
    kp = result.keypoints.data.cpu().tolist() if result.keypoints is not None else [None] * len(b)
    return [Detection(tid, tuple(box), k, aim_point(box, k), box[3] - box[1])
            for tid, box, k in zip(b.id.int().tolist(), b.xyxy.tolist(), kp)]


# ------------------------------------------------------------------- follower
class Follower:
    """Remembers who we locked onto. If the tracker's ID changes (occlusion, crossing),
    re-lock only if exactly ONE person is near where the target last was and is a
    similar size. Otherwise report SEARCHING/AMBIGUOUS and wait. Never guess between people."""

    def __init__(self, max_lost_frames=45, max_jump=0.25, size_ratio=(0.6, 1.6)):
        self.max_lost, self.max_jump, self.size_ratio = max_lost_frames, max_jump, size_ratio
        self.release()

    def release(self):
        self.id, self.last, self.lost, self.status = None, None, 0, 'NOT LOCKED'

    def lock(self, d):
        self.id = d.id
        self._seen(d)

    def update(self, dets, w, h):
        """Return the target Detection if we can see it this frame, else None."""
        if self.id is None:
            return None
        same = [d for d in dets if d.id == self.id]
        if len(same) == 1:
            return self._seen(same[0])
        self.lost += 1
        if self.lost > self.max_lost:           # gave up: a human must re-lock
            self.release()
            self.status = 'LOST - press SPACE'
            return None
        lx, ly, lh = self.last
        lo, hi = self.size_ratio
        near = [d for d in dets
                if math.hypot((d.aim[0] - lx) / w, (d.aim[1] - ly) / h) < self.max_jump
                and lo < d.h / max(lh, 1) < hi]
        if len(near) == 1:                      # unambiguous: same person, new tracker ID
            self.id = near[0].id
            return self._seen(near[0])
        self.status = 'SEARCHING' if not near else 'AMBIGUOUS'
        return None

    def _seen(self, d):
        self.last, self.lost, self.status = (d.aim[0], d.aim[1], d.h), 0, 'LOCKED'
        return d


def steering(target, w, h):
    """offset -1 (far left) .. +1 (far right). size = box height / frame height, a rough
    closeness hint ONLY (not a distance; real distance comes from UWB/lidar)."""
    offset = (target.aim[0] - w / 2) / (w / 2)
    turn = 'LEFT' if offset < -TURN_DEADZONE else 'RIGHT' if offset > TURN_DEADZONE else 'CENTERED'
    return offset, target.h / h, turn


# --------------------------------------------------------------------- camera
class FrameGrabber:
    """Reads the camera on a background thread and keeps ONLY the newest frame.
    Without this, OpenCV queues frames while the model is busy, so you process old
    video and everything feels laggy. read() blocks until a new frame arrives."""

    def __init__(self, source, width, height, mjpg=False):
        src = int(source) if source.isdecimal() else source
        self._cap = cv2.VideoCapture(src)
        if not self._cap.isOpened():
            sys.exit(f'Cannot open camera {source}. Try --source 1 (or /dev/video0 on Linux)')
        if mjpg:                                      # higher fps over USB 2.0; set before size
            self._cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*'MJPG'))
        self._cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        self._cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        self._cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        self._cond = threading.Condition()
        self._frame, self._seq, self._seen, self._dead = None, 0, 0, False
        threading.Thread(target=self._loop, daemon=True).start()

    def _loop(self):
        while not self._dead:
            ok, frame = self._cap.read()
            with self._cond:
                if not ok:
                    self._dead = True
                else:
                    self._frame, self._seq = frame, self._seq + 1
                self._cond.notify_all()

    def read(self, timeout=2.0):
        """Newest unseen frame, or None if the camera stopped."""
        with self._cond:
            self._cond.wait_for(lambda: self._seq != self._seen or self._dead, timeout)
            if self._seq == self._seen:
                return None
            self._seen = self._seq
            return self._frame

    def close(self):
        self._dead = True
        self._cap.release()


# -------------------------------------------------------------------- drawing
def draw(frame, dets, target, text):
    h, w = frame.shape[:2]
    for d in dets:
        x1, y1, x2, y2 = map(int, d.box)
        col = (0, 255, 0) if target is not None and d.id == target.id else (0, 160, 255)
        cv2.rectangle(frame, (x1, y1), (x2, y2), col, 2)
        cv2.putText(frame, f'id {d.id}', (x1, max(18, y1 - 6)), 0, 0.6, col, 2)
        if d.kpts:
            for i in BODY_POINTS:
                if d.kpts[i][2] > 0.5:
                    cv2.circle(frame, (int(d.kpts[i][0]), int(d.kpts[i][1])), 4, col, -1)
    cv2.line(frame, (w // 2, 0), (w // 2, h), (255, 255, 255), 1)
    if target is not None:
        cv2.circle(frame, (int(target.aim[0]), int(target.aim[1])), 8, (0, 0, 255), -1)
    cv2.putText(frame, text, (10, 25), 0, 0.6, (0, 255, 255), 2)


# ------------------------------------------------------------------------ main
def parse_args():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument('--source', default='0', help='camera number or /dev/videoN')
    ap.add_argument('--width', type=int, default=640)
    ap.add_argument('--height', type=int, default=480)
    ap.add_argument('--model', default='yolo11n-pose.pt',
                    help='yolo11n-pose.pt (shoulders/hips) | yolo11n.pt (boxes, faster) | '
                         'an exported folder such as yolo11n_ncnn_model (fastest on Pi)')
    ap.add_argument('--imgsz', type=int, default=320, help='model input size; smaller = faster (320/416/640)')
    ap.add_argument('--conf', type=float, default=0.5)
    ap.add_argument('--device', default='cpu')
    ap.add_argument('--mjpg', action='store_true', help='ask a USB camera for MJPG (faster on USB 2.0)')
    return ap.parse_args()


def main():
    a = parse_args()
    print('Loading YOLO (the first import takes a while)...', flush=True)
    from ultralytics import YOLO
    model = YOLO(a.model)
    model.predict(np.zeros((a.height, a.width, 3), np.uint8), imgsz=a.imgsz,
                  device=a.device, verbose=False)            # warm-up so frame 1 isn't slow

    cam = FrameGrabber(a.source, a.width, a.height, a.mjpg)
    follower, clicks = Follower(), []

    def on_mouse(event, x, y, flags, param):
        if event == cv2.EVENT_LBUTTONDOWN:
            clicks.append((x, y))

    cv2.namedWindow('IVY')
    cv2.setMouseCallback('IVY', on_mouse)
    print('Ready. SPACE/click = lock on, R = release, Q = quit', flush=True)

    fps, last_t, last_print, last_msg = 0.0, time.time(), 0.0, ''
    try:
        while True:
            frame = cam.read()
            if frame is None:
                sys.exit('Camera stopped giving frames')
            h, w = frame.shape[:2]
            result = model.track(frame, persist=True, tracker='bytetrack.yaml', classes=[0],
                                 conf=a.conf, imgsz=a.imgsz, device=a.device, max_det=6,
                                 verbose=False)[0]
            dets = parse(result)

            while clicks:                                    # click = choose who to follow
                x, y = clicks.pop()
                hit = [d for d in dets if d.box[0] <= x <= d.box[2] and d.box[1] <= y <= d.box[3]]
                if len(hit) == 1:
                    follower.lock(hit[0])

            target = follower.update(dets, w, h)
            if target is not None:
                off, size, turn = steering(target, w, h)
                msg = f'offset={off:+.3f} turn={turn} size={size:.2f}'
                text = f'LOCKED  offset {off:+.2f}  turn {turn}  size {size:.2f}'
            else:
                msg = text = follower.status

            now = time.time()                                # smoothed fps + throttled printing
            fps = 0.9 * fps + 0.1 / max(now - last_t, 1e-6)
            last_t = now
            if msg != last_msg and now - last_print >= PRINT_EVERY_S:
                print(f'{now:.2f} {msg}', flush=True)
                last_print, last_msg = now, msg

            draw(frame, dets, target, f'{text}   {fps:.0f} fps')
            cv2.imshow('IVY', frame)
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                break
            if key == ord('r'):
                follower.release()
            if key == ord(' ') and dets:                     # biggest person nearest the middle
                follower.lock(min(dets, key=lambda d: abs(d.aim[0] - w / 2) / w - 0.3 * d.h / h))
    finally:
        cam.close()
        cv2.destroyAllWindows()


if __name__ == '__main__':
    main()