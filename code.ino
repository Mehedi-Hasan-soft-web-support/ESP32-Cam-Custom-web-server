#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ===================
// Select camera model
// ===================
#define CAMERA_MODEL_AI_THINKER // Has PSRAM

// ===================
// Camera Pin Definitions
// ===================
#if defined(CAMERA_MODEL_AI_THINKER)
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
#define LED_GPIO_NUM       4  // Flash LED
#endif

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "Me";
const char* password = "mehedi113";

// ===========================
// Global Variables
// ===========================
httpd_handle_t camera_httpd = NULL;

// ===========================
// HTML Page for Live Stream
// ===========================
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32-CAM Live Stream</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin: 0;
      padding: 20px;
      background-color: #1a1a1a;
      color: #fff;
    }
    h1 {
      color: #4CAF50;
      margin-bottom: 10px;
    }
    .info {
      margin: 20px 0;
      color: #aaa;
    }
    #stream {
      max-width: 100%;
      height: auto;
      border: 3px solid #4CAF50;
      border-radius: 8px;
      box-shadow: 0 4px 8px rgba(0,0,0,0.5);
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    .status {
      display: inline-block;
      padding: 10px 20px;
      background-color: #4CAF50;
      color: white;
      border-radius: 5px;
      margin: 20px 0;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🎥 ESP32-CAM Live Stream</h1>
    <div class="status">● LIVE</div>
    <div class="info">
      <p>Streaming from ESP32-CAM</p>
    </div>
    <img id="stream" src="/stream" onerror="this.src='/stream';">
    <div class="info">
      <p>📱 Same network থেকে যেকোনো ডিভাইস দিয়ে দেখতে পারবেন</p>
    </div>
  </div>
</body>
</html>
)rawliteral";

// ===========================
// HTTP Handlers
// ===========================

// Root page handler
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "identity");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

// Stream handler
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
  static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
  static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) {
          Serial.println("JPEG compression failed");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      break;
    }
  }
  return res;
}

// ===========================
// Start Web Server
// ===========================
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  Serial.printf("Starting web server on port: '%d'\n", config.server_port);
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    Serial.println("Camera server started successfully!");
  } else {
    Serial.println("Failed to start camera server!");
  }
}

// ===========================
// Setup LED Flash
// ===========================
void setupLedFlash(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  Serial.printf("LED Flash setup on pin %d\n", pin);
}

// ===========================
// Setup Function
// ===========================
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("ESP32-CAM Local Network Live Stream Starting...");

  // Camera configuration
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
  
  // Optimized settings for live streaming
  config.frame_size = FRAMESIZE_VGA;  // 640x480 - good for live streaming
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;  // 10-15 is good quality
  config.fb_count = 2;
  
  // Check PSRAM
  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.frame_size = FRAMESIZE_SVGA; // 800x600 if PSRAM available
    Serial.println("PSRAM found - using optimized settings");
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
    Serial.println("No PSRAM - using standard settings");
  }

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }
  Serial.println("Camera initialized successfully");

  // Get camera sensor and apply settings
  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);      // Flip vertically
    s->set_brightness(s, 1);  // Brightness
    s->set_saturation(s, 0);  // Saturation
  }
  
  // Additional sensor adjustments for better live stream
  s->set_framesize(s, config.frame_size);
  s->set_quality(s, config.jpeg_quality);

  // Setup LED Flash if available
#ifdef LED_GPIO_NUM
  setupLedFlash(LED_GPIO_NUM);
#endif

  // Connect to WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("Camera Stream URL: http://");
  Serial.println(WiFi.localIP());
  
  // Start camera web server
  startCameraServer();
  
  Serial.println("\n=================================");
  Serial.println("Camera is ready!");
  Serial.print("Go to: http://");
  Serial.println(WiFi.localIP());
  Serial.println("=================================\n");
}

// ===========================
// Loop Function
// ===========================
void loop() {
  // Nothing needed here - the web server handles everything
  delay(10000);
}#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ===================
// Select camera model
// ===================
#define CAMERA_MODEL_AI_THINKER // Has PSRAM

// ===================
// Camera Pin Definitions
// ===================
#if defined(CAMERA_MODEL_AI_THINKER)
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
#define LED_GPIO_NUM       4  // Flash LED
#endif

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "Me";
const char* password = "mehedi113";

// ===========================
// Global Variables
// ===========================
httpd_handle_t camera_httpd = NULL;

// ===========================
// HTML Page for Live Stream
// ===========================
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32-CAM Live Stream</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin: 0;
      padding: 20px;
      background-color: #1a1a1a;
      color: #fff;
    }
    h1 {
      color: #4CAF50;
      margin-bottom: 10px;
    }
    .info {
      margin: 20px 0;
      color: #aaa;
    }
    #stream {
      max-width: 100%;
      height: auto;
      border: 3px solid #4CAF50;
      border-radius: 8px;
      box-shadow: 0 4px 8px rgba(0,0,0,0.5);
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    .status {
      display: inline-block;
      padding: 10px 20px;
      background-color: #4CAF50;
      color: white;
      border-radius: 5px;
      margin: 20px 0;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🎥 ESP32-CAM Live Stream</h1>
    <div class="status">● LIVE</div>
    <div class="info">
      <p>Streaming from ESP32-CAM</p>
    </div>
    <img id="stream" src="/stream" onerror="this.src='/stream';">
    <div class="info">
      <p>📱 Same network থেকে যেকোনো ডিভাইস দিয়ে দেখতে পারবেন</p>
    </div>
  </div>
</body>
</html>
)rawliteral";

// ===========================
// HTTP Handlers
// ===========================

// Root page handler
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "identity");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

// Stream handler
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
  static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
  static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) {
          Serial.println("JPEG compression failed");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      break;
    }
  }
  return res;
}

// ===========================
// Start Web Server
// ===========================
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  Serial.printf("Starting web server on port: '%d'\n", config.server_port);
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    Serial.println("Camera server started successfully!");
  } else {
    Serial.println("Failed to start camera server!");
  }
}

// ===========================
// Setup LED Flash
// ===========================
void setupLedFlash(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  Serial.printf("LED Flash setup on pin %d\n", pin);
}

// ===========================
// Setup Function
// ===========================
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("ESP32-CAM Local Network Live Stream Starting...");

  // Camera configuration
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
  
  // Optimized settings for live streaming
  config.frame_size = FRAMESIZE_VGA;  // 640x480 - good for live streaming
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;  // 10-15 is good quality
  config.fb_count = 2;
  
  // Check PSRAM
  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.frame_size = FRAMESIZE_SVGA; // 800x600 if PSRAM available
    Serial.println("PSRAM found - using optimized settings");
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
    Serial.println("No PSRAM - using standard settings");
  }

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }
  Serial.println("Camera initialized successfully");

  // Get camera sensor and apply settings
  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);      // Flip vertically
    s->set_brightness(s, 1);  // Brightness
    s->set_saturation(s, 0);  // Saturation
  }
  
  // Additional sensor adjustments for better live stream
  s->set_framesize(s, config.frame_size);
  s->set_quality(s, config.jpeg_quality);

  // Setup LED Flash if available
#ifdef LED_GPIO_NUM
  setupLedFlash(LED_GPIO_NUM);
#endif

  // Connect to WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("Camera Stream URL: http://");
  Serial.println(WiFi.localIP());
  
  // Start camera web server
  startCameraServer();
  
  Serial.println("\n=================================");
  Serial.println("Camera is ready!");
  Serial.print("Go to: http://");
  Serial.println(WiFi.localIP());
  Serial.println("=================================\n");
}

// ===========================
// Loop Function
// ===========================
void loop() {
  // Nothing needed here - the web server handles everything
  delay(10000);
}
