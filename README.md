# RTOS Gyro Mouse

An ESP32 + MPU6050 "air mouse" written twice, once as a plain super loop and once with FreeRTOS tasks, plus a browser game that makes the difference obvious. Built for a hands-on RTOS workshop.

Both sketches do the same two jobs with the same settings:

- Read the gyro every 10 ms and send how far the board turned as a serial line: `M,x,y,z,fire,gap_us`. The BOOT button is the fire button.
- Run a 30 ms "heavy job" every 100 ms, standing in for real work like Wi-Fi, a display or logging.

In `Mouse_Old` both jobs run one after another in `loop()`, so every time the heavy job runs the mouse stops for 30 ms and then jumps. In `Mouse_RTOS` the mouse is a high-priority task that wakes exactly every 10 ms with `vTaskDelayUntil`, and the heavy job is a low-priority task on the same core. The scheduler pre-empts the heavy job whenever the mouse is due, so aiming stays smooth. The `gap_us` field shows this directly: a steady 10000 means smooth updates.

## Files

- `ProjectCODES/Mouse_Old/Mouse_Old.ino` - super-loop version
- `ProjectCODES/Mouse_RTOS/Mouse_RTOS.ino` - FreeRTOS version
- `ProjectCODES/Aim_Game.html` - "Gyro Zombies", a Three.js aim game that reads the ESP32 over Web Serial
- `ProjectCODES/ESP32_MPU6050_Vis_NewCode.ino` - warm-up sketch: the ESP32 starts a Wi-Fi access point and serves a 3D orientation view (roll, pitch, yaw, raw accel/gyro) at http://192.168.4.1. Useful for checking the sensor wiring first.

## The game

Open `Aim_Game.html` in Chrome or Edge (Web Serial is needed), click Connect ESP32 and pick the board's port. "Set up aim" asks you to turn the board right and then tilt it up, and works out which gyro axes and directions to use, so the sensor can be mounted any way. Turn the board to aim, press BOOT to shoot, and survive a 60-second round. A small graph in the corner plots the update gaps live.

Without a board you can still play with the mouse, and pick "Feels like old code" or "Feels like RTOS code" to simulate the two timings. The side-by-side demo shows both timings at once for the same hand movement.

Click Disconnect in the game before uploading new code, or the serial port stays busy.

## Hardware

- ESP32 dev board (classic ESP32, BOOT button on GPIO 0)
- MPU6050 module (GY-521), address 0x68 or 0x69

| Signal | ESP32 GPIO |
| --- | --- |
| MPU6050 SDA | 21 |
| MPU6050 SCL | 22 |
| MPU6050 VCC | 3.3V |
| Fire button | 0 (BOOT) |
| Fire / status LED | 2 (onboard) |

The mouse sketches calibrate gyro bias at startup (LED blinks); keep the sensor still for a couple of seconds after reset. They accept any WHO_AM_I value, since clone chips often report a different ID.

## Stack

- Arduino C++ on the ESP32 core, FreeRTOS (built into the core), `Wire` only, no extra libraries
- Single-file HTML game with Three.js inlined, Web Serial API at 115200 baud

## Flash

Arduino IDE: install the "esp32 by Espressif Systems" board package, open `Mouse_Old.ino` or `Mouse_RTOS.ino`, select your board and port, and upload. Flash one, play a round, then flash the other and compare.

arduino-cli:

```bash
arduino-cli core install esp32:esp32
cd ProjectCODES
arduino-cli compile --fqbn esp32:esp32:esp32 Mouse_Old
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 Mouse_Old
arduino-cli compile --fqbn esp32:esp32:esp32 Mouse_RTOS
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 Mouse_RTOS
```

`ESP32_MPU6050_Vis_NewCode.ino` sits directly in `ProjectCODES/`, so move it into a folder of the same name (`ESP32_MPU6050_Vis_NewCode/`) before opening it in Arduino. Its access point name and password are set by `AP_SSID` and `AP_PASSWORD` at the top of the file.
