# ESP32-S3 DevKitC-1 — Network Scanner & Sketches / Сетевой сканер и скетчи

A collection of working sketches for ESP32-S3 DevKitC-1 (N16R8). Each sketch is in a separate branch. The most advanced one is the network scanner with web UI, Gmail reports, and EEPROM settings.

Коллекция рабочих скетчей для ESP32-S3 DevKitC-1 (N16R8). Каждый скетч в отдельной ветке. Самый продвинутый — сетевой сканер с веб интерфейсом, отчётами на Gmail и настройками в EEPROM.

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
| RGB LED | WS2812B — `neopixelWrite(RGB_BUILTIN, R, G, B)` |
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

⚠️ NEVER add / НИКОГДА не добавлять: `board_build.arduino.memory_type = qio_opi` — causes infinite crash-loop RTC_SW_SYS_RST / вызывает бесконечный крэш-луп

---

## 💡 Built-in RGB LED / Встроенный RGB светодиод

No libraries needed — built into the framework. Библиотеки не нужны — встроено во фреймворк.

```cpp
neopixelWrite(RGB_BUILTIN, R, G, B);       // 0-255 per channel / на каждый канал

neopixelWrite(RGB_BUILTIN, 0,   0,   50);  // blue   / синий   = connecting / подключение
neopixelWrite(RGB_BUILTIN, 0,   50,  0);   // green  / зелёный = ready / готово
neopixelWrite(RGB_BUILTIN, 50,  0,   0);   // red    / красный = error / ошибка
neopixelWrite(RGB_BUILTIN, 200, 200, 0);   // yellow / жёлтый  = scanning / сканирование
neopixelWrite(RGB_BUILTIN, 0,   0,   0);   // off    / выкл
```

---

## 🌐 Access Without Serial Monitor / Доступ без Serial Monitor

### ✅ Method 1 — mDNS (recommended / рекомендуется)

ESP32 announces itself on the network by name. No IP needed. No Serial Monitor needed.
ESP32 объявляет себя в сети по имени. IP не нужен. Serial Monitor не нужен.

Open in browser / Открой в браузере: `http://esp32.local`

| Platform / Платформа | Status |
|---|---|
| Windows 10/11 | ✅ Works / Работает |
| macOS | ✅ Works / Работает |
| Android Chrome | ✅ Works / Работает |
| iOS Safari | ✅ Works / Работает |

### ✅ Method 2 — Static IP / Фиксированный IP

ESP32 always gets the same IP regardless of network. Already configured in `src/main.cpp`.
ESP32 всегда получает один и тот же IP. Уже настроен в `src/main.cpp`.

Open in browser / Открой в браузере: `http://192.168.1.200`

⚠️ Change gateway IP to match your router / Измени gateway под свой роутер

---

## 🚀 Quick Start / Быстрый старт

```bash
git clone https://github.com/irreligious86/esp32-hello.git
git checkout 07-refactor-modules
```

Open folder in VSCode → PlatformIO pulls dependencies automatically → Upload → open `http://esp32.local` in browser.

Открыть в VSCode → PlatformIO подтянет зависимости → Upload → открыть `http://esp32.local` в браузере.

---

## 📁 Sketch Branches / Ветки со скетчами

| Branch / Ветка | Description / Описание | Libraries |
|---|---|---|
| `01-rgb-serial` | RGB control via Serial Monitor: red/green/blue/pink/cyan/yellow/off | — |
| `02-wifi-webserver` | Web server, RGB control from browser / Веб-сервер, управление RGB | WebServer |
| `03-wifi-network-info` | Network info page — IP, MAC, RSSI, channel, uptime / Инфо о сети | WebServer |
| `04-network-scanner-email` | Ping sweep, ARP MAC, RGB indication, Gmail HTML report | ESP32Ping, ESP Mail Client |
| `05-advanced-scanner` | HTTP banner, SSH banner, NetBIOS, OS fingerprint | ESP32Ping, ESP Mail Client, ESPmDNS |
| `06-scanner-radar-ui` | Non-blocking scan, radar animation, live updates every 5 sec | ESP32Ping, ESP Mail Client, ESPmDNS |
| `07-refactor-modules` | Modular architecture, EEPROM settings, web config page | ESP32Ping, ESP Mail Client, ESPmDNS, EEPROM |

---

## 🏗️ Project Structure (branch 07+) / Структура проекта
src/
main.cpp       — setup(), loop(), WiFi init / инициализация WiFi
scanner.cpp    — ping, ARP, ports, NetBIOS, SSH, HTTP banner
mailer.cpp     — HTML email report via Gmail / HTML отчёт на Gmail
webui.cpp      — web interface, settings page, EEPROM / веб интерфейс, настройки
include/
config.h       — all constants, credentials, EEPROM addresses / константы, пароли, адреса EEPROM
scanner.h      — Device struct, extern state, function declarations / структура Device
mailer.h       — sendReport() declaration
webui.h        — web functions, loadSettings(), saveSettings()

---

## ⚙️ Web Settings Page / Веб страница настроек

Available at / Доступна по адресу: `http://esp32.local/settings`

Allows changing without reflashing / Позволяет менять без перепрошивки:
- Wi-Fi SSID and password / SSID и пароль Wi-Fi
- Gmail App Password / пароль приложения Gmail

Settings saved to EEPROM — survive reboot / Настройки в EEPROM — переживают перезагрузку.

---

## 📡 Scanner Features / Возможности сканера

| Feature / Функция | Description / Описание |
|---|---|
| Ping sweep | Scans all 254 addresses in subnet / Сканирует все 254 адреса подсети |
| ARP MAC lookup | Gets MAC address after ping / Получает MAC после пинга |
| OUI vendor lookup | Identifies manufacturer from MAC prefix / Определяет производителя по MAC |
| Port scan | 16 ports: FTP SSH HTTP HTTPS SMB RDP MQTT MySQL Flask RTSP |
| OS fingerprint | Guesses OS from open ports: Windows / Linux / IoT / Router |
| HTTP banner | Reads web server header from port 80 / Читает заголовок веб сервера |
| SSH banner | Reads SSH version string from port 22 / Читает версию SSH |
| NetBIOS name | Gets Windows computer name via UDP 137 / Имя Windows машины |
| RGB indication | Blue=connecting Yellow=scanning Green=done Red=error |
| Radar UI | Animated radar during scan, live table every 5 sec / Радар во время скана |
| Gmail report | HTML email with full results on scan complete / HTML письмо по завершению |

---

## ⚠️ Known Issues / Известные грабли

| Problem / Проблема | Cause / Причина | Solution / Решение |
|---|---|---|
| Infinite RTC_SW_SYS_RST | `qio_opi` in platformio.ini | Remove that line / Удали строку |
| COM port not visible / Порт не виден | Missing CH340 driver | Install CH341SER.exe |
| RGB not working | Hardcoded pin / Хардкод пина | Use `RGB_BUILTIN` macro |
| Port busy on upload | Serial Monitor open / Монитор открыт | Close before Upload |
| Won't enter bootloader | — | Hold BOOT → press RST → release BOOT |
| Web UI freezes during scan | Blocking loop / Блокирующий цикл | FreeRTOS task — planned / планируется |
| Starlink AP Isolation | Devices isolated / Устройства изолированы | Use ESP32 softAP mode |
| MAC shows N/A | Randomized MAC Android/iOS | Normal / Норма — современные устройства скрывают MAC |

---

## 📅 Changelog / История

| Date / Дата | Branch / Ветка | Change / Изменение |
|---|---|---|
| 27.04.2026 | 01–04 | RGB Serial, WiFi WebServer, Network Info, Scanner + Gmail |
| 28.04.2026 | 05–06 | Advanced scanner, radar UI, non-blocking loop |
| 28.04.2026 | 07 | Modular refactor, EEPROM settings, web config |

---

## 📝 Notes / Заметки

Gmail requires App Password — generate at `myaccount.google.com/apppasswords`. Not your regular Gmail password. Gmail требует пароль приложения — не обычный пароль аккаунта.

Scan of 254 addresses takes 15–20 minutes due to port scanning per host. Сканирование 254 адресов занимает 15–20 минут из-за проверки портов на каждом хосте.

OUI vendor lookup covers 35 common manufacturers — expand `ouiTable[]` in `scanner.cpp` to add more. OUI база покрывает 35 производителей — расширяй `ouiTable[]` в `scanner.cpp`.

All credentials stored in private repository. Все пароли хранятся в приватном репозитории.