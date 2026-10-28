/*
 * IVY - ESC motor control (serial test version)
 *
 * For now the motor is commanded from the Serial Monitor:
 *
 * Serial commands:
 *   s / start      start the motor (ramps up to set speed)
 *   x / stop       stop the motor (ramps down)
 *   p <0-100>      set speed in percent (e.g. "p 30")
 *   + / -          speed up / down by 5%
 *   e / estop      emergency stop (immediate stop, no ramp)
 *   ? / status     print current state
 *   h / help       print this list
 *
 * Library: ESP32Servo (Arduino Library Manager -> "ESP32Servo" by Kevin Harrington).
 */

#include <ESP32Servo.h>

// Configuration constants
const int ESC_PIN = 18;

const int PULSE_STOP_US = 1000;      // stopped / zero throttle
const int PULSE_MAX_US = 2000;       // full throttle

const int MAX_SPEED_PERCENT = 60;    // safety cap while testing
const int DEFAULT_SPEED_PERCENT = 20;

const unsigned long ARM_TIME_MS = 3000;       // stop signal held at boot so the ESC arms
const unsigned long RAMP_INTERVAL_MS = 20;    // how often the output is updated
const int RAMP_STEP_US = 5;                   // max pulse change per update (~250 us/s)

Servo esc;

bool running = false;
int speedPercent = DEFAULT_SPEED_PERCENT;

int currentPulseUs = PULSE_STOP_US;      // pulse currently being sent
unsigned long lastRampMs = 0;

String inputLine;

// Motor control API (for easier integration with other code)

void setRunning(bool run) {
  running = run;
  Serial.println(run ? "Motor: START" : "Motor: STOP");
}

void setSpeed(int percent) {
  speedPercent = constrain(percent, 0, MAX_SPEED_PERCENT);
  Serial.print("Speed: ");
  Serial.print(speedPercent);
  Serial.println("%");
}

void emergencyStop() {
  running = false;
  currentPulseUs = PULSE_STOP_US;
  esc.writeMicroseconds(currentPulseUs);
  Serial.println("!! EMERGENCY STOP - output at stop pulse");
}

// Pulse width we want to end up at, based on running/speed
int targetPulseUs() {
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

// Serial command handling
void printHelp() {
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  s / start     start motor");
  Serial.println("  x / stop      stop motor");
  Serial.println("  p <0-100>     set speed %");
  Serial.println("  + / -         speed +/- 5%");
  Serial.println("  e / estop     emergency stop");
  Serial.println("  ? / status    show state");
  Serial.println("  h / help      this list");
  Serial.println();
}

void printStatus() {
  Serial.print("State: ");
  Serial.print(running ? "RUNNING" : "STOPPED");
  Serial.print(" | Speed: ");
  Serial.print(speedPercent);
  Serial.print("% | Pulse: ");
  Serial.print(currentPulseUs);
  Serial.print(" us (target ");
  Serial.print(targetPulseUs());
  Serial.println(" us)");
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() == 0) return;

  if (cmd == "s" || cmd == "start") {
    setRunning(true);
  } else if (cmd == "x" || cmd == "stop") {
    setRunning(false);
  } else if (cmd == "+") {
    setSpeed(speedPercent + 5);
  } else if (cmd == "-") {
    setSpeed(speedPercent - 5);
  } else if (cmd.startsWith("p")) {
    String arg = cmd.substring(1);
    arg.trim();
    if (arg.length() == 0) {
      Serial.println("Usage: p <0-100>");
    } else {
      setSpeed(arg.toInt());
    }
  } else if (cmd == "e" || cmd == "estop") {
    emergencyStop();
  } else if (cmd == "?" || cmd == "status") {
    printStatus();
  } else if (cmd == "h" || cmd == "help") {
    printHelp();
  } else {
    Serial.print("Unknown command: ");
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


void setup() {
  Serial.begin(115200);
  delay(500);

  esc.setPeriodHertz(50); // 50Hz servo signal
  esc.attach(ESC_PIN, PULSE_STOP_US, PULSE_MAX_US);
  esc.writeMicroseconds(PULSE_STOP_US);

  Serial.println();
  Serial.println("IVY motor control - arming ESC at 1000 us...");
  delay(ARM_TIME_MS);
  Serial.println("ESC armed.");
  printHelp();
  printStatus();
}

void loop() {
  readSerial();
  updateMotor();
}
