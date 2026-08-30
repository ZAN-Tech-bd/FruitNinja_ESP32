/*
  ESP32 + MPU6050 Gesture-Controlled Space War
  ------------------------------------------------
  - ESP32 runs its own WiFi Access Point (no internet / no router needed)
  - Local web server serves a full Space War shooter (HTML/CSS/JS, canvas based)
  - MPU6050 is read via the Adafruit MPU6050 library (same driver as the
    known-good test sketch)
  - Browser polls GET /data ~50x/sec for live tilt data to control the blade

  LIBRARIES NEEDED (auto-installed by PlatformIO, see lib_deps in platformio.ini):
    - Adafruit MPU6050
    - Adafruit Unified Sensor

  WIRING (MPU6050 / GY-521 -> ESP32):
    VCC -> 3.3V
    GND -> GND
    SCL -> GPIO 22
    SDA -> GPIO 21
    AD0 -> GND (or leave floating)  -> I2C address 0x68
    INT, XDA, XCL -> not connected

  HOW TO USE:
    1. Flash this sketch.
    2. On your phone/laptop, connect to WiFi network: "SpaceWar_ESP32"
       password: "12345678"
    3. Open a browser and go to: http://192.168.4.1
    4. Hold the ESP32+sensor flat, tap "Calibrate", then tilt to steer your
       fighter. Guns fire automatically - dodge enemy fire and avoid ramming mines!
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "index_html.h"   // contains the game page as INDEX_HTML (PROGMEM string)

// ---------- WiFi Access Point settings ----------
const char* AP_SSID     = "SpaceWar_ESP32";
const char* AP_PASSWORD = "12345678";   // must be 8+ chars, or set to "" for open network

// ---------- I2C / MPU6050 settings ----------
#define SDA_PIN 21
#define SCL_PIN 22

WebServer server(80);
Adafruit_MPU6050 mpu;
bool mpuOK = false;

float ax_g, ay_g, az_g;        // accel in g
float gx_dps, gy_dps, gz_dps;  // gyro in deg/s

// ---------------- MPU6050 read (Adafruit library) ----------------
void mpuReadAll() {
  if (!mpuOK) return;

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Adafruit returns accel in m/s^2 -> convert to g for the game
  ax_g   = a.acceleration.x / 9.80665f;
  ay_g   = a.acceleration.y / 9.80665f;
  az_g   = a.acceleration.z / 9.80665f;
  gx_dps = g.gyro.x;
  gy_dps = g.gyro.y;
  gz_dps = g.gyro.z;
}

unsigned long lastMpuPrint = 0;

void printMpuLine() {
  Serial.printf("[MPU6050] a[g] x=%+.2f y=%+.2f z=%+.2f | g[dps] x=%+.1f y=%+.1f z=%+.1f\n",
                ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps);
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
  delay(500);

  // --- MPU6050 init via Adafruit library (same driver as the working test sketch) ---
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if (!mpu.begin()) {
    mpuOK = false;
    Serial.println("[MPU6050] Failed to find MPU6050 chip!");
    Serial.println("[MPU6050] Check VCC->3.3V, GND->GND, SDA->GPIO21, SCL->GPIO22");
  } else {
    mpuOK = true;
    Serial.println("[MPU6050] MPU6050 Found!");

    // Same scales as before (+-2g, +-250 deg/s) + light filtering
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);

    Serial.println("[MPU6050] live readings (az ~ +1.00g at rest = sensor data valid):");
    for (int i = 0; i < 5; i++) { mpuReadAll(); printMpuLine(); delay(250); }
  }

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

  // Live MPU6050 status on Serial every second (wiring sanity check)
  unsigned long now = millis();
  if (now - lastMpuPrint >= 1000) {
    lastMpuPrint = now;
    mpuReadAll();
    printMpuLine();
  }
}
