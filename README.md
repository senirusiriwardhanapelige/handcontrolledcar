# Hand-Controlled Car

Tilt your hand, the car drives. An ADXL345 accelerometer on the transmitter reads hand tilt and beams commands over ESP-NOW to another board on the car, which drives two DC motors through an L298N motor driver. No Wi-Fi router, no phone app — just hand motion and radio waves. The receiver also hosts a camera webpage so you can see what the car sees.

## Parts

**Transmitter (hand side):**
- ESP board (any NodeMCU-style board)
- ADXL345 accelerometer
- Battery pack (3.7V LiPo or 2×AA)

**Receiver (car side):**
- ESP32-CAM AI Thinker board
- L298N motor driver
- Two DC gear motors
- 7.4V battery pack and a 5V regulator for the ESP board

## Wiring

**Transmitter:**
- GPIO 14 → ADXL345 SDA
- GPIO 12 → ADXL345 SCL
- 3V3 → ADXL345 VCC + CS
- GND → ADXL345 GND + SDO

**Receiver:**
- GPIO 13 → L298N IN1
- GPIO 12 → L298N IN2
- GPIO 14 → L298N IN3
- GPIO 15 → L298N IN4
- GPIO 2 → L298N ENA
- GPIO 4 → L298N ENB

The L298N runs off the battery pack, and its 5V output feeds the ESP board. All grounds need to be tied together — battery, regulator, ESP, and L298N — or nothing will move.

## Files

- `handcontrolledcar_tx.ino` — Transmitter code. Reads the accelerometer, sends tilt commands over ESP-NOW.
- `handcontrolledcar_rx_cam.ino` — Receiver code (ESP32-CAM). Listens for ESP-NOW commands, drives the L298N motors, hosts a camera webpage.

## Getting it running

1. Upload `handcontrolledcar_tx.ino` to the transmitter ESP board. Open Serial Monitor at 115200 — it prints its MAC address.
2. Upload `handcontrolledcar_rx_cam.ino` to the ESP32-CAM receiver. Its Serial Monitor prints its MAC address and AP IP.
3. Paste the receiver's MAC address into `carMac[]` in the transmitter code, then re-upload.
4. Connect your phone to WiFi "CarCam" (password `12345678`) and open `http://192.168.4.1` to view the camera stream.
5. Tilt the transmitter — the car should follow.

## Features

- Smooth differential steering: both motors move forward during turns, one slower than the other, so the car curves instead of spinning in place
- PWM ramping: motors accelerate/decelerate gradually instead of jerking
- Max speed capped at 140/255 for controllable driving
- Auto-stop: motors stop if no command arrives for 500ms
- Camera stream on port 81, control page on port 80
