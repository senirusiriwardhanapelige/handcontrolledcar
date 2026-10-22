# Hand-Controlled Car

A car driven by tilting your hand. An ADXL345 accelerometer on the transmitter reads hand tilt and sends direction commands over ESP-NOW to an ESP32-CAM receiver, which drives two DC motors through an L298N motor driver and hosts a camera webpage.

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

**Transmitter → ADXL345:**
- GPIO 14 → SDA
- GPIO 12 → SCL
- 3V3 → VCC + CS
- GND → GND + SDO

**Receiver → L298N:**
- GPIO 13 → IN1
- GPIO 12 → IN2
- GPIO 14 → IN3
- GPIO 15 → IN4
- GPIO 2 → ENA
- GPIO 4 → ENB

The battery pack feeds the L298N motor power and the 5V regulator input. The regulator feeds the ESP board. Tie all grounds together — battery, regulator, ESP, L298N.

## Files

- `handcontrolledcar_tx.ino` — Transmitter code
- `handcontrolledcar_rx_cam.ino` — Receiver code

## Setup

1. Upload `handcontrolledcar_tx.ino` to the transmitter board.
2. Upload `handcontrolledcar_rx_cam.ino` to the ESP32-CAM receiver.
3. Open Serial Monitor on each board at 115200. Copy the receiver's MAC address into `carMac[]` in the transmitter code, then re-upload.
4. Connect your phone to WiFi "CarCam" (password `12345678`) and open `http://192.168.4.1` for the camera stream.
5. Tilt the transmitter to drive.
