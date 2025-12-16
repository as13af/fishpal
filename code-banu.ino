#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FirebaseESP32.h>

// =====================================================
// Konfigurasi WiFi
// =====================================================
const char* ssid     = "saluran bansos";
const char* password = "gajiguruhonorer";

// =====================================================
// Konfigurasi Firebase
// =====================================================
#define FIREBASE_HOST "https://fishpal-57e60-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "AIzaSyCNu9zUQe3Envj8s5K1mk05Us01YVPKovI"

FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// =====================================================
// Pin Konfigurasi
// =====================================================
const int PIN_TURBIDITY = 33;
const int PIN_PH        = 34;
const int PIN_SERVO     = 32;
#define ONE_WIRE_BUS     17

// Ultrasonic
const int PIN_TRIG = 26;
const int PIN_ECHO = 25;

// Relay Pompa  (aktif-LOW)
const int PIN_PUMP = 16;    
int pumpState = 0;

// Servo
Servo myServo;
int servoState = 0;
int servoAngle = 0;

// Status teks
String feedingStatus  = "Tidak memberi makan";
String nutrientStatus = "Tidak memberi nutrisi";

// DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// WebServer
WebServer server(80);

// Path Firebase
const String CONTROL_PATH   = "/smart_aquarium/control";
const String DATA_BASEPATH  = "/smart_aquarium/data/";

// Timer
unsigned long lastUpdateMillis     = 0;
const unsigned long updateInterval = 5000;

unsigned long lastFirebaseCheck    = 0;
const unsigned long firebaseCheckInterval = 1000;

// =====================================================
// Fungsi Baca Sensor
// =====================================================
float readVoltage(int pin) {
  int raw = analogRead(pin);
  return (raw / 4095.0) * 3.3;
}

float readPH() {
  return -3.0 * readVoltage(PIN_PH) + 14.0; // kalibrasi sesuai sensor
}

float readTurbidity() {
  return 300.0 * readVoltage(PIN_TURBIDITY); // kalibrasi sesuai sensor
}

float readTemperature() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

float readWaterLevelCM(float maxRangeCM = 15) {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long durasi = pulseIn(PIN_ECHO, HIGH, 30000);
  float jarak = durasi * 0.0343 / 2.0;

  if (jarak < 0) jarak = 0;
  if (jarak > maxRangeCM) jarak = maxRangeCM;
  return jarak;
}

// =====================================================
// Fungsi Aktuator
// =====================================================
void setServoState(int state) {
  servoState = state ? 1 : 0;
  servoAngle = servoState ? 90 : 0;
  myServo.write(servoAngle);
  feedingStatus = servoState ? "Sedang memberi makan" : "Tidak memberi makan";
  Serial.printf("Servo set: state=%d angle=%d\n", servoState, servoAngle);
}

// *** Perbaikan utama: relay aktif-LOW ***
void setPumpState(int state) {
  pumpState = state ? 1 : 0;
  // ON = LOW, OFF = HIGH
  digitalWrite(PIN_PUMP, pumpState ? LOW : HIGH);
  nutrientStatus = pumpState ? "Sedang memberi nutrisi" : "Tidak memberi nutrisi";
  Serial.printf("Pump set: state=%d\n", pumpState);
}

// =====================================================
// Web Server Handler (opsional)
// =====================================================
void handleRoot() {
  server.send(200, "text/html",
    "<h3>ESP32 IoT Panel</h3>"
    "<p><a href=\"/read\">/read</a></p>"
    "<p>/servo?state=1 atau 0</p>"
    "<p>/pump?state=1 atau 0</p>");
}

void handleRead() {
  float phVal   = readPH();
  float turbVal = readTurbidity();
  float tempVal = readTemperature();
  float levelVal = readWaterLevelCM(15);

  String json = "{";
  json += "\"ph\":" + String(phVal, 2) + ",";
  json += "\"turbidity\":" + String(turbVal, 2) + ",";
  json += "\"temperature\":" + String(tempVal, 2) + ",";
  json += "\"water_level\":" + String(levelVal, 1) + ",";
  json += "\"servo_state\":" + String(servoState) + ",";
  json += "\"servo_angle\":" + String(servoAngle) + ",";
  json += "\"pump_state\":" + String(pumpState) + ",";
  json += "\"feeding_status\":\"" + feedingStatus + "\",";
  json += "\"nutrient_status\":\"" + nutrientStatus + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleServo() {
  if (server.hasArg("state")) {
    setServoState(server.arg("state").toInt());
    server.send(200, "text/plain", "Servo state=" + String(servoState));
  } else {
    server.send(400, "text/plain", "Missing parameter: state");
  }
}

void handlePump() {
  if (server.hasArg("state")) {
    setPumpState(server.arg("state").toInt());
    server.send(200, "text/plain", "Pump state=" + String(pumpState));
  } else {
    server.send(400, "text/plain", "Missing parameter: state");
  }
}

// =====================================================
// Firebase Update & Control
// =====================================================
void updateFirebase(float phVal, float turbVal, float tempVal, float levelVal) {
  String basePath = DATA_BASEPATH;

  Firebase.setFloat(firebaseData, basePath + "ph", phVal);
  Firebase.setFloat(firebaseData, basePath + "turbidity", turbVal);
  Firebase.setFloat(firebaseData, basePath + "temperature", tempVal);
  Firebase.setFloat(firebaseData, basePath + "water_level", levelVal);

  Firebase.setInt(firebaseData, basePath + "servo_state", servoState);
  Firebase.setInt(firebaseData, basePath + "servo_angle", servoAngle);
  Firebase.setInt(firebaseData, basePath + "pump_state", pumpState);
  Firebase.setString(firebaseData, basePath + "feeding_status", feedingStatus);
  Firebase.setString(firebaseData, basePath + "nutrient_status", nutrientStatus);
  Firebase.setString(firebaseData, basePath + "wifi_status",
                     WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");

  Serial.println("=== Data terkirim ke Firebase ===");
}

void checkFirebaseCommand() {
  // Pump
  if (Firebase.getInt(firebaseData, CONTROL_PATH + "/pump_state")) {
    if (firebaseData.dataType() == "int") {
      int val = firebaseData.intData();
      if (val != pumpState) setPumpState(val);
    }
  }
  // Servo
  if (Firebase.getInt(firebaseData, CONTROL_PATH + "/servo_state")) {
    if (firebaseData.dataType() == "int") {
      int val = firebaseData.intData();
      if (val != servoState) setServoState(val);
    }
  }
}

// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(115200);

  myServo.attach(PIN_SERVO, 500, 2500);
  setServoState(0);

  sensors.begin();
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_PUMP, OUTPUT);

  // pastikan relay OFF saat boot (aktif-LOW)
  digitalWrite(PIN_PUMP, HIGH);  
  setPumpState(0);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected, IP: " + WiFi.localIP().toString());

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Pastikan node kontrol ada
  Firebase.setInt(firebaseData, CONTROL_PATH + "/pump_state", 0);
  Firebase.setInt(firebaseData, CONTROL_PATH + "/servo_state", 0);

  // Webserver
  server.on("/", handleRoot);
  server.on("/read", handleRead);
  server.on("/servo", handleServo);
  server.on("/pump", handlePump);
  server.begin();

  lastUpdateMillis  = millis();
  lastFirebaseCheck = millis();
}

// =====================================================
// Loop
// =====================================================
void loop() {
  server.handleClient();

  // Kontrol manual via Serial (opsional)
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '1') setServoState(1);
    else if (cmd == '0') setServoState(0);
    else if (cmd == 'a') setPumpState(1);
    else if (cmd == 'b') setPumpState(0);
  }

  unsigned long now = millis();

  if (now - lastFirebaseCheck >= firebaseCheckInterval) {
    lastFirebaseCheck = now;
    checkFirebaseCommand();
  }

  if (now - lastUpdateMillis >= updateInterval) {
    lastUpdateMillis = now;
    float phVal   = readPH();
    float turbVal = readTurbidity();
    float tempVal = readTemperature();
    float levelVal = readWaterLevelCM(15);

    Serial.printf("[Loop] pH: %.2f | Turb: %.2f | Temp: %.2f | Level: %.1f cm | Pump: %d\n",
                  phVal, turbVal, tempVal, levelVal, pumpState);

    updateFirebase(phVal, turbVal, tempVal, levelVal);
  }
}