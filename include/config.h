#pragma once

// =====================
// Версия прошивки
// =====================
#define FIRMWARE_VERSION "1.0.0"
#define DEVICE_NAME      "ESP32-S3 NetScanner"

// =====================
// EEPROM
// Сохраняем настройки между перезагрузками
// =====================
#define EEPROM_SIZE      256
#define EEPROM_WIFI_SSID   0    // 32 байта
#define EEPROM_WIFI_PASS  32    // 64 байта
#define EEPROM_SMTP_PASS  96    // 32 байта
#define EEPROM_VALID_FLAG 128   // 1 байт — 0xAB если данные валидны

// =====================
// Дефолтные значения
// Используются если EEPROM пустой
// =====================
#define DEFAULT_WIFI_SSID    "STARLINK"
#define DEFAULT_WIFI_PASS    ""
#define DEFAULT_SMTP_PASS    "fokc ckxb sgap agib"

// =====================
// Email
// =====================
#define SMTP_HOST        "smtp.gmail.com"
#define SMTP_PORT        465
#define SENDER_EMAIL     "irreligious86@gmail.com"
#define SENDER_NAME      "ESP32_Advansed_NetScaner"
#define RECIPIENT_EMAIL  "irreligious86@gmail.com"

// =====================
// Сканирование
// =====================
#define MAX_DEVICES      30
#define SCAN_PING_COUNT  1

// =====================
// Bluetooth BLE
// =====================
#define BLE_DEVICE_NAME  "ESP32-Scanner"
#define BLE_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHARACTERISTIC_RX   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHARACTERISTIC_TX   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"