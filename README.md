# Hand Controlled Car

Tilt your hand, the car drives. That's the idea — an ADXL345 accelerometer on an ESP32 reads hand tilt and beams commands over ESP-NOW to an ESP8266 on the car, which drives two DC motors through an L298N. No Wi-Fi router, no phone app, nothing between your hand and the car but radio waves.

## What you need

**Transmitter (hand side):**
- ESP32 WROOM-32E board
- ADXL345 accelerometer (GY-291 module)
- Battery pack

**Receiver (car side):**
- ESP8266 NodeMCU
- L298N dual H-bridge motor driver
- Two DC gear motors
- 7.4V battery pack and a 5V step-down regulator for the ESP8266

## Wiring

**Transmitter (right side of the ESP32 board):**
- GPIO 14 → ADXL345 SDA
- GPIO 12 → ADXL345 SCL
- 3V3 → ADXL345 VCC and CS
- GND → ADXL345 GND and SDO

**Receiver (NodeMCU):**
- D1 / GPIO 5 → L298N IN1
- D2 / GPIO 4 → L298N IN2
- D5 / GPIO 14 → L298N IN3
- D6 / GPIO 12 → L298N IN4

The L298N gets motor power from the 7.4V pack, and its logic 5V feeds the NodeMCU. Keep a common ground across the battery, regulator, ESP8266, and L298N — without it nothing will move.

## Getting it running

1. Upload `handcontrolledcar_tx.ino` to the ESP32 transmitter. Open Serial Monitor at 115200 — it prints its MAC address.
2. Upload `handcontrolledcar_rx.ino` to the NodeMCU receiver. Its Serial Monitor prints its MAC address too.
3. Paste the receiver's MAC into `carMac[]` in the transmitter code, then re-upload.
4. Tilt the ESP32 forward/back/left/right and the car should follow.

## Files

- `handcontrolledcar_tx.ino` — Transmitter (ESP32). Reads the accelerometer, sends tilt commands over ESP-NOW.
- `handcontrolledcar_rx.ino` — Receiver (ESP8266). Listens for commands, drives the L298N motors. Stops if no command arrives for half a second.
