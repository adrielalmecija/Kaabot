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
bool oledOk = false;

// ---- DHT22 ----
#define DHTTYPE    DHT22
#define DHT1_PIN   4
#define DHT2_PIN   16

DHT dht1(DHT1_PIN, DHTTYPE);
DHT dht2(DHT2_PIN, DHTTYPE);

float t1 = NAN, h1 = NAN;
float t2 = NAN, h2 = NAN;

unsigned long lastReadMs = 0;

// ---- Relés (módulo 6 relés) ----
// IN1->GPIO25, IN2->GPIO26, IN3->GPIO27, IN4->GPIO32, IN5->GPIO33, IN6->GPIO23
constexpr int RELAY_PINS[6] = {25, 26, 27, 32, 33, 23};

// La mayoría de módulos: ACTIVE LOW (LOW=ON, HIGH=OFF)
// Si es al revés, poné:
// constexpr uint8_t RELAY_ON  = HIGH;
// constexpr uint8_t RELAY_OFF = LOW;
constexpr uint8_t RELAY_ON  = LOW;
constexpr uint8_t RELAY_OFF = HIGH;

bool relayState[6] = {false, false, false, false, false, false}; // false=OFF, true=ON

static void setRelay(int idx, bool on) {
  relayState[idx] = on;
  digitalWrite(RELAY_PINS[idx], on ? RELAY_ON : RELAY_OFF);
}

static void setAllRelays(bool on) {
  for (int i = 0; i < 6; i++) setRelay(i, on);
}

// ---- Reloj mock ----
uint8_t hh = 12, mm = 0;
bool blinkColon = true;
unsigned long lastTick = 0, lastUi = 0;

// ---- Test relés ----
bool doRelayTest = true;            // poné false cuando termines
unsigned long lastRelayStep = 0;
int relayIdx = 0;

static void printDegC() {
  display.write(248); // °
  display.print("C");
}

static void fmtFloat(char *out, size_t outLen, float v, int width = 4, int dec = 1) {
  if (isnan(v)) {
    strncpy(out, "--.-", outLen);
    out[outLen - 1] = '\0';
  } else {
    dtostrf(v, width, dec, out);
  }
}

void drawHeader() {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d%c%02d", hh, blinkColon ? ':' : ' ', mm);

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print(buf);

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

  // Bloque 1
  display.setTextSize(1);
  display.setCursor(0, 8);
  display.print("T1:");

  display.setTextSize(2);
  display.setCursor(22, 8);
  display.print(bT1);
  printDegC();

  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print("H1:");
  display.print(bH1);
  display.print("%");

  // Bloque 2
  display.setTextSize(1);
  display.setCursor(0, 32);
  display.print("T2:");

  display.setTextSize(2);
  display.setCursor(22, 32);
  display.print(bT2);
  printDegC();

  display.setTextSize(1);
  display.setCursor(0, 48);
  display.print("H2:");
  display.print(bH2);
  display.print("%");
}

void drawRelayStatus() {
  // Línea inferior: R:101010
  display.setTextSize(1);
  display.setCursor(78, 56);
  display.print("R:");
  for (int i = 0; i < 6; i++) display.print(relayState[i] ? '1' : '0');
}

void drawUI() {
  display.clearDisplay();
  drawHeader();
  drawTempsAndHumidity();
  drawRelayStatus();
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Terrario start ===");

  // Relés: configurar y dejar OFF (esto debería pasar siempre)
  for (int i = 0; i < 6; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], RELAY_OFF);
    relayState[i] = false;
  }
  Serial.println("Relays init -> OFF");

  // OLED
  Wire.begin(PIN_SDA, PIN_SCL);
  oledOk = display.begin(OLED_ADDR, true);
  if (!oledOk) {
    Serial.println("OLED NO detectado (revisar 3V3/GND/SDA21/SCL22)");
  } else {
    display.cp437(true);
    display.setTextWrap(false);

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0); display.println("Terrario 2xDHT22");
    display.setCursor(0, 8); display.println("Init OK");
    display.setCursor(0, 16); display.println("Relays: OFF");
    display.display();
    delay(400);
  }

  // DHTs
  dht1.begin();
  delay(20);
  dht2.begin();

  Serial.println("DHT init OK");
  Serial.println("Asegurate GND comun: ESP32 GND <-> MB102 GND <-> Relay GND");
}

void loop() {
  unsigned long now = millis();

  // reloj mock
  if (now - lastTick >= 1000) {
    lastTick = now;
    blinkColon = !blinkColon;
    if (++mm >= 60) { mm = 0; hh = (hh + 1) % 24; }
  }

  // leer DHT cada 2s
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

  // Test relés: enciende solo uno a la vez
  if (doRelayTest && (now - lastRelayStep >= 700)) {
    lastRelayStep = now;

    // apagar todos
    setAllRelays(false);

    // encender uno
    setRelay(relayIdx, true);
    Serial.printf("Relay %d ON\n", relayIdx + 1);

    relayIdx++;
    if (relayIdx >= 6) relayIdx = 0;
  }

  // UI
  if (oledOk && (now - lastUi >= 250)) {
    lastUi = now;
    drawUI();
  }
}