// =====================================================
//  MPU6050 AIR MOUSE  -  OLD VERSION (super loop)
// =====================================================
// Everything runs one after another inside loop().
// When the heavy job runs (30 ms), nothing else can run,
// so the mouse freezes and then jumps.
// Compare with Mouse_RTOS: same jobs, same settings.

#include <Wire.h>

// ---------------- SETTINGS (same in both versions) ----------------
#define SDA_PIN    21
#define SCL_PIN    22
#define BUTTON_PIN 0        // BOOT button on the ESP32 = fire
#define LED_PIN    2        // lights up while firing

const float COUNTS_PER_DEG = 25.0;  // movement units per degree turned
const float DEADZONE_DPS   = 2.0;   // ignore tiny hand shake / drift
// Speed, direction and which axis is up/down are set in the game.

const unsigned long MOUSE_PERIOD_US = 10000;  // send mouse data every 10 ms
const unsigned long HEAVY_PERIOD_MS = 100;    // heavy job every 100 ms
const unsigned long HEAVY_JOB_US    = 30000;  // heavy job takes 30 ms

// ---------------- MPU6050 ----------------
// Lessons from the workshop:
// - Do NOT reject the sensor by its WHO_AM_I id. Clone chips report other ids.
// - Just check that 0x68 (or 0x69) answers, then use it.
byte  mpuAddr = 0;
float gyroBias[3] = {0, 0, 0};
unsigned long readErrors = 0;
float acc[3] = {0, 0, 0};

void mpuWrite(byte reg, byte value) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

bool mpuFind() {
  const byte addresses[] = {0x68, 0x69};
  for (byte a : addresses) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { mpuAddr = a; return true; }
  }
  return false;
}

byte mpuWhoAmI() {                 // for information only
  Wire.beginTransmission(mpuAddr);
  Wire.write(0x75);
  if (Wire.endTransmission(false) != 0) return 0;
  if (Wire.requestFrom((int)mpuAddr, 1) != 1) return 0;
  return Wire.read();
}

bool mpuReadGyroRaw(float g[3]) {  // degrees per second, no bias removed
  Wire.beginTransmission(mpuAddr);
  Wire.write(0x43);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)mpuAddr, 6) != 6) return false;
  for (int i = 0; i < 3; i++) {
    byte hi = Wire.read();         // read high byte first, then low byte
    byte lo = Wire.read();
    g[i] = (int16_t)((hi << 8) | lo) / 65.5;   // +/-500 deg/s range
  }
  return true;
}

bool mpuReadGyro(float g[3]) {
  if (!mpuReadGyroRaw(g)) return false;
  for (int i = 0; i < 3; i++) g[i] -= gyroBias[i];
  return true;
}

void mpuSetup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);           // 100 kHz is safest with jumper wires

  if (!mpuFind()) {
    Serial.println("# MPU6050 NOT FOUND. Check VCC-3V3, GND-GND, SDA-21, SCL-22.");
    while (true) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); delay(150); }
  }
  Serial.printf("# MPU found at 0x%02X, WHO_AM_I=0x%02X (any value is OK)\n", mpuAddr, mpuWhoAmI());

  mpuWrite(0x6B, 0x80); delay(100); // reset
  mpuWrite(0x6B, 0x01); delay(50);  // wake up, use gyro clock
  mpuWrite(0x1A, 0x03);             // low pass filter ~44 Hz (smooth)
  mpuWrite(0x1B, 0x08);             // gyro +/-500 deg/s (fast flicks)
  mpuWrite(0x1C, 0x00);             // accel +/-2 g
  delay(100);

  Serial.println("# Calibrating, keep the sensor still...");
  const int samples = 400;
  int good = 0;
  float sum[3] = {0, 0, 0}, g[3];
  for (int i = 0; i < samples; i++) {
    if (mpuReadGyroRaw(g)) { for (int k = 0; k < 3; k++) sum[k] += g[k]; good++; }
    digitalWrite(LED_PIN, (i / 25) % 2);
    delay(5);
  }
  if (good > 0) for (int k = 0; k < 3; k++) gyroBias[k] = sum[k] / good;
  digitalWrite(LED_PIN, LOW);
  Serial.println("# Calibration done.");
}

// ---------------- MOUSE ----------------
// Sends one line per update:  M,x,y,z,fire,gap_us
// x,y,z = how much the board turned around each axis since the last line.
// gap_us = time since the previous update. Steady 10000 = smooth.
void mouseStep(float dt, unsigned long gapUs) {
  float g[3];
  if (!mpuReadGyro(g)) { readErrors++; return; }

  int turn[3];
  for (int i = 0; i < 3; i++) {
    float rate = g[i];
    if (fabs(rate) < DEADZONE_DPS) rate = 0;
    acc[i] += rate * dt * COUNTS_PER_DEG;
    turn[i] = (int)acc[i];
    acc[i] -= turn[i];
  }

  bool fire = digitalRead(BUTTON_PIN) == LOW;
  digitalWrite(LED_PIN, fire);
  Serial.printf("M,%d,%d,%d,%d,%lu\n", turn[0], turn[1], turn[2], fire ? 1 : 0, gapUs);
}

void statusLine(const char* version) {   // once per second
  static unsigned long last = 0;
  if (millis() - last < 1000) return;
  last = millis();
  Serial.printf("#V,%s,errors=%lu\n", version, readErrors);
}

// The "heavy job" stands for real work: Wi-Fi, a display, logging, etc.
void heavyJob() {
  unsigned long start = micros();
  volatile float x = 0;
  while (micros() - start < HEAVY_JOB_US) x = x + sqrtf(x + 1.0f);
}

// ---------------- MAIN ----------------
unsigned long lastMouseUs = 0;
unsigned long lastHeavyMs = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  delay(500);
  Serial.println("# Air mouse - OLD version (super loop)");
  mpuSetup();
  lastMouseUs = micros();
  lastHeavyMs = millis();
}

void loop() {
  unsigned long now = micros();
  if (now - lastMouseUs >= MOUSE_PERIOD_US) {
    unsigned long gap = now - lastMouseUs;
    lastMouseUs = now;
    mouseStep(gap / 1000000.0, gap);
  }

  if (millis() - lastHeavyMs >= HEAVY_PERIOD_MS) {
    lastHeavyMs = millis();
    heavyJob();                    // blocks everything for 30 ms
  }

  statusLine("OLD");
}
