/*
  ESP32 + MPU6050 Gesture-Controlled Fruit Ninja
  ------------------------------------------------
  - ESP32 runs its own WiFi Access Point (no internet / no router needed)
  - Local web server serves a full Fruit Ninja game (HTML/CSS/JS, canvas based)
  - MPU6050 is read via raw I2C registers (NO extra libraries needed)
  - Browser polls GET /data ~50x/sec for live tilt data to control the blade

  LIBRARIES NEEDED: NONE beyond the standard ESP32 Arduino core
  (WiFi.h, WebServer.h, Wire.h all ship with the ESP32 board package)

  WIRING (MPU6050 / GY-521 -> ESP32):
    VCC -> 3.3V
    GND -> GND
    SCL -> GPIO 22
    SDA -> GPIO 21
    AD0 -> GND (or leave floating)  -> I2C address 0x68
    INT, XDA, XCL -> not connected

  HOW TO USE:
    1. Install "ESP32" boards in Arduino IDE (Boards Manager) if not already.
    2. Select your ESP32 board + correct COM port.
    3. Upload this sketch as-is.
    4. On your phone/laptop, connect to WiFi network: "FruitNinja_ESP32"
       password: "12345678"
    5. Open a browser and go to: http://192.168.4.1
    6. Hold the ESP32+sensor flat, tap "Calibrate", then tilt to move the
       blade and swipe fast near fruits to slice them. Avoid the bombs!
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "index_html.h"   // contains the game page as INDEX_HTML (PROGMEM string)

// ---------- WiFi Access Point settings ----------
const char* AP_SSID     = "FruitNinja_ESP32";
const char* AP_PASSWORD = "12345678";   // must be 8+ chars, or set to "" for open network

// ---------- I2C / MPU6050 settings ----------
#define SDA_PIN 21
#define SCL_PIN 22
#define MPU_ADDR 0x68

WebServer server(80);

int16_t rawAx, rawAy, rawAz, rawGx, rawGy, rawGz;
float ax_g, ay_g, az_g;        // accel in g
float gx_dps, gy_dps, gz_dps;  // gyro in deg/s

// ---------------- MPU6050 raw I2C helpers ----------------
void mpuWriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void mpuInit() {
  mpuWriteReg(0x6B, 0x00); // PWR_MGMT_1: wake up the MPU6050 (default is sleep)
  delay(50);
  mpuWriteReg(0x1C, 0x00); // ACCEL_CONFIG: +-2g range
  mpuWriteReg(0x1B, 0x00); // GYRO_CONFIG:  +-250 deg/s range
}

void mpuReadAll() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // starting register: ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14, (bool)true);

  if (Wire.available() < 14) return;

  rawAx = (Wire.read() << 8) | Wire.read();
  rawAy = (Wire.read() << 8) | Wire.read();
  rawAz = (Wire.read() << 8) | Wire.read();
  Wire.read(); Wire.read(); // skip temperature (2 bytes)
  rawGx = (Wire.read() << 8) | Wire.read();
  rawGy = (Wire.read() << 8) | Wire.read();
  rawGz = (Wire.read() << 8) | Wire.read();

  // Convert using default full-scale ranges (+-2g, +-250 deg/s)
  ax_g   = rawAx / 16384.0;
  ay_g   = rawAy / 16384.0;
  az_g   = rawAz / 16384.0;
  gx_dps = rawGx / 131.0;
  gy_dps = rawGy / 131.0;
  gz_dps = rawGz / 131.0;
}

// ---------------- Web server handlers ----------------
void handleData() {
  mpuReadAll();
  char buf[160];
  snprintf(buf, sizeof(buf),
    "{\"ax\":%.4f,\"ay\":%.4f,\"az\":%.4f,\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f}",
    ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", buf);
}

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  mpuInit();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP started. Connect to WiFi \"");
  Serial.print(AP_SSID);
  Serial.println("\" then open http://192.168.4.1");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();
}
