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

// Публичные функции
void scanStep();
void startScan();