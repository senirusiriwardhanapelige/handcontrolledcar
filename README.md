# Hand-Controlled Car

A car driven by tilting your hand. An ADXL345 accelerometer on the transmitter reads hand tilt and sends direction commands over ESP-NOW to an ESP32-CAM receiver, which drives two DC motors through an L298N motor driver and hosts a camera webpage.

![ESP32](https://img.shields.io/badge/ESP32-Dev%20Module-blue?logo=espressif)
![ESP-NOW](https://img.shields.io/badge/Protocol-ESP--NOW-green)
![L298N](https://img.shields.io/badge/Driver-L298N-orange)

## What it does

Tilt the transmitter in your hand and the car follows — forward, backward, left, right. Speed scales with tilt angle. The receiver hosts a camera webpage so you can see what the car sees.

## Parts

**Transmitter (hand):**
- ESP32 Dev Module
- ADXL345 accelerometer
- Battery pack

**Receiver (car):**
- ESP32-CAM AI Thinker
- L298N motor driver
- Two DC gear motors
- 7.4V battery pack and 5V regulator

## Wiring

**Transmitter to ADXL345:**

| ADXL345 | ESP32 GPIO |
|---------|------------|
| SDA     | GPIO 14    |
| SCL     | GPIO 12    |
| VCC + CS| 3V3        |
| GND + SDO| GND       |

**Receiver to L298N:**

| L298N | ESP32 GPIO |
|-------|------------|
| IN1   | GPIO 13    |
| IN2   | GPIO 12    |
| IN3   | GPIO 14    |
| IN4   | GPIO 15    |
| ENA   | GPIO 2     |
| ENB   | GPIO 4     |

The battery pack feeds the L298N motor power and the 5V regulator input. The regulator feeds the ESP board. Tie all grounds together — battery, regulator, ESP, L298N.

## Files

- `handcontrolledcar_tx.ino` — Transmitter code
- `handcontrolledcar_rx_cam.ino` — Receiver code

## Setup

1. Upload `handcontrolledcar_tx.ino` to the transmitter board.
2. Upload `handcontrolledcar_rx_cam.ino` to the ESP32-CAM receiver.
3. Open Serial Monitor on each board at 115200. Copy the receiver's MAC address into `carMac[]` in the transmitter code, then re-upload.
4. Connect your phone to WiFi `CarCam` (password `12345678`) and open `http://192.168.4.1` for the camera stream.
5. Tilt the transmitter to drive.

## Speed control

- Below 5 degrees tilt: car stops
- 5 to 60 degrees: speed scales directly with tilt angle (slow at 5 degrees, full speed at 60 degrees)
- Turns: both motors move forward, one slower than the other, so the car curves instead of spinning in place

## Links

- [View transmitter code](handcontrolledcar_tx.ino)
- [View receiver code](handcontrolledcar_rx_cam.ino)

> Want to add a demo video or wiring diagram? Upload an image to this repo, copy its address, and paste it here as `![alt text](image-url)`.
