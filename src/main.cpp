#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "esp_camera.h"
#include "LittleFS.h"
#include "esp_heap_caps.h"
#include <memory>

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
bool fsReady = false;

bool initCamera()
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
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Kamera Gagal! esp_camera_init error=0x%08x\n", err);
    return false;
  }

  Serial.println("Kamera Siap.");
  return true;
}

bool ensureDir(fs::FS &fs, const char *path)
{
  if (fs.exists(path))
  {
    return true;
  }
  return fs.mkdir(path);
}

void sendRequiredFile(AsyncWebServerRequest *request, const char *path, const char *contentType)
{
  if (!fsReady)
  {
    request->send(500, "text/plain", "LittleFS is not mounted");
    return;
  }

  if (!LittleFS.exists(path))
  {
    request->send(500, "text/plain", String("Required file missing in LittleFS: ") + path);
    return;
  }

  request->send(LittleFS, path, contentType);
}

void setup()
{
  Serial.begin(921600);
  Serial.println("--- boot ---");

  fsReady = LittleFS.begin(false);
  if (!fsReady)
  {
    Serial.println("LittleFS mount failed. Upload the filesystem image before using web/storage.");
  }
  else
  {
    Serial.println("LittleFS mounted.");
  }

  if (fsReady)
  {
    Serial.println("Listing LittleFS root:");
    File root = LittleFS.open("/");
    if (root)
    {
      File file = root.openNextFile();
      while (file)
      {
        Serial.printf(" - %s (len=%u)\n", file.name(), file.size());
        file = root.openNextFile();
      }
    }
    else
    {
      Serial.println("Failed to open LittleFS root");
    }

    ensureDir(LittleFS, "/train");
    for (int i = 0; i <= 9; i++)
    {
      String path = "/train/" + String(i);
      if (ensureDir(LittleFS, path.c_str()))
      {
        Serial.printf("Folder %s siap\n", path.c_str());
      }
    }
  }
  else
  {
    Serial.println("Skipping LittleFS listing and storage directory creation.");
  }

  if (!initCamera())
  {
    Serial.println("Inisialisasi kamera gagal, endpoint kamera akan mengembalikan error.");
  }

  Serial.println("Connecting to WiFi SSID: AP_HOME1");
  WiFi.begin("wifilogger", "protectLogger");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP Address: " + WiFi.localIP().toString());

  Serial.println("Registering HTTP endpoints...");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { sendRequiredFile(request, "/index.html", "text/html"); });

  server.on("/collector", HTTP_GET, [](AsyncWebServerRequest *request)
            { sendRequiredFile(request, "/collector.html", "text/html"); });

  server.on("/save", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (!request->hasParam("x") || !request->hasParam("y"))
    {
      request->send(400, "text/plain", "Missing x or y");
      return;
    }

    String x = request->getParam("x")->value();
    String y = request->getParam("y")->value();
    if (!fsReady)
    {
      request->send(500, "text/plain", "LittleFS is not mounted");
      return;
    }

    File f = LittleFS.open("/config.ini", "w");
    if (!f)
    {
      request->send(500, "text/plain", "Failed to open /config.ini");
      return;
    }

    f.printf("[ROI]\nx=%s\ny=%s\n", x.c_str(), y.c_str());
    f.close();

    Serial.printf("ROI Disimpan: X=%s, Y=%s\n", x.c_str(), y.c_str());
    request->send(200, "text/plain", "Saved");
  });

  server.on("/collect", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (!request->hasParam("label"))
    {
      request->send(400, "text/plain", "Missing label");
      return;
    }

    String label = request->getParam("label")->value();
    if (label.length() != 1 || label[0] < '0' || label[0] > '9')
    {
      request->send(400, "text/plain", "Label must be 0-9");
      return;
    }

    if (!fsReady)
    {
      request->send(500, "text/plain", "LittleFS is not mounted");
      return;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
      request->send(500, "text/plain", "Camera Capture Failed");
      return;
    }

    String path = "/train/" + label + "/" + String(millis()) + ".jpg";
    File file = LittleFS.open(path, FILE_WRITE);
    if (file)
    {
      file.write(fb->buf, fb->len);
      file.close();
      Serial.printf("Dataset tersimpan: %s\n", path.c_str());
      request->send(200, "text/plain", "Berhasil simpan ke kelas: " + label);
    }
    else
    {
      request->send(500, "text/plain", "Gagal simpan file");
    }

    esp_camera_fb_return(fb);
  });

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (!fsReady)
    {
      request->send(500, "text/plain", "LittleFS is not mounted");
      return;
    }

    String output = "Daftar Dataset:\n";
    for (int i = 0; i <= 9; i++)
    {
      String path = "/train/" + String(i);
      File root = LittleFS.open(path);
      int count = 0;
      if (root)
      {
        File file = root.openNextFile();
        while (file)
        {
          count++;
          file = root.openNextFile();
        }
      }
      output += "Kelas " + String(i) + ": " + String(count) + " foto\n";
    }
    request->send(200, "text/plain", output);
  });

  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
      request->send(500, "text/plain", "Gagal Capture");
      return;
    }

    if (fb->format != PIXFORMAT_JPEG)
    {
      esp_camera_fb_return(fb);
      request->send(500, "text/plain", "Camera frame is not JPEG");
      return;
    }

    size_t jpgLen = fb->len;
    std::shared_ptr<uint8_t> jpgBuf(
      static_cast<uint8_t *>(heap_caps_malloc(jpgLen, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
      free
    );

    if (!jpgBuf)
    {
      esp_camera_fb_return(fb);
      request->send(500, "text/plain", "Gagal alokasi buffer JPEG");
      return;
    }

    memcpy(jpgBuf.get(), fb->buf, jpgLen);
    esp_camera_fb_return(fb);

    request->send("image/jpeg", jpgLen, [jpgBuf, jpgLen](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      size_t remaining = jpgLen - index;
      size_t bytesToCopy = remaining < maxLen ? remaining : maxLen;
      memcpy(buffer, jpgBuf.get() + index, bytesToCopy);
      return bytesToCopy;
    });
  });

  server.serveStatic("/", LittleFS, "/");

  server.onNotFound([](AsyncWebServerRequest *request)
                    {
    Serial.printf("HTTP Request: method=%d url=%s\n", request->method(), request->url().c_str());
    request->send(404, "text/plain", "Not Found");
  });

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {}
