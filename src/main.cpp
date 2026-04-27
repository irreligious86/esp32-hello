#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Ping.h>
#include <ESP_Mail_Client.h>
#include "lwip/etharp.h"
#include "lwip/netif.h"

const char* ssid = "STARLINK";

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define SENDER_EMAIL "irreligious86@gmail.com"
#define SENDER_PASSWORD "fokc ckxb sgap agib"
#define RECIPIENT_EMAIL "irreligious86@gmail.com"

WebServer server(80);
SMTPSession smtp;

struct Device {
  String ip;
  String mac;
};

const int MAX_DEVICES = 254;
Device devices[MAX_DEVICES];
int deviceCount = 0;
bool scanning = false;
bool wifiLost = false;
unsigned long scanStart = 0;
unsigned long lastBlink = 0;
bool blinkState = false;

// =====================
// RGB
// =====================
void setColor(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_BUILTIN, r, g, b);
}

void celebrateFound() {
  setColor(255, 0, 0);   delay(80);
  setColor(0, 255, 0);   delay(80);
  setColor(0, 0, 255);   delay(80);
  setColor(255, 0, 255); delay(80);
  setColor(255, 255, 0); delay(80);
  setColor(0, 0, 0);     delay(80);
}

// =====================
// MAC по IP через ARP
// =====================
String getMacByIP(const String& ip) {
  ip4_addr_t target;
  ip4addr_aton(ip.c_str(), &target);
  delay(100);

  struct eth_addr* eth = nullptr;
  const ip4_addr_t* ip_ret = nullptr;

  for (struct netif* n = netif_list; n != nullptr; n = n->next) {
    int8_t result = etharp_find_addr(n, &target, &eth, &ip_ret);
    if (result >= 0 && eth != nullptr) {
      char mac[18];
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
        eth->addr[0], eth->addr[1], eth->addr[2],
        eth->addr[3], eth->addr[4], eth->addr[5]);
      return String(mac);
    }
  }
  return "N/A";
}

// =====================
// Email отчёт
// =====================
void sendReport() {
  Serial.println("Отправляю email...");

  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = SENDER_EMAIL;
  session.login.password = SENDER_PASSWORD;
  session.login.user_domain = "";

  SMTP_Message message;
  message.sender.name = "ESP32 Scanner";
  message.sender.email = SENDER_EMAIL;
  message.subject = "ESP32 Network Scan Report";
  message.addRecipient("Alex", RECIPIENT_EMAIL);

  String body = "Сканирование завершено!\n\n";
  body += "Найдено устройств: " + String(deviceCount) + "\n";
  body += "Время: " + String((millis() - scanStart) / 1000) + " сек\n\n";
  body += "Список устройств:\n";
  body += "--------------------------------\n";

  for (int i = 0; i < deviceCount; i++) {
    body += String(i + 1) + ". IP: " + devices[i].ip;
    body += " | MAC: " + devices[i].mac + "\n";
  }

  message.text.content = body;
  message.text.charSet = "utf-8";

  if (!smtp.connect(&session)) {
    Serial.println("Ошибка SMTP");
    return;
  }
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Ошибка отправки: " + smtp.errorReason());
  } else {
    Serial.println("Email отправлен!");
  }
  smtp.closeSession();
}

// =====================
// Сканирование
// =====================
void doScan() {
  scanning = true;
  deviceCount = 0;
  scanStart = millis();

  String base = WiFi.localIP().toString();
  base = base.substring(0, base.lastIndexOf('.') + 1);
  Serial.println("Сканирую: " + base + "0/24");

  for (int i = 1; i <= 254; i++) {
    // Жёлтое моргание во время сканирования
    setColor(200, 200, 0); delay(40);
    setColor(0, 0, 0);     delay(40);

    String ip = base + String(i);
    bool alive = Ping.ping(ip.c_str(), 1);

    if (alive) {
      String mac = getMacByIP(ip);
      devices[deviceCount].ip  = ip;
      devices[deviceCount].mac = mac;
      deviceCount++;

      Serial.printf("ОНЛАЙН: %s | MAC: %s\n", ip.c_str(), mac.c_str());
      celebrateFound();
    }

    // Проверка связи каждые 20 адресов
    if (i % 20 == 0 && WiFi.status() != WL_CONNECTED) {
      wifiLost = true;
      scanning = false;
      return;
    }
  }

  Serial.printf("Готово за %lu сек. Найдено: %d\n",
    (millis() - scanStart) / 1000, deviceCount);

  scanning = false;
  setColor(0, 50, 0); // зелёный = готово
  sendReport();
}

// =====================
// Веб страница
// =====================
void sendScanPage() {
  String html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>Network Scanner</title>"
    "<style>"
    "body{font-family:monospace;padding:20px;background:#1a1a1a;color:#00ff88;}"
    "h2{color:white;}"
    "table{width:100%;border-collapse:collapse;margin-top:16px;}"
    "td,th{padding:10px;border-bottom:1px solid #333;text-align:left;}"
    "th{color:#aaa;font-size:12px;}"
    ".online{color:#2ecc71;}"
    ".mac{color:#3498db;}"
    ".warn{color:#f39c12;}"
    "a{display:inline-block;margin:10px 4px;padding:12px 24px;"
    "border-radius:8px;text-decoration:none;color:white;font-weight:bold;}"
    ".scan{background:#e67e22;}"
    "@keyframes blink{0%,100%{opacity:1}50%{opacity:0.3}}"
    ".blink{animation:blink 1s infinite;}"
    "</style>";

  if (scanning) {
    html += "<meta http-equiv='refresh' content='3'>";
  }

  html += "</head><body><h2>📡 Network Scanner</h2>";

  if (wifiLost) {
    html += "<p style='color:#e74c3c' class='blink'>⛔ Потеряна связь с роутером!</p>";
  } else if (scanning) {
    html += "<p class='warn blink'>⏳ Сканирование... найдено: " + String(deviceCount) + "</p>";
    html += "<p style='color:#555'>Обновление каждые 3 сек</p>";

    if (deviceCount > 0) {
      html += "<table><tr><th>#</th><th>IP адрес</th><th>MAC адрес</th></tr>";
      for (int i = 0; i < deviceCount; i++) {
        html += "<tr><td>" + String(i+1) + "</td>";
        html += "<td>" + devices[i].ip + "</td>";
        html += "<td class='mac'>" + devices[i].mac + "</td></tr>";
      }
      html += "</table>";
    }
  } else {
    html += "<a href='/scan' class='scan'>🔍 Начать сканирование</a>";

    if (deviceCount > 0) {
      html += "<p>Найдено: <b>" + String(deviceCount) + "</b>";
      html += " | Время: " + String((millis() - scanStart) / 1000) + " сек</p>";
      html += "<table><tr><th>#</th><th>IP адрес</th><th>MAC адрес</th><th>Статус</th></tr>";

      for (int i = 0; i < deviceCount; i++) {
        html += "<tr><td>" + String(i+1) + "</td>";
        html += "<td>" + devices[i].ip + "</td>";
        html += "<td class='mac'>" + devices[i].mac + "</td>";
        html += "<td class='online'>● ОНЛАЙН</td></tr>";
      }
      html += "</table>";
      html += "<p style='color:#555;font-size:12px'>📧 Отчёт отправлен на email</p>";
    } else {
      html += "<p style='color:#555'>Нажми кнопку для сканирования</p>";
    }
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

// =====================
// Setup
// =====================
void setup() {
  Serial.begin(115200);
  delay(3000);

  setColor(0, 0, 50);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, "");

  Serial.print("Подключаюсь");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    setColor(0, 50, 0);
    Serial.println("\nПодключено!");
    Serial.print("Открой: http://");
    Serial.println(WiFi.localIP());

    server.on("/", sendScanPage);
    server.on("/scan", []() {
      sendScanPage();
      doScan();
    });

    server.begin();
    Serial.println("Сервер запущен!");
  } else {
    setColor(50, 0, 0);
    Serial.println("\nОшибка подключения! Статус: " + String(WiFi.status()));
  }
}

// =====================
// Loop
// =====================
void loop() {
  server.handleClient();

  // Красное моргание при потере связи
  if (WiFi.status() != WL_CONNECTED && !scanning) {
    wifiLost = true;
    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      blinkState = !blinkState;
      blinkState ? setColor(50, 0, 0) : setColor(0, 0, 0);
    }
  } else if (!scanning) {
    wifiLost = false;
  }
}