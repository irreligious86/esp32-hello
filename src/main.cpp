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
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); Serial.print("."); attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    neopixelWrite(RGB_BUILTIN, 0, 50, 0);
    Serial.println("\nПодключено!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    Serial.println("Открой: http://192.168.1.200");
    Serial.println("Или:    http://esp32.local");
  } else {
    neopixelWrite(RGB_BUILTIN, 50, 0, 0);
    Serial.println("\nОшибка подключения!");
  }
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  String wifiSsid, wifiPass;
  loadSettings(wifiSsid, wifiPass, smtpPassword);

  setupWiFi(wifiSsid, wifiPass);

  if (WiFi.status() == WL_CONNECTED) {
    if (MDNS.begin("esp32"))
      Serial.println("mDNS: http://esp32.local");

    setupWebUI(server);
    server.begin();
    Serial.println("Сервер запущен!");
  }
}

void loop() {
  server.handleClient();

  if (scanning) {
    scanStep();
    if (!scanning && scanDone) {
      sendReport();
    }
    return;
  }

  static unsigned long lastBlink = 0;
  static bool blinkState = false;
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      blinkState = !blinkState;
      neopixelWrite(RGB_BUILTIN, blinkState ? 50 : 0, 0, 0);
    }
  }
}