#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Ping.h>
#include <ESP_Mail_Client.h>
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>

const char* ssid = "STARLINK";

#define SMTP_HOST       "smtp.gmail.com"
#define SMTP_PORT       465
#define SENDER_EMAIL    "irreligious86@gmail.com"
#define SENDER_PASSWORD "fokc ckxb sgap agib"
#define RECIPIENT_EMAIL "irreligious86@gmail.com"

WebServer server(80);
SMTPSession smtp;

// Forward declarations
void sendReport();
void sendScanPage();

// =====================
// OUI база
// =====================
struct OUI { const char* prefix; const char* vendor; };
const OUI ouiTable[] = {
  {"D8:3B:DA", "Espressif (ESP32)"},
  {"24:A1:60", "Espressif"},
  {"A4:CF:12", "Espressif"},
  {"DC:A6:32", "Raspberry Pi"},
  {"B8:27:EB", "Raspberry Pi"},
  {"E4:5F:01", "Raspberry Pi"},
  {"00:17:88", "Philips Hue"},
  {"EC:FA:BC", "Apple"},
  {"F0:18:98", "Apple"},
  {"AC:BC:32", "Apple"},
  {"60:57:18", "Apple"},
  {"00:1A:11", "Google"},
  {"54:60:09", "Google"},
  {"F4:F5:D8", "Google"},
  {"50:DC:E7", "Samsung"},
  {"8C:79:F0", "Samsung"},
  {"CC:2D:8C", "Samsung"},
  {"00:50:F2", "Microsoft"},
  {"28:16:A8", "Xiaomi"},
  {"AC:C1:EE", "Xiaomi"},
  {"FC:64:BA", "Xiaomi"},
  {"18:31:BF", "Amazon"},
  {"FC:A1:83", "Amazon"},
  {"74:75:48", "Amazon"},
  {"00:1E:C2", "TP-Link"},
  {"50:C7:BF", "TP-Link"},
  {"98:DA:C4", "TP-Link"},
  {"74:24:9F", "Starlink/SpaceX"},
  {"C8:3A:35", "Tenda"},
  {"00:0C:29", "VMware"},
  {"00:50:56", "VMware"},
  {"08:00:27", "VirtualBox"},
  {"00:1B:21", "Intel"},
  {"8C:8D:28", "Intel"},
  {"",         "Unknown"}
};

String getVendor(const String& mac) {
  String prefix = mac.substring(0, 8);
  prefix.toUpperCase();
  for (int i = 0; i < (int)(sizeof(ouiTable)/sizeof(ouiTable[0])) - 1; i++) {
    if (prefix == String(ouiTable[i].prefix)) return String(ouiTable[i].vendor);
  }
  return "Unknown";
}

// =====================
// Структура устройства
// =====================
struct Device {
  String ip;
  String mac;
  String vendor;
  long   pingMs;
  String openPorts;
  String httpBanner;   // заголовок HTTP сервера
  String osGuess;      // угаданная ОС по TTL
  String netbiosName;  // имя Windows машины
};

const int MAX_DEVICES = 30;
Device devices[MAX_DEVICES];
int deviceCount = 0;
bool scanning = false;
unsigned long scanStart = 0;
unsigned long lastBlink = 0;
bool blinkState = false;
int currentScanIP = 0;

// =====================
// RGB
// =====================
void setColor(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_BUILTIN, r, g, b);
}

void celebrateFound() {
  for (int i = 0; i < 3; i++) {
    setColor(255, 0,   0);   delay(60);
    setColor(0,   255, 0);   delay(60);
    setColor(0,   0,   255); delay(60);
    setColor(255, 255, 0);   delay(60);
  }
  setColor(200, 200, 0);
}

// =====================
// MAC через ARP
// =====================
String getMacByIP(const String& ip) {
  ip4_addr_t target;
  ip4addr_aton(ip.c_str(), &target);
  delay(150);
  struct eth_addr* eth = nullptr;
  const ip4_addr_t* ip_ret = nullptr;
  for (struct netif* n = netif_list; n != nullptr; n = n->next) {
    if (etharp_find_addr(n, &target, &eth, &ip_ret) >= 0 && eth != nullptr) {
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
// HTTP Banner Grabbing
// Читаем заголовок Server: с веб-сервера
// =====================
String getHttpBanner(const String& ip) {
  WiFiClient client;
  client.setTimeout(1000);

  if (!client.connect(ip.c_str(), 80)) return "";

  client.print("HEAD / HTTP/1.0\r\nHost: " + ip + "\r\n\r\n");

  unsigned long t = millis();
  while (client.available() == 0 && millis() - t < 1000) delay(10);

  String banner = "";
  while (client.available()) {
    String line = client.readStringUntil('\n');
    line.trim();

    // Ищем заголовок Server:
    if (line.startsWith("Server:")) {
      banner = line.substring(8);
      banner.trim();
      break;
    }
    // Или первую строку HTTP ответа
    if (line.startsWith("HTTP/") && banner.length() == 0) {
      banner = line;
    }
  }
  client.stop();
  return banner.length() > 0 ? banner : "HTTP (no banner)";
}

// =====================
// TTL Fingerprint
// Угадываем ОС по TTL пинга
// =====================
String guessTTL(const String& ip) {
  // ESP32Ping не возвращает TTL напрямую
  // Используем косвенный метод — время отклика + логику
  // Реальный TTL получим через raw socket (упрощённо)
  
  // Подключаемся и смотрим на поведение TCP стека
  WiFiClient client;
  client.setTimeout(500);
  
  // Пробуем порт 80 и 22 — по поведению угадываем ОС
  bool http = client.connect(ip.c_str(), 80);
  if (http) client.stop();
  bool ssh = client.connect(ip.c_str(), 22);
  if (ssh) client.stop();
  bool smb = client.connect(ip.c_str(), 445);
  if (smb) client.stop();
  bool rdp = client.connect(ip.c_str(), 3389);
  if (rdp) client.stop();

  if (smb || rdp)  return "Windows";
  if (ssh && http) return "Linux/Server";
  if (ssh)         return "Linux/Unix";
  if (http)        return "IoT/Router";
  return "Unknown OS";
}

// =====================
// NetBIOS Name Query
// Запрашиваем имя Windows машины
// =====================
String getNetbiosName(const String& ip) {
  WiFiUDP udp;
  udp.begin(0);

  // NetBIOS Name Service запрос
  uint8_t query[] = {
    0x82, 0x28,       // Transaction ID
    0x00, 0x00,       // Flags: query
    0x00, 0x01,       // Questions: 1
    0x00, 0x00,       // Answer RRs: 0
    0x00, 0x00,       // Authority RRs: 0
    0x00, 0x00,       // Additional RRs: 0
    // Name: * (wildcard NBSTAT query)
    0x20,
    0x43, 0x4B, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x00,
    0x00, 0x21,       // Type: NBSTAT
    0x00, 0x01        // Class: IN
  };

  IPAddress target;
  target.fromString(ip);
  udp.beginPacket(target, 137);
  udp.write(query, sizeof(query));
  udp.endPacket();

  unsigned long t = millis();
  while (millis() - t < 500) {
    int len = udp.parsePacket();
    if (len > 0) {
      uint8_t buf[256];
      udp.read(buf, min(len, 256));

      // Имя начинается с байта 57 в ответе NBSTAT
      if (len > 57) {
        int nameCount = buf[56];
        if (nameCount > 0 && len > 57 + 15) {
          char name[16] = {0};
          memcpy(name, buf + 57, 15);
          // Убираем пробелы
          String n = String(name);
          n.trim();
          if (n.length() > 0) {
            udp.stop();
            return n;
          }
        }
      }
      break;
    }
    delay(10);
  }
  udp.stop();
  return "";
}

// =====================
// Сканирование портов
// =====================
String scanPorts(const String& ip) {
  const int ports[] = {21,22,23,25,53,80,443,554,1883,3000,3306,5000,8080,8443,445,3389};
  const char* names[] = {"FTP","SSH","Telnet","SMTP","DNS","HTTP","HTTPS","RTSP","MQTT","Node","MySQL","Flask","HTTP-alt","HTTPS-alt","SMB","RDP"};
  const int count = sizeof(ports) / sizeof(ports[0]);

  String result = "";
  WiFiClient client;
  client.setTimeout(300);

  for (int i = 0; i < count; i++) {
    if (client.connect(ip.c_str(), ports[i])) {
      if (result.length() > 0) result += " ";
      result += String(ports[i]) + "(" + names[i] + ")";
      client.stop();
    }
  }
  return result.length() > 0 ? result : "none";
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
  message.subject = "ESP32 Advanced Network Scan Report";
  message.addRecipient("Alex", RECIPIENT_EMAIL);

  String body = "=== ESP32 Advanced Network Scanner ===\n\n";
  body += "Найдено: " + String(deviceCount) + " устройств\n";
  body += "Время: " + String((millis() - scanStart) / 1000) + " сек\n";
  body += "Сеть: " + WiFi.localIP().toString() + "\n\n";

  for (int i = 0; i < deviceCount; i++) {
    body += "----------------------------------------\n";
    body += String(i+1) + ". IP:      " + devices[i].ip + "\n";
    body += "   MAC:    " + devices[i].mac + "\n";
    body += "   Vendor: " + devices[i].vendor + "\n";
    body += "   OS:     " + devices[i].osGuess + "\n";
    body += "   Ping:   " + String(devices[i].pingMs) + " ms\n";
    body += "   Ports:  " + devices[i].openPorts + "\n";
    if (devices[i].httpBanner.length() > 0)
      body += "   HTTP:   " + devices[i].httpBanner + "\n";
    if (devices[i].netbiosName.length() > 0)
      body += "   Name:   " + devices[i].netbiosName + "\n";
    body += "\n";
  }

  message.text.content = body;
  message.text.charSet = "utf-8";

  if (!smtp.connect(&session)) { Serial.println("Ошибка SMTP"); return; }
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Ошибка: " + smtp.errorReason());
  else
    Serial.println("Email отправлен!");
  smtp.closeSession();
}

// =====================
// Сканирование сети
// =====================
void doScan() {
  scanning = true;
  deviceCount = 0;
  scanStart = millis();
  currentScanIP = 0;

  String base = WiFi.localIP().toString();
  base = base.substring(0, base.lastIndexOf('.') + 1);
  Serial.println("Сканирую: " + base + "0/24");

  for (int i = 1; i <= 254; i++) {
    currentScanIP = i;
    setColor(200, 200, 0); delay(30);
    setColor(0,   0,   0); delay(30);

    String ip = base + String(i);
    bool alive = Ping.ping(ip.c_str(), 1);

    if (alive && deviceCount < MAX_DEVICES) {
      long ms = Ping.averageTime();
      Serial.printf("ОНЛАЙН: %s | %ldms\n", ip.c_str(), ms);

      String mac     = getMacByIP(ip);
      String vendor  = getVendor(mac);
      String ports   = scanPorts(ip);
      String osGuess = guessTTL(ip);
      String banner  = "";
      String nbName  = "";

      if (ports.indexOf("HTTP") >= 0) {
        banner = getHttpBanner(ip);
        Serial.println("  HTTP Banner: " + banner);
      }

      nbName = getNetbiosName(ip);
      if (nbName.length() > 0)
        Serial.println("  NetBIOS: " + nbName);

      devices[deviceCount] = {ip, mac, vendor, ms, ports, banner, osGuess, nbName};
      deviceCount++;

      Serial.printf("  MAC: %s | Vendor: %s | OS: %s\n",
        mac.c_str(), vendor.c_str(), osGuess.c_str());

      celebrateFound();
    }

    if (i % 20 == 0 && WiFi.status() != WL_CONNECTED) {
      scanning = false;
      return;
    }
  }

  Serial.printf("\nГотово за %lu сек. Найдено: %d\n",
    (millis() - scanStart) / 1000, deviceCount);

  scanning = false;
  setColor(0, 50, 0);
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
    "<title>ESP32 Advanced Scanner</title>"
    "<style>"
    "body{font-family:monospace;padding:20px;background:#0d0d0d;color:#00ff88;margin:0;}"
    "h2{color:white;border-bottom:1px solid #222;padding-bottom:10px;}"
    ".info{color:#555;font-size:12px;margin-bottom:15px;}"
    "table{width:100%;border-collapse:collapse;margin-top:16px;font-size:12px;}"
    "td,th{padding:8px 10px;border-bottom:1px solid #1a1a1a;text-align:left;vertical-align:top;}"
    "th{color:#444;font-size:10px;text-transform:uppercase;letter-spacing:1px;}"
    "tr:hover td{background:#111;}"
    ".ip{color:#00ff88;font-weight:bold;}"
    ".mac{color:#3498db;}"
    ".vendor{color:#9b59b6;}"
    ".ping{color:#f39c12;}"
    ".ports{color:#e74c3c;font-size:11px;word-break:break-all;}"
    ".os{color:#1abc9c;}"
    ".banner{color:#e67e22;font-size:11px;}"
    ".nbname{color:#f1c40f;font-weight:bold;}"
    ".none{color:#2a2a2a;}"
    "a{display:inline-block;margin:10px 4px;padding:12px 28px;"
    "border-radius:6px;text-decoration:none;color:white;font-weight:bold;}"
    ".scan{background:#e67e22;}"
    "@keyframes blink{0%,100%{opacity:1}50%{opacity:0.2}}"
    ".blink{animation:blink 0.8s infinite;}"
    ".progress{background:#111;border-radius:4px;height:6px;margin:10px 0;}"
    ".bar{background:#e67e22;height:6px;border-radius:4px;}"
    "</style>";

  if (scanning) html += "<meta http-equiv='refresh' content='3'>";

  html += "</head><body><h2>📡 ESP32 Advanced Network Scanner</h2>";
  html += "<div class='info'>";
  html += "ESP32: " + WiFi.localIP().toString();
  html += " | SSID: " + String(WiFi.SSID());
  html += " | RSSI: " + String(WiFi.RSSI()) + " dBm";
  html += " | Uptime: " + String(millis()/1000) + "s</div>";

  if (scanning) {
    int pct = (currentScanIP * 100) / 254;
    html += "<p class='blink' style='color:#f39c12'>⏳ Сканирование... " + String(currentScanIP) + "/254 (" + String(pct) + "%)</p>";
    html += "<div class='progress'><div class='bar' style='width:" + String(pct) + "%'></div></div>";
    html += "<p style='color:#555'>Найдено: <b style='color:#2ecc71'>" + String(deviceCount) + "</b></p>";
  } else {
    html += "<a href='/scan' class='scan'>🔍 Сканировать сеть</a>";
  }

  if (deviceCount > 0) {
    if (!scanning) {
      html += "<p style='color:#555'>Найдено: <b style='color:#2ecc71'>" + String(deviceCount) + "</b>";
      html += " | Время: " + String((millis()-scanStart)/1000) + " сек</p>";
    }

    html += "<table><tr>"
            "<th>#</th><th>IP</th><th>MAC</th><th>Vendor</th>"
            "<th>OS</th><th>Ping</th><th>Open Ports</th><th>HTTP</th><th>Name</th>"
            "</tr>";

    for (int i = 0; i < deviceCount; i++) {
      html += "<tr>";
      html += "<td>" + String(i+1) + "</td>";
      html += "<td class='ip'>" + devices[i].ip + "</td>";
      html += "<td class='mac'>" + devices[i].mac + "</td>";
      html += "<td class='vendor'>" + devices[i].vendor + "</td>";
      html += "<td class='os'>" + devices[i].osGuess + "</td>";
      html += "<td class='ping'>" + String(devices[i].pingMs) + "ms</td>";

      String p = devices[i].openPorts;
      html += "<td class='" + String(p=="none"?"none":"ports") + "'>" + p + "</td>";

      String b = devices[i].httpBanner;
      html += "<td class='banner'>" + (b.length()>0 ? b : "-") + "</td>";

      String n = devices[i].netbiosName;
      html += "<td class='nbname'>" + (n.length()>0 ? n : "-") + "</td>";
      html += "</tr>";
    }
    html += "</table>";

    if (!scanning)
      html += "<p style='color:#444;font-size:11px;margin-top:20px'>📧 Отчёт → " + String(RECIPIENT_EMAIL) + "</p>";
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
    delay(500); Serial.print("."); attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    setColor(0, 50, 0);
    Serial.println("\nПодключено!");
    Serial.print("Открой: http://");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("esp32"))
      Serial.println("mDNS: http://esp32.local");

    server.on("/", sendScanPage);
    server.on("/scan", []() { sendScanPage(); doScan(); });
    server.begin();
    Serial.println("Сервер запущен!");
  } else {
    setColor(50, 0, 0);
    Serial.println("\nОшибка! Статус: " + String(WiFi.status()));
  }
}

// =====================
// Loop
// =====================
void loop() {
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED && !scanning) {
    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      blinkState = !blinkState;
      blinkState ? setColor(50, 0, 0) : setColor(0, 0, 0);
    }
  }
}