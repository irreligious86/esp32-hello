#pragma once
#include <Arduino.h>

struct Device {
  String ip;
  String mac;
  String vendor;
  long   pingMs;
  String openPorts;
  String httpBanner;
  String osGuess;
  String netbiosName;
  String sshBanner;
};

// Глобальное состояние сканирования
extern Device devices[];
extern int    deviceCount;
extern bool   scanning;
extern bool   scanDone;
extern unsigned long scanStart;
extern int    currentScanIP;
extern String scanBase;

// Функции
void scanStep();
void startScan();
String getMacByIP(const String& ip);
String getVendor(const String& mac);
String scanPorts(const String& ip);
String getHttpBanner(const String& ip);
String getSshBanner(const String& ip);
String guessTTL(const String& ip);
String getNetbiosName(const String& ip);