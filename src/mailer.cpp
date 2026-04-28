#include "mailer.h"
#include "scanner.h"
#include "config.h"
#include <ESP_Mail_Client.h>
#include <WiFi.h>

// Пароль приложения — может быть изменён через веб интерфейс
String smtpPassword = DEFAULT_SMTP_PASS;

SMTPSession smtp;

void sendReport() {
  Serial.println("Отправляю email...");

  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port      = SMTP_PORT;
  session.login.email      = SENDER_EMAIL;
  session.login.password   = smtpPassword.c_str();
  session.login.user_domain = "";

  SMTP_Message message;
  message.sender.name  = SENDER_NAME;
  message.sender.email = SENDER_EMAIL;
  message.subject = "ESP32 Network Scan — " + String(deviceCount) + " devices found";
  message.addRecipient("Alex", RECIPIENT_EMAIL);

  String html =
    "<html><body style='font-family:monospace;background:#0d0d0d;"
    "color:#00ff88;padding:20px;'>"
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
          "Sent by ESP32-S3 DevKitC-1 | " FIRMWARE_VERSION "</p>"
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