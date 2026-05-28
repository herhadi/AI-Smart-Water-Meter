# 📸 ESP32-S3 Smart Meter PDAM - Dataset Collector 🚀

Proyek ini adalah sistem berbasis **ESP32-S3 (N16R8)** yang dirancang khusus untuk mengumpulkan dataset foto angka digital/roda mekanis pada meteran air PDAM. Sistem ini mengintegrasikan sensor kamera murni (Non-JPEG internal seperti RHYX) menggunakan format RAW `RGB565`, menyediakan antarmuka web interaktif via WiFi, dan menyimpan hasil jepretan langsung ke dalam **LittleFS** (Flash internal 16MB) menggunakan pemetaan kategori folder `0-9` berbasis input keyboard secara *real-time*.

---

## 📌 Fitur Utama

* **Native USB CDC Integration:** Proses *flashing* dan *monitoring* otomatis berjalan secara *plug-and-play* tanpa perlu menekan tombol reset/boot manual berkat fitur USB CDC on Boot.
* **Dual-Core Processing Optimization:** Memanfaatkan performa ESP32-S3 dengan memori eksternal Octal SPI (OPI) PSRAM 8MB.
* **RAW-to-JPEG Software Compression:** Mengonversi data mentah `RGB565` dari sensor non-JPEG menjadi format `.jpg` standar secara dinamis menggunakan fungsi `frame2jpg`.
* **Asynchronous Web Server:** Memanfaatkan `ESPAsyncWebServer` untuk menangani *request* HTTP tanpa mengganggu stabilitas pembacaan sensor.
* **Smart Dataset Directory Automount:** Otomatis mendeteksi, melakukan *mounting*, dan membuat struktur folder `/train/0` sampai `/train/9` di LittleFS saat *booting*.
* **Keyboard-Triggered Capture:** Pengambilan sampel foto jarak jauh (remote) cukup dengan memfokuskan kursor di browser dan menekan tombol angka `0-9`.

---

## 🔌 Skema Pengkabelan (Hardware Wiring)

Sistem ini dikoneksikan langsung menggunakan jalur **Native USB CDC** (Data positif dan Data negatif langsung dari pin internal ESP32-S3 ke komputer/iMac):

| Jalur Kabel USB | ESP32-S3 Pin | Fungsi / Catatan |
| :--- | :--- | :--- |
| **VCC (5V)** | **5V / VIN** | Suplai daya utama dari port USB |
| **GND** | **GND** | Ground Utama |
| **Data + (D+)** | **GPIO 20** | Jalur Data Positif USB CDC |
| **Data - (D-)** | **GPIO 19** | Jalur Data Negatif USB CDC |

### 🛠️ Mode Flash Otomatis
Karena flag `-DARDUINO_USB_CDC_ON_BOOT=1` aktif, PlatformIO akan otomatis memerintahkan ESP32-S3 masuk ke mode *download* setiap kali Anda mengklik tombol **Upload**, membuat proses *development* menjadi jauh lebih praktis.

---

## 💻 Konfigurasi PlatformIO (`platformio.ini`)

Gunakan konfigurasi berikut untuk memastikan alokasi memori **OPI PSRAM 8MB** dan sistem partisi **Flash 16MB** terkonfigurasi dengan benar:

```ini
[env:esp32-s3-devkitc-1-n16r8]
platform = espressif32
board = 4d_systems_esp32s3_gen4_r8n16
framework = arduino

; Konfigurasi Flash & PSRAM
board_upload.flash_size = 16MB
board_build.flash_mode = qio
board_build.flash_freq = 80MHz
board_build.partitions = default_16MB.csv
board_build.filesystem = littlefs

; Flag Aktivasi PSRAM & USB CDC (Wajib)
build_flags = 
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DBOARD_HAS_PSRAM

; Port Operasional (Sesuaikan dengan port komputer Anda)
upload_port = COM4
monitor_port = COM4

upload_speed = 921600
monitor_speed = 921600

lib_deps = 
    espressif/esp32-camera @ ^2.0.4
    [https://github.com/me-no-dev/ESPAsyncWebServer.git](https://github.com/me-no-dev/ESPAsyncWebServer.git)
    [https://github.com/me-no-dev/AsyncTCP.git](https://github.com/me-no-dev/AsyncTCP.git)
    bblanchon/ArduinoJson @ ^7.0.0

LittleFS Root (/)
├── index.html          # Halaman monitoring utama meteran air
├── collector.html      # Antarmuka web pengumpul dataset (Input Keyboard)
├── config.ini          # File konfigurasi koordinat ROI (X, Y) hasil /save
└── train/              # Direktori utama Dataset AI
    ├── 0/              # Wadah foto angka 0 (*.jpg)
    ├── 1/              # Wadah foto angka 1 (*.jpg)
    ├── 2/              # Wadah foto angka 2 (*.jpg)
    └── ... [3-9]       # Wadah foto angka s.d 9