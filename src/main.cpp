#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "STARLINK";

WebServer server(80);

void sendNetworkPage() {
  String html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<meta http-equiv='refresh' content='5'>"  // обновление каждые 5 сек
    "<title>ESP32 Network Info</title>"
    "<style>"
    "body{font-family:monospace;padding:20px;background:#1a1a1a;color:#00ff88;}"
    "h2{color:white;}"
    "table{width:100%;border-collapse:collapse;}"
    "td{padding:8px 12px;border-bottom:1px solid #333;}"
    "td:first-child{color:#aaa;width:200px;}"
    ".good{color:#2ecc71}.warn{color:#f39c12}.bad{color:#e74c3c}"
    "</style></head><body>"
    "<h2>📡 ESP32 Network Info</h2>"
    "<table>";

  // Определяем качество сигнала
  int rssi = WiFi.RSSI();
  String rssiClass = rssi > -60 ? "good" : rssi > -75 ? "warn" : "bad";
  String rssiDesc = rssi > -60 ? "Отличный" : rssi > -75 ? "Средний" : "Слабый";

  html += "<tr><td>IP адрес</td><td>" + WiFi.localIP().toString() + "</td></tr>";
  html += "<tr><td>Маска сети</td><td>" + WiFi.subnetMask().toString() + "</td></tr>";
  html += "<tr><td>Шлюз</td><td>" + WiFi.gatewayIP().toString() + "</td></tr>";
  html += "<tr><td>DNS</td><td>" + WiFi.dnsIP().toString() + "</td></tr>";
  html += "<tr><td>MAC адрес</td><td>" + WiFi.macAddress() + "</td></tr>";
  html += "<tr><td>Сеть SSID</td><td>" + String(WiFi.SSID()) + "</td></tr>";
  html += "<tr><td>BSSID роутера</td><td>" + WiFi.BSSIDstr() + "</td></tr>";
  html += "<tr><td>Канал</td><td>" + String(WiFi.channel()) + "</td></tr>";
  html += "<tr><td>Сигнал RSSI</td><td class='" + rssiClass + "'>" + rssi + " dBm — " + rssiDesc + "</td></tr>";
  html += "<tr><td>Uptime ESP32</td><td>" + String(millis() / 1000) + " сек</td></tr>";

  html += "</table>"
          "<p style='color:#555;font-size:12px'>Обновление каждые 5 секунд</p>"
          "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  neopixelWrite(RGB_BUILTIN, 0, 0, 50); // синий = подключаюсь

  WiFi.begin(ssid);
  Serial.print("Подключаюсь");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  neopixelWrite(RGB_BUILTIN, 0, 50, 0); // зелёный = подключился

  Serial.println("\nПодключено!");
  Serial.print("Открой: http://");
  Serial.println(WiFi.localIP());

  server.on("/", sendNetworkPage);
  server.begin();
}

void loop() {
  server.handleClient();
}