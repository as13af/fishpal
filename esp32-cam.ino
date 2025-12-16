/*
 * ESP32-CAM OV3660 - MEDIUM RESOLUTION
 * 320x240 or 640x480 based on PSRAM availability
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <FirebaseESP32.h>

// ============================
// WiFi Configuration
// ============================
const char* ssid = "bomas coffee";
const char* password = "belidulugaksih";

// ============================
// Firebase Realtime Database (using FirebaseESP32, same style as code-banu.ino)
// Host must include https:// and trailing slash for this library
#define FIREBASE_HOST "https://fishpal-57e60-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "AIzaSyCNu9zUQe3Envj8s5K1mk05Us01YVPKovI"

FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

const String CONTROL_PATH = "/smart_aquarium/control";

// ============================
// Camera Pins for ESP32-CAM
// ============================
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

WebServer server(80);
bool cameraActive = false;
String camResolution = "";  // CHANGED: resolution → camResolution

// ============================
// Camera Initialization with Better Resolution
// ============================
bool initCameraBetter() {
  Serial.println("\n=== Initializing Camera (Better Resolution) ===");
  
  // First, check PSRAM availability
  Serial.print("PSRAM Available: ");
  if(psramFound()) {
    Serial.println("YES (Good!)");
  } else {
    Serial.println("NO (Limited resolution)");
  }
  
  camera_config_t config;
  
  // ESP32-CAM fixed pinout
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
  
  // Balanced settings for better quality
  config.xclk_freq_hz = 15000000;  // 15MHz (good balance)
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // CHOOSE YOUR RESOLUTION HERE:
  // Option 1: If you have PSRAM (most ESP32-CAM do)
  if(psramFound()) {
    config.frame_size = FRAMESIZE_VGA;     // 640x480 (4x better than 320x240!)
    config.jpeg_quality = 15;              // Better quality (lower number = better)
    config.fb_count = 2;                   // Two buffers for smooth operation
    camResolution = "640x480 (VGA)";       // CHANGED
    Serial.println("Using: 640x480 (VGA) with PSRAM");
  } 
  // Option 2: If no PSRAM or conservative
  else {
    config.frame_size = FRAMESIZE_QVGA;    // 320x240 (2x better than 160x120)
    config.jpeg_quality = 20;              // Medium quality
    config.fb_count = 1;                   // One buffer
    camResolution = "320x240 (QVGA)";      // CHANGED
    Serial.println("Using: 320x240 (QVGA) - No PSRAM");
  }

  Serial.printf("Setting up camera at %s...\n", camResolution.c_str());  // CHANGED
  
  esp_err_t err = esp_camera_init(&config);
  
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    
    // Fallback to lower resolution if needed
    Serial.println("Trying fallback to 320x240...");
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 20;
    config.fb_count = 1;
    camResolution = "320x240 (Fallback)";  // CHANGED
    
    err = esp_camera_init(&config);
    
    if (err != ESP_OK) {
      Serial.println("Fallback also failed. Trying 160x120...");
      config.frame_size = FRAMESIZE_QQVGA;
      config.jpeg_quality = 30;
      camResolution = "160x120 (Minimal)";  // CHANGED
      
      err = esp_camera_init(&config);
      
      if (err != ESP_OK) {
        return false;
      }
    }
  }
  
  Serial.printf("✅ Camera initialized at %s\n", camResolution.c_str());  // CHANGED
  
  // Configure sensor with balanced settings
  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_framesize(s, config.frame_size);
    s->set_quality(s, config.jpeg_quality);
    
    // Enable SOME features for better quality
    s->set_brightness(s, 0);     // Neutral
    s->set_contrast(s, 0);       // Neutral
    s->set_saturation(s, 0);     // Neutral
    s->set_special_effect(s, 0); // No effect
    s->set_whitebal(s, 1);       // AUTO white balance (better colors!)
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 0);        // Auto mode
    s->set_exposure_ctrl(s, 1);  // AUTO exposure (better lighting!)
    s->set_aec2(s, 0);           // Keep AEC2 off for stability
    s->set_ae_level(s, 0);
    s->set_aec_value(s, 1200);   // Medium exposure
    s->set_gain_ctrl(s, 1);      // AUTO gain
    s->set_agc_gain(s, 1);       // Medium gain
    s->set_gainceiling(s, GAINCEILING_4X);
    s->set_bpc(s, 0);            // Disable black pixel correction
    s->set_wpc(s, 0);            // Disable white pixel correction
    s->set_raw_gma(s, 1);
    s->set_lenc(s, 1);           // ENABLE lens correction (better image!)
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
    s->set_dcw(s, 1);            // Enable DCW for downscaling
    s->set_colorbar(s, 0);
    
    Serial.println("Camera configured for better quality");
  }
  
  return true;
}

// ============================
// Safe Capture Function
// ============================
bool safeCapture() {
  // Clear buffer
  esp_camera_fb_return(esp_camera_fb_get());
  delay(50);  // Reduced delay for faster capture
  
  // Capture
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Capture failed");
    return false;
  }
  
  Serial.printf("✅ Capture: %d bytes at %dx%d\n", fb->len, fb->width, fb->height);
  esp_camera_fb_return(fb);
  return true;
}

// ============================
// Web Server Handlers
// ============================
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32-CAM Test</title>";
  html += "<style>";
  html += "body {font-family: Arial; text-align: center; margin: 40px; background: #f0f0f0;}";
  html += ".container {max-width: 600px; margin: 0 auto; background: white; padding: 25px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1);}";
  html += "h1 {color: #2c3e50; margin-bottom: 10px;}";
  html += ".status {background: #e8f4fd; padding: 15px; border-radius: 8px; margin: 20px 0; border-left: 5px solid #2196F3;}";
  html += ".btn {display: inline-block; padding: 14px 28px; margin: 12px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; text-decoration: none; border-radius: 8px; font-size: 17px; border: none; cursor: pointer; transition: transform 0.2s, box-shadow 0.2s;}";
  html += ".btn:hover {transform: translateY(-2px); box-shadow: 0 6px 12px rgba(0,0,0,0.15);}";
  html += ".btn-capture {background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);}";
  html += ".btn-stream {background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);}";
  html += ".resolution {background: #e8f7ef; color: #27ae60; padding: 8px 15px; border-radius: 20px; display: inline-block; margin: 10px; font-weight: bold;}";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>🎥 ESP32-CAM OV3660</h1>";
  html += "<div class='status'>";
  html += "<strong>📡 IP Address:</strong> " + WiFi.localIP().toString() + "<br>";
  html += "<strong>📊 Resolution:</strong> <span class='resolution'>" + camResolution + "</span><br>";  // CHANGED
  html += "<strong>📸 Status:</strong> " + String(cameraActive ? "✅ Active" : "❌ Inactive");
  html += "</div>";
  
  if (cameraActive) {
    html += "<div style='margin: 25px 0;'>";
    html += "<button class='btn btn-capture' onclick=\"captureImage()\">📸 Capture Photo</button><br>";
    html += "<button class='btn btn-stream' onclick=\"window.open('/stream')\">📹 Live Stream</button>";
    html += "</div>";
    
    html += "<div style='background: #f9f9f9; padding: 15px; border-radius: 8px; margin: 20px 0;'>";
    html += "<p><strong>💾 Save images to:</strong></p>";
    html += "<p style='font-family: monospace; background: #2c3e50; color: white; padding: 10px; border-radius: 5px;'>";
    html += "C:\\Users\\PC\\Documents\\CameraTestESP32\\";
    html += "</p>";
    html += "<p><small>Photos download automatically when you click 'Capture Photo'</small></p>";
    html += "</div>";
    
    html += "<div style='margin-top: 20px;'>";
    html += "<p><a href='/test' style='color: #3498db; text-decoration: none;'>🔧 Test Camera</a> | ";
    html += "<a href='/stats' style='color: #3498db; text-decoration: none;'>📈 System Stats</a></p>";
    html += "</div>";
  } else {
    html += "<div style='background: #ffeaa7; padding: 20px; border-radius: 8px; margin: 20px 0;'>";
    html += "<p style='color: #d63031;'><strong>⚠️ Camera Not Available</strong></p>";
    html += "<p>Check Serial Monitor for error messages</p>";
    html += "</div>";
  }
  html += "</div>";
  
  html += "<script>";
  html += "function captureImage() {";
  html += "  var link = document.createElement('a');";
  html += "  var timestamp = new Date().toISOString().replace(/[:.]/g, '-');";
  html += "  link.href = '/capture';";
  html += "  link.download = 'ESP32-CAM_' + timestamp + '.jpg';";
  html += "  document.body.appendChild(link);";
  html += "  link.click();";
  html += "  document.body.removeChild(link);";
  
  html += "  // Show success message";
  html += "  var msg = document.createElement('div');";
  html += "  msg.innerHTML = '<div style=\"margin-top:15px; padding:10px; background:#d4edda; color:#155724; border-radius:5px;\">";
  html += "  ✅ Photo downloading! Check your Downloads folder.</div>';";
  html += "  document.querySelector('.container').appendChild(msg);";
  html += "  setTimeout(function(){ msg.remove(); }, 4000);";
  html += "}";
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleCapture() {
  if (!cameraActive) {
    server.send(503, "text/plain", "Camera not ready");
    return;
  }
  
  Serial.println("📸 Web capture requested...");
  
  delay(30);  // Small delay for stability
  
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Capture failed");
    return;
  }
  
  // Generate filename with timestamp
  char filename[80];
  snprintf(filename, sizeof(filename), "ESP32-CAM_%lux%lu_%lu.jpg", 
           fb->width, fb->height, millis());
  
  // Send as downloadable file
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Disposition", String("attachment; filename=\"") + filename + "\"");
  server.sendHeader("Content-Length", String(fb->len));
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  
  Serial.printf("✅ Image sent: %d bytes (%dx%d)\n", fb->len, fb->width, fb->height);
  esp_camera_fb_return(fb);
}

void handleStream() {
  Serial.println("📹 Starting video stream...");
  
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);
  
  unsigned long lastFrame = 0;
  const int frameDelay = 150;  // ~6-7 FPS (stable)
  
  while(client.connected()) {
    unsigned long now = millis();
    if(now - lastFrame >= frameDelay) {
      camera_fb_t *fb = esp_camera_fb_get();
      if(!fb) {
        Serial.println("Frame capture failed");
        break;
      }
      
      client.print("--frame\r\n");
      client.print("Content-Type: image/jpeg\r\n\r\n");
      client.write(fb->buf, fb->len);
      client.print("\r\n");
      
      esp_camera_fb_return(fb);
      lastFrame = now;
    }
    delay(1);
  }
  Serial.println("Stream ended");
}

void handleTest() {
  if (safeCapture()) {
    String result = "✅ Camera Test PASSED!\n\n";
    result += "Resolution: " + camResolution + "\n";  // CHANGED
    result += "IP Address: " + WiFi.localIP().toString() + "\n";
    result += "\nGo to /capture for photos";
    server.send(200, "text/plain", result);
  } else {
    server.send(500, "text/plain", "❌ Camera test failed");
  }
}

void handleStats() {
  String stats = "📊 ESP32-CAM System Stats\n";
  stats += "============================\n";
  stats += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
  stats += "Chip Model: " + String(ESP.getChipModel()) + "\n";
  stats += "CPU Freq: " + String(ESP.getCpuFreqMHz()) + " MHz\n";
  stats += "PSRAM: " + String(psramFound() ? "Available" : "Not Available") + "\n";
  stats += "WiFi RSSI: " + String(WiFi.RSSI()) + " dBm\n";
  stats += "IP: " + WiFi.localIP().toString() + "\n";
  stats += "Resolution: " + camResolution + "\n";  // CHANGED
  stats += "Camera: " + String(cameraActive ? "Active" : "Inactive") + "\n";
  
  server.send(200, "text/plain", stats);
}

void publishStreamUrl() {
  String ip = WiFi.localIP().toString();
  if (ip.length() == 0 || ip == "0.0.0.0") {
    Serial.println("[Firebase] IP not ready, skip publish");
    return;
  }

  String streamUrl = String("http://") + ip + "/stream";
  Serial.printf("[Firebase] Publishing stream URL: %s\n", streamUrl.c_str());
  bool ok = Firebase.setString(firebaseData, CONTROL_PATH + "/esp32_cam_url", streamUrl);
  if (!ok) {
    Serial.printf("[Firebase] Publish failed: %s\n", firebaseData.errorReason().c_str());
  }
}

// ============================
// Setup & Loop
// ============================
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n══════════════════════════════════════════════");
  Serial.println("   ESP32-CAM OV3660 - MEDIUM RESOLUTION");
  Serial.println("   Better image quality (320x240 or 640x480)");
  Serial.println("══════════════════════════════════════════════\n");
  
  // Connect WiFi
  Serial.println("[1] Connecting to WiFi...");
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);  // Keep WiFi active
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 25) {
    delay(500);
    Serial.print(".");
    digitalWrite(33, !digitalRead(33));  // Blink LED
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✅ WiFi: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("⚠️ WiFi failed - starting AP mode");
    WiFi.softAP("ESP32-CAM", "12345678");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }
  
  // Initialize camera with better resolution
  Serial.println("\n[2] Initializing camera with better resolution...");
  cameraActive = initCameraBetter();
  
  // Init Firebase (uses same host/auth style as code-banu.ino)
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (cameraActive) {
    Serial.println("✅ Camera ready!");
    
    // Test capture
    if (safeCapture()) {
      Serial.println("✅ Capture test passed!");
    }
    
    // Start web server
    server.on("/", handleRoot);
    server.on("/capture", handleCapture);
    server.on("/stream", handleStream);
    server.on("/test", handleTest);
    server.on("/stats", handleStats);
    server.onNotFound([](){
      server.send(404, "text/plain", "404: Not Found");
    });
    
    server.begin();
    Serial.println("\n[3] Web server started!");
    Serial.print("🌐 Open browser: http://");
    Serial.println(WiFi.localIP());
    Serial.println("   or use direct link: http://" + WiFi.localIP().toString() + "/capture");

    // Publish stream URL to Firebase for the Flutter app
    publishStreamUrl();
    
    // Success message
    Serial.println("\n🎉 SUCCESS! Camera running at good resolution!");
    Serial.println("📸 Take photos and save to: C:\\Users\\PC\\Documents\\CameraTestESP32\\");
  } else {
    Serial.println("❌ Camera initialization failed");
    Serial.println("Try: 1. Restart ESP32  2. Check power supply");
  }
  
  Serial.println("\n══════════════════════════════════════════════");
}

void loop() {
  server.handleClient();
  
  // Slow LED blink to show it's alive
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 2000) {
    digitalWrite(33, !digitalRead(33));
    lastBlink = millis();
  }
  
  delay(10);
}