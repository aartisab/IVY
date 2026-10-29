/*
 * IVY - remote motor control
 *
 * Combines:
 *   dev/motor_control    ESC output with ramping + Serial Monitor commands
 *   dev/IMU_controller   Switch Pro Controller (Bluepad32) + MPU-6050 tilt sensing
 *
 * The LEFT STICK is a throttle: pushing it up ramps the motor up, releasing it
 * ramps the motor back down. Pulling it down does nothing (the ESC only spins one
 * direction for now). Full stick up = MAX_SPEED_PERCENT.
 *
 * Throttle safety: after boot, a controller (re)connect, a stop/e-stop or a mode
 * change, the stick has to come back to center before it will drive the motor again.
 *
 * Control modes:
 *   STICK  (default)  left stick drives the motor
 *   SERIAL            original motor_control behavior (s / x / p / + / -)
 *
 * Serial commands (set the Serial Monitor line ending to Newline or Both NL & CR):
 *   m / mode       toggle STICK / SERIAL control (motor stops on switch)
 *   s / start      start the motor (SERIAL mode, ramps up to set speed)
 *   x / stop       stop the motor (ramps down, any mode)
 *   p <0-100>      set speed in percent (SERIAL mode, e.g. "p 30")
 *   + / -          speed up / down by 5% (SERIAL mode)
 *   e / estop      emergency stop (immediate stop, no ramp, any mode)
 *   c / cal        calibrate the IMU (same as HOME on the controller)
 *   t / telemetry  toggle the 20 Hz "$,..." telemetry lines
 *   ? / status     print current state
 *   h / help       print this list
 *
 * Telemetry ("$,...") is the same as IMU_controller with the ESC pulse (us) appended
 * as a last field, so dev_controller_gui.py still works. Status lines start with "#".
 *
 * Board: esp32_bluepad32 -> ESP32 Dev Module (see dev/IMU_controller/README.md)
 * Library: ESP32Servo (Arduino Library Manager -> "ESP32Servo" by Kevin Harrington).
 */

#include <Bluepad32.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <math.h>

// ---------------- Controller / IMU tunables ----------------
const int DEADZONE = 40; // raw stick units (range about -512..511)
const float AXIS_MAX = 512.0f;
const uint32_t PRINT_PERIOD_MS = 50; // 20 Hz output
const bool FORGET_ON_BOOT = true;    // true = re-pair controller every boot

const int I2C_SDA = 21;
const int I2C_SCL = 22;
const uint8_t MPU_ADDR = 0x68; // 0x69 if the AD0 pin is tied to 3V3

const float TIP_ON_DEG = 10.0f;      // tilt beyond this -> TIPPING
const float TIP_OFF_DEG = 6.0f;      // must come back under this -> UPRIGHT (hysteresis)
const float FILTER_ALPHA = 0.98f;    // gyro vs accel trust (higher = smoother, slower to correct)
const uint32_t IMU_PERIOD_US = 5000; // 200 Hz IMU sampling
const uint32_t CAL_MS = 600;         // how long to average when calibrating
const bool RUMBLE = true;            // buzz the controller on calibrate / tip

// ---------------- Motor tunables ----------------
const int ESC_PIN = 18;

const int PULSE_STOP_US = 1000;      // stopped / zero throttle
const int PULSE_MAX_US = 2000;       // full throttle

const int MAX_SPEED_PERCENT = 60;    // safety cap while testing (also full stick)
const int DEFAULT_SPEED_PERCENT = 20;

const unsigned long ARM_TIME_MS = 3000;       // stop signal held at boot so the ESC arms
const unsigned long RAMP_INTERVAL_MS = 20;    // how often the output is updated
const int RAMP_STEP_US = 5;                   // max pulse change per update (~250 us/s)

// ---------------- Small 3D vector helpers ----------------
struct V3 {
  float x, y, z;
};
static inline V3 vAdd(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static inline V3 vSub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static inline V3 vScale(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static inline float vDot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 vCross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static inline float vLen(V3 a) { return sqrtf(vDot(a, a)); }
static inline V3 vNorm(V3 a) {
  float l = vLen(a);
  return l > 1e-9f ? vScale(a, 1.0f / l) : a;
}

// ---------------- Controller / IMU state ----------------
ControllerPtr pad = nullptr;
uint32_t lastPrint = 0, lastWaitMsg = 0;
bool prevCalBtn = false;
bool telemetryOn = true;

enum ImuState {
  IMU_NONE,
  IMU_NOCAL,
  IMU_CAL,
  IMU_OK,
  IMU_TIP
};
const char *IMU_NAMES[] = {"NOIMU", "NOCAL", "CAL", "OK", "TIP"};
ImuState imuState = IMU_NONE;

V3 up = {0, 0, 1};
bool upInit = false;
V3 refUp = {0, 0, 1};              // up direction captured at calibration
V3 e1 = {1, 0, 0}, e2 = {0, 1, 0}; // axes around the rod for lean direction
V3 gyroBias = {0, 0, 0};
float tiltDeg = 0, tiltDir = 0;

uint32_t lastImuUs = 0, calStartMs = 0;
int calN = 0;
V3 calAcc = {0, 0, 0}, calGyro = {0, 0, 0};

// ---------------- Motor state ----------------
Servo esc;

enum ControlMode {
  MODE_STICK,
  MODE_SERIAL
};
ControlMode mode = MODE_STICK;

bool running = false;                    // SERIAL mode on/off
int speedPercent = DEFAULT_SPEED_PERCENT; // SERIAL mode speed

float stickThrottle = 0.0f;              // STICK mode, 0..1 from the left stick
bool stickArmed = false;                 // stick must return to center before it drives

int currentPulseUs = PULSE_STOP_US;      // pulse currently being sent
unsigned long lastRampMs = 0;

String inputLine;

// ---------------- Controller callbacks ----------------
void onConnectedController(ControllerPtr c) {
  if (pad == nullptr) {
    pad = c;
    Serial.println("# Controller connected");
  } else {
    Serial.println("# Extra controller ignored");
  }
}

void onDisconnectedController(ControllerPtr c) {
  if (c == pad) {
    pad = nullptr;
    Serial.println("# Controller disconnected");
  }
}

void buzz(uint16_t ms, uint8_t weak, uint8_t strong) {
  if (RUMBLE && pad && pad->isConnected())
    pad->playDualRumble(0, ms, weak, strong);
}

// ---------------- MPU-6050 (raw I2C, no extra library) ----------------
bool mpuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool mpuInit() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  Wire.beginTransmission(MPU_ADDR);
  if (Wire.endTransmission() != 0)
    return false; // nobody answered at that address

  mpuWrite(0x6B, 0x00); // PWR_MGMT_1: wake up
  delay(50);
  mpuWrite(0x1A, 0x03); // CONFIG: ~44 Hz low-pass filter
  mpuWrite(0x1B, 0x00); // GYRO_CONFIG: +/-250 deg/s  (131 LSB per deg/s)
  mpuWrite(0x1C, 0x00); // ACCEL_CONFIG: +/-2 g       (16384 LSB per g)

  Serial.println("# IMU found");
  return true;
}

// accel in g, gyro in rad/s
bool mpuRead(V3 &a, V3 &g) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0)
    return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)14) != 14)
    return false;
  int16_t r[7];
  for (int i = 0; i < 7; i++) {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    r[i] = (int16_t)((hi << 8) | lo);
  }
  a = {r[0] / 16384.0f, r[1] / 16384.0f, r[2] / 16384.0f};
  const float k = DEG_TO_RAD / 131.0f;
  g = {r[4] * k, r[5] * k, r[6] * k}; // r[3] is temperature
  return true;
}

void startCalibration() {
  if (imuState == IMU_NONE) {
    Serial.println("# IMU not found");
    return;
  }
  imuState = IMU_CAL;
  calStartMs = millis();
  calN = 0;
  calAcc = {0, 0, 0};
  calGyro = {0, 0, 0};
  Serial.println("# Calibrating");
}

void finishCalibration() {
  if (calN < 10) {
    imuState = IMU_NOCAL;
    Serial.println("# Calibration failed");
    return;
  }

  V3 avgA = vScale(calAcc, 1.0f / calN);
  float g = vLen(avgA);
  if (g < 0.8f || g > 1.2f) {
    imuState = IMU_NOCAL;
    Serial.println("# Calibration failed");
    return;
  }
  gyroBias = vScale(calGyro, 1.0f / calN);
  refUp = vNorm(avgA);
  up = refUp;

  // Reference axes perpendicular to the rod, built from whichever sensor axis
  // is most sideways. Lean direction 0 deg points along that axis.
  V3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  int best = 0;
  for (int i = 1; i < 3; i++)
    if (fabsf(vDot(axes[i], refUp)) < fabsf(vDot(axes[best], refUp)))
      best = i;
  e1 = vNorm(vSub(axes[best], vScale(refUp, vDot(axes[best], refUp))));
  e2 = vCross(refUp, e1);

  tiltDeg = 0;
  tiltDir = 0;
  imuState = IMU_OK;
  Serial.println("# Calibrated");
  buzz(150, 0x80, 0x40);
}

void imuUpdate() {
  if (imuState == IMU_NONE)
    return;
  uint32_t now = micros();
  uint32_t elapsed = now - lastImuUs;
  if (elapsed < IMU_PERIOD_US)
    return;
  lastImuUs = now;
  float dt = elapsed * 1e-6f;
  if (dt > 0.05f)
    dt = 0.05f;

  V3 a, g;
  if (!mpuRead(a, g))
    return;

  if (imuState == IMU_CAL) {
    calAcc = vAdd(calAcc, a);
    calGyro = vAdd(calGyro, g);
    calN++;
    if (millis() - calStartMs >= CAL_MS)
      finishCalibration();
    return;
  }

  if (!upInit) {
    up = vNorm(a);
    upInit = true;
  }
  V3 w = vSub(g, gyroBias);
  up = vAdd(up, vScale(vCross(up, w), dt));
  float aLen = vLen(a);
  if (aLen > 0.85f && aLen < 1.15f) { // only trust accel when it's mostly gravity
    up = vAdd(vScale(up, FILTER_ALPHA), vScale(vScale(a, 1.0f / aLen), 1.0f - FILTER_ALPHA));
  }
  up = vNorm(up);

  if (imuState == IMU_OK || imuState == IMU_TIP) {
    float c = vDot(up, refUp);
    if (c > 1.0f)
      c = 1.0f;
    if (c < -1.0f)
      c = -1.0f;
    tiltDeg = acosf(c) * RAD_TO_DEG;

    // Which way the top of the rod is leaning
    V3 lean = vScale(vSub(up, vScale(refUp, c)), -1.0f);
    if (vLen(lean) > 1e-4f)
      tiltDir = atan2f(vDot(lean, e2), vDot(lean, e1)) * RAD_TO_DEG;

    if (imuState == IMU_OK && tiltDeg > TIP_ON_DEG) {
      imuState = IMU_TIP;
      Serial.println("# Tipping");
      buzz(250, 0xC0, 0xC0);
    } else if (imuState == IMU_TIP && tiltDeg < TIP_OFF_DEG) {
      imuState = IMU_OK;
      Serial.println("# Upright");
    }
  }
}

// ---------------- Stick helpers ----------------
float shape(int raw) {
  int a = abs(raw);
  if (a < DEADZONE)
    return 0.0f;
  float s = (a - DEADZONE) / (AXIS_MAX - DEADZONE);
  if (s > 1.0f)
    s = 1.0f;
  return raw > 0 ? s : -s;
}

String buttonString(ControllerPtr c) {
  String s;
  auto add = [&](bool on, const char *name) {
    if (!on)
      return;
    if (s.length())
      s += '+';
    s += name;
  };

  add(c->b(), "A");
  add(c->a(), "B");
  add(c->y(), "X");
  add(c->x(), "Y");
  add(c->l1(), "L");
  add(c->r1(), "R");
  add(c->l2(), "ZL");
  add(c->r2(), "ZR");
  add(c->thumbL(), "LS");
  add(c->thumbR(), "RS");
  uint8_t d = c->dpad();
  add(d & DPAD_UP, "UP");
  add(d & DPAD_DOWN, "DOWN");
  add(d & DPAD_LEFT, "LEFT");
  add(d & DPAD_RIGHT, "RIGHT");
  add(c->miscStart(), "PLUS");
  add(c->miscSelect(), "MINUS");
  add(c->miscSystem(), "HOME");
  return s.length() ? s : String("-");
}

// ---------------- Motor control API ----------------
void disarmStick() {
  stickArmed = false;
  stickThrottle = 0.0f;
}

void setRunning(bool run) {
  running = run;
  Serial.println(run ? "# Motor: START" : "# Motor: STOP");
}

void setSpeed(int percent) {
  speedPercent = constrain(percent, 0, MAX_SPEED_PERCENT);
  Serial.print("# Speed: ");
  Serial.print(speedPercent);
  Serial.println("%");
}

void setMode(ControlMode m) {
  mode = m;
  running = false;
  disarmStick();
  Serial.println(m == MODE_STICK ? "# Mode: STICK (center the left stick to enable)"
                                 : "# Mode: SERIAL (use s / x / p / + / -)");
}

void emergencyStop() {
  running = false;
  disarmStick();
  currentPulseUs = PULSE_STOP_US;
  esc.writeMicroseconds(currentPulseUs);
  Serial.println("# !! EMERGENCY STOP - output at stop pulse");
}

// Stick throttle: up = 0..1, down/center = 0. Called every loop in STICK mode.
void updateStickThrottle(bool connected, float fwd) {
  if (!connected) {
    if (stickArmed)
      Serial.println("# Stick throttle disabled (no controller)");
    disarmStick();
    return;
  }
  stickThrottle = fwd > 0.0f ? fwd : 0.0f;
  if (!stickArmed && stickThrottle == 0.0f) {
    stickArmed = true;
    Serial.println("# Stick throttle ready");
  }
}

// Pulse width we want to end up at, based on mode / stick / running / speed
int targetPulseUs() {
  if (mode == MODE_STICK) {
    if (!stickArmed) return PULSE_STOP_US;
    const float fullStickUs = (PULSE_MAX_US - PULSE_STOP_US) * MAX_SPEED_PERCENT / 100.0f;
    return PULSE_STOP_US + (int)(fullStickUs * stickThrottle);
  }
  if (!running) return PULSE_STOP_US;
  return PULSE_STOP_US + (PULSE_MAX_US - PULSE_STOP_US) * speedPercent / 100;
}

// Called every loop, moves the output toward the target gradually
void updateMotor() {
  unsigned long now = millis();
  if (now - lastRampMs < RAMP_INTERVAL_MS) return;
  lastRampMs = now;

  int target = targetPulseUs();
  if (currentPulseUs < target) {
    currentPulseUs = min(currentPulseUs + RAMP_STEP_US, target);
  } else if (currentPulseUs > target) {
    currentPulseUs = max(currentPulseUs - RAMP_STEP_US, target);
  }

  esc.writeMicroseconds(currentPulseUs);
}

// ---------------- Serial command handling ----------------
void printHelp() {
  Serial.println("#");
  Serial.println("# Commands:");
  Serial.println("#   m / mode       toggle STICK / SERIAL control");
  Serial.println("#   s / start      start motor (SERIAL)");
  Serial.println("#   x / stop       stop motor");
  Serial.println("#   p <0-100>      set speed % (SERIAL)");
  Serial.println("#   + / -          speed +/- 5% (SERIAL)");
  Serial.println("#   e / estop      emergency stop");
  Serial.println("#   c / cal        calibrate IMU");
  Serial.println("#   t / telemetry  toggle $ telemetry lines");
  Serial.println("#   ? / status     show state");
  Serial.println("#   h / help       this list");
  Serial.println("#");
}

void printStatus() {
  Serial.print("# Mode: ");
  Serial.print(mode == MODE_STICK ? "STICK" : "SERIAL");
  if (mode == MODE_STICK) {
    Serial.print(stickArmed ? " (ready)" : " (center stick)");
    Serial.print(" | Stick: ");
    Serial.print((int)(stickThrottle * 100.0f));
    Serial.print("%");
  } else {
    Serial.print(" | State: ");
    Serial.print(running ? "RUNNING" : "STOPPED");
    Serial.print(" | Speed: ");
    Serial.print(speedPercent);
    Serial.print("%");
  }
  Serial.print(" | Pulse: ");
  Serial.print(currentPulseUs);
  Serial.print(" us (target ");
  Serial.print(targetPulseUs());
  Serial.print(" us) | IMU: ");
  Serial.println(IMU_NAMES[imuState]);
}

bool requireSerialMode() {
  if (mode == MODE_SERIAL) return true;
  Serial.println("# In STICK mode - send 'm' to switch to SERIAL control");
  return false;
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() == 0) return;

  if (cmd == "m" || cmd == "mode") {
    setMode(mode == MODE_STICK ? MODE_SERIAL : MODE_STICK);
  } else if (cmd == "s" || cmd == "start") {
    if (requireSerialMode()) setRunning(true);
  } else if (cmd == "x" || cmd == "stop") {
    if (mode == MODE_STICK) {
      disarmStick();
      Serial.println("# Motor: STOP (center the stick to re-enable)");
    } else {
      setRunning(false);
    }
  } else if (cmd == "+") {
    if (requireSerialMode()) setSpeed(speedPercent + 5);
  } else if (cmd == "-") {
    if (requireSerialMode()) setSpeed(speedPercent - 5);
  } else if (cmd.startsWith("p")) {
    if (!requireSerialMode()) return;
    String arg = cmd.substring(1);
    arg.trim();
    if (arg.length() == 0) {
      Serial.println("# Usage: p <0-100>");
    } else {
      setSpeed(arg.toInt());
    }
  } else if (cmd == "e" || cmd == "estop") {
    emergencyStop();
  } else if (cmd == "c" || cmd == "cal") {
    startCalibration();
  } else if (cmd == "t" || cmd == "telemetry") {
    telemetryOn = !telemetryOn;
    Serial.println(telemetryOn ? "# Telemetry ON" : "# Telemetry OFF");
  } else if (cmd == "?" || cmd == "status") {
    printStatus();
  } else if (cmd == "h" || cmd == "help") {
    printHelp();
  } else {
    Serial.print("# Unknown command: ");
    Serial.println(cmd);
  }
}

void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      handleCommand(inputLine);
      inputLine = "";
    } else {
      inputLine += c;
    }
  }
}

// ---------------- Arduino entry points ----------------
void setup() {
  Serial.begin(115200);
  delay(300);

  // ESC first so it sees the stop pulse as early as possible
  esc.setPeriodHertz(50); // 50Hz servo signal
  esc.attach(ESC_PIN, PULSE_STOP_US, PULSE_MAX_US);
  esc.writeMicroseconds(PULSE_STOP_US);
  unsigned long armStartMs = millis();
  Serial.println();
  Serial.println("# IVY remote motor control - arming ESC at 1000 us...");

  BP32.setup(&onConnectedController, &onDisconnectedController);
  if (FORGET_ON_BOOT)
    BP32.forgetBluetoothKeys();
  BP32.enableVirtualDevice(false);

  if (mpuInit()) {
    imuState = IMU_NOCAL;
    Serial.println("# Press HOME to calibrate");
  } else {
    imuState = IMU_NONE;
    Serial.println("# IMU not found");
  }

  // Finish holding the stop pulse for the rest of the arming time
  while (millis() - armStartMs < ARM_TIME_MS)
    delay(10);
  Serial.println("# ESC armed.");
  printHelp();
  printStatus();

  lastImuUs = micros();
}

void loop() {
  BP32.update();
  imuUpdate();
  readSerial();

  bool connected = pad && pad->isConnected() && pad->isGamepad();

  // HOME button: calibrate on press (not while held)
  bool calBtn = connected && pad->miscSystem();
  if (calBtn && !prevCalBtn)
    startCalibration();
  prevCalBtn = calBtn;

  int lx = 0, ly = 0, rx = 0, ry = 0;
  if (connected) {
    lx = pad->axisX();
    ly = pad->axisY(); // up = negative Y
    rx = pad->axisRX();
    ry = pad->axisRY();
  }
  float fwd = -shape(ly), side = shape(lx), turn = shape(rx);

  if (mode == MODE_STICK)
    updateStickThrottle(connected, fwd);
  updateMotor();

  uint32_t now = millis();
  if (telemetryOn && now - lastPrint >= PRINT_PERIOD_MS) {
    lastPrint = now;
    String btn = connected ? buttonString(pad) : String("-");
    float mag = sqrtf(fwd * fwd + side * side);
    if (mag > 1.0f)
      mag = 1.0f;
    float angle = (mag > 0.0f) ? atan2f(side, fwd) * RAD_TO_DEG : 0.0f;

    Serial.printf("$,%.1f,%.1f,%.1f,%.1f,%d,%d,%d,%d,%s,%d,%s,%.1f,%.1f,%d\n",
                  fwd * 100.0f, turn * 100.0f, angle, mag * 100.0f,
                  lx, ly, rx, ry, btn.c_str(), connected ? 1 : 0,
                  IMU_NAMES[imuState], tiltDeg, tiltDir, currentPulseUs);
  }

  if (!connected && now - lastWaitMsg >= 3000) {
    lastWaitMsg = now;
    Serial.println("# Waiting for controller, press sync button");
  }

  delay(1);
}
