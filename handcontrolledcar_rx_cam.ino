/*
 * handcontrolledcar_rx_cam.ino
 * ESP32-CAM AI Thinker Car Receiver
 * Receives ESP-NOW commands, drives L298N motors with smooth steering, streams camera via local webpage
 * 
 * BOARD: ESP32 Dev Module (AI Thinker ESP32-CAM)
 * Upload via USB programmer shield: hold BOOT, press RESET, release BOOT, then click Upload
 * After upload: remove GPIO 0->GND jumper, press RESET to run
 * 
 * MOTOR PINS:
 *   IN1 = GPIO 13, IN2 = GPIO 12, IN3 = GPIO 14, IN4 = GPIO 15
 *   ENA = GPIO 16,   ENB = GPIO 17
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
#define ENA 16
#define ENB 17

#define PWM_CH_A 0
#define PWM_CH_B 1
#define PWM_FREQ 1000
#define PWM_RES 8

// Speed limits (lower = slower, 0-255 range)
#define MAX_SPEED   140   // Full forward speed (was 200, too fast)
#define TURN_SPEED   90   // Slower motor during a turn
#define BASE_SPEED   100  // Base speed for smooth driving

// Ramping step (how fast PWM changes per loop iteration)
#define RAMP_STEP  15     // Smaller = smoother, slower response

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
volatile char lastCmd = 'S';
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
  <title>ESP32-CAM Car Receiver</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; text-align: center; background: #1a1a1a; color: #eee; margin: 0; padding: 20px; }
    h1 { color: #0d47a1; margin-bottom: 10px; }
    img { width: 100%; max-width: 640px; border: 3px solid #0d47a1; border-radius: 8px; background: #000; }
    .controls { margin-top: 20px; }
    .status { margin-top: 10px; color: #888; font-size: 14px; }
    .btn { display: inline-block; padding: 10px 20px; margin: 5px; background: #0d47a1; color: #fff; text-decoration: none; border-radius: 4px; font-weight: bold; }
    .btn:active { background: #1565c0; }
    .btn.stop { background: #b71c1c; }
    .btn.stop:active { background: #d32f2f; }
  </style>
</head>
<body>
  <h1>ESP32-CAM Car Receiver</h1>
  <img id="stream" src="/stream?t=0" alt="Camera stream">
  <div class="controls">
    <a class="btn" href="/forward">Forward</a>
    <a class="btn" href="/backward">Backward</a>
    <a class="btn" href="/left">Left</a>
    <a class="btn" href="/right">Right</a>
    <a class="btn stop" href="/stop">Stop</a>
  </div>
  <div class="status" id="status">Last command: --</div>
  <script>
    var t = 0;
    setInterval(function() {
      t++;
      document.getElementById('stream').src = '/stream?t=' + t;
      fetch('/status').then(r => r.text()).then(txt => {
        document.getElementById('status').textContent = 'Last command: ' + txt;
      });
    }, 150);
  </script>
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
  ledcSetup(PWM_CH_A, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA, PWM_CH_A);
  ledcSetup(PWM_CH_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENB, PWM_CH_B);
  stopMotors();

  // Print MAC address
  WiFi.mode(WIFI_AP);
  WiFi.softAP("CarCam", "12345678");
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

  server.on("/forward", HTTP_GET, []() {
    lastCmd = 'F';
    lastRx = millis();
    server.send(200, "text/plain", "Forward");
  });

  server.on("/backward", HTTP_GET, []() {
    lastCmd = 'B';
    lastRx = millis();
    server.send(200, "text/plain", "Backward");
  });

  server.on("/left", HTTP_GET, []() {
    lastCmd = 'L';
    lastRx = millis();
    server.send(200, "text/plain", "Left");
  });

  server.on("/right", HTTP_GET, []() {
    lastCmd = 'R';
    lastRx = millis();
    server.send(200, "text/plain", "Right");
  });

  server.on("/stop", HTTP_GET, []() {
    lastCmd = 'S';
    lastRx = millis();
    server.send(200, "text/plain", "Stop");
  });

  server.on("/status", HTTP_GET, []() {
    server.send(200, "text/plain", String(lastCmd));
  });

  server.begin();
  Serial.print("Camera stream: http://");
  Serial.println(WiFi.localIP());

  // WiFi + ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) delay(1000);
  }

  esp_now_register_recv_cb([](const esp_now_recv_info_t* info, const uint8_t* data, int data_len) {
    if (data_len > 0) {
      lastCmd = (char)data[0];
      lastRx = millis();
      Serial.print("ESP-NOW received: ");
      Serial.println(lastCmd);
    }
  });

  Serial.println("ESP-NOW ready. Waiting for commands...");
}

void loop() {
  server.handleClient();

  // Set target PWM based on current command
  switch (lastCmd) {
    case 'F':
      targetPWM_A = MAX_SPEED;
      targetPWM_B = MAX_SPEED;
      break;
    case 'B':
      targetPWM_A = MAX_SPEED;
      targetPWM_B = MAX_SPEED;
      break;
    case 'L':
      // Smooth left turn: left motor slower, right motor faster (both forward)
      targetPWM_A = TURN_SPEED;
      targetPWM_B = MAX_SPEED;
      break;
    case 'R':
      // Smooth right turn: right motor slower, left motor faster (both forward)
      targetPWM_A = MAX_SPEED;
      targetPWM_B = TURN_SPEED;
      break;
    case 'S':
    default:
      targetPWM_A = 0;
      targetPWM_B = 0;
      break;
  }

  // Ramp current PWM toward target PWM (smooth acceleration/deceleration)
  if (currentPWM_A < targetPWM_A) {
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
  applyMotors(lastCmd, currentPWM_A, currentPWM_B);

  delay(10);
}

// ================= MOTOR FUNCTIONS =================
// Applies direction + PWM to both motors
void applyMotors(char cmd, int pwmA, int pwmB) {
  if (pwmA == 0 && pwmB == 0) {
    // Stop
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
    ledcWrite(PWM_CH_A, 0);
    ledcWrite(PWM_CH_B, 0);
    return;
  }

  // Determine direction from command
  bool dirA_forward, dirB_forward;
  switch (cmd) {
    case 'F': dirA_forward = true;  dirB_forward = true;  break;
    case 'B': dirA_forward = false; dirB_forward = false; break;
    case 'L': dirA_forward = true;  dirB_forward = true;  break; // both forward, left slower
    case 'R': dirA_forward = true;  dirB_forward = true;  break; // both forward, right slower
    default:  dirA_forward = true;  dirB_forward = true;  break;
  }

  // Left motor (IN1/IN2, ENA)
  if (dirA_forward) {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  } else {
    digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  }
  ledcWrite(PWM_CH_A, pwmA);

  // Right motor (IN3/IN4, ENB)
  if (dirB_forward) {
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  } else {
    digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  }
  ledcWrite(PWM_CH_B, pwmB);
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  ledcWrite(PWM_CH_A, 0);
  ledcWrite(PWM_CH_B, 0);
}
