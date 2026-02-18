#pragma once
#include <Arduino.h>

struct InfluxTelemetry {
  float dht1_temp;
  float dht1_hum;
  float dht2_temp;
  float dht2_hum;
  float ds18_temp;
  int relay[6];
  int rssi;
};

// Inicializa NTP (opcional) y cualquier setup que quieras
void influxInit();

// Envía telemetría. Devuelve true si escribió OK (HTTP 204)
bool influxWrite(const InfluxTelemetry& t);

// Helpers de scheduling (cambio -> 1min, reposo -> 5min)
bool influxHasMeaningfulChange(const InfluxTelemetry& cur, const InfluxTelemetry& lastSent);
uint32_t influxCurrentIntervalMs();
void influxMarkChanged();  // llamar cuando detectás un cambio
