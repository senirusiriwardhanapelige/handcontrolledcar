/*
 * handcontrolledcar_rx_cam.ino
 * ESP32-CAM AI Thinker Car Receiver
 * Receives ESP-NOW commands (cmd + speed), drives L298N motors with smooth steering,
 * streams camera via local webpage.
 * 
 * BOARD: ESP32 Dev Module (AI Thinker ESP32-CAM)
 * Upload via USB programmer shield: hold BOOT, press RESET, release BOOT, then click Upload
 * After upload: remove GPIO 0->GND jumper, press RESET to run
 * 
 * MOTOR PINS:
 *   IN1 = GPIO 13, IN2 = GPIO 12, IN3 = GPIO 14, IN4 = GPIO 15
 *   ENA = GPIO 2,   ENB = GPIO 4   (GPIO 16 tied to PSRAM, GPIO 17 not broken out)
 * 
 * POWER:
 *   7.4V pack -> L298N motor power + 5V regulator input
 *   Regulator 5V out -> ESP32-CAM 5V pin
 *   Common GND across battery, regulator, ESP32-CAM, L298N
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_camera.h>
#include <WebServer.h>
#include <driver/ledc.h>

// ================= MOTOR PINS =================
#define IN1 13
#define IN2 12
#define IN3 14
#define IN4 15
#define ENA 2
#define ENB 4

#define PWM_FREQ 1000
#define PWM_RES 8

#define RAMP_STEP   15    // Smaller = smoother acceleration
#define MIN_SPEED   40    // minimum speed so both wheels turn at low tilt

// ================= CAMERA PINS (AI Thinker ESP32-CAM) =================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ================= GLOBALS =================
struct Command {
  char cmd;     // 'F', 'B', 'L', 'R', 'S'
  uint8_t speed; // 0-255
};

volatile Command lastCmd = {'S', 0};
volatile unsigned long lastRx = 0;

int currentPWM_A = 0;   // Current PWM for left motor (ramping)
int currentPWM_B = 0;   // Current PWM for right motor (ramping)
int targetPWM_A = 0;    // Target PWM for left motor
int targetPWM_B = 0;    // Target PWM for right motor

WebServer server(80);

// Forward declarations
void stopMotors();
void applyMotors(char cmd, int pwmA, int pwmB);

// ================= HTML PAGE =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Car Camera</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html, body { margin: 0; height: 100%; background: #000; }
    img { width: 100%; height: 100%; object-fit: contain; display: block; }
  </style>
</head>
<body>
  <img src="http://192.168.4.1:81/stream" alt="">
</body>
</html>
)rawliteral";

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=== ESP32-CAM Car Receiver (RX) ===");

  // Motor driver pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  ledcAttachChannel(ENA, PWM_FREQ, PWM_RES, 2);
  ledcAttachChannel(ENB, PWM_FREQ, PWM_RES, 3);
  stopMotors();

  // WiFi in AP mode — broadcasts its own network, no router needed
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("CarCam", "12345678", 1);
  Serial.print("RX MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Initialize camera (AI Thinker)
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    while (1) delay(1000);
  }
  Serial.println("Camera initialized");

  // Web server routes
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", index_html);
  });

  server.on("/stream", HTTP_GET, []() {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      server.send(500, "text/plain", "Camera capture failed");
      return;
    }
    server.send(200, "image/jpeg", String((const char *)fb->buf, fb->len));
    esp_camera_fb_return(fb);
  });

  server.begin();
  Serial.print("Camera stream: http://");
  Serial.println(WiFi.softAPIP());

  // WiFi + ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) delay(1000);
  }

  esp_now_register_recv_cb([](const esp_now_recv_info_t* info, const uint8_t* data, int data_len) {
    if (data_len == sizeof(Command)) {
      memcpy((void *)&lastCmd, data, sizeof(Command));
      lastRx = millis();
      Serial.print("ESP-NOW: cmd=");
      Serial.print(lastCmd.cmd);
      Serial.print(" speed=");
      Serial.println(lastCmd.speed);
    }
  });

  Serial.println("ESP-NOW ready. Waiting for commands...");
}

void loop() {
  server.handleClient();

  // Stop if no command for 500 ms
  char cmd = (millis() - lastRx > 500) ? 'S' : lastCmd.cmd;
  int speed = (millis() - lastRx > 500) ? 0 : lastCmd.speed;

  // Set target PWM based on command and proportional speed
  switch (cmd) {
    case 'F':
      targetPWM_A = speed;
      targetPWM_B = speed;
      break;
    case 'B':
      targetPWM_A = speed;
      targetPWM_B = speed;
      break;
    case 'L':
      // Smooth left turn: left motor slower, right motor faster (both forward)
      targetPWM_A = max((int)(speed * 0.6), MIN_SPEED);
      targetPWM_B = speed;
      break;
    case 'R':
      // Smooth right turn: right motor slower, left motor faster (both forward)
      targetPWM_A = speed;
      targetPWM_B = max((int)(speed * 0.6), MIN_SPEED);
      break;
    default:
      targetPWM_A = 0;
      targetPWM_B = 0;
      break;
  }

  // Ramp current PWM toward target PWM (smooth acceleration/deceleration)
    currentPWM_A = min(currentPWM_A + RAMP_STEP, targetPWM_A);
  } else if (currentPWM_A > targetPWM_A) {
    currentPWM_A = max(currentPWM_A - RAMP_STEP, targetPWM_A);
  }

  if (currentPWM_B < targetPWM_B) {
    currentPWM_B = min(currentPWM_B + RAMP_STEP, targetPWM_B);
  } else if (currentPWM_B > targetPWM_B) {
    currentPWM_B = max(currentPWM_B - RAMP_STEP, targetPWM_B);
  }

  // Apply motor directions and speeds
  applyMotors(cmd, currentPWM_A, currentPWM_B);

  delay(10);
}

// ================= MOTOR FUNCTIONS =================
void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

void applyMotors(char cmd, int pwmA, int pwmB) {
  if (pwmA == 0 && pwmB == 0) {
    stopMotors();
    return;
  }

  // Determine direction from command
  bool fwd = (cmd != 'B');

  // Left motor (IN1/IN2, ENA)
  if (fwd) {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  } else {
    digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  }
  analogWrite(ENA, pwmA);

  // Right motor (IN3/IN4, ENB)
  if (fwd) {
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  } else {
    digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  }
  analogWrite(ENB, pwmB);
}
