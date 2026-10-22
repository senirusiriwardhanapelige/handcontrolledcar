#include <WiFi.h>
#include <esp_now.h>
#include <esp_camera.h>
#include <esp_http_server.h>
#include <WebServer.h>

// Motor pins
#define IN1 13
#define IN2 12
#define IN3 14
#define IN4 15
#define ENA 2
#define ENB 4

#define PWM_FREQ 1000
#define PWM_RES 8

#define MAX_SPEED  100
#define TURN_SPEED  90
#define RAMP_STEP   15

// Camera pins (AI Thinker)
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

volatile char lastCmd = 'S';
volatile unsigned long lastRx = 0;

int currentPWM_A = 0, currentPWM_B = 0;
int targetPWM_A = 0, targetPWM_B = 0;

WebServer server(80);

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

// Video stream on port 81
static esp_err_t stream_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  char part[80];
  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return ESP_FAIL;
    size_t hlen = snprintf(part, sizeof(part),
      "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
    esp_err_t res = httpd_resp_send_chunk(req, part, hlen);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, "\r\n", 2);
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;
  }
  return ESP_OK;
}

void startStream() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 81;
  cfg.ctrl_port = 32769;
  httpd_handle_t h = NULL;
  httpd_uri_t uri = {"/stream", HTTP_GET, stream_handler, NULL};
  if (httpd_start(&h, &cfg) == ESP_OK) httpd_register_uri_handler(h, &uri);
}

// Motors
void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);
}

void applyMotors(char cmd, int pwmA, int pwmB) {
  if (pwmA == 0 && pwmB == 0) {
    stopMotors();
    return;
  }
  bool leftFwd  = (cmd != 'B' && cmd != 'L');
  bool rightFwd = (cmd != 'B' && cmd != 'R');
  digitalWrite(IN1, leftFwd ? HIGH : LOW);
  digitalWrite(IN2, leftFwd ? LOW : HIGH);
  digitalWrite(IN3, rightFwd ? HIGH : LOW);
  digitalWrite(IN4, rightFwd ? LOW : HIGH);
  ledcWrite(ENA, pwmA);
  ledcWrite(ENB, pwmB);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32-CAM Car Receiver ===");

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  ledcAttachChannel(ENA, PWM_FREQ, PWM_RES, 2);
  ledcAttachChannel(ENB, PWM_FREQ, PWM_RES, 3);
  stopMotors();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("CarCam", "12345678", 1);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  Serial.print("RX MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    while (1) delay(1000);
  }
  Serial.println("Camera initialized");

  sensor_t *s = esp_camera_sensor_get();
  s->set_whitebal(s, 1);
  s->set_gain_ctrl(s, 1);
  s->set_exposure_ctrl(s, 1);
  s->set_lenc(s, 1);
  s->set_dcw(s, 1);

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", index_html); });
  server.begin();
  startStream();
  Serial.println("Page: http://192.168.4.1");

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) delay(1000);
  }
  esp_now_register_recv_cb([](const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len > 0) {
      lastCmd = (char)data[0];
      lastRx = millis();
    }
  });
  Serial.println("ESP-NOW ready");
}

void loop() {
  server.handleClient();

  // Stop if no command for 500 ms
  char cmd = (millis() - lastRx > 500) ? 'S' : lastCmd;

  switch (cmd) {
    case 'F':
    case 'B': targetPWM_A = MAX_SPEED;  targetPWM_B = MAX_SPEED;  break;
    case 'L':
    case 'R': targetPWM_A = TURN_SPEED; targetPWM_B = TURN_SPEED; break;
    default:  targetPWM_A = 0;          targetPWM_B = 0;          break;
  }

  if (currentPWM_A < targetPWM_A) currentPWM_A = min(currentPWM_A + RAMP_STEP, targetPWM_A);
  else if (currentPWM_A > targetPWM_A) currentPWM_A = max(currentPWM_A - RAMP_STEP, targetPWM_A);

  if (currentPWM_B < targetPWM_B) currentPWM_B = min(currentPWM_B + RAMP_STEP, targetPWM_B);
  else if (currentPWM_B > targetPWM_B) currentPWM_B = max(currentPWM_B - RAMP_STEP, targetPWM_B);

  applyMotors(cmd, currentPWM_A, currentPWM_B);
  delay(10);
}