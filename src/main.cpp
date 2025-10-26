#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define I2C_ADDRESS 0x3C

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!display.begin(I2C_ADDRESS, true)) {
    Serial.println("No se detecto el OLED");
    while (1);
  }

  display.clearDisplay();
  // --- Header fijo (no lo tocaremos más) ---
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);             // y = 0 px
  display.println("Hola Mundo!");
  display.setCursor(0, 8);             // siguiente linea (8 px de alto en size 1)
  display.println("ESP32 + SH1106");
  display.display();
}

void loop() {
  static int counter = 0;

  // Limpiamos SOLO el rectángulo donde va el contador (debajo del header)
  // Header ocupa ~16 px de alto (2 líneas en size=1). Usamos desde y=16 hacia abajo.
  display.fillRect(0, 16, SCREEN_WIDTH, SCREEN_HEIGHT - 16, SH110X_BLACK);

  display.setCursor(0, 16);
  display.setTextSize(2);               // más grande y legible
  display.print("Contador: ");
  display.println(counter++);

  display.display();
  delay(1000);
}
