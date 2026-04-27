#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("Команды: red / green / blue / pink / cyan / yellow / off");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if      (cmd == "red")   { neopixelWrite(RGB_BUILTIN, 255, 0, 0); Serial.println("RED"); }
    else if (cmd == "green") { neopixelWrite(RGB_BUILTIN, 0, 255, 0); Serial.println("GREEN"); }
    else if (cmd == "blue")  { neopixelWrite(RGB_BUILTIN, 0, 0, 255); Serial.println("BLUE"); }
    else if (cmd == "pink")  { neopixelWrite(RGB_BUILTIN, 255, 0, 130); Serial.println("PINK"); }
    else if (cmd == "cyan")  { neopixelWrite(RGB_BUILTIN, 0, 255, 255); Serial.println("CYAN");}
    else if (cmd == "yellow") { neopixelWrite(RGB_BUILTIN, 255, 200, 0); Serial.println("YELLOW");}
    else if (cmd == "off")   { neopixelWrite(RGB_BUILTIN, 0, 0, 0);   Serial.println("OFF"); }
    else                     { Serial.println("Неизвестная команда: " + cmd); }
  }
}