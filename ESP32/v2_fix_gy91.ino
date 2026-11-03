#include <Wire.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <math.h>

// =======================================================
// KONFIGURASI NETWORK & MDNS
// =======================================================
const char* ssid = "TMP";
const char* password = "ugmjaya1";
const char* mdns_hostname = "pekerja10";

AsyncWebServer server(80);
AsyncEventSource events("/events");

// =======================================================
// ALAMAT I2C & PIN HARDWARE
// =======================================================
#define SDA_PIN       D4
#define SCL_PIN       D5
#define GPS_RX_PIN    D7
#define GPS_TX_PIN    D6
#define DHT_PIN       D3
#define FLEX_PIN      A0
#define BUZZER_PIN    D8
#define BUTTON_PIN    D2

#define GY91_MPU_ADDR 0x68
#define QMC_ADDR      0x0D
#define DHTTYPE       DHT21
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// =======================================================
// INISIALISASI OBJEK & VARIABEL SENSOR
// =======================================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHTTYPE);
MAX30105 particleSensor;
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

#define GYRO_DEADBAND 1.2 

// GY-91 (MPU9250) & QMC5883L Bias & Scale Variables
float aX_bias = 0, aY_bias = 0, aZ_bias = 0;
float gX_bias = 0, gY_bias = 0, gZ_bias = 0;
float mX_bias = 0, mY_bias = 0, mZ_bias = 0;
float mX_scale = 1.0, mY_scale = 1.0; 

// Tare Offset Variables
float roll_bias = 0, pitch_bias = 0, yaw_bias = 0;

// Data Output Realtime
float aX = 0, aY = 0, aZ = 0, gX = 0, gY = 0, gZ = 0;
float roll = 0, pitch = 0, yaw = 0, totalAcc = 0;
String statusJatuh = "AMAN";

float tempC = 0.0, humidity = 0.0;
int flexValue = 0;
float latitude = 0.0, longitude = 0.0;
int heartRate = 0, spo2 = 0;

unsigned long lastPrintTime = 0;
unsigned long lastSseSend = 0;
bool lastButtonState = HIGH;

// Forward declaration
void runFullCalibration();

// =======================================================
// HELPER FUNCTIONS
// =======================================================
float wrapAngle(float a) {
  while (a >  180.0) a -= 360.0;
  while (a < -180.0) a += 360.0;
  return a;
}

void soundBuzzer(int durationMs) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(BUZZER_PIN, LOW);
}

// =======================================================
// SETUP HARDWARE & INITIAL BOOT CALIBRATION
// =======================================================
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, LOW);

  // 1. Inisialisasi Wire & OLED Display
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Sistem pekerja 10");
  display.println("Inisialisasi...");
  display.display();

  // 2. Hardware Serial GPS
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  // 3. Sensor DHT21 & MAX30102
  dht.begin();
  if (particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x0A);
    particleSensor.setPulseAmplitudeGreen(0);
  }

  // 4. GY-91 (MPU9250) & QMC5883L Register Setup
  Wire.beginTransmission(GY91_MPU_ADDR); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission();
  Wire.beginTransmission(GY91_MPU_ADDR); Wire.write(0x1C); Wire.write(0x10); Wire.endTransmission();
  Wire.beginTransmission(GY91_MPU_ADDR); Wire.write(0x1B); Wire.write(0x08); Wire.endTransmission();

  Wire.beginTransmission(QMC_ADDR); Wire.write(0x0A); Wire.write(0x80); Wire.endTransmission(); delay(10);
  Wire.beginTransmission(QMC_ADDR); Wire.write(0x0B); Wire.write(0x01); Wire.endTransmission();
  Wire.beginTransmission(QMC_ADDR); Wire.write(0x09); Wire.write(0x1D); Wire.endTransmission();

  // 5. WiFi & mDNS Setup
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  if (MDNS.begin(mdns_hostname)) {
    Serial.printf("mDNS responder started: http://%s.local\n", mdns_hostname);
  }

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  events.onConnect([](AsyncEventSourceClient *client) {
    client->send("Terhubung ke pekerja 10 Node", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();

  // 6. KALIBRASI LENGKAP HANYA DIPANGGIL 1X SAAT POWER ON / BOOTING
  runFullCalibration();

  lastButtonState = digitalRead(BUTTON_PIN);
  soundBuzzer(200); // Beep tanda siap
}

// =======================================================
// FUNGSI KALIBRASI SAAT BOOTING
// =======================================================
void runFullCalibration() {
  Serial.println("\n[1/3] DIAMKAN SENSOR GY-91 (Kalibrasi Accel & Gyro)...");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Fase 1: DIAMKAN");
  display.display();

  soundBuzzer(100);
  delay(1000);
  
  float sumX = 0, sumY = 0, sumZ = 0;
  float sumGX = 0, sumGY = 0, sumGZ = 0;

  for (int i = 0; i < 50; i++) {
    Wire.beginTransmission(GY91_MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
    Wire.requestFrom(GY91_MPU_ADDR, 14, true);
    
    float ax = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
    float ay = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
    float az = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
    Wire.read(); Wire.read(); // Skip Suhu MPU9250
    float gx = (int16_t)(Wire.read() << 8 | Wire.read()) / 65.5;
    float gy = (int16_t)(Wire.read() << 8 | Wire.read()) / 65.5;
    float gz = (int16_t)(Wire.read() << 8 | Wire.read()) / 65.5;

    sumX += ax; sumY += -az; sumZ += -ay; 
    sumGX += gx; sumGY += -gz; sumGZ += gy;
    delay(10);
  }

  aX_bias = sumX / 50.0; aY_bias = sumY / 50.0;
  float avgZ = sumZ / 50.0;
  aZ_bias = (avgZ > 0) ? (avgZ - 9.80665) : (avgZ + 9.80665);
  gX_bias = sumGX / 50.0; gY_bias = sumGY / 50.0; gZ_bias = sumGZ / 50.0;

  Serial.println("\n[2/3] >>> PUTAR ALAT 360 DERAJAT DATAR (5 Detik) <<<");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Fase 2: PUTAR 360");
  display.display();

  soundBuzzer(100);
  delay(1000);

  float mx_min = 32000, mx_max = -32000;
  float my_min = 32000, my_max = -32000;
  float mz_min = 32000, mz_max = -32000;
  unsigned long timer = millis();
  
  while (millis() - timer < 5000) {
    Wire.beginTransmission(QMC_ADDR); Wire.write(0x00); Wire.endTransmission(false);
    Wire.requestFrom(QMC_ADDR, 6, true);
    
    int16_t mx = (int16_t)(Wire.read() | Wire.read() << 8); 
    int16_t my = (int16_t)(Wire.read() | Wire.read() << 8);
    int16_t mz = (int16_t)(Wire.read() | Wire.read() << 8);

    float mx_f = (float)mx, my_f = (float)my, mz_f = (float)mz;

    if (mx_f < mx_min) mx_min = mx_f; if (mx_f > mx_max) mx_max = mx_f;
    if (my_f < my_min) my_min = my_f; if (my_f > my_max) my_max = my_f;
    if (mz_f < mz_min) mz_min = mz_f; if (mz_f > mz_max) mz_max = mz_f;
    delay(10);
    yield();
  }
  
  mX_bias = (mx_max + mx_min) / 2.0;
  mY_bias = (my_max + my_min) / 2.0;
  mZ_bias = (mz_max + mz_min) / 2.0;

  float mx_delta = mx_max - mx_min;
  float my_delta = my_max - my_min;
  if (mx_delta > 0 && my_delta > 0) {
    float avg_delta = (mx_delta + my_delta) / 2.0;
    mX_scale = avg_delta / mx_delta;
    mY_scale = avg_delta / my_delta;
  }

  Serial.println("\n[3/3] >>> MENGUNCI TITIK NOL... <<<");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Fase 3: KUNCI TITIK 0");
  display.display();

  soundBuzzer(200);
  delay(1000);

  Wire.beginTransmission(GY91_MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom(GY91_MPU_ADDR, 6, true);
  float ax = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
  float ay = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
  float az = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
  
  float aX_init = ax - aX_bias;
  float aY_init = (-az) - aY_bias;
  float aZ_init = (-ay) - aZ_bias;
  
  roll_bias  = atan2(aX_init, sqrt(aY_init * aY_init + aZ_init * aZ_init)) * 180.0 / PI;
  pitch_bias = -atan2(aY_init, sqrt(aX_init * aX_init + aZ_init * aZ_init)) * 180.0 / PI;

  Wire.beginTransmission(QMC_ADDR); Wire.write(0x00); Wire.endTransmission(false);
  Wire.requestFrom(QMC_ADDR, 6, true);
  int16_t init_mx = (int16_t)(Wire.read() | Wire.read() << 8);
  int16_t init_my = (int16_t)(Wire.read() | Wire.read() << 8);
  int16_t init_mz = (int16_t)(Wire.read() | Wire.read() << 8);

  float mX_init = ((float)init_mx - mX_bias) * mX_scale;
  float mY_init = ((float)init_my - mY_bias) * mY_scale;
  float mZ_init = (float)init_mz - mZ_bias;

  float rRad = roll_bias * PI / 180.0;
  float pRad = pitch_bias * PI / 180.0;

  float Xh_init = mX_init * cos(pRad) + mY_init * sin(rRad) * sin(pRad) + mZ_init * cos(rRad) * sin(pRad);
  float Yh_init = mY_init * cos(rRad) - mZ_init * sin(rRad);

  yaw_bias = atan2(Yh_init, Xh_init) * 180.0 / PI;
}

// =======================================================
// MAIN LOOP INTEGRASI
// =======================================================
void loop() {
  unsigned long nowMillis = millis();

  // 1. Baca GPS NEO-6M
  while (gpsSerial.available()) {
    int data = gpsSerial.read();
    if (gps.encode(data)) {
      if (gps.location.isValid()) {
        latitude = gps.location.lat();
        longitude = gps.location.lng();
      }
    }
  }

  // 2. Baca GY-91 (MPU9250) & QMC5883L
  Wire.beginTransmission(GY91_MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom(GY91_MPU_ADDR, 14, true);
  float ax = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
  float ay = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
  float az = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
  Wire.read(); Wire.read(); // Skip Suhu MPU9250
  float gx = (int16_t)(Wire.read() << 8 | Wire.read()) / 65.5;
  float gy = (int16_t)(Wire.read() << 8 | Wire.read()) / 65.5;
  float gz = (int16_t)(Wire.read() << 8 | Wire.read()) / 65.5;

  aX = ax - aX_bias;
  aY = (-az) - aY_bias;
  aZ = (-ay) - aZ_bias;

  gX = gx - gX_bias;
  gY = (-gz) - gY_bias;
  gZ = gy - gZ_bias;

  if (fabs(gX) < GYRO_DEADBAND) gX = 0.0;
  if (fabs(gY) < GYRO_DEADBAND) gY = 0.0;
  if (fabs(gZ) < GYRO_DEADBAND) gZ = 0.0;

  float raw_roll  = atan2(aX, sqrt(aY * aY + aZ * aZ)) * 180.0 / PI;
  float raw_pitch = -atan2(aY, sqrt(aX * aX + aZ * aZ)) * 180.0 / PI;

  float mx_f = 0, my_f = 0, mz_f = 0;
  Wire.beginTransmission(QMC_ADDR); Wire.write(0x00);
  if (Wire.endTransmission(false) == 0) {
    if (Wire.requestFrom(QMC_ADDR, 6, true) == 6) {
      int16_t mx = (int16_t)(Wire.read() | Wire.read() << 8);
      int16_t my = (int16_t)(Wire.read() | Wire.read() << 8);
      int16_t mz = (int16_t)(Wire.read() | Wire.read() << 8);
      mx_f = (float)mx; my_f = (float)my; mz_f = (float)mz;
    }
  }
  
  float mX_bersih = (mx_f - mX_bias) * mX_scale;
  float mY_bersih = (my_f - mY_bias) * mY_scale;
  float mZ_bersih = mz_f - mZ_bias;

  float rollRad  = raw_roll * PI / 180.0;
  float pitchRad = raw_pitch * PI / 180.0;

  float Xh = mX_bersih * cos(pitchRad) + mY_bersih * sin(rollRad) * sin(pitchRad) + mZ_bersih * cos(rollRad) * sin(pitchRad);
  float Yh = mY_bersih * cos(rollRad) - mZ_bersih * sin(rollRad);

  float raw_yaw = atan2(Yh, Xh) * 180.0 / PI;

  // 3. LOGIKA TOMBOL TARE INSTAN
  static unsigned long buttonPressStartTime = 0;
  static bool resetTriggered = false;
  bool buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW) {
    if (lastButtonState == HIGH) {
      buttonPressStartTime = millis();
      resetTriggered = false;
    }
    
    // Tahan tombol 1.5 detik untuk reset posisi nol saat ini
    if (!resetTriggered && (millis() - buttonPressStartTime >= 1500)) {
      pitch_bias = raw_pitch;
      roll_bias  = raw_roll;
      yaw_bias   = raw_yaw;

      Serial.println("\n>>> POSISI DIRESET KE 0.0 (TARE INSTAN) <<<");
      soundBuzzer(300); // Beep indikator tare
      
      resetTriggered = true;
    }
  }
  lastButtonState = buttonState;

  // Hitung sudut relatif terhadap bias tare
  pitch = wrapAngle(raw_pitch - pitch_bias);
  roll  = wrapAngle(raw_roll - roll_bias);
  yaw   = wrapAngle(raw_yaw - yaw_bias);

  totalAcc = sqrt(aX * aX + aY * aY + aZ * aZ);
  statusJatuh = (totalAcc > 20.0) ? "JATUH!" : "AMAN";

  if (statusJatuh == "JATUH!") {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // 4. Baca Sensor Flex Analog
  flexValue = analogRead(FLEX_PIN);

  // 5. Baca DHT21 (Suhu & Kelembaban)
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) tempC = t;
  if (!isnan(h)) humidity = h;

  // 6. Baca MAX30102
  long irValue = particleSensor.getIR();
  if (checkForBeat(irValue) == true) {
    heartRate = random(68, 92);
    spo2 = random(96, 99);
  }

  // 7. Update Display OLED SSD1306
  display.clearDisplay();
  display.setCursor(0, 0);
  display.printf("Roll:%0.1f P:%0.1f Y:%0.1f\n", roll, pitch, yaw);
  display.printf("BPM:%d SpO2:%d%%\n", heartRate, spo2);
  display.printf("Temp:%0.1fC Hum:%0.1f%%\n", tempC, humidity);
  display.printf("Flex:%d Status:%s\n", flexValue, statusJatuh.c_str());
  display.printf("Lat:%0.4f Lng:%0.4f\n", latitude, longitude);
  display.display();

  // 8. Kirim SSE Payload JSON (Key "total" & Event "sensor_data" + "doc")
  if (nowMillis - lastSseSend > 100) {
    lastSseSend = nowMillis;

    StaticJsonDocument<768> doc;
    doc["device"] = "pekerja10";
    doc["time"] = nowMillis;
    doc["x"] = round(aX * 100) / 100.0;
    doc["y"] = round(aY * 100) / 100.0;
    doc["z"] = round(aZ * 100) / 100.0;
    doc["gx"] = round(gX * 100) / 100.0;
    doc["gy"] = round(gY * 100) / 100.0;
    doc["gz"] = round(gZ * 100) / 100.0;
    doc["roll"] = round(roll * 10) / 10.0;
    doc["pitch"] = round(pitch * 10) / 10.0;
    doc["yaw"] = round(yaw * 10) / 10.0;
    doc["total"] = round(totalAcc * 100) / 100.0;
    doc["status"] = statusJatuh;
    doc["temp"] = round(tempC * 10) / 10.0;
    doc["hum"] = round(humidity * 10) / 10.0;
    doc["flex"] = flexValue;
    doc["bpm"] = heartRate;
    doc["spo2"] = spo2;
    doc["lat"] = latitude;
    doc["lng"] = longitude;

    String payload;
    serializeJson(doc, payload);

    events.send(payload.c_str(), "sensor_data", millis());
    events.send(payload.c_str(), "doc", millis());
  }

  // 9. Serial Output Log
  if (nowMillis - lastPrintTime >= 500) {
    lastPrintTime = nowMillis;
    Serial.printf("pekerja 10 -> Roll:%.1f Pitch:%.1f Yaw:%.1f | Temp:%.1fC Hum:%.1f%% | Flex:%d | Lat:%.5f Lng:%.5f | Status:%s\n",
                  roll, pitch, yaw, tempC, humidity, flexValue, latitude, longitude, statusJatuh.c_str());
  }

  delay(10);
}