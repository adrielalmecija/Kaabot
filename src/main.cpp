#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define OLED_ADDR   0x3C
#define OLED_RESET  -1
#define OLED_W      128
#define OLED_H      64
#define PIN_SDA     21
#define PIN_SCL     22

Adafruit_SH1106G display(OLED_W, OLED_H, &Wire, OLED_RESET);

// ---- Estado simulado (luego reemplazamos por sensores/relés) ----
float t1 = 28.9f, t2 = 29.6f;
float SETPOINT = 28.0f, HYST = 0.8f;
bool  L1 = true, L2 = false, HEAT = true;
bool  modeDay = true;
uint8_t hh = 13, mm = 24;
bool blinkColon = true;

unsigned long lastUi  = 0;
unsigned long lastTick = 0;

// ---- Dibujos auxiliares (sin emojis) ----
void drawRelayBox(int x, int y, bool on, const char* label) {
  display.drawRect(x, y, 10, 10, SH110X_WHITE);
  if (on) display.fillRect(x+1, y+1, 8, 8, SH110X_WHITE);
  display.setCursor(x+12, y+1);
  display.setTextSize(1);
  display.print(label);
}

void drawHeader() {
  // Hora (HH:MM) con colon intermitente
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d%c%02d", hh, blinkColon ? ':' : ' ', mm);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print(buf);

  // Estados L1/L2/H
  drawRelayBox(46, 0,  L1,  "L1");
  drawRelayBox(76, 0,  L2,  "L2");
  drawRelayBox(106, 0, HEAT,"H");
}

void drawTemps() {
  // --- T1 ---
  display.setTextSize(1);
  display.setCursor(0, 16); display.print("T1:");
  display.setTextSize(2);
  display.setCursor(22, 12); display.printf("%.1f", t1);

  // Panel derecho (alineado para no cortar)
  char spbuf[8], hbuf[8];
  dtostrf(SETPOINT, 4, 1, spbuf);  // "28.0"
  dtostrf(HYST,     4, 1, hbuf);   // "0.8"

  display.setTextSize(1);
  display.setCursor(76, 16); display.print("SP:");
  display.print(spbuf);

  // --- T2 ---
  display.setTextSize(1);
  display.setCursor(0, 38); display.print("T2:");
  display.setTextSize(2);
  display.setCursor(22, 34); display.printf("%.1f", t2);

  // Histeresis a la derecha
  display.setTextSize(1);
  display.setCursor(76, 38); display.print("H:");
  display.print(hbuf);
}

void drawFooter() {
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("Modo: ");
  display.print(modeDay ? "DIA" : "NOCHE");
}

void drawUI() {
  display.clearDisplay();
  drawHeader();
  drawTemps();
  drawFooter();
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);

  if (!display.begin(OLED_ADDR, true)) {
    Serial.println("OLED no detectado.");
    while (1) delay(100);
  }

  // Fuente y render
  display.cp437(true);        // habilita ° (código 248)
  display.setTextWrap(false); // evita saltos de línea automáticos

  // Splash
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);   display.println("Terrario UI");
  display.setCursor(0, 8);   display.println("Layout refinado");
  display.display();
  delay(700);
}

void loop() {
  unsigned long now = millis();

  // Tick demo de reloj
  if (now - lastTick >= 1000) {
    lastTick = now;
    blinkColon = !blinkColon;
    if (++mm >= 60) { mm = 0; hh = (hh + 1) % 24; }
  }

  // Redibujar UI
  if (now - lastUi >= 250) {
    lastUi = now;
    drawUI();
  }
}
