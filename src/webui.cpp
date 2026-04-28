#include "webui.h"
#include "scanner.h"
#include "mailer.h"
#include "config.h"
#include <EEPROM.h>
#include <WiFi.h>

// Внешние переменные из mailer.cpp
extern String smtpPassword;

// =====================
// EEPROM утилиты
// =====================
void saveSettings(const String& ssid, const String& pass, const String& smtpPass) {
  EEPROM.begin(EEPROM_SIZE);

  // Очищаем
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);

  // Пишем SSID
  for (int i = 0; i < (int)ssid.length() && i < 31; i++)
    EEPROM.write(EEPROM_WIFI_SSID + i, ssid[i]);

  // Пишем пароль WiFi
  for (int i = 0; i < (int)pass.length() && i < 63; i++)
    EEPROM.write(EEPROM_WIFI_PASS + i, pass[i]);

  // Пишем SMTP пароль
  for (int i = 0; i < (int)smtpPass.length() && i < 31; i++)
    EEPROM.write(EEPROM_SMTP_PASS + i, smtpPass[i]);

  // Флаг валидности
  EEPROM.write(EEPROM_VALID_FLAG, 0xAB);
  EEPROM.commit();
  EEPROM.end();

  Serial.println("Настройки сохранены в EEPROM");
}

void loadSettings(String& ssid, String& pass, String& smtpPass) {
  EEPROM.begin(EEPROM_SIZE);

  if (EEPROM.read(EEPROM_VALID_FLAG) != 0xAB) {
    // EEPROM пустой — используем дефолты
    ssid     = DEFAULT_WIFI_SSID;
    pass     = DEFAULT_WIFI_PASS;
    smtpPass = DEFAULT_SMTP_PASS;
    Serial.println("EEPROM пустой — используем дефолты");
    EEPROM.end();
    return;
  }

  ssid = ""; pass = ""; smtpPass = "";
  for (int i = 0; i < 32; i++) {
    char c = EEPROM.read(EEPROM_WIFI_SSID + i);
    if (c == 0) break;
    ssid += c;
  }
  for (int i = 0; i < 64; i++) {
    char c = EEPROM.read(EEPROM_WIFI_PASS + i);
    if (c == 0) break;
    pass += c;
  }
  for (int i = 0; i < 32; i++) {
    char c = EEPROM.read(EEPROM_SMTP_PASS + i);
    if (c == 0) break;
    smtpPass += c;
  }

  EEPROM.end();
  Serial.println("Загружено из EEPROM: SSID=" + ssid);
}

// =====================
// Страница сканера
// =====================
void sendScanPage(WebServer& server) {
  String html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>ESP32 Scanner</title>"
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
    "a,button{display:inline-block;margin:6px 4px;padding:10px 24px;"
    "border-radius:6px;text-decoration:none;color:white;font-weight:bold;"
    "border:none;cursor:pointer;font-family:monospace;font-size:13px;}"
    ".scan{background:#e67e22;}"
    ".settings{background:#2c3e50;}"
    "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}"
    "@keyframes ping{0%{transform:scale(1);opacity:1}100%{transform:scale(2.5);opacity:0}}"
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
    ".done-wrap{text-align:center;padding:20px 0;}"
    ".done-circle{width:100px;height:100px;border-radius:50%;"
    "border:3px solid #00ff88;margin:0 auto 15px;"
    "animation:glow 1.5s ease-in-out infinite;"
    "display:flex;align-items:center;justify-content:center;font-size:40px;}"
    ".done-title{color:white;font-size:20px;font-weight:bold;margin:10px 0;}"
    ".done-sub{color:#555;font-size:12px;}"
    "</style>";

  if (scanning) html += "<meta http-equiv='refresh' content='5'>";

  html += "</head><body>";
  html += "<h2>📡 " DEVICE_NAME "</h2>";
  html += "<div class='info'>";
  html += "IP: " + WiFi.localIP().toString();
  html += " | SSID: " + String(WiFi.SSID());
  html += " | RSSI: " + String(WiFi.RSSI()) + " dBm";
  html += " | Uptime: " + String(millis()/1000) + "s";
  html += " | FW: " FIRMWARE_VERSION "</div>";

  // Кнопки
  if (!scanning) {
    html += "<a href='/startscan' class='scan'>🔍 Сканировать</a>";
    html += "<a href='/settings' class='settings'>⚙️ Настройки</a>";
  }

  // Радар во время сканирования
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
    html += "<p style='color:#333;font-size:11px;text-align:center'>"
            "обновление каждые 5 сек</p>";
  }

  // Финальный экран
  if (scanDone && !scanning) {
    html += "<div class='done-wrap'>"
            "<div class='done-circle'>✓</div>"
            "<div class='done-title'>Сканирование завершено</div>"
            "<div class='done-sub'>Найдено: " + String(deviceCount) +
            " · Время: " + String((millis()-scanStart)/1000) + " сек</div>"
            "<div class='done-sub' style='margin-top:6px'>"
            "📧 Отчёт → " RECIPIENT_EMAIL "</div>"
            "</div>";
  }

  // Таблица
  if (deviceCount > 0) {
    html += "<table><tr>"
            "<th>#</th><th>IP</th><th>MAC</th><th>Vendor</th>"
            "<th>OS</th><th>Ping</th><th>Ports</th>"
            "<th>HTTP</th><th>SSH</th><th>Name</th>"
            "</tr>";
    for (int i = 0; i < deviceCount; i++) {
      String p = devices[i].openPorts;
      html += "<tr>";
      html += "<td>" + String(i+1) + "</td>";
      html += "<td class='ip'>" + devices[i].ip + "</td>";
      html += "<td class='mac'>" + devices[i].mac + "</td>";
      html += "<td class='vendor'>" + devices[i].vendor + "</td>";
      html += "<td class='os'>" + devices[i].osGuess + "</td>";
      html += "<td class='ping'>" + String(devices[i].pingMs) + "ms</td>";
      html += "<td class='" + String(p=="none"?"none":"ports") + "'>" + p + "</td>";
      html += "<td class='banner'>" + (devices[i].httpBanner.length()>0 ? devices[i].httpBanner : "-") + "</td>";
      html += "<td class='ssh'>" + (devices[i].sshBanner.length()>0 ? devices[i].sshBanner : "-") + "</td>";
      html += "<td class='nbname'>" + (devices[i].netbiosName.length()>0 ? devices[i].netbiosName : "-") + "</td>";
      html += "</tr>";
    }
    html += "</table>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

// =====================
// Страница настроек
// =====================
void sendSettingsPage(WebServer& server) {
  String currentSsid = WiFi.SSID();

  String html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>Настройки</title>"
    "<style>"
    "body{font-family:monospace;padding:20px;background:#0d0d0d;color:#00ff88;margin:0;}"
    "h2{color:white;border-bottom:1px solid #222;padding-bottom:10px;}"
    "h3{color:#aaa;font-size:14px;margin-top:24px;margin-bottom:10px;}"
    ".group{background:#111;border-radius:8px;padding:16px;margin-bottom:16px;}"
    "label{display:block;color:#555;font-size:11px;margin-bottom:4px;margin-top:12px;}"
    "input{width:100%;padding:10px;background:#1a1a1a;border:1px solid #333;"
    "border-radius:4px;color:#00ff88;font-family:monospace;font-size:13px;"
    "box-sizing:border-box;}"
    "input:focus{outline:none;border-color:#00ff88;}"
    "button{margin-top:16px;padding:12px 28px;border-radius:6px;"
    "border:none;cursor:pointer;font-family:monospace;font-size:13px;"
    "font-weight:bold;color:white;}"
    ".save{background:#27ae60;}"
    ".back{background:#2c3e50;}"
    ".warn{color:#e74c3c;font-size:11px;margin-top:8px;}"
    "a{color:#555;text-decoration:none;}"
    "</style></head><body>"
    "<h2>⚙️ Настройки</h2>"

    "<form method='POST' action='/savesettings'>"

    "<div class='group'>"
    "<h3>📶 Wi-Fi</h3>"
    "<label>SSID (имя сети)</label>"
    "<input type='text' name='ssid' value='" + currentSsid + "' placeholder='Название сети'>"
    "<label>Пароль Wi-Fi</label>"
    "<input type='password' name='wifipass' placeholder='Оставь пустым если без пароля'>"
    "</div>"

    "<div class='group'>"
    "<h3>📧 Gmail App Password</h3>"
    "<label>Пароль приложения Gmail</label>"
    "<input type='password' name='smtppass' placeholder='xxxx xxxx xxxx xxxx'>"
    "<p class='warn'>⚠️ Оставь пустым чтобы не менять текущий пароль</p>"
    "</div>"

    "<button type='submit' class='save'>💾 Сохранить и перезагрузить</button>"
    "</form>"
    "<br>"
    "<a href='/'><button class='back'>← Назад</button></a>"
    "</body></html>";

  server.send(200, "text/html", html);
}

// =====================
// Регистрация маршрутов
// =====================
void setupWebUI(WebServer& server) {
  server.on("/", [&server]() {
    sendScanPage(server);
  });

  server.on("/startscan", [&server]() {
    if (!scanning) startScan();
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });

  server.on("/settings", [&server]() {
    sendSettingsPage(server);
  });

  server.on("/savesettings", HTTP_POST, [&server]() {
    String newSsid     = server.arg("ssid");
    String newWifiPass = server.arg("wifipass");
    String newSmtpPass = server.arg("smtppass");

    // Если поле пустое — оставляем текущее
    if (newSsid.length() == 0)     newSsid     = WiFi.SSID();
    if (newSmtpPass.length() > 0)  smtpPassword = newSmtpPass;

    saveSettings(newSsid, newWifiPass, smtpPassword);

    // Страница подтверждения
    server.send(200, "text/html",
      "<html><body style='font-family:monospace;background:#0d0d0d;"
      "color:#00ff88;padding:40px;text-align:center'>"
      "<h2 style='color:white'>✓ Сохранено</h2>"
      "<p style='color:#555'>Перезагружаюсь и подключаюсь к: <b style='color:#00ff88'>"
      + newSsid + "</b></p>"
      "<p style='color:#333;font-size:12px'>Страница обновится через 8 секунд</p>"
      "<meta http-equiv='refresh' content='8;url=/'>"
      "</body></html>");

    delay(1000);
    ESP.restart();
  });
}