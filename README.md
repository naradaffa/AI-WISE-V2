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
3. **Buka File Web Dashboard di Live Server** - Jalankan `index.html` melalui VS Code Live Server agar dashboard *standby* menerima koneksi SSE.
4. **Nyalakan Device & Jalankan Kalibrasi 1 per 1** - Letakkan alat menghadap depan saat dinyalakan, tunggu hingga proses kalibrasi 3 fase selesai dan layar OLED menampilkan status berjalan, baru lanjutkan menyalakan unit berikutnya.

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
| **Fase 1 (~1.5s)** | `Fase 1: DIAMKAN` + Bunyi Buzzer 1x | Letakkan alat menghadap depan di meja. |
| **Persiapan 2 (3s)** | `Siap putar 360...` | Bersiap memutar perangkat secara mendatar. |
| **Fase 2 (5s)** | `Fase 2: PUTAR 360` + Bunyi Buzzer 1x | Putar alat secara CCW 360° secara perlahan. |
| **Persiapan 3 (3s)** | `Hadapkan depan & diamkan (3 dtk)` | Arahkan perangkat lurus ke depan sesuai orientasi pemakaian. |
| **Fase 3** | `Fase 3: KUNCI TITIK 0` + Bunyi Buzzer 1x | Tahan diam sesaat untuk mengunci referensi titik 0°. |
| **Berjalan** | Status `BAT`, `GPS`, `KOMUNIKASI: SSE` + Buzzer 1x | Pasang di sabuk/tubuh pekerja, perangkat aktif mengirim data. |

---

## 📊 Standar Pengukuran & Parameter Sensor

- **ROLL:** Menunduk (-), Tengadah (+) dalam derajat (`°`)
- **PITCH:** Miring Kiri (-), Miring Kanan (+) dalam derajat (`°`)
- **YAW:** Putar Kiri (+), Putar Kanan (-) dalam derajat (`°`)
- **Akselerometer (`x`, `y`, `z` & `total`):** Percepatan linear dalam $\text{m/s}^2$
- **Giroskop (`gx`, `gy`, `gz`):** Kecepatan sudut rotasi dalam $\text{deg/s}$ (*deadband* $\pm 1.2\text{ deg/s}$)
- **Biometrik (`bpm`, `spo2`):** Detak jantung (BPM) dan saturasi oksigen (%) dari **MAX30102**
- **Lingkungan (`temp`, `hum`):** Suhu ($^\circ\text{C}$) dan kelembapan relatif (%) dari **DHT21**
- **Lokasi (`lat`, `lng`):** Koordinat GPS dari **u-blox NEO-6M**
- **Baterai (`bat`, `chg`):** Persentase kapasitas baterai (**100% dikalibrasi pada 4.05 V**, batas bawah **0% pada 3.30 V**) serta status pengisian daya

---

## 🛠️ Tombol Reset Posisi (Tare Instan)

- **Fungsi:** Mengembalikan referensi orientasi sudut ke titik `0.0°` (`Roll`, `Pitch`, `Yaw`) tanpa perlu *reboot* perangkat.
- **Cara Pakai:** Tekan dan tahan tombol selama **1,5 detik**.
- **Indikator Sukses:** Buzzer berbunyi selama 0.3s dan layar OLED menampilkan `REFERENSI TITIK 0 DIRESET!` selama 2 detik.

---

## 📂 Struktur Repositori

- **ESP32 (`v2_fix_mpu6050.ino`):** Kode program ESP32.
- **Web Dashboard:** File antarmuka visual berbasis web yang menerima aliran data *real-time* melalui protokol SSE.
