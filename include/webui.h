#pragma once
#include <Arduino.h>
#include <WebServer.h>

void setupWebUI(WebServer& server);
void sendScanPage(WebServer& server);
void sendSettingsPage(WebServer& server);
void saveSettings(const String& ssid, const String& pass, const String& smtpPass);
void loadSettings(String& ssid, String& pass, String& smtpPass);  // ← добавь эту строку