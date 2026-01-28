#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// ---- OLED ----
#define OLED_ADDR   0x3C
#define OLED_RESET  -1
#define OLED_W      128
#define OLED_H      64
#define PIN_SDA     21
#define PIN_SCL     22
Adafruit_SH1106G display(OLED_W, OLED_H, &Wire, OLED_RESET);

// ---- DHT22 ----
#define DHTTYPE    DHT22
#define DHT1_PIN   4
#define DHT2_PIN   16

DHT dht1(DHT1_PIN, DHTTYPE);
DHT dht2(DHT2_PIN, DHTTYPE);

float t1 = NAN, h1 = NAN;
float t2 = NAN, h2 = NAN;

unsigned long lastReadMs = 0;

// ---- Reloj mock para header (opcional) ----
uint8_t hh = 12, mm = 0;
bool blinkColon = true;
unsigned long lastTick = 0, lastUi = 0;

static void printDegC() {
  display.write(248); // °
  display.print("C");
}

static void fmtFloat(char *out, size_t outLen, float v, int width = 4, int dec = 1) {
  if (isnan(v)) {
    strncpy(out, "--.-", outLen);
    out[outLen - 1] = '\0';
  } else {
    dtostrf(v, width, dec, out); // ancho fijo para que no “wrappee”
  }
}

void drawHeader() {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d%c%02d", hh, blinkColon ? ':' : ' ', mm);

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print(buf);

  // Estado sensores (sin ocupar mucho)
  display.setCursor(50, 0);
  display.print("D1:");
  display.print((isnan(t1) || isnan(h1)) ? "--" : "OK");
  display.setCursor(85, 0);
  display.print("D2:");
  display.print((isnan(t2) || isnan(h2)) ? "--" : "OK");
}

void drawTempsAndHumidity() {
  char bT1[8], bH1[8], bT2[8], bH2[8];
  fmtFloat(bT1, sizeof(bT1), t1);
  fmtFloat(bH1, sizeof(bH1), h1);
  fmtFloat(bT2, sizeof(bT2), t2);
  fmtFloat(bH2, sizeof(bH2), h2);

  // ---- Bloque 1 (arriba) ----
  // Temperatura 1 grande
  display.setTextSize(1);
  display.setCursor(0, 8);
  display.print("T1:");

  display.setTextSize(2);
  display.setCursor(22, 8);          // y=8 ocupa 16px (hasta y=23)
  display.print(bT1);
  printDegC();

  // Humedad 1 debajo (sin solape)
  display.setTextSize(1);
  display.setCursor(0, 24);          // línea justo debajo del size2
  display.print("H1:");
  display.print(bH1);
  display.print("%");

  // ---- Bloque 2 (abajo) ----
  display.setTextSize(1);
  display.setCursor(0, 32);
  display.print("T2:");

  display.setTextSize(2);
  display.setCursor(22, 32);         // y=32 ocupa 16px (hasta y=47)
  display.print(bT2);
  printDegC();

  display.setTextSize(1);
  display.setCursor(0, 48);          // debajo de T2
  display.print("H2:");
  display.print(bH2);
  display.print("%");
}

void drawUI() {
  display.clearDisplay();
  drawHeader();
  drawTempsAndHumidity();
  display.display();
}

void setup() {
  Serial.begin(115200);

  Wire.begin(PIN_SDA, PIN_SCL);
  if (!display.begin(OLED_ADDR, true)) {
    Serial.println("OLED no detectado");
    while (1) delay(100);
  }

  display.cp437(true);        // permite ° con 248
  display.setTextWrap(false); // evita saltos de linea automáticos

  dht1.begin();
  delay(20);
  dht2.begin();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0); display.println("Terrario 2xDHT22");
  display.setCursor(0, 8); display.println("Init...");
  display.display();
  delay(600);
}

void loop() {
  unsigned long now = millis();

  // reloj mock
  if (now - lastTick >= 1000) {
    lastTick = now;
    blinkColon = !blinkColon;
    if (++mm >= 60) { mm = 0; hh = (hh + 1) % 24; }
  }

  // leer cada 2s
  if (now - lastReadMs >= 2000) {
    lastReadMs = now;

    float tt, hhx;

    tt = dht1.readTemperature();
    hhx = dht1.readHumidity();
    if (!isnan(tt) && !isnan(hhx)) { t1 = tt; h1 = hhx; }
    else Serial.println("DHT1: lectura invalida");

    tt = dht2.readTemperature();
    hhx = dht2.readHumidity();
    if (!isnan(tt) && !isnan(hhx)) { t2 = tt; h2 = hhx; }
    else Serial.println("DHT2: lectura invalida");

    Serial.printf("D1 T=%.1fC H=%.1f%% | D2 T=%.1fC H=%.1f%%\n", t1, h1, t2, h2);
  }

  // refresco UI
  if (now - lastUi >= 250) {
    lastUi = now;
    drawUI();
  }
}
