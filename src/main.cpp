#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "config.h"
#include "scanner.h"
#include "mailer.h"
#include "webui.h"

WebServer server(80);
extern String smtpPassword;

TaskHandle_t webTaskHandle  = NULL;
TaskHandle_t scanTaskHandle = NULL;

#define BOOT_BUTTON 0

void webTask(void* param) {
  for (;;) {
    server.handleClient();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void scanTask(void* param) {
  for (;;) {
    if (scanning) {
      scanStep();
      if (!scanning && scanDone) {
        sendReport();
      }
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void startAP() {
  Serial.println("Запускаю AP режим...");
  neopixelWrite(RGB_BUILTIN, 255, 100, 0);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-Setup", "12345678");

  Serial.println("Сеть: ESP32-Setup");
  Serial.println("Пароль: 12345678");
  Serial.print("Настройки: http://");
  Serial.println(WiFi.softAPIP());
}

void setupWiFi(const String& ssid, const String& pass) {
  neopixelWrite(RGB_BUILTIN, 0, 0, 50);

  WiFi.mode(WIFI_STA);

  IPAddress local(192, 168, 1, 200);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(8, 8, 8, 8);
  WiFi.config(local, gateway, subnet, dns);

  if (pass.length() > 0)
    WiFi.begin(ssid.c_str(), pass.c_str());
  else
    WiFi.begin(ssid.c_str());

  Serial.print("Подключаюсь к " + ssid);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 5) {
    delay(500); Serial.print("."); attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    neopixelWrite(RGB_BUILTIN, 0, 50, 0);
    Serial.println("\nПодключено!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    Serial.println("Открой: http://192.168.1.200");
    Serial.println("Или:    http://esp32.local");
  } else {
    Serial.println("\nWiFi недоступен — поднимаю AP...");
    startAP();
  }
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  // Ждём 3 секунды — если BOOT зажата всё время то AP режим
  Serial.println("Держи BOOT 3 сек для AP режима...");
  neopixelWrite(RGB_BUILTIN, 100, 100, 100);

  bool forceAP = false;
  unsigned long holdStart = millis();
  while (millis() - holdStart < 3000) {
    if (digitalRead(BOOT_BUTTON) == HIGH) {
      forceAP = false;
      break;
    }
    forceAP = true;
    delay(50);
  }

  String wifiSsid, wifiPass;
  loadSettings(wifiSsid, wifiPass, smtpPassword);

  if (forceAP) {
    Serial.println("AP режим активирован!");
    startAP();
  } else {
    setupWiFi(wifiSsid, wifiPass);
  }

  if (MDNS.begin("esp32"))
    Serial.println("mDNS: http://esp32.local");

  setupWebUI(server);
  server.begin();
  Serial.println("Сервер запущен!");

  xTaskCreatePinnedToCore(webTask,  "WebTask",  8192,  NULL, 1, &webTaskHandle,  0);
  xTaskCreatePinnedToCore(scanTask, "ScanTask", 16384, NULL, 1, &scanTaskHandle, 1);

  Serial.println("FreeRTOS: Web→Core0 Scan→Core1");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastBlink = 0;
    static bool blinkState = false;
    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      blinkState = !blinkState;
      neopixelWrite(RGB_BUILTIN, blinkState ? 50 : 0, 0, 0);
    }
  }
  vTaskDelay(100 / portTICK_PERIOD_MS);
}