# ESP32-S3 DevKitC-1 — Advanced Network Scanner

> Engineering reference. For full user guide see [`docs/manual.html`](docs/manual.html)
> Інженерна документація. Повна інструкція користувача: [`docs/manual.html`](docs/manual.html)


---


#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define SENDER_EMAIL "irreligious86@gmail.com"
#define SENDER_PASSWORD "fokc ckxb sgap agib"


---


## 🔧 Hardware / Железо

| Parameter / Параметр | Value / Значення |
|---|---|
| Board / Плата | ESP32-S3-DevKitC-1 |
| Chip / Чіп | ESP32-S3 (QFN56), revision v0.2 |
| Cores / Ядра | 2× Xtensa LX7, 240 MHz |
| RAM | 512KB SRAM + 8MB PSRAM (Embedded, AP_3v3) |
| Flash | 16MB (QD) |
| Wi-Fi | 802.11 b/g/n, 2.4GHz |
| Bluetooth | BLE 5.0 |
| USB | 2× Type-C — UART (CH343) + native USB-OTG |
| RGB LED | WS2812B — `neopixelWrite(RGB_BUILTIN, R, G, B)` |
| MAC | D8:3B:DA:A4:CC:08 |

---

## ⚙️ Development Environment / Середовище розробки

| Tool / Інструмент | Version / Версія |
|---|---|
| IDE | VSCode + PlatformIO |
| Platform | espressif32 @ 6.13.0 |
| Framework | Arduino |
| Toolchain | xtensa-esp32s3 @ 8.4.0 |
| OS | Windows 11 |
| USB Driver / Драйвер | CH341SER (CH343) |

---

## 📋 platformio.ini — minimal working config / мінімальний робочий конфіг

```ini
[env:esp32-s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
upload_speed = 921600

lib_deps =
    marian-craciunescu/ESP32Ping @ ^1.7
    mobizt/ESP Mail Client @ ^3.4.19
    EEPROM
```

> ⚠️ NEVER add / НІКОЛИ не додавати: `board_build.arduino.memory_type = qio_opi`
> Causes infinite crash-loop RTC_SW_SYS_RST / Викликає нескінченний креш-луп

---

## 💡 Built-in RGB LED / Вбудований RGB світлодіод

No libraries needed — built into the framework.
Бібліотеки не потрібні — вбудовано у фреймворк.

```cpp
neopixelWrite(RGB_BUILTIN, R, G, B);         // 0-255 per channel / на кожен канал

neopixelWrite(RGB_BUILTIN, 100, 100, 100);   // ⬜ WHITE  — boot window (3s)
neopixelWrite(RGB_BUILTIN, 0,   0,   50);    // 🔵 BLUE   — connecting to WiFi
neopixelWrite(RGB_BUILTIN, 0,   50,  0);     // 🟢 GREEN  — ready
neopixelWrite(RGB_BUILTIN, 255, 100, 0);     // 🟠 ORANGE — AP mode active
neopixelWrite(RGB_BUILTIN, 200, 200, 0);     // 🟡 YELLOW — scanning (double blink)
// rainbow flash                             // 🌈 RAINBOW — device found
neopixelWrite(RGB_BUILTIN, 50,  0,   0);     // 🔴 RED    — WiFi lost (blink)
neopixelWrite(RGB_BUILTIN, 0,   0,   0);     // ⚫ OFF
```

---

## 🚀 Quick Start / Швидкий старт

```bash
git clone https://github.com/irreligious86/esp32-hello.git
git checkout 11-bluetooth
```

Open in VSCode → PlatformIO pulls dependencies automatically → Upload.
Відкрити у VSCode → PlatformIO підтягне залежності → Upload.

After flashing open in browser / Після прошивки відкрий у браузері:
http://esp32.local
http://192.168.1.200

---

## 📁 Sketch Branches / Гілки зі скетчами

| Branch / Гілка | Description / Опис | Libraries |
|---|---|---|
| `01-rgb-serial` | RGB control via Serial Monitor commands: red/green/blue/pink/cyan/yellow/off | — |
| `02-wifi-webserver` | Web server, RGB control from browser | WebServer |
| `03-wifi-network-info` | Network info page — IP, MAC, RSSI, channel, uptime | WebServer |
| `04-network-scanner-email` | Ping sweep, ARP MAC, RGB indication, Gmail HTML report | ESP32Ping, ESP Mail Client |
| `05-advanced-scanner` | HTTP banner, SSH banner, NetBIOS, OS fingerprint | ESP32Ping, ESP Mail Client, ESPmDNS |
| `06-scanner-radar-ui` | Non-blocking scan, radar animation, live updates | ESP32Ping, ESP Mail Client, ESPmDNS |
| `07-refactor-modules` | Modular architecture, EEPROM settings, web config page | ESP32Ping, ESP Mail Client, ESPmDNS, EEPROM |
| `08-docs-and-static-ip` | Static IP, mDNS, bilingual README | — |
| `09-static-ip-freertos-bt` | Static IP confirmed working | — |
| `10-freertos` | FreeRTOS dual-core, AP mode, BOOT button trigger | — |
| `11-bluetooth` | BLE NUS control, HTML user manual ← **latest** | BLEDevice |

---

## 🏗️ Project Structure (branch 11) / Структура проекту
src/
main.cpp      — setup(), loop(), WiFi, AP mode, BOOT button, FreeRTOS tasks, BLE
scanner.cpp   — ping, ARP, OUI lookup, port scan, OS fingerprint, HTTP/SSH banner, NetBIOS
mailer.cpp    — HTML email report via Gmail SMTP
webui.cpp     — web UI, settings page, EEPROM read/write
include/
config.h      — all constants, EEPROM addresses, BLE UUIDs, credentials
scanner.h     — Device struct, extern state variables, function declarations
mailer.h      — sendReport() declaration
webui.h       — setupWebUI(), loadSettings(), saveSettings() declarations
docs/
manual.html   — full styled user manual (EN)

---

## ⚙️ Architecture / Архітектура
Core 0 (FreeRTOS)              Core 1 (FreeRTOS)
──────────────────             ──────────────────
webTask()                      scanTask()
server.handleClient()          scanStep() × 254
→ instant response             → ping + ports + banners
even during scan               → sendReport() on complete

Shared state / Спільний стан: `scanning`, `scanDone`, `deviceCount`, `currentScanIP`, `devices[]`

---

## 📡 Scanner Features / Можливості сканера

| Feature / Функція | Description / Опис |
|---|---|
| Ping sweep | Scans all 254 addresses in subnet / Сканує всі 254 адреси підмережі |
| ARP MAC lookup | Gets MAC address after ping / Отримує MAC після пінгу |
| OUI vendor lookup | Identifies manufacturer from MAC prefix (35 vendors) / Визначає виробника |
| Port scan | 16 ports: FTP SSH HTTP HTTPS SMB RDP MQTT MySQL Flask RTSP DNS Telnet |
| OS fingerprint | Guesses OS from open ports: Windows / Linux / IoT / Router |
| HTTP banner | Reads web server header from port 80 / Читає заголовок веб-сервера |
| SSH banner | Reads SSH version string from port 22 / Читає версію SSH |
| NetBIOS name | Gets Windows computer name via UDP 137 / Ім'я Windows машини |
| Radar UI | Animated radar, live table auto-refresh every 5 sec / Радар, живі оновлення |
| Gmail report | Styled HTML email sent automatically on scan complete |

---

## 🌐 Web Access & Settings / Веб-доступ та налаштування

| Situation / Ситуація | Address / Адреса |
|---|---|
| Main page (same network) | `http://esp32.local` or `http://192.168.1.200` |
| Settings (same network) | `http://esp32.local/settings` |
| Main page (AP mode) | `http://192.168.4.1` |
| Settings (AP mode) | `http://192.168.4.1/settings` |

Settings page allows changing without reflashing / Налаштування без перепрошивки:
- Wi-Fi SSID and password / SSID та пароль Wi-Fi
- Gmail App Password / пароль додатку Gmail

Settings saved to EEPROM — survive reboot / Зберігаються в EEPROM — переживають перезавантаження.

---

## 🔌 Boot Modes & Network Switching / Режими завантаження та зміна мережі

### Normal boot / Звичайний запуск

Press RST → release. ESP32 connects to saved network automatically.
Натисни RST → відпусти. ESP32 підключиться до збереженої мережі.

RGB sequence / RGB послідовність:
⬜ WHITE (3s) → 🔵 BLUE (connecting) → 🟢 GREEN (ready)

---

### Force AP mode / Примусовий AP режим

Use when you need to configure a new network.
Використовуй коли потрібно налаштувати нову мережу.

Press RST → release
Immediately hold BOOT button
Hold 3 seconds → RGB turns 🟠 ORANGE
Release BOOT
Connect to: ESP32-Setup / password: 12345678
Open browser: http://192.168.4.1/settings
Enter new credentials → Save & Reboot
RGB turns 🟢 GREEN when connected to new network


> ⚠️ Do NOT hold BOOT before releasing RST — on ESP32-S3 this activates firmware download mode.
> Always: RST first → release → then hold BOOT.

---

### Auto AP mode / Автоматичний AP режим

If saved network is unreachable after 5 attempts → ESP32 automatically starts AP.
Якщо збережена мережа недоступна після 5 спроб → ESP32 автоматично піднімає AP.

RGB turns 🟠 ORANGE. Connect to `ESP32-Setup` / `12345678` → open `192.168.4.1/settings`.

---

## 📱 Bluetooth BLE Control / Керування через Bluetooth

ESP32 runs a BLE NUS (Nordic UART Service) server visible as `ESP32-Scanner`.
Works independently of Wi-Fi — useful when IP is unknown or network is unavailable.
Працює незалежно від Wi-Fi — зручно коли IP невідомий або мережа недоступна.

### Required App / Додаток

| Platform | App | Where |
|---|---|---|
| Android | Serial Bluetooth Terminal by Kai Morich | Play Store |
| iOS | nRF Toolbox or LightBlue | App Store |

---

### Commands / Команди

| Command | Response / Відповідь |
|---|---|
| 27.04.2026 | Project started — RGB Serial, WiFi WebServer, Network Info, Network Scanner with Gmail / Старт проекта |   
