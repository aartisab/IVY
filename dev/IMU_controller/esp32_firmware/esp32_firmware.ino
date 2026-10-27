// DEV - Switch Pro Controller + tilt sensing on ESP32

#include <Bluepad32.h>
#include <Wire.h>
#include <math.h>

// ---------------- Tunables ----------------
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

// ---------------- Small 3D vector helpers ----------------
struct V3
{
  float x, y, z;
};
static inline V3 vAdd(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static inline V3 vSub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static inline V3 vScale(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static inline float vDot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 vCross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static inline float vLen(V3 a) { return sqrtf(vDot(a, a)); }
static inline V3 vNorm(V3 a)
{
  float l = vLen(a);
  return l > 1e-9f ? vScale(a, 1.0f / l) : a;
}

// ---------------- State ----------------
ControllerPtr pad = nullptr;
uint32_t lastPrint = 0, lastWaitMsg = 0;
bool prevCalBtn = false;

enum ImuState
{
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

// ---------------- Controller callbacks ----------------
void onConnectedController(ControllerPtr c)
{
  if (pad == nullptr)
  {
    pad = c;
    Serial.println("# Controller connected");
  }
  else
  {
    Serial.println("# Extra controller ignored");
  }
}

void onDisconnectedController(ControllerPtr c)
{
  if (c == pad)
  {
    pad = nullptr;
    Serial.println("# Controller disconnected");
  }
}

void buzz(uint16_t ms, uint8_t weak, uint8_t strong)
{
  if (RUMBLE && pad && pad->isConnected())
    pad->playDualRumble(0, ms, weak, strong);
}

// ---------------- MPU-6050 (raw I2C, no extra library) ----------------
bool mpuWrite(uint8_t reg, uint8_t val)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool mpuInit()
{
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
bool mpuRead(V3 &a, V3 &g)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0)
    return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)14) != 14)
    return false;
  int16_t r[7];
  for (int i = 0; i < 7; i++)
  {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    r[i] = (int16_t)((hi << 8) | lo);
  }
  a = {r[0] / 16384.0f, r[1] / 16384.0f, r[2] / 16384.0f};
  const float k = DEG_TO_RAD / 131.0f;
  g = {r[4] * k, r[5] * k, r[6] * k}; // r[3] is temperature
  return true;
}

void startCalibration()
{
  if (imuState == IMU_NONE)
  {
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

void finishCalibration()
{
  if (calN < 10)
  {
    imuState = IMU_NOCAL;
    Serial.println("# Calibration failed");
    return;
  }

  V3 avgA = vScale(calAcc, 1.0f / calN);
  float g = vLen(avgA);
  if (g < 0.8f || g > 1.2f)
  {
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

void imuUpdate()
{
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

  if (imuState == IMU_CAL)
  {
    calAcc = vAdd(calAcc, a);
    calGyro = vAdd(calGyro, g);
    calN++;
    if (millis() - calStartMs >= CAL_MS)
      finishCalibration();
    return;
  }

  if (!upInit)
  {
    up = vNorm(a);
    upInit = true;
  }
  V3 w = vSub(g, gyroBias);
  up = vAdd(up, vScale(vCross(up, w), dt));
  float aLen = vLen(a);
  if (aLen > 0.85f && aLen < 1.15f)
  { // only trust accel when it's mostly gravity
    up = vAdd(vScale(up, FILTER_ALPHA), vScale(vScale(a, 1.0f / aLen), 1.0f - FILTER_ALPHA));
  }
  up = vNorm(up);

  if (imuState == IMU_OK || imuState == IMU_TIP)
  {
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

    if (imuState == IMU_OK && tiltDeg > TIP_ON_DEG)
    {
      imuState = IMU_TIP;
      Serial.println("# Tipping");
      buzz(250, 0xC0, 0xC0);
    }
    else if (imuState == IMU_TIP && tiltDeg < TIP_OFF_DEG)
    {
      imuState = IMU_OK;
      Serial.println("# Upright");
    }
  }
}

// ---------------- Stick helpers ----------------
float shape(int raw)
{
  int a = abs(raw);
  if (a < DEADZONE)
    return 0.0f;
  float s = (a - DEADZONE) / (AXIS_MAX - DEADZONE);
  if (s > 1.0f)
    s = 1.0f;
  return raw > 0 ? s : -s;
}

String buttonString(ControllerPtr c)
{
  String s;
  auto add = [&](bool on, const char *name)
  {
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

// ---------------- Arduino entry points ----------------
void setup()
{
  Serial.begin(115200);
  delay(300);

  BP32.setup(&onConnectedController, &onDisconnectedController);
  if (FORGET_ON_BOOT)
    BP32.forgetBluetoothKeys();
  BP32.enableVirtualDevice(false);

  if (mpuInit())
  {
    imuState = IMU_NOCAL;
    Serial.println("# Press HOME to calibrate");
  }
  else
  {
    imuState = IMU_NONE;
    Serial.println("# IMU not found");
  }
  lastImuUs = micros();
}

void loop()
{
  BP32.update();
  imuUpdate();

  // 'c' from the laptop also calibrates
  while (Serial.available())
  {
    char ch = Serial.read();
    if (ch == 'c' || ch == 'C')
      startCalibration();
  }

  bool connected = pad && pad->isConnected() && pad->isGamepad();

  // HOME button: calibrate on press (not while held)
  bool calBtn = connected && pad->miscSystem();
  if (calBtn && !prevCalBtn)
    startCalibration();
  prevCalBtn = calBtn;

  uint32_t now = millis();
  if (now - lastPrint >= PRINT_PERIOD_MS)
  {
    lastPrint = now;
    int lx = 0, ly = 0, rx = 0, ry = 0;
    String btn = "-";
    if (connected)
    {
      lx = pad->axisX();
      ly = pad->axisY(); // up = negative Y
      rx = pad->axisRX();
      ry = pad->axisRY();
      btn = buttonString(pad);
    }
    float fwd = -shape(ly), side = shape(lx), turn = shape(rx);
    float mag = sqrtf(fwd * fwd + side * side);
    if (mag > 1.0f)
      mag = 1.0f;
    float angle = (mag > 0.0f) ? atan2f(side, fwd) * RAD_TO_DEG : 0.0f;

    Serial.printf("$,%.1f,%.1f,%.1f,%.1f,%d,%d,%d,%d,%s,%d,%s,%.1f,%.1f\n",
                  fwd * 100.0f, turn * 100.0f, angle, mag * 100.0f,
                  lx, ly, rx, ry, btn.c_str(), connected ? 1 : 0,
                  IMU_NAMES[imuState], tiltDeg, tiltDir);
  }

  if (!connected && now - lastWaitMsg >= 3000)
  {
    lastWaitMsg = now;
    Serial.println("# Waiting for controller, press sync button");
  }

  delay(1);
}
