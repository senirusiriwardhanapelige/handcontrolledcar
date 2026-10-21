#include <ESP8266WiFi.h>
#include <espnow.h>

// L298N inputs
#define IN1 5   // D1
#define IN2 4   // D2
#define IN3 14  // D5
#define IN4 12  // D6

volatile char lastCmd = 'S';
volatile unsigned long lastRx = 0;

void drive(int a, int b, int c, int d) {
  digitalWrite(IN1, a);
  digitalWrite(IN2, b);
  digitalWrite(IN3, c);
  digitalWrite(IN4, d);
}

void onReceive(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len < 1) return;
  lastCmd = (char)data[0];
  lastRx = millis();
}

void setup() {
  Serial.begin(115200);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  drive(LOW, LOW, LOW, LOW);

  WiFi.mode(WIFI_STA);
  wifi_set_channel(1);

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    while (1) delay(1000);
  }
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onReceive);
}

void loop() {
  // Stop if no command for 500 ms
  char cmd = (millis() - lastRx > 500) ? 'S' : lastCmd;

  switch (cmd) {
    case 'F': drive(HIGH, LOW, HIGH, LOW); break;
    case 'B': drive(LOW, HIGH, LOW, HIGH); break;
    case 'L': drive(LOW, HIGH, HIGH, LOW); break;
    case 'R': drive(HIGH, LOW, LOW, HIGH); break;
    default:  drive(LOW, LOW, LOW, LOW);   break;
  }
  delay(10);
}