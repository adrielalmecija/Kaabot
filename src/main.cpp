#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include "secrets.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// =====================
// WIFI CONFIG
// =====================
static const char* WIFI_SSID = WIFI_SSID_SECRET;
static const char* WIFI_PASS = WIFI_PASS_SECRET;

// =====================
// NTP CONFIG
// =====================
static const char* TZ_INFO = "ART3";
static const char* NTP1 = "pool.ntp.org";
static const char* NTP2 = "time.google.com";
static const char* NTP3 = "time.nist.gov";

// Re-sync cada 24h (no bloqueante)
static const uint32_t NTP_RESYNC_MS = 24UL * 60UL * 60UL * 1000UL; // 86400000 ms
static unsigned long lastNtpResyncMs = 0;

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
unsigned long lastUi = 0;

// =====================
// RELAYS
// =====================
constexpr int RELAY_PINS[6] = {25, 26, 27, 32, 33, 23};
constexpr uint8_t RELAY_ON  = LOW;   // active-low típico
constexpr uint8_t RELAY_OFF = HIGH;

bool relayState[6] = {false,false,false,false,false,false};

static void setRelay(int idx, bool on) {
  relayState[idx] = on;
  digitalWrite(RELAY_PINS[idx], on ? RELAY_ON : RELAY_OFF);
}

// =====================
// Helpers
// =====================
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

static void connectWiFi(uint32_t timeoutMs = 15000) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.setSleep(false);

  Serial.printf("Connecting WiFi: '%s'\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected ✅");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    Serial.print("RSSI: "); Serial.println(WiFi.RSSI());
  } else {
    Serial.println("WiFi NOT connected ❌ (timeout)");
  }
}

static void setupOrResyncNTP() {
  configTzTime(TZ_INFO, NTP1, NTP2, NTP3);
  lastNtpResyncMs = millis();
  Serial.printf("NTP configured/resync. TZ='%s'\n", TZ_INFO);
}

static bool timeIsValid() {
  time_t now = time(nullptr);
  return now > 1577836800; // 2020-01-01
}

static void getTimeHHMM(char out[6]) {
  if (!timeIsValid()) { strncpy(out, "--:--", 6); return; }
  struct tm t;
  if (!getLocalTime(&t, 50)) { strncpy(out, "--:--", 6); return; }
  snprintf(out, 6, "%02d:%02d", t.tm_hour, t.tm_min);
}

// =====================
// UI
// =====================
void drawHeader() {
  char hhmm[6];
  getTimeHHMM(hhmm);

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print(hhmm);

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
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print((WiFi.status() == WL_CONNECTED) ? "WiFi:OK" : "WiFi:--");

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

// =====================
// Setup / Loop
// =====================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Terrario start ===");

  // Relés OFF primero
  for (int i = 0; i < 6; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], RELAY_OFF);
    relayState[i] = false;
  }

  // OLED
  Wire.begin(PIN_SDA, PIN_SCL);
  oledOk = display.begin(OLED_ADDR, true);
  if (!oledOk) {
    Serial.println("OLED NO detectado");
  } else {
    display.cp437(true);
    display.setTextWrap(false);
  }

  // DHTs
  dht1.begin();
  delay(20);
  dht2.begin();

  // WiFi + NTP (sin esperas)
  connectWiFi(15000);
  if (WiFi.status() == WL_CONNECTED) {
    setupOrResyncNTP();
  } else {
    Serial.println("NTP skipped (no WiFi)");
  }
}

void loop() {
  unsigned long now = millis();

  // Re-sync NTP cada 24h (solo si hay WiFi)
  if (WiFi.status() == WL_CONNECTED && (now - lastNtpResyncMs >= NTP_RESYNC_MS)) {
    setupOrResyncNTP();
  }

  // Leer DHT cada 2s
  if (now - lastReadMs >= 2000) {
    lastReadMs = now;

    float tt = dht1.readTemperature();
    float hhx = dht1.readHumidity();
    if (!isnan(tt) && !isnan(hhx)) { t1 = tt; h1 = hhx; }

    tt = dht2.readTemperature();
    hhx = dht2.readHumidity();
    if (!isnan(tt) && !isnan(hhx)) { t2 = tt; h2 = hhx; }
  }

  // UI
  if (oledOk && (now - lastUi >= 250)) {
    lastUi = now;
    drawUI();
  }
}
