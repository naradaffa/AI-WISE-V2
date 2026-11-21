# AI WISE V2

Sistem *wearable motion tracking*, pemantauan biometrik, dan *fall detection* real-time untuk keselamatan pekerja. Berbasis **ESP32** dengan **MPU6050**, **GY273**, **OLED SSD1306**, **GPS NEO-6M**, **MAX30102**, **DHT21**, dan transmisi data real-time ke Web Dashboard via WiFi.

---

## 🚀 Persiapan & Koneksi WiFi

Perangkat menggunakan **mDNS** sehingga alamat akses otomatis mengikuti nama *hostname* masing-masing perangkat tanpa perlu mengecek IP dinamis.

- **WiFi SSID:** `TMP`
- **Password:** `ugmjaya1`
- **Endpoint SSE:** `http://mdns_hostname.local/events` *(sesuaikan `mdns_hostname` pada setiap unit perangkat, contoh: pekerja1)*

### ⚠️ Urutan Menyalakan Perangkat

> Disarankan menyalakan perangkat **satu per satu** dan menunggu hingga proses kalibrasi 3 fase selesai sebelum menyalakan perangkat berikutnya.

1. **Nyalakan Router** - Tunggu hingga jaringan WiFi `TMP` aktif.
2. **Hubungkan Laptop / PC** - Sambungkan komputer ke jaringan WiFi `TMP`.
3. **Buka File Web Dashboard di Live Server** — Jalankan `index.html` melalui VS Code Live Server agar dashboard *standby* menerima koneksi SSE.
4. **Nyalakan Device & Jalankan Kalibrasi 1 per 1** — Pegang alat tegak menghadap depan saat dinyalakan, tunggu hingga proses kalibrasi 3 fase selesai dan layar OLED menampilkan status berjalan, baru lanjutkan menyalakan unit berikutnya.

---

## 🔗 Cara Menjalankan Web Dashboard

1. **Unduh File Project:** Download folder **Web Dashboard** (format `.zip`) dari repositori ini.
2. **Ekstrak File:** Ekstrak file `.zip` tersebut ke folder komputer Anda.
3. **Buka di VS Code:**
   - Buka aplikasi **Visual Studio Code**.
   - Pilih menu **File** > **Open Folder...**, lalu pilih folder hasil ekstrak tadi.
4. **Jalankan Web Dashboard:**
   - Cari file `index.html` pada panel Explorer di sebelah kiri.
   - Klik kanan pada file `index.html`, lalu pilih **Open with Live Server**.
   - Dashboard akan otomatis terbuka di browser default Anda.

> **Catatan:** Pastikan ekstensi **Live Server** oleh Ritwick Dey sudah terinstal di Visual Studio Code.

---

## 🚨 Kalibrasi & Indikator OLED / Buzzer

Pegang alat dalam posisi tegak menghadap ke depan saat pertama kali dinyalakan (setelah WiFi terhubung).

| Tahap | Tampilan OLED & Buzzer | Instruksi & Tujuan |
| :--- | :--- | :--- |
| **Booting** | `Sistem pekerja10 / Inisialisasi...` | Tunggu inisialisasi sensor I2C dan koneksi WiFi. |
| **Fase 1 (~1.5s)** | `Fase 1: DIAMKAN` + Bunyi Buzzer 1x | Pegang alat tegak & diam total (kalibrasi bias Accel & Gyro). |
| **Persiapan 2 (3s)** | `Siap putar 360...` | Bersiap memutar perangkat secara mendatar. |
| **Fase 2 (5s)** | `Fase 2: PUTAR 360` + Bunyi Buzzer 1x | Putar alat horizontal 360° secara perlahan (kalibrasi *hard-iron* kompas QMC5883L & deteksi sumbu horizontal). |
| **Persiapan 3 (3s)** | `Hadapkan depan & diamkan (3 dtk)` | Arahkan perangkat lurus ke depan sesuai orientasi pemakaian. |
| **Fase 3** | `Fase 3: KUNCI TITIK 0` + Bunyi Buzzer 1x | Tahan diam sesaat untuk mengunci referensi titik 0° (Roll, Pitch, Yaw). |
| **Berjalan** | Status `BAT`, `GPS`, `KOMUNIKASI: SSE` + Buzzer 1x | Pasang di sabuk/tubuh pekerja, perangkat aktif mengirim data 100 Hz. |

---

## 📊 Standar Pengukuran & Parameter Sensor

- **ROLL:** Menunduk (-), Tengadah (+) dalam derajat (`°`)
- **PITCH:** Miring Kiri (-), Miring Kanan (+) dalam derajat (`°`)
- **YAW:** Putar Kiri (+), Putar Kanan (-) dalam derajat (`°`)
- **Akselerometer (`x`, `y`, `z` & `total`):** Percepatan linear dalam $\text{m/s}^2$
- **Giroskop (`gx`, `gy`, `gz`):** Kecepatan sudut rotasi dalam $\text{deg/s}$ (*deadband* $\pm 1.2\text{ deg/s}$)
- **Biometrik (`bpm`, `spo2`):** Detak jantung (BPM) dan saturasi oksigen (%) dari **MAX30102** (perhitungan rasio optik AC/DC Red & IR secara *non-blocking* di 50 Hz; otomatis `0` jika sensor tidak menempel kulit)
- **Lingkungan (`temp`, `hum`):** Suhu ($^\circ\text{C}$) dan kelembapan relatif (%) dari **DHT21** (pembaruan tiap 5 detik)
- **Lokasi (`lat`, `lng`):** Koordinat GPS dari **u-blox NEO-6M**
- **Flex Sensor (`flex`):** Pembacaan *voltage divider* mentah (`0 – 4095`) atau sudut SI dalam **radian** (`0.000 – 1.571 rad`) setelah kalibrasi
- **Baterai (`bat`, `chg`):** Persentase kapasitas baterai LiPo 1S (**100% dikalibrasi pada 4.05 V**, batas bawah **0% pada 3.20 V**) serta status pengisian daya dari pin `CHRG` TP4056

---

## 🛠️ Tombol Reset Posisi (Tare Instan)

- **Fungsi:** Mengembalikan referensi orientasi sudut ke titik `0.0°` (`Roll`, `Pitch`, `Yaw`) tanpa perlu *reboot* perangkat.
- **Cara Pakai:** Tekan dan tahan tombol (`D2`) selama **1,5 detik**.
- **Indikator Sukses:** Buzzer berbunyi selama 300 ms dan layar OLED menampilkan `REFERENSI TITIK 0 DIRESET!` selama 2 detik.

---

## 🔌 Konfigurasi Pin Hardware (ESP32)

| Komponen / Fungsi | Pin ESP32 | Keterangan |
| :--- | :--- | :--- |
| **I2C SDA** (MPU6050, QMC5883L, MAX30102, OLED) | `D4` | Kecepatan bus 400 kHz (*Fast Mode*) |
| **I2C SCL** (MPU6050, QMC5883L, MAX30102, OLED) | `D5` | Kecepatan bus 400 kHz (*Fast Mode*) |
| **GPS NEO-6M (RX / TX)** | `D7` / `D6` | UART Serial1 (9600 bps) |
| **Sensor Suhu & Kelembapan (DHT21)** | `D3` | Digital One-Wire |
| **Flex Sensor** | `A0` | Input Analog ADC (*Voltage Divider*) |
| **Buzzer Aktif** | `D8` | Output Digital |
| **Tombol Tare / Reset Posisi** | `D2` | `INPUT_PULLUP` (Aktif `LOW`) |
| **Sensor Tegangan Baterai** | `D9` | Pembagi tegangan 1:2 (`analogReadMilliVolts`) |
| **Status Charging (TP4056)** | `D10` | `INPUT_PULLUP` (`LOW` = Sedang mengisi daya) |

---

## 🔧 Catatan Kalibrasi & Troubleshooting Hardware

### 1. Flex Sensor (Voltage Divider & Stabilitas)
- **Logging Data Mentah:** Sesuai standar pembacaan pembagi tegangan murni, data `flex` dapat di-*logging* apa adanya dalam rentang nilai ADC mentah (`0 – 4095`) maupun dikonversi ke satuan SI (**radian**) berdasarkan nilai titik lurus (`FLEX_FLAT_ADC`) dan tekuk maksimal (`FLEX_BENT_ADC`).
- **Mengatasi Nilai Fluktuatif (50–100 diam vs 500–2500 saat disentuh):**
  - Pastikan jalur resistor pembagi ke `GND` tersambung solid (tidak *floating*).
  - Lapisi area sensor dan pangkal kaki pin menggunakan selotip isolasi / *heatshrink* agar tidak terjadi kontak langsung dengan kulit tangan yang memicu *noise* 50 Hz.
  - Tambahkan kapasitor keramik **100 nF (kode `104`)** paralel antara pin `A0` dan `GND` untuk meredam lonjakan *noise*.
  - Gunakan **Op-Amp Rail-to-Rail 3.3V** (seperti **MCP6002** atau **LMV358**) sebagai *impedance buffer* hanya jika menggunakan sensor berimpedansi sangat tinggi ($>100\text{ k}\Omega$ hingga $\text{M}\Omega$).

### 2. Manajemen Daya & Charging (TP4056)
- **Kalibrasi Persentase Baterai:** Batas atas tegangan penuh ditetapkan pada **4.05 V = 100%** menggunakan fungsi `analogReadMilliVolts()` dengan *exponential moving average* agar indikator tetap stabil dan tidak tertahan di angka 90%-an saat tegangan baterai mulai turun setelah dilepas dari *charger*.
- **Isolasi Jalur Baterai:** Saat melakukan pengujian atau *flashing* melalui USB tanpa baterai terpasang, pastikan modul TP4056 tidak mengalami arus balik (*back-feeding*) ke jalur baterai, dan gunakan dioda pemisah (*power path*) atau sakelar fisik pada jalur baterai menuju regulator 3.3V ESP32.

---

## 📂 Struktur Repositori

- **ESP32 (`v2_fix_mpu6050.ino`):** Kode program utama *non-blocking* (100 Hz IMU & SSE, 50 Hz MAX30102, 5 Hz ADC/Baterai, 0.2 Hz DHT21) beserta dependensi (`Wire.h`, `WiFi.h`, `ESPmDNS.h`, `ESPAsyncWebServer`, `ArduinoJson`, `TinyGPS++`, `Adafruit_GFX`, `Adafruit_SSD1306`, `DHT`, `MAX30105`).
- **Web Dashboard:** File antarmuka visual berbasis web (`index.html`, `style.css`, `script.js`) yang menerima aliran data *real-time* melalui protokol SSE.
