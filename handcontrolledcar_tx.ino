#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_Sensor.h>

const float TILT_THRESHOLD = 5.0;
uint8_t carMac[] = {0x5C, 0xCF, 0x7F, 0xAA, 0x62, 0x12};

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
          Serial.printf("Found 0x%02X  SDA=%d SCL=%d\n", addr, sda, scl);
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
  delay(2000);
  Serial.println("Scanning for ADXL345...");

  uint8_t addr = 0;
  if (!(findSensor(addr) && accel.begin(addr))) {
    while (1) {
      Serial.println("ADXL345 not found");
      delay(1000);
    }
  }
  accel.setRange(ADXL345_RANGE_16_G);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    while (1) {
      Serial.println("ESP-NOW init failed");
      delay(1000);
    }
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, carMac, 6);
  peer.channel = 1;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.println("Ready");
}

void loop() {
  sensors_event_t event;
  accel.getEvent(&event);
  float x = event.acceleration.x;
  float y = event.acceleration.y;

  char cmd = 'S';
  if (abs(x) > abs(y)) {
    if (x > TILT_THRESHOLD) cmd = 'R';
    else if (x < -TILT_THRESHOLD) cmd = 'L';
  } else {
    if (y > TILT_THRESHOLD) cmd = 'F';
    else if (y < -TILT_THRESHOLD) cmd = 'B';
  }

  esp_now_send(carMac, (uint8_t *)&cmd, 1);
  Serial.printf("X=%.2f Y=%.2f | %c\n", x, y, cmd);
  delay(80);
}