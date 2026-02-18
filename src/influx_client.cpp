#include "influx_client.h"
#include "secrets.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <math.h>

static const uint32_t INTERVAL_ACTIVE_MS = 60UL * 1000UL;       // 1 min
static const uint32_t INTERVAL_IDLE_MS   = 5UL * 60UL * 1000UL; // 5 min
static const uint32_t ACTIVE_WINDOW_MS   = 10UL * 60UL * 1000UL;

static const float TEMP_EPS = 0.2f;
static const float HUM_EPS  = 1.0f;

static uint32_t lastChangeMs = 0;

static bool differs(float a, float b, float eps) {
  if (isnan(a) && isnan(b)) return false;
  if (isnan(a) != isnan(b)) return true;
  return fabsf(a - b) >= eps;
}

void influxInit() {
  lastChangeMs = millis();
}

void influxMarkChanged() {
  lastChangeMs = millis();
}

uint32_t influxCurrentIntervalMs() {
  if (millis() - lastChangeMs < ACTIVE_WINDOW_MS) return INTERVAL_ACTIVE_MS;
  return INTERVAL_IDLE_MS;
}

bool influxHasMeaningfulChange(const InfluxTelemetry& cur, const InfluxTelemetry& lastSent) {
  if (differs(cur.dht1_temp, lastSent.dht1_temp, TEMP_EPS)) return true;
  if (differs(cur.dht2_temp, lastSent.dht2_temp, TEMP_EPS)) return true;
  if (differs(cur.ds18_temp, lastSent.ds18_temp, TEMP_EPS)) return true;

  if (differs(cur.dht1_hum, lastSent.dht1_hum, HUM_EPS)) return true;
  if (differs(cur.dht2_hum, lastSent.dht2_hum, HUM_EPS)) return true;

  for (int i = 0; i < 6; i++) {
    if (cur.relay[i] != lastSent.relay[i]) return true;
  }
  return false;
}

static String buildLineProtocol(const InfluxTelemetry& t) {
  // Measurement + tags
  String line = "terrario,device=";
  line += DEVICE_ID;

  // Fields
  line += " ";
  line += "dht1_temp="; line += (isnan(t.dht1_temp) ? "nan" : String(t.dht1_temp, 2));
  line += ",dht1_hum=";  line += (isnan(t.dht1_hum)  ? "nan" : String(t.dht1_hum, 1));
  line += ",dht2_temp="; line += (isnan(t.dht2_temp) ? "nan" : String(t.dht2_temp, 2));
  line += ",dht2_hum=";  line += (isnan(t.dht2_hum)  ? "nan" : String(t.dht2_hum, 1));
  line += ",ds18_temp="; line += (isnan(t.ds18_temp) ? "nan" : String(t.ds18_temp, 2));

  line += ",relay1="; line += String(t.relay[0]) + "i";
  line += ",relay2="; line += String(t.relay[1]) + "i";
  line += ",relay3="; line += String(t.relay[2]) + "i";
  line += ",relay4="; line += String(t.relay[3]) + "i";
  line += ",relay5="; line += String(t.relay[4]) + "i";
  line += ",relay6="; line += String(t.relay[5]) + "i";

  line += ",rssi="; line += String(t.rssi) + "i";

  // Timestamp opcional (precision=s). Si no hay NTP, se omite y usa server time.
  time_t now = time(nullptr);
  if (now > 100000) {
    line += " ";
    line += String((long)now);
  }

  return line;
}

bool influxWrite(const InfluxTelemetry& t) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure(); // luego lo endurecemos con CA

  HTTPClient http;
  String url = String(INFLUX_URL) + "/api/v2/write?org=" + INFLUX_ORG
             + "&bucket=" + INFLUX_BUCKET + "&precision=s";

  if (!http.begin(client, url)) return false;

  http.addHeader("Authorization", String("Token ") + INFLUX_TOKEN);
  http.addHeader("Content-Type", "text/plain; charset=utf-8");

  String lp = buildLineProtocol(t);
  int code = http.POST(lp);
  http.end();

  return code == 204;
}
