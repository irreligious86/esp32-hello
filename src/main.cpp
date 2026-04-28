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

void sendReport();
void sendScanPage();

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

struct Device {
  String ip;
  String mac;
  String vendor;
  long   pingMs;
  String openPorts;
  String httpBanner;
  String osGuess;
  String netbiosName;
  String sshBanner;
};

const int MAX_DEVICES = 30;
Device devices[MAX_DEVICES];
int deviceCount   = 0;
bool scanning     = false;
bool scanDone     = false;
unsigned long scanStart = 0;
unsigned long lastBlink = 0;
bool blinkState   = false;
int currentScanIP = 0;  // текущий IP в цикле (1..254)
String scanBase   = ""; // например "192.168.1."

// =====================
// RGB
// =====================
void setColor(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_BUILTIN, r, g, b);
}

void celebrateFound() {
  for (int i = 0; i < 2; i++) {
    setColor(255, 0,   0);   delay(50);
    setColor(0,   255, 0);   delay(50);
    setColor(0,   0,   255); delay(50);
    setColor(255, 255, 0);   delay(50);
  }
  setColor(200, 200, 0);
}

// =====================
// MAC через ARP
// =====================
String getMacByIP(const String& ip) {
  ip4_addr_t target;
  ip4addr_aton(ip.c_str(), &target);
  delay(100);
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
// HTTP Banner
// =====================
String getHttpBanner(const String& ip) {
  WiFiClient client;
  client.setTimeout(800);
  if (!client.connect(ip.c_str(), 80)) return "";
  client.print("HEAD / HTTP/1.0\r\nHost: " + ip + "\r\n\r\n");
  unsigned long t = millis();
  while (!client.available() && millis() - t < 800) delay(10);
  String banner = "";
  while (client.available()) {
    String line = client.readStringUntil('\n'); line.trim();
    if (line.startsWith("Server:")) { banner = line.substring(8); banner.trim(); break; }
    if (line.startsWith("HTTP/") && banner.length() == 0) banner = line;
  }
  client.stop();
  return banner.length() > 0 ? banner : "HTTP OK";
}

// =====================
// SSH Banner
// =====================
String getSshBanner(const String& ip) {
  WiFiClient client;
  client.setTimeout(1000);
  if (!client.connect(ip.c_str(), 22)) return "";
  unsigned long t = millis();
  String banner = "";
  while (millis() - t < 1000) {
    if (client.available()) { banner = client.readStringUntil('\n'); banner.trim(); break; }
    delay(10);
  }
  client.stop();
  return banner.startsWith("SSH") ? banner : "";
}

// =====================
// OS Fingerprint
// =====================
String guessTTL(const String& ip) {
  WiFiClient client; client.setTimeout(400);
  bool http = client.connect(ip.c_str(), 80);  if (http) client.stop();
  bool ssh  = client.connect(ip.c_str(), 22);  if (ssh)  client.stop();
  bool smb  = client.connect(ip.c_str(), 445); if (smb)  client.stop();
  bool rdp  = client.connect(ip.c_str(), 3389);if (rdp)  client.stop();
  if (smb && rdp)  return "Windows (RDP+SMB)";
  if (smb)         return "Windows";
  if (rdp)         return "Windows (RDP)";
  if (ssh && http) return "Linux/Server";
  if (ssh)         return "Linux/Unix";
  if (http)        return "IoT/Router";
  return "Unknown OS";
}

// =====================
// NetBIOS
// =====================
String getNetbiosName(const String& ip) {
  WiFiUDP udp; udp.begin(0);
  uint8_t query[] = {
    0x82,0x28,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
    0x20,
    0x43,0x4B,0x41,0x41,0x41,0x41,0x41,0x41,
    0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,
    0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,
    0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,
    0x00,0x00,0x21,0x00,0x01
  };
  IPAddress target; target.fromString(ip);
  udp.beginPacket(target, 137);
  udp.write(query, sizeof(query));
  udp.endPacket();
  unsigned long t = millis();
  while (millis() - t < 400) {
    int len = udp.parsePacket();
    if (len > 0) {
      uint8_t buf[256];
      udp.read(buf, min(len, 256));
      if (len > 57 && buf[56] > 0 && len > 72) {
        char name[16] = {0};
        memcpy(name, buf + 57, 15);
        String n = String(name); n.trim();
        if (n.length() > 0) { udp.stop(); return n; }
      }
      break;
    }
    delay(10);
  }
  udp.stop();
  return "";
}

// =====================
// Port Scan
// =====================
String scanPorts(const String& ip) {
  const int ports[]   = {21,22,23,25,53,80,443,554,1883,3000,3306,5000,8080,8443,445,3389};
  const char* names[] = {"FTP","SSH","Telnet","SMTP","DNS","HTTP","HTTPS","RTSP","MQTT","Node","MySQL","Flask","HTTP-alt","HTTPS-alt","SMB","RDP"};
  const int count = sizeof(ports)/sizeof(ports[0]);
  String result = "";
  WiFiClient client; client.setTimeout(250);
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
  message.sender.name  = "ESP32_Advansed_NetScaner";
  message.sender.email = SENDER_EMAIL;
  message.subject = "ESP32 Network Scan — " + String(deviceCount) + " devices found";
  message.addRecipient("Alex", RECIPIENT_EMAIL);

  String html =
    "<html><body style='font-family:monospace;background:#0d0d0d;color:#00ff88;padding:20px;'>"
    "<h2 style='color:white;border-bottom:1px solid #333;padding-bottom:10px'>"
    "📡 ESP32 Network Scanner Report</h2>"
    "<p style='color:#555'>ESP32: " + WiFi.localIP().toString() +
    " | SSID: " + String(WiFi.SSID()) +
    " | RSSI: " + String(WiFi.RSSI()) + " dBm" +
    " | Scan time: " + String((millis()-scanStart)/1000) + " sec" +
    " | Devices: <b style='color:#2ecc71'>" + String(deviceCount) + "</b></p>"
    "<table style='width:100%;border-collapse:collapse;font-size:12px'>"
    "<tr style='color:#444;font-size:10px'>"
    "<th style='padding:8px;text-align:left'>#</th>"
    "<th style='padding:8px;text-align:left'>IP</th>"
    "<th style='padding:8px;text-align:left'>MAC</th>"
    "<th style='padding:8px;text-align:left'>Vendor</th>"
    "<th style='padding:8px;text-align:left'>OS</th>"
    "<th style='padding:8px;text-align:left'>Ping</th>"
    "<th style='padding:8px;text-align:left'>Ports</th>"
    "<th style='padding:8px;text-align:left'>HTTP</th>"
    "<th style='padding:8px;text-align:left'>SSH</th>"
    "<th style='padding:8px;text-align:left'>Name</th>"
    "</tr>";

  for (int i = 0; i < deviceCount; i++) {
    String bg = i % 2 == 0 ? "#111" : "#0d0d0d";
    html += "<tr style='background:" + bg + ";border-bottom:1px solid #1a1a1a'>";
    html += "<td style='padding:8px;color:#555'>" + String(i+1) + "</td>";
    html += "<td style='padding:8px;color:#00ff88;font-weight:bold'>" + devices[i].ip + "</td>";
    html += "<td style='padding:8px;color:#3498db'>" + devices[i].mac + "</td>";
    html += "<td style='padding:8px;color:#9b59b6'>" + devices[i].vendor + "</td>";
    html += "<td style='padding:8px;color:#1abc9c'>" + devices[i].osGuess + "</td>";
    html += "<td style='padding:8px;color:#f39c12'>" + String(devices[i].pingMs) + "ms</td>";
    html += "<td style='padding:8px;color:#e74c3c;font-size:11px'>" + devices[i].openPorts + "</td>";
    html += "<td style='padding:8px;color:#e67e22'>" + (devices[i].httpBanner.length()>0 ? devices[i].httpBanner : "-") + "</td>";
    html += "<td style='padding:8px;color:#2ecc71'>" + (devices[i].sshBanner.length()>0 ? devices[i].sshBanner : "-") + "</td>";
    html += "<td style='padding:8px;color:#f1c40f;font-weight:bold'>" + (devices[i].netbiosName.length()>0 ? devices[i].netbiosName : "-") + "</td>";
    html += "</tr>";
  }

  html += "</table>"
          "<p style='color:#333;font-size:10px;margin-top:20px'>"
          "Sent by ESP32-S3 DevKitC-1</p>"
          "</body></html>";

  message.html.content = html;
  message.html.charSet = "utf-8";

  if (!smtp.connect(&session)) { Serial.println("Ошибка SMTP"); return; }
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Ошибка: " + smtp.errorReason());
  else
    Serial.println("HTML Email отправлен!");
  smtp.closeSession();
}

// =====================
// Один шаг сканирования
// Вызывается из loop() — не блокирует
// =====================
void scanStep() {
  if (!scanning) return;

  // Двойное жёлтое моргание
  setColor(200, 200, 0); delay(60);
  setColor(0,   0,   0); delay(60);
  setColor(200, 200, 0); delay(60);
  setColor(0,   0,   0); delay(60);

  String ip = scanBase + String(currentScanIP);
  bool alive = Ping.ping(ip.c_str(), 1);

  if (alive && deviceCount < MAX_DEVICES) {
    long ms = Ping.averageTime();
    Serial.printf("ОНЛАЙН: %s | %ldms\n", ip.c_str(), ms);

    String mac    = getMacByIP(ip);
    String vendor = getVendor(mac);
    String ports  = scanPorts(ip);
    String os     = guessTTL(ip);
    String banner = "";
    String ssh    = "";
    String nb     = "";

    if (ports.indexOf("HTTP") >= 0) {
      banner = getHttpBanner(ip);
      Serial.println("  HTTP: " + banner);
    }
    if (ports.indexOf("SSH") >= 0) {
      ssh = getSshBanner(ip);
      Serial.println("  SSH: " + ssh);
    }
    nb = getNetbiosName(ip);
    if (nb.length() > 0) Serial.println("  NetBIOS: " + nb);

    devices[deviceCount] = {ip, mac, vendor, ms, ports, banner, os, nb, ssh};
    deviceCount++;

    Serial.printf("  MAC: %s | %s | %s\n",
      mac.c_str(), vendor.c_str(), os.c_str());

    celebrateFound();
  }

  currentScanIP++;

  // Проверка конца цикла
  if (currentScanIP > 254) {
    Serial.printf("\nГотово за %lu сек. Найдено: %d\n",
      (millis()-scanStart)/1000, deviceCount);

    scanning = false;
    scanDone = true;
    setColor(0, 80, 0); // зелёный — готово
    sendReport();
  }

  // Потеря связи
  if (WiFi.status() != WL_CONNECTED) {
    scanning = false;
    scanDone = false;
    setColor(50, 0, 0);
  }
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
    "table{width:100%;border-collapse:collapse;margin-top:16px;font-size:11px;}"
    "td,th{padding:7px 8px;border-bottom:1px solid #1a1a1a;text-align:left;vertical-align:top;}"
    "th{color:#444;font-size:10px;text-transform:uppercase;letter-spacing:1px;}"
    "tr:hover td{background:#111;}"
    ".ip{color:#00ff88;font-weight:bold;}"
    ".mac{color:#3498db;}"
    ".vendor{color:#9b59b6;}"
    ".ping{color:#f39c12;}"
    ".ports{color:#e74c3c;word-break:break-all;}"
    ".os{color:#1abc9c;}"
    ".banner{color:#e67e22;}"
    ".ssh{color:#2ecc71;}"
    ".nbname{color:#f1c40f;font-weight:bold;}"
    ".none{color:#2a2a2a;}"
    "a{display:inline-block;margin:10px 4px;padding:12px 28px;"
    "border-radius:6px;text-decoration:none;color:white;font-weight:bold;}"
    ".scan{background:#e67e22;}"
    "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}"
    "@keyframes ping{0%{transform:scale(1);opacity:1}100%{transform:scale(2.5);opacity:0}}"
    "@keyframes blink{0%,100%{opacity:1}50%{opacity:0.2}}"
    "@keyframes glow{0%,100%{box-shadow:0 0 10px #00ff88}50%{box-shadow:0 0 30px #00ff88,0 0 60px #00ff8844}}"
    ".progress{background:#111;border-radius:4px;height:6px;margin:10px 0;}"
    ".bar{background:#e67e22;height:6px;border-radius:4px;transition:width 0.5s;}"
    ".radar-wrap{position:relative;width:120px;height:120px;margin:20px auto;}"
    ".radar-circle{width:120px;height:120px;border-radius:50%;"
    "border:1px solid #00ff8844;position:absolute;}"
    ".radar-circle2{width:80px;height:80px;border-radius:50%;"
    "border:1px solid #00ff8833;position:absolute;top:20px;left:20px;}"
    ".radar-circle3{width:40px;height:40px;border-radius:50%;"
    "border:1px solid #00ff8855;position:absolute;top:40px;left:40px;}"
    ".radar-sweep{width:60px;height:2px;"
    "background:linear-gradient(90deg,transparent,#00ff88);"
    "position:absolute;top:59px;left:60px;transform-origin:left center;"
    "animation:spin 2s linear infinite;}"
    ".radar-dot{width:6px;height:6px;border-radius:50%;background:#00ff88;"
    "position:absolute;top:57px;left:57px;}"
    ".ping-ring{width:120px;height:120px;border-radius:50%;"
    "border:2px solid #00ff8866;position:absolute;"
    "animation:ping 2s ease-out infinite;}"
    ".counter{font-size:48px;color:#00ff88;text-align:center;font-weight:bold;margin:5px 0;}"
    ".status-text{text-align:center;color:#555;font-size:12px;margin:0;}"
    ".elapsed{text-align:center;color:#333;font-size:11px;margin-top:4px;}"
    // Финальный экран
    ".done-wrap{text-align:center;padding:20px 0;}"
    ".done-circle{width:100px;height:100px;border-radius:50%;"
    "border:3px solid #00ff88;margin:0 auto 15px;"
    "animation:glow 1.5s ease-in-out infinite;"
    "display:flex;align-items:center;justify-content:center;"
    "font-size:40px;}"
    ".done-title{color:white;font-size:20px;font-weight:bold;margin:10px 0;}"
    ".done-sub{color:#555;font-size:12px;}"
    "</style>";

  // Автообновление только во время сканирования
  if (scanning) html += "<meta http-equiv='refresh' content='5'>";

  html += "</head><body><h2>📡 ESP32 Advanced Network Scanner</h2>";
  html += "<div class='info'>";
  html += "ESP32: " + WiFi.localIP().toString();
  html += " | SSID: " + String(WiFi.SSID());
  html += " | RSSI: " + String(WiFi.RSSI()) + " dBm";
  html += " | Uptime: " + String(millis()/1000) + "s</div>";

  // ---- Состояние: IDLE ----
  if (!scanning && !scanDone) {
    html += "<a href='/startscan' class='scan'>🔍 Сканировать сеть</a>";
  }

  // ---- Состояние: SCANNING ----
  if (scanning) {
    int pct = (currentScanIP * 100) / 254;
    unsigned long elapsed = (millis() - scanStart) / 1000;

    html += "<div class='radar-wrap'>"
            "<div class='ping-ring'></div>"
            "<div class='radar-circle'></div>"
            "<div class='radar-circle2'></div>"
            "<div class='radar-circle3'></div>"
            "<div class='radar-sweep'></div>"
            "<div class='radar-dot'></div>"
            "</div>";

    html += "<div class='counter'>" + String(deviceCount) + "</div>";
    html += "<p class='status-text'>устройств обнаружено</p>";
    html += "<p class='elapsed'>сканирую " + String(currentScanIP) +
            "/254 · " + String(pct) + "% · " + String(elapsed) + "с</p>";
    html += "<div class='progress'><div class='bar' style='width:" +
            String(pct) + "%'></div></div>";
    html += "<p style='color:#333;font-size:11px;text-align:center;margin-top:6px'>"
            "страница обновляется каждые 5 сек</p>";
  }

  // ---- Состояние: DONE ----
  if (scanDone && !scanning) {
    unsigned long elapsed = (millis() - scanStart) / 1000;
    html += "<div class='done-wrap'>"
            "<div class='done-circle'>✓</div>"
            "<div class='done-title'>Сканирование завершено</div>"
            "<div class='done-sub'>Найдено: " + String(deviceCount) +
            " устройств · Время: " + String(elapsed) + " сек</div>"
            "<div class='done-sub' style='margin-top:6px'>📧 Отчёт отправлен на " +
            String(RECIPIENT_EMAIL) + "</div>"
            "</div>";
    html += "<a href='/startscan' class='scan' style='margin-top:10px'>"
            "🔍 Сканировать снова</a>";
  }

  // ---- Таблица результатов ----
  if (deviceCount > 0) {
    html += "<table style='margin-top:20px'><tr>"
            "<th>#</th><th>IP</th><th>MAC</th><th>Vendor</th>"
            "<th>OS</th><th>Ping</th><th>Ports</th>"
            "<th>HTTP</th><th>SSH</th><th>Name</th>"
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
      html += "<td class='banner'>" +
              (devices[i].httpBanner.length()>0 ? devices[i].httpBanner : "-") + "</td>";
      html += "<td class='ssh'>" +
              (devices[i].sshBanner.length()>0 ? devices[i].sshBanner : "-") + "</td>";
      html += "<td class='nbname'>" +
              (devices[i].netbiosName.length()>0 ? devices[i].netbiosName : "-") + "</td>";
      html += "</tr>";
    }
    html += "</table>";
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

    // Кнопка запуска сканирования
    server.on("/startscan", []() {
      if (!scanning) {
        deviceCount   = 0;
        scanDone      = false;
        scanning      = true;
        currentScanIP = 1;
        scanStart     = millis();

        String base = WiFi.localIP().toString();
        scanBase = base.substring(0, base.lastIndexOf('.') + 1);

        Serial.println("Сканирую: " + scanBase + "0/24");
        setColor(200, 200, 0);
      }
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
    });

    server.begin();
    Serial.println("Сервер запущен!");
  } else {
    setColor(50, 0, 0);
    Serial.println("\nОшибка! Статус: " + String(WiFi.status()));
  }
}

// =====================
// Loop — неблокирующий
// =====================
void loop() {
  server.handleClient();

  // Один шаг сканирования за итерацию
  if (scanning) {
    scanStep();
    return; // не идём дальше пока сканируем
  }

  // Красное моргание при потере связи
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      blinkState = !blinkState;
      blinkState ? setColor(50, 0, 0) : setColor(0, 0, 0);
    }
  }
}