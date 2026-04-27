#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "STARLINK";

WebServer server(80);

const char* html =
  "<!DOCTYPE html><html><head>"
  "<meta charset='utf-8'>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<title>ESP32 RGB</title>"
  "<style>"
  "body{font-family:sans-serif;text-align:center;padding:40px;background:#1a1a1a;color:white;}"
  "a{display:inline-block;margin:8px;padding:16px 32px;border-radius:8px;text-decoration:none;color:white;font-size:18px;}"
  ".red{background:#e74c3c}.green{background:#2ecc71}.blue{background:#3498db}"
  ".pink{background:#ff69b4}.cyan{background:#00bcd4}.yellow{background:#f1c40f;color:black}"
  ".off{background:#444}"
  "</style></head><body>"
  "<h2>ESP32 RGB</h2>"
  "<a href='/red' class='red'>RED</a>"
  "<a href='/green' class='green'>GREEN</a>"
  "<a href='/blue' class='blue'>BLUE</a><br>"
  "<a href='/pink' class='pink'>PINK</a>"
  "<a href='/cyan' class='cyan'>CYAN</a>"
  "<a href='/yellow' class='yellow'>YELLOW</a><br>"
  "<a href='/off' class='off'>OFF</a>"
  "</body></html>";

void sendPage() {
  server.send(200, "text/html", html);
}

void setColor(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_BUILTIN, r, g, b);
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  neopixelWrite(RGB_BUILTIN, 0, 0, 0);

  WiFi.begin(ssid);  // без пароля
  Serial.print("Подключаюсь");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nПодключено!");
  Serial.print("Открой: http://");
  Serial.println(WiFi.localIP());

  server.on("/",       sendPage);
  server.on("/red",    []() { setColor(255, 0,   0);   sendPage(); });
  server.on("/green",  []() { setColor(0,   255, 0);   sendPage(); });
  server.on("/blue",   []() { setColor(0,   0,   255); sendPage(); });
  server.on("/pink",   []() { setColor(255, 0,   130); sendPage(); });
  server.on("/cyan",   []() { setColor(0,   255, 255); sendPage(); });
  server.on("/yellow", []() { setColor(255, 200, 0);   sendPage(); });
  server.on("/off",    []() { setColor(0,   0,   0);   sendPage(); });

  server.begin();
  Serial.println("Сервер запущен!");
}

void loop() {
  server.handleClient();
}