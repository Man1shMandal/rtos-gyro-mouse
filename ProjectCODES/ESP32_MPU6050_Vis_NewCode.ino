
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <math.h>

// ---------------- CONFIGURATION ----------------

#define MPU_ADDR 0x68
#define SDA_PIN 21
#define SCL_PIN 22

const char* AP_SSID = "China";
const char* AP_PASSWORD = "12345678";

WebServer server(80);

float roll = 0;
float pitch = 0;
float yaw = 0;

float gxOffset = 0;
float gyOffset = 0;
float gzOffset = 0;

float axG = 0, ayG = 0, azG = 0;
float gxDPS = 0, gyDPS = 0, gzDPS = 0;

bool sensorOK = false;

unsigned long lastUpdate = 0;

// ---------------- MPU6050 ----------------

bool readMPU() {

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);

  if (Wire.endTransmission(false) != 0)
    return false;

  if (Wire.requestFrom(MPU_ADDR, 14) != 14)
    return false;

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();

  Wire.read();
  Wire.read();

  int16_t gx = (Wire.read() << 8) | Wire.read();
  int16_t gy = (Wire.read() << 8) | Wire.read();
  int16_t gz = (Wire.read() << 8) | Wire.read();

  axG = ax / 16384.0;
  ayG = ay / 16384.0;
  azG = az / 16384.0;

  gxDPS = gx / 131.0 - gxOffset;
  gyDPS = gy / 131.0 - gyOffset;
  gzDPS = gz / 131.0 - gzOffset;

  return true;
}

// ---------------- CALIBRATION ----------------

void calibrateGyro() {

  Serial.println("Calibrating. Keep sensor still.");

  long x = 0, y = 0, z = 0;

  const int samples = 500;

  for (int i = 0; i < samples; i++) {

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x43);
    Wire.endTransmission(false);

    if (Wire.requestFrom(MPU_ADDR, 6) == 6) {

      int16_t gx = (Wire.read() << 8) | Wire.read();
      int16_t gy = (Wire.read() << 8) | Wire.read();
      int16_t gz = (Wire.read() << 8) | Wire.read();

      x += gx;
      y += gy;
      z += gz;
    }

    delay(3);
  }

  gxOffset = (float)x / samples / 131.0;
  gyOffset = (float)y / samples / 131.0;
  gzOffset = (float)z / samples / 131.0;

  Serial.println("Calibration complete.");
}

// ---------------- SENSOR INITIALIZATION ----------------

void initializeMPU() {

  // Wake up MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x01);
  Wire.endTransmission();

  delay(100);

  // Accelerometer +/- 2g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission();

  // Gyroscope +/- 250 degrees/sec
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B);
  Wire.write(0x00);
  Wire.endTransmission();

  // Low pass filter
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1A);
  Wire.write(0x03);
  Wire.endTransmission();

  delay(100);

  calibrateGyro();
}

// ---------------- ORIENTATION ----------------

void updateOrientation() {

  unsigned long now = micros();

  float dt = (now - lastUpdate) / 1000000.0;
  lastUpdate = now;

  if (dt <= 0 || dt > 0.1)
    return;

  if (!readMPU()) {
    sensorOK = false;
    return;
  }

  sensorOK = true;

  float accelRoll =
    atan2(ayG, azG) * 180.0 / PI;

  float accelPitch =
    atan2(-axG, sqrt(ayG * ayG + azG * azG))
    * 180.0 / PI;

  // Complementary filter
  roll = 0.98 * (roll + gxDPS * dt)
       + 0.02 * accelRoll;

  pitch = 0.98 * (pitch + gyDPS * dt)
        + 0.02 * accelPitch;

  // Gyroscope-only yaw
  yaw += gzDPS * dt;

  if (yaw > 180) yaw -= 360;
  if (yaw < -180) yaw += 360;
}

// ---------------- WEBPAGE ----------------

const char webpage[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html>
<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>MPU6050 3D Visualizer</title>

<style>

* {
  box-sizing: border-box;
}

body {
  margin: 0;
  background: #080d18;
  color: white;
  font-family: Arial, sans-serif;
}

header {
  padding: 22px;
  border-bottom: 1px solid #253047;
}

header h2 {
  margin: 0;
  font-size: 21px;
}

header p {
  color: #8795ab;
  font-size: 13px;
}

.status {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 13px;
  margin-top: 15px;
}

.dot {
  width: 9px;
  height: 9px;
  border-radius: 50%;
  background: #ff4545;
}

.dot.ok {
  background: #20dc91;
}

.container {
  max-width: 900px;
  margin: auto;
  padding: 15px;
}

.panel {
  background: #111a2b;
  border: 1px solid #27344c;
  border-radius: 15px;
  padding: 18px;
  margin-bottom: 15px;
}

h3 {
  font-size: 15px;
  margin-top: 0;
}

.scene {
  height: 330px;
  display: flex;
  justify-content: center;
  align-items: center;
  perspective: 850px;
  overflow: hidden;
  background: radial-gradient(#20304a, #0a1120);
  border-radius: 12px;
}

.board {
  width: 180px;
  height: 125px;
  position: relative;
  transform-style: preserve-3d;
  transform: rotateX(-25deg) rotateY(35deg);
  transition: transform 0.06s linear;
}

.face {
  position: absolute;
  width: 180px;
  height: 125px;
  background: linear-gradient(135deg, #139d72, #07553e);
  border: 2px solid #54d5a7;
  border-radius: 9px;
  backface-visibility: hidden;
}

.face.front {
  transform: translateZ(9px);
}

.face.back {
  transform: rotateY(180deg) translateZ(9px);
}

.chip {
  position: absolute;
  width: 55px;
  height: 55px;
  background: #202735;
  border: 3px solid #657184;
  border-radius: 5px;
  top: 33px;
  left: 62px;
  transform: translateZ(12px);
  display: flex;
  justify-content: center;
  align-items: center;
  font-size: 10px;
  font-weight: bold;
}

.pin {
  position: absolute;
  width: 12px;
  height: 12px;
  background: #d9b65c;
  border-radius: 2px;
  transform: translateZ(10px);
}

.p1 {left: 15px; top: 10px;}
.p2 {left: 15px; bottom: 10px;}
.p3 {right: 15px; top: 10px;}
.p4 {right: 15px; bottom: 10px;}

.axis {
  position: absolute;
  font-size: 13px;
  font-weight: bold;
  transform: translateZ(18px);
}

.axis-x {right: -35px; color: #ff5555;}
.axis-y {top: -30px; color: #50e18b;}
.axis-z {left: -35px; color: #55aaff;}

.grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 10px;
}

.metric {
  background: #1b2940;
  padding: 15px 5px;
  text-align: center;
  border-radius: 10px;
}

.metric span {
  font-size: 11px;
  color: #95a4ba;
}

.metric strong {
  display: block;
  font-size: 21px;
  color: #53baff;
  margin-top: 9px;
}

.note {
  color: #8291a9;
  font-size: 12px;
  margin-top: 15px;
}

.footer {
  text-align: center;
  padding: 20px;
  color: #77859a;
  font-size: 12px;
}

@media(max-width:450px) {
  .scene {height: 290px;}
  .metric strong {font-size: 17px;}
}

</style>
</head>

<body>

<header>

<h2>MPU6050 3D Visualizer</h2>

<p>ESP32 Motion Tracking System</p>

<div class="status">
  <div class="dot" id="dot"></div>
  <span id="status">Connecting...</span>
</div>

</header>

<div class="container">

<div class="panel">

<h3>Live 3D Orientation</h3>

<div class="scene">

<div class="board" id="board">

<div class="face front"></div>
<div class="face back"></div>

<div class="chip">MPU6050</div>

<div class="pin p1"></div>
<div class="pin p2"></div>
<div class="pin p3"></div>
<div class="pin p4"></div>

<div class="axis axis-x">X</div>
<div class="axis axis-y">Y</div>
<div class="axis axis-z">Z</div>

</div>

</div>

<p class="note">
Live sensor orientation. Rotate the physical sensor to move the 3D model.
</p>

</div>

<div class="panel">

<h3>Orientation</h3>

<div class="grid">

<div class="metric">
<span>ROLL</span>
<strong id="roll">0°</strong>
</div>

<div class="metric">
<span>PITCH</span>
<strong id="pitch">0°</strong>
</div>

<div class="metric">
<span>YAW</span>
<strong id="yaw">0°</strong>
</div>

</div>
</div>

<div class="panel">

<h3>Accelerometer (g)</h3>

<div class="grid">

<div class="metric">
<span>X</span>
<strong id="ax">0.00</strong>
</div>

<div class="metric">
<span>Y</span>
<strong id="ay">0.00</strong>
</div>

<div class="metric">
<span>Z</span>
<strong id="az">0.00</strong>
</div>

</div>
</div>

<div class="panel">

<h3>Gyroscope (°/s)</h3>

<div class="grid">

<div class="metric">
<span>X</span>
<strong id="gx">0.00</strong>
</div>

<div class="metric">
<span>Y</span>
<strong id="gy">0.00</strong>
</div>

<div class="metric">
<span>Z</span>
<strong id="gz">0.00</strong>
</div>

</div>
</div>

</div>

<div class="footer">
ESP32 DevKit V1 | MPU6050 | Offline 3D Dashboard
</div>

<script>

const board = document.getElementById("board");

let targetRoll = 0;
let targetPitch = 0;
let targetYaw = 0;

let currentRoll = -25;
let currentPitch = 35;
let currentYaw = 0;

// Smooth 3D animation
function animate() {

  currentRoll += (targetPitch - currentRoll) * 0.15;
  currentPitch += (-targetRoll - currentPitch) * 0.15;
  currentYaw += (targetYaw - currentYaw) * 0.15;

  board.style.transform =
    `rotateX(${currentRoll}deg)
     rotateZ(${currentPitch}deg)
     rotateY(${currentYaw}deg)`;

  requestAnimationFrame(animate);
}

animate();

// Fetch ESP32 sensor values
async function updateData() {

  try {

    const response = await fetch("/data");

    if (!response.ok)
      throw new Error("API error");

    const d = await response.json();

    document.getElementById("dot").classList.toggle("ok", d.ok);

    document.getElementById("status").textContent =
      d.ok ? "MPU6050 Connected" : "Sensor Disconnected";

    if (!d.ok) return;

    targetRoll = d.roll;
    targetPitch = d.pitch;
    targetYaw = d.yaw;

    const fields = [
      "roll", "pitch", "yaw",
      "ax", "ay", "az",
      "gx", "gy", "gz"
    ];

    fields.forEach(key => {

      let value = Number(d[key]).toFixed(2);

      if (["roll", "pitch", "yaw"].includes(key))
        value += "°";

      document.getElementById(key).textContent = value;

    });

  } catch(error) {

    document.getElementById("dot").classList.remove("ok");

    document.getElementById("status").textContent =
      "ESP32 Disconnected";

  }
}

setInterval(updateData, 100);
updateData();

</script>

</body>
</html>

)rawliteral";

// ---------------- API ----------------

void handleRoot() {
  server.send_P(200, "text/html", webpage);
}

void handleData() {

  String json = "{";

  json += "\"ok\":";
  json += sensorOK ? "true" : "false";

  json += ",\"roll\":" + String(roll, 2);
  json += ",\"pitch\":" + String(pitch, 2);
  json += ",\"yaw\":" + String(yaw, 2);

  json += ",\"ax\":" + String(axG, 3);
  json += ",\"ay\":" + String(ayG, 3);
  json += ",\"az\":" + String(azG, 3);

  json += ",\"gx\":" + String(gxDPS, 3);
  json += ",\"gy\":" + String(gyDPS, 3);
  json += ",\"gz\":" + String(gzDPS, 3);

  json += "}";

  server.send(200, "application/json", json);
}

// ---------------- SETUP ----------------

void setup() {

  Serial.begin(115200);

  delay(500);

  Serial.println("\nESP32 MPU6050 Visualizer");

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  Wire.beginTransmission(MPU_ADDR);

  if (Wire.endTransmission() != 0) {

    Serial.println("MPU6050 not detected!");

    while (true) delay(1000);
  }

  Serial.println("MPU6050 responding at 0x68");

  initializeMPU();

  WiFi.mode(WIFI_AP);

  WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.println("\nWi-Fi started");

  Serial.print("Network: ");
  Serial.println(AP_SSID);

  Serial.print("Password: ");
  Serial.println(AP_PASSWORD);

  Serial.print("Dashboard: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);

  server.begin();

  lastUpdate = micros();

  Serial.println("Dashboard ready!");
}

// ---------------- LOOP ----------------

void loop() {

  server.handleClient();

  static unsigned long previous = 0;

  if (millis() - previous >= 10) {

    previous = millis();

    updateOrientation();
  }
}
