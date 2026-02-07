#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include "secrets.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

#include <OneWire.h>
#include <DallasTemperature.h>

// =====================
// WIFI / NTP
// =====================
static const char* WIFI_SSID = WIFI_SSID_SECRET;
static const char* WIFI_PASS = WIFI_PASS_SECRET;

static const char* TZ_INFO = "ART3";
static const char* NTP1 = "pool.ntp.org";
static const char* NTP2 = "time.google.com";
static const char* NTP3 = "time.nist.gov";

static const uint32_t NTP_RESYNC_MS = 24UL * 60UL * 60UL * 1000UL;
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

// =====================
// DS18B20 (Bed temp)
// =====================
#define DS_PIN 17
OneWire oneWire(DS_PIN);
DallasTemperature ds(&oneWire);
float tBed = NAN;

// =====================
// Timing
// =====================
unsigned long lastReadMs = 0;
unsigned long lastUi = 0;

// =====================
// RELAYS (6ch module)
// =====================
constexpr int RELAY_PINS[6] = { 32, 33, 25, 26, 27, 23}; // IN1..IN6
constexpr uint8_t RELAY_ON  = LOW;   // active-low típico
constexpr uint8_t RELAY_OFF = HIGH;

bool relayState[6] = {false,false,false,false,false,false};

static void setRelay(int idx, bool on) {
  relayState[idx] = on;
  digitalWrite(RELAY_PINS[idx], on ? RELAY_ON : RELAY_OFF);
}
static bool getRelay(int idx) { return relayState[idx]; }

// Relay indices (lógicos)
constexpr int R1_CERAMIC = 0;
constexpr int R2_BED     = 1;
constexpr int R3_LAMP    = 2;
constexpr int R4_LIGHT   = 3;

// =====================
// CONTROL PARAMS
// =====================
// Iluminación 09:00 - 21:00
constexpr int LIGHT_ON_HH  = 9;
constexpr int LIGHT_ON_MM  = 0;
constexpr int LIGHT_OFF_HH = 21;
constexpr int LIGHT_OFF_MM = 0;

// Calefacción (controla por T1)
constexpr float SP_HEAT       = 29.0f;
constexpr float HYST_MAIN     = 0.5f;
constexpr float BED_ON_DELTA  = 1.0f;
constexpr float LAMP_ON_DELTA = 2.0f;

// Seguridad cama de calor (DS18B20)
constexpr float BED_MAX_C     = 30.0f;

// Anti-ciclado
constexpr uint32_t MIN_SWITCH_MS = 120000UL; // 2 min
unsigned long lastSwitchMs[6] = {0,0,0,0,0,0};

// ---- Anti doble-hotspot (verano) ----
constexpr float HEAT_ENABLE_DELTA = 0.5f; // exige que T1 sea al menos 0.5°C menor que T2 para permitir encender R1

// ---- R3 incandescente (solo de día) ----
constexpr float INC_ON_DELTA  = 1.2f; // enciende si T1 < SP - 1.2
constexpr float INC_OFF_DELTA = 0.2f; // apaga si T1 >= SP - 0.2 (queda cerca del SP)


// =====================
// Helpers
// =====================
static void safeSetRelay(int idx, bool on) {
  unsigned long now = millis();
  if (relayState[idx] == on) return;
  if (now - lastSwitchMs[idx] < MIN_SWITCH_MS) return;
  setRelay(idx, on);
  lastSwitchMs[idx] = now;
}

static void forceRelay(int idx, bool on) {
  setRelay(idx, on);
  lastSwitchMs[idx] = millis();
}

static bool timeIsValid() {
  time_t now = time(nullptr);
  return now > 1577836800;
}

static bool getLocalHM(int &hh, int &mm) {
  if (!timeIsValid()) return false;
  struct tm t;
  if (!getLocalTime(&t, 50)) return false;
  hh = t.tm_hour;
  mm = t.tm_min;
  return true;
}

static void getTimeHHMM(char out[6]) {
  int hh, mm;
  if (!getLocalHM(hh, mm)) { strncpy(out, "--:--", 6); return; }
  snprintf(out, 6, "%02d:%02d", hh, mm);
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

static void fmtFloat(char *out, size_t outLen, float v, int width = 4, int dec = 1) {
  if (isnan(v)) {
    strncpy(out, "--.-", outLen);
    out[outLen - 1] = '\0';
  } else {
    dtostrf(v, width, dec, out);
  }
}

// =====================
// Logic
// =====================
static bool isInSchedule(int hh, int mm, int onH, int onM, int offH, int offM) {
  const int cur = hh * 60 + mm;
  const int on  = onH * 60 + onM;
  const int off = offH * 60 + offM;
  if (on < off) return cur >= on && cur < off;
  return (cur >= on) || (cur < off);
}

static bool isDaytime() {
  int hh, mm;
  if (!getLocalHM(hh, mm)) return false;
  return isInSchedule(hh, mm, LIGHT_ON_HH, LIGHT_ON_MM, LIGHT_OFF_HH, LIGHT_OFF_MM);
}

static void printDegC() {
  display.write(248); // °
  display.print("C");
}

static void applyLightingControl() {
  int hh, mm;
  if (!getLocalHM(hh, mm)) return;
  bool shouldOn = isInSchedule(hh, mm, LIGHT_ON_HH, LIGHT_ON_MM, LIGHT_OFF_HH, LIGHT_OFF_MM);
  safeSetRelay(R4_LIGHT, shouldOn);
}

static void applyHeatingControl() {
  // Fail-safe: si T1 no es válida, apagar calefacción
  if (isnan(t1)) {
    forceRelay(R1_CERAMIC, false);
    forceRelay(R2_BED, false);
    forceRelay(R3_LAMP, false);
    return;
  }

  // -----------------------
  // R1 Cerámica (principal)
  // Permitir ENCENDER solo si T2 indica que el "lado frío" no está más caliente.
  // Si T2 es inválida, permitimos (para no quedarnos sin calefacción).
  // -----------------------
  const bool t2Valid = !isnan(t2);
  const bool heatAllowed = !t2Valid || (t1 <= (t2 - HEAT_ENABLE_DELTA));

  bool ceramicOn = getRelay(R1_CERAMIC);

  if (ceramicOn) {
    // Apagar normal por histéresis
    if (t1 >= (SP_HEAT + HYST_MAIN)) safeSetRelay(R1_CERAMIC, false);
    // Extra: si se invierte el gradiente fuerte, también apagar (opcional)
    if (t2Valid && t2 > (t1 + 0.8f)) safeSetRelay(R1_CERAMIC, false);
  } else {
    // Encender solo si está permitido
    if (heatAllowed && t1 <= (SP_HEAT - HYST_MAIN)) safeSetRelay(R1_CERAMIC, true);
  }

  // -----------------------
  // R2 Cama (regla fija)
  // -----------------------
  if (t1 < 26.5f) safeSetRelay(R2_BED, true);
  if (t1 > 27.5f) safeSetRelay(R2_BED, false);

  // -----------------------
  // R3 Incandescente (solo de día, apoyo suave)
  // -----------------------
  if (!isDaytime()) {
    safeSetRelay(R3_LAMP, false);
  } else {
    // ON si baja bastante, OFF si vuelve cerca del objetivo
    if (t1 < (SP_HEAT - INC_ON_DELTA))  safeSetRelay(R3_LAMP, true);
    if (t1 >= (SP_HEAT - INC_OFF_DELTA)) safeSetRelay(R3_LAMP, false);
  }
}

// Corte de seguridad de cama por DS18B20
static void applyBedSafety() {
  if (!isnan(tBed) && tBed >= BED_MAX_C) {
    forceRelay(R2_BED, false);
  }
}

// =====================
// UI (Dashboard)
// =====================
static void drawRelayBlocks(int x, int y, int w, int h, int gap) {
  for (int i = 0; i < 4; i++) {
    int bx = x + i * (w + gap);
    display.drawRect(bx, y, w, h, SH110X_WHITE);
    if (relayState[i]) display.fillRect(bx + 2, y + 2, w - 4, h - 4, SH110X_WHITE);
  }
}

void drawDashboard() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  // Header
  char hhmm[6];
  getTimeHHMM(hhmm);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(hhmm);

  display.setCursor(42, 0);
  display.print((WiFi.status() == WL_CONNECTED) ? "WiFi" : "--");

  drawRelayBlocks(86, 0, 6, 8, 2);

  // T1 grande
  char bT1[8];
  fmtFloat(bT1, sizeof(bT1), t1, 4, 1);

  display.setTextSize(1);
  display.setCursor(0, 14);
  display.print("T1");

  display.setTextSize(3);
  display.setCursor(22, 12);
  display.print(bT1);

  display.setTextSize(1);
  display.setCursor(108, 18);
  printDegC();

  // Línea media: HEAT + BED
  bool heatOn = relayState[R1_CERAMIC] || relayState[R2_BED] || relayState[R3_LAMP];

  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("HEAT:");
  display.print(heatOn ? "ON" : "OFF");

  char bBed[8];
  fmtFloat(bBed, sizeof(bBed), tBed, 4, 1);
  display.setCursor(64, 40);
  display.print("BED:");
  display.print(bBed);
  display.write(248);
  display.print("C");

  // Línea inferior: T2 + H1
  char bT2[8], bH1[8];
  fmtFloat(bT2, sizeof(bT2), t2, 4, 1);
  fmtFloat(bH1, sizeof(bH1), h1, 3, 0);

  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print("T2:");
  display.print(bT2);
  display.write(248);
  display.print(" ");

  display.print("H1:");
  display.print(bH1);
  display.print("%");

  display.display();
}

void drawUI() { drawDashboard(); }

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
    lastSwitchMs[i] = 0;
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

  // DS18B20
  ds.begin();
  ds.setWaitForConversion(false); // no bloquear
  ds.requestTemperatures();       // dispara primera conversión

  // WiFi + NTP
  connectWiFi(15000);
  if (WiFi.status() == WL_CONNECTED) setupOrResyncNTP();
}

void loop() {
  unsigned long now = millis();

  // Re-sync NTP cada 24h
  if (WiFi.status() == WL_CONNECTED && (now - lastNtpResyncMs >= NTP_RESYNC_MS)) {
    setupOrResyncNTP();
  }

  // Leer sensores cada 2s
  if (now - lastReadMs >= 2000) {
    lastReadMs = now;

    // DHTs
    float tt = dht1.readTemperature();
    float hhx = dht1.readHumidity();
    if (!isnan(tt) && !isnan(hhx)) { t1 = tt; h1 = hhx; }

    tt = dht2.readTemperature();
    hhx = dht2.readHumidity();
    if (!isnan(tt) && !isnan(hhx)) { t2 = tt; h2 = hhx; }

    // DS18B20 (no bloqueante)
    tBed = ds.getTempCByIndex(0);
    if (tBed <= -100.0f || tBed == DEVICE_DISCONNECTED_C) tBed = NAN;
    ds.requestTemperatures(); // siguiente conversión

    Serial.printf("T1=%.1fC H1=%.1f%% | T2=%.1fC H2=%.1f%% | BED=%.1fC\n", t1, h1, t2, h2, tBed);
  }

  // Control
  applyLightingControl();
  applyHeatingControl();
  applyBedSafety(); // corta cama si BED >= 30C

  // UI
  if (oledOk && (now - lastUi >= 250)) {
    lastUi = now;
    drawUI();
  }
}
