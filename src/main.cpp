#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WiFi.h>
#include "secrets.h"


// =====================
// WIFI CONFIG
// =====================
const char* WIFI_SSID = WIFI_SSID_SECRET;
const char* WIFI_PASS = WIFI_PASS_SECRET;

// =====================
// OLED
// =====================
#define OLED_ADDR   0x3C
#define OLED_RESET  -1
#define OLED_W      128
#define OLED_H      64
#define PIN_SDA     21
#define PIN_SCL     22
Adafruit_SH1106G display(OLED_W, OLED_H, &Wire, OLED_RESET);
bool oledOk = false;

// =====================
// DHT22
// =====================
#define DHTTYPE    DHT22
#define DHT1_PIN   4
#define DHT2_PIN   16
DHT dht1(DHT1_PIN, DHTTYPE);
DHT dht2(DHT2_PIN, DHTTYPE);

float t1 = NAN, h1 = NAN;
float t2 = NAN, h2 = NAN;
unsigned long lastReadMs = 0;

// =====================
// RELAYS (6 ch)
// =====================
constexpr int RELAY_PINS[6] = {25, 26, 27, 32, 33, 23};

// Si tu módulo es active-low (lo usual):
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

// =====================
// MOCK CLOCK (for now)
// =====================
uint8_t hh = 12, mm = 0;
bool blinkColon = true;
unsigned long lastTick = 0, lastUi = 0;

// =====================
// WIFI STATE
// =====================
bool wifiTried = false;
unsigned long lastWifiPrint = 0;

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

void connectWiFi(uint32_t timeoutMs = 15000) {
  wifiTried = true;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  Serial.printf("Connecting to WiFi SSID: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected ✅");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println("WiFi NOT connected ❌ (timeout)");
  }
}

void drawHeader() {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d%c%02d", hh, blinkColon ? ':' : ' ', mm);

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print(buf);

  // Estado sensores
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

void drawBottomBar() {
  // Izq: WiFi status
  display.setTextSize(1);
  display.setCursor(0, 56);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("WiFi:OK ");
    // RSSI aproximado
    int rssi = WiFi.RSSI();
    display.print(rssi);
    display.print("dB");
  } else if (wifiTried) {
    display.print("WiFi:--");
  } else {
    display.print("WiFi:..");
  }

  // Der: estado relés R:xxxxxx
  display.setCursor(78, 56);
  display.print("R:");
  for (int i = 0; i < 6; i++) display.print(relayState[i] ? '1' : '0');
}

void drawUI() {
  display.clearDisplay();
  drawHeader();
  drawTempsAndHumidity();
  drawBottomBar();
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Terrario start ===");

  // Relés: configurar y dejar OFF (lo primero)
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
    display.setCursor(0, 0); display.println("Terrario");
    display.setCursor(0, 10); display.println("Init...");
    display.display();
  }

  // DHTs
  dht1.begin();
  delay(20);
  dht2.begin();
  Serial.println("DHT init OK");

  // WiFi (con timeout, no bloquea infinito)
  connectWiFi(15000);
}

void loop() {
  unsigned long now = millis();

  // Mock clock
  if (now - lastTick >= 1000) {
    lastTick = now;
    blinkColon = !blinkColon;
    if (++mm >= 60) { mm = 0; hh = (hh + 1) % 24; }
  }

  // Leer DHT cada 2s
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

  // Log WiFi cada 5s si está conectado
  if (now - lastWifiPrint >= 5000) {
    lastWifiPrint = now;
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("WiFi OK | IP=%s | RSSI=%d dBm\n",
                    WiFi.localIP().toString().c_str(),
                    WiFi.RSSI());
    } else {
      Serial.println("WiFi not connected (aun).");
    }
  }

  // UI
  if (oledOk && (now - lastUi >= 250)) {
    lastUi = now;
    drawUI();
  }
}