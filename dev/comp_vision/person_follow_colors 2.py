"""IVY person follower with clothing color detection and cautious re-identification.

pip install ultralytics opencv-python numpy
python person_follow_colors_updated.py

SPACE: lock central person; click: select person; R: release; Q: quit.
Only prints steering instructions; does not control motors.
"""
import argparse
import math
import sys
import threading
import time
from dataclasses import dataclass

import cv2
import numpy as np

NOSE, L_SH, R_SH, L_HIP, R_HIP, L_KNEE, R_KNEE = 0, 5, 6, 11, 12, 13, 14
BODY_POINTS = (NOSE, L_SH, R_SH, L_HIP, R_HIP)
TURN_DEADZONE = 0.10
PRINT_EVERY_S = 0.25


@dataclass
class Detection:
    id: int
    box: tuple
    kpts: list | None
    aim: tuple
    h: float
    shirt_color: str = 'UNKNOWN'
    pants_color: str = 'UNKNOWN'
    shirt_hist: np.ndarray | None = None


def aim_point(box, kpts):
    if kpts is not None:
        pts = [kpts[i] for i in (L_SH, R_SH, L_HIP, R_HIP) if kpts[i][2] > 0.5]
        if len(pts) >= 2:
            return (sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts))
    return ((box[0] + box[2]) / 2, (box[1] + box[3]) / 2)


def parse(result):
    b = result.boxes
    if b is None or b.id is None:
        return []
    kp = result.keypoints.data.cpu().tolist() if result.keypoints is not None else [None] * len(b)
    return [Detection(tid, tuple(box), k, aim_point(box, k), box[3] - box[1])
            for tid, box, k in zip(b.id.int().tolist(), b.xyxy.tolist(), kp)]


def clothing_crop(frame, d, upper=True):
    """Take a conservative central clothing crop; use pose if available."""
    H, W = frame.shape[:2]
    x1, y1, x2, y2 = d.box
    bw, bh = x2 - x1, y2 - y1
    if bw <= 0 or bh <= 0:
        return None
    if not upper:
        # Hips locate the waist; knees are optional (often off-screen).
        if d.kpts is None:
            return None
        hips = [d.kpts[i] for i in (L_HIP, R_HIP) if d.kpts[i][2] >= 0.45]
        if len(hips) != 2:
            return None
        lh, rh = hips
        hip_y = max(lh[1], rh[1])
        left, right = sorted((lh[0], rh[0]))
        span = right - left
        if span < 12:
            return None
        # Sample the upper thighs, just below the waist. Don't require knees.
        # A real visible patch below the hips must be present in the image.
        visible_bottom = min(H, y2)
        available = visible_bottom - hip_y
        if available < max(25, 0.07 * bh):
            return None
        xa = max(0, int(left + 0.18 * span))
        xb = min(W, int(right - 0.18 * span))
        ya = max(0, int(hip_y + 0.25 * available))
        yb = min(H, int(hip_y + 0.65 * available))
        if xb - xa < 8 or yb - ya < 12:
            return None
        return frame[ya:yb, xa:xb]
    # Prefer a compact chest patch below the shoulder line. Avoid face, arms,
    # and the lower torso (which may be occluded or outside the frame).
    if d.kpts is not None and all(d.kpts[i][2] >= 0.5 for i in (L_SH, R_SH)):
        ls, rs = d.kpts[L_SH], d.kpts[R_SH]
        sx1, sx2 = sorted((ls[0], rs[0]))
        sy = (ls[1] + rs[1]) / 2
        span = sx2 - sx1
        if span >= 14:
            cx = (sx1 + sx2) / 2
            xa, xb = cx - .28 * span, cx + .28 * span
            ya = sy + .22 * span
            yb = sy + .68 * span
            # Do not sample beyond a confidently visible hip line.
            hips = [d.kpts[i] for i in (L_HIP, R_HIP) if d.kpts[i][2] >= .5]
            if hips:
                yb = min(yb, min(p[1] for p in hips) - 3)
            x1, x2, y1, y2 = xa, xb, ya, yb
        else:
            return None
    else:
        # Without shoulder keypoints, a bounding box may include the face.
        # Do not invent a shirt color from that ambiguous region.
        return None
    x1, x2 = max(0, int(x1)), min(W, int(x2))
    y1, y2 = max(0, int(y1)), min(H, int(y2))
    if x2 - x1 < 8 or y2 - y1 < 8:
        return None
    return frame[y1:y2, x1:x2]


def color_name(crop):
    if crop is None or crop.size == 0:
        return 'UNKNOWN'
    hsv = cv2.cvtColor(crop, cv2.COLOR_BGR2HSV)
    # Count colored pixels instead of taking a median hue (hue wraps at red).
    h, s, v = cv2.split(hsv)
    colored = (s >= 55) & (v >= 45)
    if np.count_nonzero(colored) < .12 * h.size:
        val = float(np.median(v))
        if val < 60:
            return 'BLACK'
        return 'WHITE' if val >= 190 else 'GRAY'

    hue = h[colored]
    sat = s[colored]
    val = v[colored]
    labels = {
        'RED': ((hue < 11) | (hue >= 170)),
        'ORANGE': ((hue >= 11) & (hue < 20)),
        'YELLOW': ((hue >= 20) & (hue < 35)),
        'GREEN': ((hue >= 35) & (hue < 85)),
        'BLUE': ((hue >= 85) & (hue < 130)),
        'PURPLE': ((hue >= 130) & (hue < 170)),
    }
    # Brown is a *dark orange*, not a dark red.
    labels['BROWN'] = labels['ORANGE'] & (val < 155)
    labels['ORANGE'] = labels['ORANGE'] & (val >= 155)
    scores = {name: int(np.count_nonzero(mask)) for name, mask in labels.items()}
    best = max(scores, key=scores.get)
    return best if scores[best] >= .25 * len(hue) else 'UNKNOWN'


def color_histogram(crop):
    if crop is None:
        return None
    hsv = cv2.cvtColor(crop, cv2.COLOR_BGR2HSV)
    hist = cv2.calcHist([hsv], [0, 1], None, [18, 8], [0, 180, 0, 256])
    cv2.normalize(hist, hist, alpha=1, norm_type=cv2.NORM_L1)
    return hist


def add_appearance(frame, dets):
    for d in dets:
        shirt = clothing_crop(frame, d, upper=True)
        pants = clothing_crop(frame, d, upper=False)
        d.shirt_color = color_name(shirt)
        d.pants_color = color_name(pants) if pants is not None else 'NOT VISIBLE'
        d.shirt_hist = color_histogram(shirt)


class Follower:
    """Keep target ID; only reacquire a new ID when position, size AND appearance agree.

    Appearance is an extra veto, not proof of identity. Ambiguous cases stop following.
    """
    def __init__(self, max_lost_frames=45, max_jump=.25, size_ratio=(.6, 1.6), min_similarity=.60):
        self.max_lost, self.max_jump = max_lost_frames, max_jump
        self.size_ratio, self.min_similarity = size_ratio, min_similarity
        self.release()

    def release(self):
        self.id, self.last, self.lost, self.status = None, None, 0, 'NOT LOCKED'
        self.reference_hist = None
        self.reference_color = 'UNKNOWN'

    def lock(self, d):
        self.id = d.id
        self.reference_hist = None if d.shirt_hist is None else d.shirt_hist.copy()
        self.reference_color = d.shirt_color
        self._seen(d)

    def _appearance_ok(self, d):
        if self.reference_hist is None or d.shirt_hist is None:
            return False  # Do not reacquire without appearance evidence.
        similarity = cv2.compareHist(self.reference_hist, d.shirt_hist, cv2.HISTCMP_CORREL)
        return similarity >= self.min_similarity

    def update(self, dets, w, h):
        if self.id is None:
            return None
        same = [d for d in dets if d.id == self.id]
        if len(same) == 1:
            # ByteTrack IDs can occasionally swap. Refuse obvious appearance mismatch.
            if self.reference_hist is not None and same[0].shirt_hist is not None and not self._appearance_ok(same[0]):
                self.status = 'APPEARANCE MISMATCH - STOP'
                return None
            return self._seen(same[0])
        self.lost += 1
        if self.lost > self.max_lost:
            self.release()
            self.status = 'LOST - press SPACE'
            return None
        lx, ly, lh = self.last
        lo, hi = self.size_ratio
        near = [d for d in dets
                if math.hypot((d.aim[0] - lx) / w, (d.aim[1] - ly) / h) < self.max_jump
                and lo < d.h / max(lh, 1) < hi]
        matching = [d for d in near if self._appearance_ok(d)]
        if len(near) == 1 and len(matching) == 1:
            self.id = matching[0].id
            return self._seen(matching[0])
        self.status = 'AMBIGUOUS - STOP' if near else 'SEARCHING - STOP'
        return None

    def _seen(self, d):
        self.last = (d.aim[0], d.aim[1], d.h)
        self.lost, self.status = 0, 'LOCKED'
        return d


def steering(target, w, h):
    offset = (target.aim[0] - w / 2) / (w / 2)
    turn = 'LEFT' if offset < -TURN_DEADZONE else 'RIGHT' if offset > TURN_DEADZONE else 'CENTERED'
    return offset, target.h / h, turn


class FrameGrabber:
    def __init__(self, source, width, height, mjpg=False):
        src = int(source) if source.isdecimal() else source
        self._cap = cv2.VideoCapture(src)
        if not self._cap.isOpened():
            sys.exit(f'Cannot open camera {source}')
        if mjpg:
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
        with self._cond:
            self._cond.wait_for(lambda: self._seq != self._seen or self._dead, timeout)
            if self._seq == self._seen:
                return None
            self._seen = self._seq
            return self._frame.copy()

    def close(self):
        self._dead = True
        self._cap.release()


def draw(frame, dets, target, text):
    h, w = frame.shape[:2]
    for d in dets:
        x1, y1, x2, y2 = map(int, d.box)
        col = (0, 255, 0) if target is not None and d.id == target.id else (0, 160, 255)
        cv2.rectangle(frame, (x1, y1), (x2, y2), col, 2)
        label = f'ID {d.id}  shirt:{d.shirt_color}  pants:{d.pants_color}'
        cv2.putText(frame, label, (x1, max(18, y1 - 6)), cv2.FONT_HERSHEY_SIMPLEX, .45, col, 2)
        if d.kpts:
            for i in BODY_POINTS:
                if d.kpts[i][2] > .5:
                    cv2.circle(frame, (int(d.kpts[i][0]), int(d.kpts[i][1])), 4, col, -1)
    cv2.line(frame, (w // 2, 0), (w // 2, h), (255, 255, 255), 1)
    if target is not None:
        cv2.circle(frame, (int(target.aim[0]), int(target.aim[1])), 8, (0, 0, 255), -1)
    cv2.putText(frame, text, (10, 25), cv2.FONT_HERSHEY_SIMPLEX, .5, (0, 255, 255), 2)


def parse_args():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', default='0')
    ap.add_argument('--width', type=int, default=640)
    ap.add_argument('--height', type=int, default=480)
    ap.add_argument('--model', default='yolo11n-pose.pt')
    ap.add_argument('--imgsz', type=int, default=320)
    ap.add_argument('--conf', type=float, default=.5)
    ap.add_argument('--device', default='cpu')
    ap.add_argument('--mjpg', action='store_true')
    return ap.parse_args()


def main():
    a = parse_args()
    print('Loading YOLO...', flush=True)
    from ultralytics import YOLO
    model = YOLO(a.model)
    model.predict(np.zeros((a.height, a.width, 3), np.uint8), imgsz=a.imgsz,
                  device=a.device, verbose=False)
    cam = FrameGrabber(a.source, a.width, a.height, a.mjpg)
    follower, clicks = Follower(), []

    def on_mouse(event, x, y, flags, param):
        if event == cv2.EVENT_LBUTTONDOWN:
            clicks.append((x, y))

    cv2.namedWindow('IVY')
    cv2.setMouseCallback('IVY', on_mouse)
    print('Ready. SPACE/click = lock on, R = release, Q = quit', flush=True)
    fps, last_t, last_print, last_msg = 0., time.time(), 0., ''
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
            add_appearance(frame, dets)
            while clicks:
                x, y = clicks.pop()
                hit = [d for d in dets if d.box[0] <= x <= d.box[2] and d.box[1] <= y <= d.box[3]]
                if len(hit) == 1:
                    follower.lock(hit[0])
            target = follower.update(dets, w, h)
            if target is not None:
                off, size, turn = steering(target, w, h)
                msg = f'ID {target.id} shirt={target.shirt_color} pants={target.pants_color} offset={off:+.3f} turn={turn} size={size:.2f}'
                label = f'LOCKED shirt:{target.shirt_color} pants:{target.pants_color} turn:{turn}'
            else:
                msg = label = follower.status
            now = time.time()
            fps = .9 * fps + .1 / max(now - last_t, 1e-6)
            last_t = now
            if msg != last_msg and now - last_print >= PRINT_EVERY_S:
                print(f'{now:.2f} {msg}', flush=True)
                last_print, last_msg = now, msg
            draw(frame, dets, target, f'{label}  {fps:.0f} fps')
            cv2.imshow('IVY', frame)
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                break
            if key == ord('r'):
                follower.release()
            if key == ord(' ') and dets:
                follower.lock(min(dets, key=lambda d: abs(d.aim[0] - w / 2) / w - .3 * d.h / h))
    finally:
        cam.close()
        cv2.destroyAllWindows()


if __name__ == '__main__':
    main()
