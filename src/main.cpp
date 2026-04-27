#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("Готов! Введи: red / green / blue / off");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if      (cmd == "red")   { neopixelWrite(RGB_BUILTIN, 255, 0, 0); Serial.println("RED"); }
    else if (cmd == "green") { neopixelWrite(RGB_BUILTIN, 0, 255, 0); Serial.println("GREEN"); }
    else if (cmd == "blue")  { neopixelWrite(RGB_BUILTIN, 0, 0, 255); Serial.println("BLUE"); }
    else if (cmd == "off")   { neopixelWrite(RGB_BUILTIN, 0, 0, 0);   Serial.println("OFF"); }
    else                     { Serial.println("Неизвестная команда: " + cmd); }
  }
}