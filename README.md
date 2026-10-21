# Hand-Controlled Car

A car you drive by tilting your hand. An ADXL345 accelerometer on the transmitter board picks up the tilt, and ESP-NOW sends the command straight to the car board — no router, no app, nothing between your hand and the motors but radio waves.

## Parts

Transmitter side:
- Any NodeMCU-style ESP board
- ADXL345 accelerometer
- Battery pack

Receiver side:
- NodeMCU-style ESP board
- L298N motor driver
- Two DC gear motors
- 7.4V battery pack and a 5V regulator for the ESP

## Wiring

Transmitter:
- GPIO 14 → ADXL345 SDA
- GPIO 12 → ADXL345 SCL
- 3V3 → ADXL345 VCC + CS
- GND → ADXL345 GND + SDO

Receiver:
- GPIO 5 → L298N IN1
- GPIO 4 → L298N IN2
- GPIO 14 → L298N IN3
- GPIO 12 → L298N IN4

The L298N runs off the battery pack, and its 5V output feeds the ESP board. All grounds need to be tied together — battery, regulator, ESP, and L298N — or nothing will move.

## Files

handcontrolledcar_tx.ino — reads the accelerometer, sends tilt commands over ESP-NOW

handcontrolledcar_rx.ino — listens for commands, drives the L298N motors. Stops if nothing comes in for half a second.

Upload each sketch to its board, grab the receiver's MAC from Serial Monitor, paste it into carMac[] in the transmitter code, and upload again.
