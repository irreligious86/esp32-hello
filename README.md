# ESP32-S3 DevKitC-1 — Advanced Network Scanner

> Engineering reference. For full user guide see [`docs/manual.html`](docs/manual.html)
> Інженерна документація. Повна інструкція користувача: [`docs/manual.html`](docs/manual.html)

---

## 🔧 Hardware / Залізо

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
| `scan` | Starts full network scan / Запускає сканування |
| `status` | WiFi state, IP, RSSI, scan flag / Стан WiFi, IP, RSSI |
| `ip` | Current IP and web URLs / Поточний IP та URL |
| `results` | All found devices with MAC, vendor, OS / Всі знайдені пристрої |
| `help` | All available commands / Всі доступні команди |

Auto-notifications during scan / Автоповідомлення під час сканування:
Scanning... 127/254 (50%) found: 3        ← every 30 sec
Scan complete! Found 13 devices. Report sent to email.   ← on finish

---

### Usage Scenarios / Сценарії використання

**Scenario 1 — Unknown IP in new location**
Connect via BLE → type `ip` → get address → open in browser.
Підключись через BLE → введи `ip` → отримай адресу → відкрий у браузері.

**Scenario 2 — Quick check without browser**
Type `status` to see if scan is running → type `results` to see devices in terminal.
Введи `status` → введи `results` щоб побачити пристрої прямо в терміналі.

**Scenario 3 — Remote scan trigger**
ESP32 is at home. Connect via BLE → type `scan` → wait for completion →
check email for full HTML report.
ESP32 вдома. Підключись по BLE → `scan` → дочекайся → перевір пошту.

**Scenario 4 — Configure network without laptop**
AP mode active. Connect phone to `ESP32-Setup` + keep BLE terminal open.
Use `status` to monitor while configuring via `192.168.4.1/settings`.
AP режим. Підключи телефон до `ESP32-Setup` + тримай BLE термінал відкритим.

---

## ⚠️ Known Issues / Відомі проблеми

| Problem / Проблема | Cause / Причина | Solution / Рішення |
|---|---|---|
| Infinite RTC_SW_SYS_RST | `qio_opi` in platformio.ini | Remove that line / Видали рядок |
| COM port not visible | Missing CH340 driver | Install CH341SER.exe |
| RGB not working | Hardcoded pin number | Use `RGB_BUILTIN` macro |
| Port busy on upload | Serial Monitor open | Close before Upload |
| Won't enter bootloader | — | Hold BOOT → press RST → release BOOT |
| AP not appearing | BOOT held before RST release | RST first → release → then hold BOOT |
| MAC shows N/A | Randomized MAC (Android/iOS) | Normal — modern devices hide real MAC |
| Email not sending | Wrong App Password | Regenerate at myaccount.google.com/apppasswords |
| Starlink AP Isolation | Devices can't reach each other | Use ESP32 softAP mode |

---

## 📅 Changelog / Історія змін

| Date / Дата | Branch / Гілка | Change / Зміна |
|---|---|---|
| 27.04.2026 | 01–04 | RGB Serial, WiFi WebServer, Network Info, Scanner + Gmail |
| 28.04.2026 | 05–06 | Advanced scanner, radar UI, non-blocking loop |
| 28.04.2026 | 07 | Modular refactor, EEPROM settings, web config |
| 28.04.2026 | 08–09 | Static IP (192.168.1.200), mDNS (esp32.local) |
| 29.04.2026 | 10 | FreeRTOS dual-core, AP mode, BOOT button trigger |
| 29.04.2026 | 11 | BLE NUS control, HTML user manual |

---

## 📝 Notes / Нотатки

Gmail requires App Password — generate at `myaccount.google.com/apppasswords`. Not your regular account password.
Gmail потребує пароль додатку — не звичайний пароль акаунту.

Full scan (254 hosts + port scan) takes 15–20 minutes.
Повне сканування (254 хости + порти) займає 15–20 хвилин.

OUI vendor database covers 35 manufacturers — extend `ouiTable[]` in `scanner.cpp` to add more.
База OUI покриває 35 виробників — розширюй `ouiTable[]` у `scanner.cpp`.

All credentials stored in private repository. Changeable via web UI without reflashing.
Всі паролі зберігаються у приватному репозиторії. Змінюються через веб без перепрошивки.