# Hand Controlled Car

Tilt your hand, the car drives. An ADXL345 accelerometer on an ESP board reads hand tilt and beams commands over ESP-NOW to another board on the car, which drives two DC motors through an L298N motor driver. No Wi-Fi router, no phone app — just hand motion and radio waves.

## Parts

**Transmitter (hand side):**
- ESP board (any NodeMCU-style board)
- ADXL345 accelerometer
- Battery pack

**Receiver (car side):**
- ESP board (NodeMCU-style)
- L298N motor driver
- Two DC gear motors
- Battery pack and a 5V regulator for the ESP board

## Wiring

**Transmitter:**
- GPIO 14 → ADXL345 SDA
- GPIO 12 → ADXL345 SCL
- 3V3 → ADXL345 VCC + CS
- GND → ADXL345 GND + SDO

**Receiver:**
- GPIO 5 → L298N IN1
- GPIO 4 → L298N IN2
- GPIO 14 → L298N IN3
- GPIO 12 → L298N IN4

The L298N runs off the battery pack, and its 5V output feeds the ESP board. Keep a common ground across the battery, regulator, ESP, and L298N — without it nothing will move.

## Files

- `handcontrolledcar_tx.ino` — Transmitter code. Reads the accelerometer, sends tilt commands over ESP-NOW.
- `handcontrolledcar_rx.ino` — Receiver code. Listens for commands, drives the L298N motors. Stops if no command arrives for half a second.

Upload each sketch to its board, paste the receiver's MAC address into `carMac[]` in the transmitter code, and you're set.
