# ESP32 Space War 🚀 (MPU6050 Gesture Controlled)

A self-contained, offline arcade shooter that runs entirely on an **ESP32**.
The ESP32 hosts its own WiFi hotspot and web server — no internet, router, or
app install needed. Tilt an **MPU6050** sensor to steer your fighter left and
right along the defense line while your guns fire automatically; dodge enemy
fire, avoid ramming mines, and shoot enemies before they close in.

> This repo/project is still named `FruitNinja_ESP32` for historical reasons
> (it started as a fruit-slicing game), but the game currently embedded in
> `index_html.h` and served by the firmware is **Space War**, a vertical
> scrolling shooter — not Fruit Ninja.

- No router / internet required — the ESP32 *is* the network (WiFi Access Point).
- MPU6050 is read via the **Adafruit MPU6050** + **Adafruit Unified Sensor**
  libraries (installed automatically by PlatformIO).
- The entire game (HTML + CSS + JS, canvas-based) is embedded in the firmware
  and served from flash memory.
- Sound effects are synthesized on the fly in the browser (Web Audio API) —
  no audio files needed.

---

## How It Works

1. The ESP32 boots, initializes the MPU6050 over I2C, and starts a WiFi
   **Access Point** (`WIFI_AP` mode).
2. It runs a small web server (port 80) with two routes:
   - `GET /` — serves the full game page (`index_html.h`).
   - `GET /data` — returns live MPU6050 accelerometer/gyro readings as JSON,
     e.g. `{"ax":0.01,"ay":0.98,"az":0.05,"gx":1.2,"gy":-0.3,"gz":0.1}`.
3. Your phone/laptop connects to the ESP32's WiFi network and opens the game
   page in a browser.
4. The page polls `GET /data` about 50 times per second and uses the tilt
   (`ax`, `ay`) to steer the fighter on screen.
5. The Serial Monitor also prints live MPU6050 readings once per second for
   wiring sanity checks.

---

## Hardware Required

| Part | Notes |
|---|---|
| ESP32 development board | Any standard ESP32 dev board (WROOM-32, DevKit V1, etc.) |
| MPU6050 (GY-521 breakout) | 6-axis accelerometer + gyroscope, I2C |
| USB cable | For programming and power |
| Jumper wires | 4 needed (VCC, GND, SDA, SCL) |

---

## Pin Diagram / Wiring

Connect the **MPU6050 (GY-521)** to the **ESP32** as follows:

| MPU6050 / GY-521 Pin | ESP32 Pin | Purpose |
|:---:|:---:|---|
| **VCC** | **3.3V** | Power (do **not** use 5V — most GY-521 boards are 3.3V logic) |
| **GND** | **GND** | Ground |
| **SCL** | **GPIO 22** | I2C Clock |
| **SDA** | **GPIO 21** | I2C Data |
| **AD0** | **GND** (or leave floating) | Sets I2C address to `0x68` |
| INT, XDA, XCL | *Not connected* | Unused in this project |

```
                 ESP32 DevKit                       MPU6050 (GY-521)
              ┌──────────────────┐                 ┌──────────────────┐
              │                  │                 │                  │
              │            3.3V ●──────────────────● VCC              │
              │             GND ●──────────────────● GND              │
              │                  │                 │                  │
              │  GPIO21 (SDA)   ●──────────────────● SDA              │
              │  GPIO22 (SCL)   ●──────────────────● SCL              │
              │                  │                 │                  │
              │                  │        GND ──────● AD0 (I2C addr   │
              │                  │                 │   = 0x68)        │
              │                  │                 │  INT  (unused)   │
              │                  │                 │  XDA  (unused)   │
              │                  │                 │  XCL  (unused)   │
              └──────────────────┘                 └──────────────────┘
```

> **Tip:** GPIO 21 (SDA) and GPIO 22 (SCL) are the ESP32's default I2C pins,
> so no `Wire.setPins()` remapping is needed — the sketch calls
> `Wire.begin(SDA_PIN, SCL_PIN)` explicitly with these values anyway.

---

## Project Files

| File | Description |
|---|---|
| [FruitNinja_ESP32.ino](FruitNinja_ESP32.ino) | Main sketch: WiFi AP, web server, MPU6050 driver (Adafruit library) |
| [index_html.h](index_html.h) | The entire game (HTML/CSS/JS canvas game) as a `PROGMEM` string, served at `/` |
| [platformio.ini](platformio.ini) | PlatformIO environment config and library dependencies |

---

## Software Setup / How to Upload the Code

### Option A: PlatformIO (recommended)

This project ships a [platformio.ini](platformio.ini) targeting a generic
`esp32dev` board and pulls in the required libraries automatically:

```ini
lib_deps =
	adafruit/Adafruit MPU6050@^2.2.6
	adafruit/Adafruit Unified Sensor@^1.1.14
```

1. Install [PlatformIO](https://platformio.org/) (standalone or the VS Code
   extension).
2. Open this folder as a PlatformIO project.
3. Wire the MPU6050 as described in the [pin diagram](#pin-diagram--wiring).
4. Update `upload_port` / `monitor_port` in `platformio.ini` to match your
   ESP32's serial port if it isn't `/dev/ttyUSB0`.
5. Build and upload (PlatformIO: Upload).

### Option B: Arduino IDE

1. **File → Preferences** → add to "Additional Board Manager URLs":
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. **Tools → Board → Boards Manager** → install **esp32** (by Espressif Systems).
3. **Tools → Manage Libraries** → install **Adafruit MPU6050** and
   **Adafruit Unified Sensor**.
4. Open `FruitNinja_ESP32.ino`. Make sure `index_html.h` is in the **same
   folder** as the `.ino` file (the IDE will show it as a second tab
   automatically).
5. **Tools → Board** → choose your ESP32 board (e.g. "ESP32 Dev Module").
6. **Tools → Port** → select the COM port your ESP32 is connected to.
7. Wire the MPU6050 as described in the [pin diagram](#pin-diagram--wiring).
8. Click **Upload**. If the board doesn't enter flashing mode automatically,
   hold the **BOOT** button while the IDE says "Connecting...".

### Connect and Play

1. Open the **Serial Monitor** (115200 baud) to confirm the AP started —
   you should see:
   ```
   AP started. Connect to WiFi "SpaceWar_ESP32" then open http://192.168.4.1
   AP IP address: 192.168.4.1
   ```
2. On your phone or laptop, connect to the WiFi network:
   - **SSID:** `SpaceWar_ESP32`
   - **Password:** `12345678`
3. Open a browser and go to **http://192.168.4.1**
4. Hold the ESP32 + MPU6050 flat, tap **Calibrate**, then tilt left/right to
   steer your fighter along the defense line.

---

## Gameplay

- **Steer:** Tilt the sensor left/right — the fighter slides along the bottom
  of the screen. Guns fire automatically.
- **Dodge:** Avoid enemy fire and don't ram mines — shoot mines from a
  distance instead (destroying one is worth 15 points).
- **Enemies:** Five enemy types (scout, interceptor, saucer, drone, cruiser),
  worth 10–22 points each. Cruisers shoot back.
- **Combo scoring:** Destroying enemies in quick succession builds a combo
  for bonus points.
- **Energy orb:** A blue power orb occasionally drifts down — collecting it
  is worth 30 points and triggers a temporary **overdrive** (faster firing
  rate) plus a brief slow-motion effect.
- **Lives:** You start with 3 hearts, shown top-left; getting hit costs a
  life.
- **High score:** Saved locally in the browser (`localStorage`), shown on
  the start/game-over screen.
- **Calibrate button:** Zeroes out the current tilt as "center" — use it any
  time the neutral resting angle drifts.

---

## Customization

- **WiFi name/password:** edit `AP_SSID` and `AP_PASSWORD` near the top of
  `FruitNinja_ESP32.ino` (password must be 8+ characters, or set to `""` for
  an open network).
- **I2C pins:** edit `SDA_PIN` / `SCL_PIN` in `FruitNinja_ESP32.ino` if you
  wire the MPU6050 to different GPIOs.
- **MPU6050 sensitivity/filtering:** edit the `mpu.setAccelerometerRange()`,
  `mpu.setGyroRange()`, `mpu.setFilterBandwidth()`, and
  `mpu.setHighPassFilter()` calls in `setup()`.
- **Game look/feel/difficulty:** edit `index_html.h` — it's plain HTML/CSS/JS
  (enemy types, spawn rates, scoring, etc. are defined near the top of the
  `<script>` section).

---

## Troubleshooting

| Problem | Likely Cause |
|---|---|
| Can't see the `SpaceWar_ESP32` WiFi network | ESP32 didn't boot / upload failed — check Serial Monitor for errors |
| Page won't load at `192.168.4.1` | Make sure your device is connected to the ESP32's WiFi, not your home WiFi |
| Serial prints "Failed to find MPU6050 chip!" | Check MPU6050 wiring, especially SDA/SCL and 3.3V power |
| Blade/ship drifts / doesn't center | Hold the sensor flat and tap **Calibrate** |
| Upload fails / port not found | Hold **BOOT** button during upload; check USB cable/drivers (CP2102/CH340); verify `upload_port` in `platformio.ini` |
| PlatformIO build fails, can't find libraries | Make sure PlatformIO has internet access on first build to fetch `lib_deps`, or install them manually via Library Manager |

---

## License

No license specified — add one if you plan to share or open-source this project.
