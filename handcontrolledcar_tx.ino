/*
 * handcontrolledcar_tx.ino
 * ESP32 Dev Module Hand Controller (Transmitter)
 * Reads ADXL345 tilt, converts angle to speed, sends over ESP-NOW to car.
 * 
 * BOARD: ESP32 Dev Module
 * PINS (right side): GPIO 14 = SDA, GPIO 12 = SCL, 3V3 = VCC+CS, GND = GND+SDO
 * 
 * Tilt angle to speed:
 *   Below 20°   = STOP
 *   20° to 60°  = speed ramps up from 60 to 255
 *   Over 60°    = full speed 255
 */

#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_Sensor.h>
#include <math.h>

const float TILT_THRESHOLD = 5.0;    // degrees - start moving at this angle
const float MIN_SPEED = 40;          // speed at 5° (slower start)
const float MAX_SPEED = 150;         // speed at 60° or more
const float GRAVITY = 9.81;          // m/s^2
const float MAX_TILT_ANGLE = 60.0;   // degrees - full speed at this angle

uint8_t carMac[] = {0xA0, 0xB7, 0x65, 0x5F, 0x15, 0xD0};

struct Command {
  char cmd;     // 'F', 'B', 'L', 'R', 'S'
  uint8_t speed; // 0-255
};
Command txCmd;

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

const int scanPins[] = {1, 2, 3, 6, 7, 8, 9, 10, 11, 12, 14, 15,
                        17, 18, 21, 38, 39, 40, 41, 42, 47, 48};

bool findSensor(uint8_t &addrOut) {
  for (int sda : scanPins) {
    for (int scl : scanPins) {
      if (sda == scl) continue;
      Wire.begin(sda, scl);
      for (uint8_t addr : {0x53, 0x1D}) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
          addrOut = addr;
          return true;
        }
      }
      Wire.end();
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 Hand Controller TX ===");

  Wire.begin();
  uint8_t addr = 0;
  if (!findSensor(addr) || !accel.begin(addr)) {
    Serial.println("ADXL345 not found! Check wiring.");
    while (1) delay(1000);
  }
  accel.setRange(ADXL345_RANGE_16_G);
  Serial.println("ADXL345 initialized");

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) delay(1000);
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, carMac, 6);
  peer.channel = 1;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.println("TX Ready. Tilt to drive.");
}

void loop() {
  sensors_event_t event;
  accel.getEvent(&event);

  float x = event.acceleration.x;
  float y = event.acceleration.y;

  char cmd = 'S';
  uint8_t speed = 0;

  if (abs(x) > abs(y)) {
    // X-axis dominant: Left / Right
    float g = abs(x) / GRAVITY;
    if (g > 1.0) g = 1.0;
    float angle = asin(g) * 180.0 / PI;

    if (angle >= TILT_THRESHOLD) {
      cmd = (x > 0) ? 'R' : 'L';
      speed = (uint8_t)constrain(
        MIN_SPEED + (angle - TILT_THRESHOLD) * (MAX_SPEED - MIN_SPEED) / (MAX_TILT_ANGLE - TILT_THRESHOLD),
        MIN_SPEED, MAX_SPEED);
    }
  } else {
    // Y-axis dominant: Forward / Back
    float g = abs(y) / GRAVITY;
    if (g > 1.0) g = 1.0;
    float angle = asin(g) * 180.0 / PI;

    if (angle >= TILT_THRESHOLD) {
      cmd = (y > 0) ? 'F' : 'B';
      speed = (uint8_t)constrain(
        MIN_SPEED + (angle - TILT_THRESHOLD) * (MAX_SPEED - MIN_SPEED) / (MAX_TILT_ANGLE - TILT_THRESHOLD),
        MIN_SPEED, MAX_SPEED);
    }
  }

  txCmd.cmd = cmd;
  txCmd.speed = speed;

  esp_now_send(carMac, (uint8_t *)&txCmd, sizeof(txCmd));

  Serial.print("X="); Serial.print(x, 2);
  Serial.print(" Y="); Serial.print(y, 2);
  Serial.print(" Cmd="); Serial.print(cmd);
  Serial.print(" Spd="); Serial.println(speed);

  delay(50);
}
