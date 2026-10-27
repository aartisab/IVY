# IVY Person Follow (computer vision MVP)

A small computer-vision demo for **IVY**. Uses a camera to find people, lets you lock onto one person, keeps tracking that same person over time, and prints a left/right steering signal that the pole could use to stay lined up with them.

## What it does

1. **Detects people** in each camera frame using a YOLO11 model (Ultralytics). With the default pose model it also finds the head, shoulders, and hips.
2. **Tracks them** across frames, giving each person an ID (ByteTrack).
3. **Locks onto one person.** Press SPACE (most central person) or click someone. The lock follows the shoulder/hip midpoint (the torso).
4. **Keeps the same person.** If the tracker changes the person's ID (for example after someone walks in front), the program re-locks only if exactly one person is close to where the target last was and is a similar size. If it can't tell, it shows `SEARCHING` or `AMBIGUOUS` and waits. After about 45 lost frames it gives up and asks you to lock again. It doesn't between two people.
5. **Outputs a steering signal:**
   - `offset`: -1 (far left of the image) to +1 (far right). 0 is centered.
   - `turn`: `LEFT`, `RIGHT`, or `CENTERED` (a small dead zone around 0).
   - `size`: the person's height as a fraction of the frame, a rough "how close" hint. It is **not** a real distance. Distance will come from UWB and lidar.

People are told apart by position and size, not by face or clothing, so two people crossing very closely can still fool it. Nothing is saved to disk; video is only shown in the preview window.

## Requirements

- **Python 3.10 or newer** (check with `python --version`, or `python3 --version` on Mac)
- A **webcam or USB (UVC) camera**
- A screen: the program opens a preview window and uses mouse clicks and key presses
- Internet on the first run (it downloads the model)
- A few GB of free disk space (PyTorch is installed as part of Ultralytics)

## Setup and run: macOS

Open **Terminal** in the project folder, then:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python person_follow.py
```

The first time, macOS will ask for camera permission. If the window stays black, go to **System Settings → Privacy & Security → Camera** and allow your terminal app (Terminal, iTerm, or VS Code).

## Setup and run: Windows

Open **PowerShell** or **Command Prompt** in the project folder, then:

```
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python person_follow.py
```

Notes for Windows:
- If `python` isn't found, try `py` instead (`py -m venv .venv`, `py person_follow.py`), or install Python from python.org and tick **Add python.exe to PATH**.
- In PowerShell, if activation says scripts are disabled, run `Set-ExecutionPolicy -Scope Process Bypass` and activate again, or use Command Prompt.
- **"filename too long" errors during install** (common with the Microsoft Store version of Python): create the virtual environment at a short path instead, for example `python -m venv C:\ivy_venv`, then `C:\ivy_venv\Scripts\activate`, then `pip install -r requirements.txt`. Installing Python from python.org also avoids this.
- If the camera is blocked, check **Settings → Privacy & security → Camera**.

Note for me (mc lol):
.venv didn't work in the directory I was in, so for me it's
`C:\ivy_venv\Scripts\activate`
Also for some reason it only works if i copy paste the above line lol

## Every time after the first setup

Activate the environment again, then run:

```
source .venv/bin/activate        # macOS / Linux
.venv\Scripts\activate           # Windows
python person_follow.py
```

You'll see `(.venv)` at the start of your prompt when it's active. The first launch is slow because PyTorch takes a while to load. Wait for the line `Ready.` before using the window.

## Controls

| Input | Action |
|---|---|
| SPACE | Lock onto the most central person |
| Click on a person | Lock onto that person |
| R | Release the lock |
| Q | Quit |

Click the video window first so it receives key presses. In the window, the locked person is green, other people are orange, and the red dot is the point being aimed at.

## Options

```
python person_follow.py --source 1           # use a different camera (try 0, 1, 2)
python person_follow.py --model yolo11n.pt   # boxes only, no pose points; faster
python person_follow.py --imgsz 640          # larger model input; slower but sees farther
python person_follow.py --mjpg               # ask a USB camera for MJPG (faster over USB 2.0)
python person_follow.py --width 1280 --height 720
python person_follow.py --conf 0.4           # lower = detects more, with more false hits
```

| Option | Default | Meaning |
|---|---|---|
| `--source` | `0` | Camera number, or a device path such as `/dev/video0` on Linux |
| `--model` | `yolo11n-pose.pt` | Model file. Downloaded automatically the first time |
| `--imgsz` | `320` | Model input size. Smaller is faster |
| `--conf` | `0.5` | Minimum detection confidence |
| `--width`, `--height` | `640`, `480` | Requested camera size (the camera may pick the closest one it supports) |
| `--device` | `cpu` | Where to run the model |
| `--mjpg` | off | Request MJPG video from a USB camera |

## Output

While running, the terminal prints a few lines per second, for example:

```
1730000000.12 offset=-0.406 turn=LEFT size=0.62
1730000000.40 offset=+0.031 turn=CENTERED size=0.61
1730000000.65 SEARCHING
```

Statuses when no target is visible: `NOT LOCKED` (nobody chosen yet), `SEARCHING` (target out of view), `AMBIGUOUS` (several possible matches), and `LOST - press SPACE` (gave up, lock again).

## Raspberry Pi (not yet tested)

The same code is intended to run on a Raspberry Pi 5 with a **USB** camera:

```bash
sudo apt install -y v4l-utils
v4l2-ctl --list-devices                      # find the camera, e.g. /dev/video0
v4l2-ctl -d /dev/video0 --list-formats-ext   # see the resolutions it really supports
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python person_follow.py --source /dev/video0 --model yolo11n.pt --mjpg --width 640 --height 360
```

A much lower frame rate than on a laptop is expected. Ways to speed it up: use `--model yolo11n.pt`, a smaller `--imgsz`, or export the model to NCNN (`yolo export model=yolo11n.pt format=ncnn`, then `--model yolo11n_ncnn_model`). If you get a Qt/xcb/wayland window error, try `export QT_QPA_PLATFORM=xcb`. Ribbon-cable (CSI) cameras are not supported by this script.

## Known limits

- No distance measurement from the camera alone (the `size` value is only a hint).
- Identity is by position and size, so close crossings can swap people.
- Needs a display for the preview window; there is no headless mode yet.
- The lock logic has been tested with simulated detections. Testing with real people, and on the Pi, is still to do.

## Troubleshooting

| Problem | Try |
|---|---|
| `Cannot open camera` or black window | `--source 1` (or 2); check camera permissions |
| Nothing prints for a minute on first run | Normal: PyTorch is loading and the model is downloading |
| Very slow or laggy | `--model yolo11n.pt`, smaller `--imgsz`, close other apps |
| `No module named ultralytics` | The environment isn't active, or the install didn't finish. Activate it and rerun `pip install -r requirements.txt` |
| Person never gets locked | Move so your torso is visible; lower `--conf` to 0.4 |

## Files

- `person_follow.py`: the program
- `requirements.txt`: Python packages (`ultralytics`, `opencv-python`)
