# ESP32-S3 DevKitC-1 — Sketches & Experiments
# ESP32-S3 DevKitC-1 — Скетчи и эксперименты

A collection of working sketches for ESP32-S3 DevKitC-1 (N16R8).
Each sketch is in a separate branch with a detailed description.

Коллекция рабочих скетчей для ESP32-S3 DevKitC-1 (N16R8).
Каждый скетч в отдельной ветке с подробным описанием.

---

## 🔧 Hardware / Железо

| Parameter / Параметр | Value / Значение |
|---|---|
| Board / Плата | ESP32-S3-DevKitC-1 |
| Chip / Чип | ESP32-S3 (QFN56), revision v0.2 |
| Cores / Ядра | 2x Xtensa LX7, 240 MHz |
| RAM | 512KB SRAM + 8MB PSRAM (Embedded, AP_3v3) |
| Flash | 16MB (QD) |
| Wi-Fi | 802.11 b/g/n, 2.4GHz |
| Bluetooth | BLE 5.0 |
| USB | 2x Type-C — UART (CH343) + native USB-OTG |
| RGB LED | WS2812B, pin defined by macro RGB_BUILTIN |
| MAC | D8:3B:DA:A4:CC:08 |

---

## ⚙️ Development Environment / Окружение разработки

| Tool / Инструмент | Version / Версия |
|---|---|
| IDE | VSCode + PlatformIO |
| Platform | espressif32 @ 6.13.0 |
| Framework | Arduino |
| Toolchain | xtensa-esp32s3 @ 8.4.0 |
| OS | Windows 11 |
| USB Driver / Драйвер | CH341SER (CH343) |

---

## 📋 platformio.ini — minimal working config / рабочий минимум

```ini
[env:esp32-s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

monitor_speed = 115200
upload_speed = 921600
```

⚠️ **Do NOT add / Не добавлять** `board_build.arduino.memory_type = qio_opi`
Causes infinite crash-loop (RTC_SW_SYS_RST) / Вызывает бесконечный крэш-луп

---

## 💡 Built-in RGB LED / Встроенный RGB светодиод

```cpp
// No libraries needed — function is built into the framework
// Библиотеки не нужны — функция встроена во фреймворк
neopixelWrite(RGB_BUILTIN, R, G, B);  // 0-255 per channel / каждый канал

// Examples / Примеры
neopixelWrite(RGB_BUILTIN, 255, 0,   0);  // red / красный
neopixelWrite(RGB_BUILTIN, 0,   255, 0);  // green / зелёный
neopixelWrite(RGB_BUILTIN, 0,   0,   255);// blue / синий
neopixelWrite(RGB_BUILTIN, 255, 0,   130);// pink / розовый
neopixelWrite(RGB_BUILTIN, 0,   255, 255);// cyan / циан
neopixelWrite(RGB_BUILTIN, 255, 200, 0);  // yellow / жёлтый
neopixelWrite(RGB_BUILTIN, 0,   0,   0);  // off / выкл
```

---

## 📁 Sketches / Скетчи

| Branch / Ветка | Description / Описание | Libraries / Библиотеки |
|---|---|---|
| `01-rgb-serial` | RGB control via Serial Monitor commands: red/green/blue/pink/cyan/yellow/off / Управление RGB через Serial Monitor | — |
| `02-wifi-webserver` | Web server, RGB control from browser with styled UI / Веб-сервер, управление RGB с браузера | WebServer |
| `03-wifi-network-info` | Network info in browser — IP, MAC, RSSI, channel, uptime, auto-refresh / Информация о сети в браузере | WebServer |
| `04-network-scanner-email` | LAN device scanner — ping sweep, ARP MAC detection, RGB status indication, Gmail report / Сканер устройств — ping sweep, MAC через ARP, RGB индикация, отчёт на Gmail | ESP32Ping, ESP Mail Client |

---

## ⚠️ Known Issues / Известные грабли

| Problem / Проблема | Cause / Причина | Solution / Решение |
|---|---|---|
| Infinite RTC_SW_SYS_RST | board_build.arduino.memory_type = qio_opi | Remove from platformio.ini |
| COM port not visible / Порт не виден | Missing driver / Нет драйвера | Install CH341SER.exe |
| RGB not responding / RGB не реагирует | Hardcoded pin / Хардкод пина | Use RGB_BUILTIN macro |
| Port busy on upload / Порт занят | Serial Monitor open / Монитор открыт | Close Serial Monitor before Upload |
| Won't enter bootloader / Не входит в bootloader | — | Hold BOOT → press RST → release BOOT |
| Starlink AP Isolation | Devices can't see each other / Устройства не видят друг друга | Use ESP32 softAP mode |

---

## 🚀 Quick Start / Быстрый старт

```bash
git clone https://github.com/irreligious86/esp32-s3-devkitc1-sketches.git
git checkout 01-rgb-serial
```

Open folder in VSCode → PlatformIO pulls dependencies automatically → Upload.
Открыть в VSCode → PlatformIO подтянет зависимости → Upload.

---

## 📅 Changelog / История

| Date / Дата | Event / Событие |
|---|---|
| 27.04.2026 | Project started — RGB Serial, WiFi WebServer, Network Info, Network Scanner with Gmail / Старт проекта |   