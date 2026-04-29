#include "scanner.h"
#include "config.h"
#include <ESP32Ping.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include "lwip/etharp.h"
#include "lwip/netif.h"

// =====================
// Глобальное состояние
// =====================
Device devices[MAX_DEVICES];
int    deviceCount   = 0;
bool   scanning      = false;
bool   scanDone      = false;
unsigned long scanStart = 0;
int    currentScanIP = 0;
static String scanBase = "";

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

static String getVendor(const String& mac) {
  String prefix = mac.substring(0, 8);
  prefix.toUpperCase();
  for (int i = 0; i < (int)(sizeof(ouiTable)/sizeof(ouiTable[0])) - 1; i++) {
    if (prefix == String(ouiTable[i].prefix)) return String(ouiTable[i].vendor);
  }
  return "Unknown";
}

static String getMacByIP(const String& ip) {
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

static String getHttpBanner(const String& ip) {
  WiFiClient client; client.setTimeout(800);
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

static String getSshBanner(const String& ip) {
  WiFiClient client; client.setTimeout(1000);
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

static String guessTTL(const String& ip) {
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

static String getNetbiosName(const String& ip) {
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

static String scanPorts(const String& ip) {
  const int ports[]   = {21,22,23,25,53,80,443,554,1883,3000,3306,5000,8080,8443,445,3389};
  const char* names[] = {"FTP","SSH","Telnet","SMTP","DNS","HTTP","HTTPS","RTSP",
                         "MQTT","Node","MySQL","Flask","HTTP-alt","HTTPS-alt","SMB","RDP"};
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

void startScan() {
  deviceCount   = 0;
  scanDone      = false;
  scanning      = true;
  currentScanIP = 1;
  scanStart     = millis();
  String base   = WiFi.localIP().toString();
  scanBase      = base.substring(0, base.lastIndexOf('.') + 1);
  Serial.println("Сканирую: " + scanBase + "0/24");
}

void scanStep() {
  if (!scanning) return;

  neopixelWrite(RGB_BUILTIN, 200, 200, 0); delay(60);
  neopixelWrite(RGB_BUILTIN, 0,   0,   0); delay(60);

  String ip = scanBase + String(currentScanIP);
  bool alive = Ping.ping(ip.c_str(), SCAN_PING_COUNT);

  if (alive && deviceCount < MAX_DEVICES) {
    long ms = Ping.averageTime();
    Serial.printf("ОНЛАЙН: %s | %ldms\n", ip.c_str(), ms);

    String mac    = getMacByIP(ip);
    String vendor = getVendor(mac);
    String ports  = scanPorts(ip);
    String os     = guessTTL(ip);
    String banner = ports.indexOf("HTTP") >= 0 ? getHttpBanner(ip) : "";
    String ssh    = ports.indexOf("SSH")  >= 0 ? getSshBanner(ip)  : "";
    String nb     = getNetbiosName(ip);

    if (banner.length() > 0) Serial.println("  HTTP: " + banner);
    if (ssh.length() > 0)    Serial.println("  SSH: " + ssh);
    if (nb.length() > 0)     Serial.println("  NetBIOS: " + nb);

    devices[deviceCount++] = {ip, mac, vendor, ms, ports, banner, os, nb, ssh};

    // Celebrate
    for (int i = 0; i < 2; i++) {
      neopixelWrite(RGB_BUILTIN, 255, 0,   0);   delay(50);
      neopixelWrite(RGB_BUILTIN, 0,   255, 0);   delay(50);
      neopixelWrite(RGB_BUILTIN, 0,   0,   255); delay(50);
      neopixelWrite(RGB_BUILTIN, 255, 255, 0);   delay(50);
    }
    neopixelWrite(RGB_BUILTIN, 200, 200, 0);
  }

  currentScanIP++;

  if (currentScanIP > 254) {
    Serial.printf("Готово за %lu сек. Найдено: %d\n",
      (millis()-scanStart)/1000, deviceCount);
    scanning = false;
    scanDone = true;
    neopixelWrite(RGB_BUILTIN, 0, 80, 0);
  }

  if (WiFi.status() != WL_CONNECTED) {
    scanning = false;
    neopixelWrite(RGB_BUILTIN, 50, 0, 0);
  }
}