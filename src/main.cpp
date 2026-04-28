#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "config.h"
#include "scanner.h"
#include "mailer.h"
#include "webui.h"

WebServer server(80);
extern String smtpPassword;

TaskHandle_t webTaskHandle  = NULL;
TaskHandle_t scanTaskHandle = NULL;

#define BOOT_BUTTON 0

// =====================
// BLE
// =====================
BLEServer*         bleServer   = nullptr;
BLECharacteristic* bleTxChar   = nullptr;
bool               bleConnected = false;

// Отправить строку по BLE
void bleSend(const String& msg) {
  if (bleConnected && bleTxChar) {
    bleTxChar->setValue(msg.c_str());
    bleTxChar->notify();
  }
}

// Обработчик подключения/отключения BLE
class BLEConnectionHandler : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    bleConnected = true;
    Serial.println("BLE: клиент подключился");
    bleSend("ESP32-Scanner ready. Commands: scan / status / ip / results");
  }
  void onDisconnect(BLEServer* s) override {
    bleConnected = false;
    Serial.println("BLE: клиент отключился");
    // Перезапускаем рекламу чтобы можно было подключиться снова
    BLEDevice::startAdvertising();
  }
};

// Обработчик входящих команд по BLE
class BLECommandHandler : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    String cmd = c->getValue().c_str();
    cmd.trim();
    cmd.toLowerCase();

    Serial.println("BLE команда: " + cmd);

    if (cmd == "scan") {
      if (scanning) {
        bleSend("ERROR: scan already running");
      } else {
        bleSend("Starting network scan...");
        startScan();
      }

    } else if (cmd == "status") {
      String s = "WiFi: " + String(WiFi.isConnected() ? "connected" : "disconnected");
      s += " | IP: " + WiFi.localIP().toString();
      s += " | SSID: " + String(WiFi.SSID());
      s += " | RSSI: " + String(WiFi.RSSI()) + "dBm";
      s += " | Scanning: " + String(scanning ? "yes" : "no");
      bleSend(s);

    } else if (cmd == "ip") {
      bleSend("IP: " + WiFi.localIP().toString());
      bleSend("URL: http://" + WiFi.localIP().toString());
      bleSend("mDNS: http://esp32.local");

    } else if (cmd == "results") {
      if (deviceCount == 0) {
        bleSend("No devices found yet. Run scan first.");
      } else {
        bleSend("Found " + String(deviceCount) + " devices:");
        for (int i = 0; i < deviceCount; i++) {
          String line = String(i+1) + ". " + devices[i].ip;
          line += " | " + devices[i].mac;
          line += " | " + devices[i].vendor;
          line += " | " + devices[i].osGuess;
          bleSend(line);
          delay(50); // небольшая пауза между строками
        }
        bleSend("--- end of results ---");
      }

    } else if (cmd == "help") {
      bleSend("Commands:");
      bleSend("  scan    — start network scan");
      bleSend("  status  — WiFi and scan status");
      bleSend("  ip      — show IP and URLs");
      bleSend("  results — show found devices");
      bleSend("  help    — show this help");

    } else {
      bleSend("Unknown command: " + cmd);
      bleSend("Type 'help' for available commands");
    }
  }
};

// Инициализация BLE
void setupBLE() {
  BLEDevice::init(BLE_DEVICE_NAME);
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new BLEConnectionHandler());

  BLEService* bleService = bleServer->createService(BLE_SERVICE_UUID);

  // TX — ESP32 → телефон
  bleTxChar = bleService->createCharacteristic(
    BLE_CHARACTERISTIC_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  bleTxChar->addDescriptor(new BLE2902());

  // RX — телефон → ESP32
  BLECharacteristic* bleRxChar = bleService->createCharacteristic(
    BLE_CHARACTERISTIC_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  bleRxChar->setCallbacks(new BLECommandHandler());

  bleService->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("BLE запущен: " BLE_DEVICE_NAME);
}

// =====================
// FreeRTOS задачи
// =====================
void webTask(void* param) {
  for (;;) {
    server.handleClient();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void scanTask(void* param) {
  bool reportSent = false;
  for (;;) {
    if (scanning) {
      reportSent = false;
      scanStep();
    }
    if (!scanning && scanDone && !reportSent) {
      reportSent = true;
      sendReport();
      bleSend("Scan complete! Found " + String(deviceCount) + " devices. Report sent to email.");
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

// =====================
// WiFi
// =====================
void startAP() {
  Serial.println("Starting AP mode...");
  neopixelWrite(RGB_BUILTIN, 255, 100, 0);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-Setup", "12345678");

  Serial.println("Network: ESP32-Setup");
  Serial.println("Password: 12345678");
  Serial.print("Settings: http://");
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

  Serial.print("Connecting to " + ssid);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 5) {
    delay(500); Serial.print("."); attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    neopixelWrite(RGB_BUILTIN, 0, 50, 0);
    Serial.println("\nConnected!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    Serial.println("Open: http://192.168.1.200");
    Serial.println("Or:   http://esp32.local");
  } else {
    Serial.println("\nWiFi unavailable — starting AP...");
    startAP();
  }
}

// =====================
// Setup
// =====================
void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  Serial.println("Hold BOOT 3 sec for AP mode...");
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
    Serial.println("AP mode forced!");
    startAP();
  } else {
    setupWiFi(wifiSsid, wifiPass);
  }

  if (MDNS.begin("esp32"))
    Serial.println("mDNS: http://esp32.local");

  setupWebUI(server);
  server.begin();
  Serial.println("Web server started!");

  // Запускаем BLE
  setupBLE();

  xTaskCreatePinnedToCore(webTask,  "WebTask",  8192,  NULL, 1, &webTaskHandle,  0);
  xTaskCreatePinnedToCore(scanTask, "ScanTask", 16384, NULL, 1, &scanTaskHandle, 1);

  Serial.println("FreeRTOS: Web→Core0 Scan→Core1");
  Serial.println("BLE ready — connect with 'Serial Bluetooth Terminal'");
}

// =====================
// Loop
// =====================
void loop() {
  // BLE уведомление о прогрессе сканирования каждые 30 сек
  static unsigned long lastBleUpdate = 0;
  if (scanning && bleConnected && millis() - lastBleUpdate > 30000) {
    lastBleUpdate = millis();
    int pct = (currentScanIP * 100) / 254;
    bleSend("Scanning... " + String(currentScanIP) + "/254 (" +
            String(pct) + "%) found: " + String(deviceCount));
  }

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