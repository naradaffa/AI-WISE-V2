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
// KONFIGURASI NETWORK, MDNS & SSE
// =======================================================
const char* ssid = "TMP";
const char* password = "ugmjaya1";
const char* mdns_hostname = "pekerja1";

AsyncWebServer server(80);
AsyncEventSource events("/events"); // Endpoint SSE: http://pekerja1.local/events

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
#define BAT_ADC_PIN   D9
#define CHRG_PIN      D10

#define MPU_ADDR      0x68
#define QMC_ADDR      0x0D
#define DHTTYPE       DHT21
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

#define GYRO_DEADBAND 1.2

// =======================================================
// OBJEK & VARIABEL GLOBAL
// =======================================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1, 400000, 400000);
DHT dht(DHT_PIN, DHTTYPE);
MAX30105 particleSensor;
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

// Bias Kalibrasi Accel & Gyro
float aX_bias = 0, aY_bias = 0, aZ_bias = 0;
float gX_bias = 0, gY_bias = 0, gZ_bias = 0;

// Hard-iron bias & filter untuk 3 sumbu QMC5883L (0=X, 1=Y, 2=Z)
float m_bias[3] = {0, 0, 0};
float m_filt[3] = {0, 0, 0};
bool magInitialized = false;

// Indeks 2 sumbu horizontal
int hAxis1 = 0;
int hAxis2 = 1;

float roll_bias = 0, pitch_bias = 0, yaw_bias = 0;

// Variabel Data Sensor
float tempC = 0.0, humidity = 0.0;
int flexValue = 0;
float latitude = 0.0, longitude = 0.0;
int heartRate = 0, spo2 = 0;
int batPercent = 0;
bool isCharging = false;
bool gpsLocked = false;

// Filter Baterai
float filteredBatV = 0.0;

// Timer Non-Blocking
unsigned long lastImuSend = 0;
unsigned long lastMaxRead = 0;
unsigned long lastAdcRead = 0;
unsigned long lastDhtRead = 0;

// Tombol Tare, Buzzer & OLED
bool lastButtonState = HIGH;
unsigned long buttonPressStartTime = 0;
bool resetTriggered = false;
unsigned long tareBeepEndTime = 0;
unsigned long tareDisplayUntil = 0;

// State terakhir OLED (agar hanya update saat ada perubahan)
int lastOledBat = -1;
bool lastOledChg = false;
bool lastOledGps = false;
bool lastOledTare = false;

// =======================================================
// HELPER FUNCTIONS
// =======================================================
float wrapAngle(float a) {
  while (a >  180.0) a -= 360.0;
  while (a < -180.0) a += 360.0;
  return a;
}

bool readQMC(float m[3]) {
  Wire.beginTransmission(QMC_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom(QMC_ADDR, 6, true) == 6) {
    int16_t mx = (int16_t)(Wire.read() | Wire.read() << 8);
    int16_t my = (int16_t)(Wire.read() | Wire.read() << 8);
    int16_t mz = (int16_t)(Wire.read() | Wire.read() << 8);
    m[0] = (float)mx;
    m[1] = (float)my;
    m[2] = (float)mz;
    return true;
  }
  return false;
}

void soundBuzzer(int durationMs) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(BUZZER_PIN, LOW);
}

void updateOLED(bool force = false) {
  bool isTareMode = (millis() < tareDisplayUntil);

  if (!force &&
      isTareMode == lastOledTare &&
      batPercent == lastOledBat &&
      isCharging == lastOledChg &&
      gpsLocked == lastOledGps) {
    return;
  }

  lastOledTare = isTareMode;
  lastOledBat = batPercent;
  lastOledChg = isCharging;
  lastOledGps = gpsLocked;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (isTareMode) {
    display.setCursor(10, 18);
    display.println("REFERENSI TITIK 0");
    display.setCursor(32, 34);
    display.println("DIRESET!");
  } else {
    display.setCursor(0, 0);
    display.printf("DEVICE : %s", mdns_hostname);
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 16);
    display.printf("BAT: %d%%", batPercent);
    if (isCharging) display.print(" CHARGING");

    display.setCursor(0, 32);
    display.print(gpsLocked ? "GPS: LOCK" : "GPS: SEARCHING");

    display.setCursor(0, 48);
    display.print("KOMUNIKASI: SSE");
  }
  display.display();
}

// =======================================================
// FUNGSI KALIBRASI SAAT BOOTING
// =======================================================
void runFullCalibration() {
  // --- FASE 1: KALIBRASI ACCEL & GYRO (DIAM) ---
  Serial.println("\n[1/3] DIAMKAN SENSOR (Kalibrasi Accel & Gyro)...");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Fase 1: DIAMKAN");
  display.display();

  soundBuzzer(100);
  delay(1000);

  float sumX = 0, sumY = 0, sumZ = 0;
  float sumGX = 0, sumGY = 0, sumGZ = 0;

  for (int i = 0; i < 50; i++) {
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 14, true);

    float ax = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
    float ay = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
    float az = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
    Wire.read(); Wire.read();
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

  // --- PERSIAPAN FASE 2 ---
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Siap putar 360...");
  display.display();
  delay(3000);

  // --- FASE 2: KALIBRASI MAG (PUTAR 360 DERAJAT DATAR) ---
  Serial.println("\n[2/3] >>> PUTAR ALAT 360 DERAJAT DATAR (5 Detik) <<<");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Fase 2: PUTAR 360");
  display.display();

  soundBuzzer(100);

  float m_min[3] = { 32000,  32000,  32000};
  float m_max[3] = {-32000, -32000, -32000};
  float m_raw[3];
  unsigned long timer = millis();

  while (millis() - timer < 5000) {
    if (readQMC(m_raw)) {
      for (int i = 0; i < 3; i++) {
        if (m_raw[i] < m_min[i]) m_min[i] = m_raw[i];
        if (m_raw[i] > m_max[i]) m_max[i] = m_raw[i];
      }
    }
    delay(10);
    yield();
  }

  // Hard-Iron Bias murni
  float delta[3];
  for (int i = 0; i < 3; i++) {
    m_bias[i] = (m_max[i] + m_min[i]) / 2.0;
    delta[i]  = m_max[i] - m_min[i];
  }

  // Deteksi otomatis 2 sumbu horizontal
  if (delta[0] <= delta[1] && delta[0] <= delta[2]) {
    hAxis1 = 1; hAxis2 = 2; // Sumbu Y & Z
  } else if (delta[1] <= delta[0] && delta[1] <= delta[2]) {
    hAxis1 = 0; hAxis2 = 2; // Sumbu X & Z
  } else {
    hAxis1 = 0; hAxis2 = 1; // Sumbu X & Y
  }

  const char axisName[3] = {'X', 'Y', 'Z'};
  Serial.printf("\nDelta -> X:%.0f  Y:%.0f  Z:%.0f\n", delta[0], delta[1], delta[2]);
  Serial.printf("Sumbu Horizontal Terpilih: %c & %c\n", axisName[hAxis1], axisName[hAxis2]);

  // --- PERSIAPAN FASE 3 ---
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Hadapkan depan");
  display.println("& diamkan (3 dtk)");
  display.display();
  delay(3000);

  // --- FASE 3: KUNCI TITIK NOL ---
  Serial.println("\n[3/3] >>> MENGUNCI TITIK NOL... <<<");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Fase 3: KUNCI TITIK 0");
  display.display();

  soundBuzzer(200);

  Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  float ax = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
  float ay = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
  float az = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;

  float aX_init = ax - aX_bias;
  float aY_init = (-az) - aY_bias;
  float aZ_init = (-ay) - aZ_bias;

  roll_bias  = atan2(aX_init, sqrt(aY_init * aY_init + aZ_init * aZ_init)) * 180.0 / PI;
  pitch_bias = -atan2(aY_init, sqrt(aX_init * aX_init + aZ_init * aZ_init)) * 180.0 / PI;

  if (readQMC(m_raw)) {
    for (int i = 0; i < 3; i++) m_filt[i] = m_raw[i];
    magInitialized = true;
    yaw_bias = atan2(m_filt[hAxis2] - m_bias[hAxis2], m_filt[hAxis1] - m_bias[hAxis1]) * 180.0 / PI;
  }
}

// =======================================================
// SETUP
// =======================================================
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(CHRG_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("Sistem %s\n", mdns_hostname);
  display.println("Inisialisasi...");
  display.display();

  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  dht.begin();

  if (particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x0A);
    particleSensor.setPulseAmplitudeGreen(0);
  }
  Wire.setClock(400000);

  // MPU6050 & QMC5883L Setup
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission();
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x1C); Wire.write(0x10); Wire.endTransmission();
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x1B); Wire.write(0x08); Wire.endTransmission();

  Wire.beginTransmission(QMC_ADDR); Wire.write(0x0A); Wire.write(0x80); Wire.endTransmission(); delay(10);
  Wire.beginTransmission(QMC_ADDR); Wire.write(0x0B); Wire.write(0x01); Wire.endTransmission();
  Wire.beginTransmission(QMC_ADDR); Wire.write(0x09); Wire.write(0x0D); Wire.endTransmission();

  // WiFi, mDNS & SSE Setup
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
    Serial.printf("mDNS started: http://%s.local/events\n", mdns_hostname);
  }

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  events.onConnect([](AsyncEventSourceClient *client) {
    client->send("Terhubung ke ESP32", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();

  runFullCalibration();

  lastButtonState = digitalRead(BUTTON_PIN);
  soundBuzzer(200);
  updateOLED(true);
}

// =======================================================
// LOOP UTAMA
// =======================================================
void loop() {
  unsigned long now = millis();

  // 1. Baca GPS NEO-6M
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
  gpsLocked = gps.location.isValid() && (gps.location.age() < 5000);
  if (gpsLocked) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
  }

  // 2. Baca MAX30102
  if (now - lastMaxRead >= 20) {
    lastMaxRead = now;
    long irValue = particleSensor.getIR();

    static unsigned long lastBeatTime = 0;

    // Cek apakah MAX30102 benar-benar menempel di kulit
    if (irValue < 50000) {
      heartRate = 0;
      spo2 = 0;
      lastBeatTime = 0;
    } else {
      if (checkForBeat(irValue) == true) {
        unsigned long delta = now - lastBeatTime;
        lastBeatTime = now;

        // Validasi jarak antar detak
        if (delta > 333 && delta < 1333) {
          float beatsPerMinute = 60000.0 / delta;
          heartRate = (int)beatsPerMinute;
          spo2 = random(96, 99); // Placeholder SpO2 selama detak valid
        }
      }

      // Timeout: jika 3 detik tidak ada detak valid, reset ke 0
      if (lastBeatTime != 0 && (now - lastBeatTime > 3000)) {
        heartRate = 0;
        spo2 = 0;
      }
    }
  }

  // 3. Baca Flex & Baterai
  if (now - lastAdcRead >= 200) {
    lastAdcRead = now;
    flexValue = analogRead(FLEX_PIN);

    uint32_t mvSum = 0;
    for (int k = 0; k < 16; k++) mvSum += analogReadMilliVolts(BAT_ADC_PIN);
    float rawBatV = ((mvSum / 16.0) / 1000.0) * 2.0;

    filteredBatV = (filteredBatV == 0.0) ? rawBatV : (0.92 * filteredBatV + 0.08 * rawBatV);
    batPercent = constrain(map((int)(filteredBatV * 100), 330, 405, 0, 100), 0, 100);
    isCharging = (digitalRead(CHRG_PIN) == LOW);
  }

  // 4. Baca DHT21
  if (now - lastDhtRead >= 5000) {
    lastDhtRead = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) tempC = t;
    if (!isnan(h)) humidity = h;
  }

  // 5. Baca IMU (MPU6050 + QMC5883L) & Kirim SSE
  if (now - lastImuSend >= 10) {
    lastImuSend = now;

    Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 14, true);
    if (Wire.available() < 14) return;

    float ax = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
    float ay = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
    float az = (int16_t)(Wire.read() << 8 | Wire.read()) * 9.80665 / 4096.0;
    Wire.read(); Wire.read();
    float gx = (int16_t)(Wire.read() << 8 | Wire.read()) / 65.5;
    float gy = (int16_t)(Wire.read() << 8 | Wire.read()) / 65.5;
    float gz = (int16_t)(Wire.read() << 8 | Wire.read()) / 65.5;

    float aX = ax - aX_bias;
    float aY = (-az) - aY_bias;
    float aZ = (-ay) - aZ_bias;

    float gX = gx - gX_bias;
    float gY = (-gz) - gY_bias;
    float gZ = gy - gZ_bias;

    if (fabs(gX) < GYRO_DEADBAND) gX = 0.0;
    if (fabs(gY) < GYRO_DEADBAND) gY = 0.0;
    if (fabs(gZ) < GYRO_DEADBAND) gZ = 0.0;

    float raw_roll  = atan2(aX, sqrt(aY * aY + aZ * aZ)) * 180.0 / PI;
    float raw_pitch = -atan2(aY, sqrt(aX * aX + aZ * aZ)) * 180.0 / PI;

    // Baca MAG 2D Murni
    static float raw_yaw = 0;
    float m_raw[3];
    if (readQMC(m_raw)) {
      if (!magInitialized) {
        for (int i = 0; i < 3; i++) m_filt[i] = m_raw[i];
        magInitialized = true;
      } else {
        for (int i = 0; i < 3; i++) {
          m_filt[i] = (0.75 * m_filt[i]) + (0.25 * m_raw[i]);
        }
      }
      float m1_bersih = m_filt[hAxis1] - m_bias[hAxis1];
      float m2_bersih = m_filt[hAxis2] - m_bias[hAxis2];
      raw_yaw = atan2(m2_bersih, m1_bersih) * 180.0 / PI;
    }

    // Tombol Tare Instan
    bool buttonState = digitalRead(BUTTON_PIN);
    if (buttonState == LOW) {
      if (lastButtonState == HIGH) {
        buttonPressStartTime = now;
        resetTriggered = false;
      }
      if (!resetTriggered && (now - buttonPressStartTime >= 1500)) {
        pitch_bias = raw_pitch;
        roll_bias  = raw_roll;
        yaw_bias   = raw_yaw;
        Serial.println("\n>>> POSISI DIRESET KE 0.0 (TARE INSTAN) <<<");
        tareBeepEndTime = now + 300;
        tareDisplayUntil = now + 2000;
        resetTriggered = true;
      }
    }
    lastButtonState = buttonState;

    // Buzzer Non-Blocking
    digitalWrite(BUZZER_PIN, (now < tareBeepEndTime) ? HIGH : LOW);

    float pitch = wrapAngle(raw_pitch - pitch_bias);
    float roll  = wrapAngle(raw_roll - roll_bias);
    float yaw   = wrapAngle(raw_yaw - yaw_bias);
    float totalAcc = sqrt(aX * aX + aY * aY + aZ * aZ);

    // Kirim data via SSE
    if (events.count() > 0 && events.avgPacketsWaiting() < 2) {
      StaticJsonDocument<768> doc;
      doc["device"] = mdns_hostname;
      doc["time"]   = now;
      doc["x"]      = round(aX * 100) / 100.0;
      doc["y"]      = round(aY * 100) / 100.0;
      doc["z"]      = round(aZ * 100) / 100.0;
      doc["gx"]     = round(gX * 100) / 100.0;
      doc["gy"]     = round(gY * 100) / 100.0;
      doc["gz"]     = round(gZ * 100) / 100.0;
      doc["roll"]   = round(roll * 10) / 10.0;
      doc["pitch"]  = round(pitch * 10) / 10.0;
      doc["yaw"]    = round(yaw * 10) / 10.0;
      doc["total"]  = round(totalAcc * 100) / 100.0;
      doc["temp"]   = round(tempC * 10) / 10.0;
      doc["hum"]    = round(humidity * 10) / 10.0;
      doc["flex"]   = flexValue;
      doc["bpm"]    = heartRate;
      doc["spo2"]   = spo2;
      doc["lat"]    = latitude;
      doc["lng"]    = longitude;
      doc["bat"]    = batPercent;
      doc["chg"]    = isCharging;

      char jsonBuffer[768];
      serializeJson(doc, jsonBuffer);
      events.send(jsonBuffer, "sensor_data", now);
    }

    // Update OLED
    updateOLED();

  }
}
