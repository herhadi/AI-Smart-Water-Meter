#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "esp_camera.h"
#include "LittleFS.h"
#include "img_converters.h" // Untuk konversi format gambar

// Pinout Kamera untuk ESP32-S3 (Sesuaikan jika board Anda beda, ini umum untuk S3-Sense)
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5
#define Y9_GPIO_NUM 16
#define Y8_GPIO_NUM 17
#define Y7_GPIO_NUM 18
#define Y6_GPIO_NUM 12
#define Y5_GPIO_NUM 10
#define Y4_GPIO_NUM 8
#define Y3_GPIO_NUM 9
#define Y2_GPIO_NUM 11
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

AsyncWebServer server(80);

void initCamera()
{
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
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM; // WAJIB UNTUK S3 N16R8
  config.jpeg_quality = 12;
  config.fb_count = 2;

  if (esp_camera_init(&config) != ESP_OK)
  {
    Serial.println("Kamera Gagal!");
    return;
  }
  Serial.println("Kamera Siap.");
}

void setup()
{
  Serial.begin(115200);
  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS Error!");
    return;
  }

  // Buat folder utama dan sub-folder 0-9
  if (!LittleFS.exists("/train"))
    LittleFS.mkdir("/train");

  for (int i = 0; i <= 9; i++)
  {
    String path = "/train/" + String(i);
    if (!LittleFS.exists(path))
    {
      LittleFS.mkdir(path);
      Serial.printf("Folder %s berhasil dibuat\n", path.c_str());
    }
  }

  initCamera();

  // WiFi Setup (Ganti dengan SSID Anda)
  WiFi.begin("Sendang Kamulyan", "");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nIP Address: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html", "text/html"); });

  server.on("/save", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (request->hasParam("x") && request->hasParam("y")) {
        String x = request->getParam("x")->value();
        String y = request->getParam("y")->value();
        
        File f = LittleFS.open("/config.ini", "w");
        f.printf("[ROI]\nx=%s\ny=%s\n", x.c_str(), y.c_str());
        f.close();
        
        Serial.printf("ROI Disimpan: X=%s, Y=%s\n", x.c_str(), y.c_str());
        request->send(200, "text/plain", "Saved");
    } });

  server.on("/collect", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (request->hasParam("label")) {
        String label = request->getParam("label")->value();
        
        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) return request->send(500, "text/plain", "Camera Capture Failed");

        // Tentukan path penyimpanan (Gunakan millis agar nama file unik)
        String path = "/train/" + label + "/" + String(millis()) + ".jpg";
        
        // Buat folder jika belum ada (Opsional, tergantung versi LittleFS)
        // LittleFS.mkdir("/train/" + label); 

        File file = LittleFS.open(path, FILE_WRITE);
        if (file) {
            // Simpan seluruh frame atau hasil crop? 
            // Untuk training, lebih baik simpan hasil crop 32x20 pixel
            // Tapi untuk awal, kita simpan full frame agar bisa kita crop di Mac
            file.write(fb->buf, fb->len);
            file.close();
            Serial.printf("Dataset tersimpan: %s\n", path.c_str());
            request->send(200, "text/plain", "Berhasil simpan ke kelas: " + label);
        } else {
            request->send(500, "text/plain", "Gagal simpan file");
        }
        
        esp_camera_fb_return(fb);
    } });

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String output = "Daftar Dataset:\n";
    for (int i = 0; i <= 9; i++) {
        String path = "/train/" + String(i);
        File root = LittleFS.open(path);
        File file = root.openNextFile();
        int count = 0;
        while(file) { count++; file = root.openNextFile(); }
        output += "Kelas " + String(i) + ": " + String(count) + " foto\n";
    }
    request->send(200, "text/plain", output); });

  // Endpoint untuk ambil foto ke Browser
  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request)
            {
        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) return request->send(500, "text/plain", "Gagal Capture");
        AsyncWebServerResponse *response = request->beginResponse(200, "image/jpeg", fb->buf, fb->len);
        request->send(response);
        esp_camera_fb_return(fb); });

  server.begin();
}

void loop() {}