# Terrario Controller (ESP32 + OLED + DHT22)

Proyecto para automatizar y monitorear un terrario usando un **ESP32 NodeMCU-32S**, un **OLED 1.3" SH1106 (I²C)** y **sensores DHT22**.  
Actualmente muestra en pantalla **T1/T2** (temperatura) y **H1/H2** (humedad) y deja lista la base para sumar **relés** (luces + calefacción) y riego/nebulización.

---

## Estado del proyecto

✅ ESP32 + PlatformIO (VSCode en Ubuntu)  
✅ OLED SH1106 (I²C) funcionando con UI limpia (sin solapamientos)  
✅ 2× DHT22 leyendo y mostrando:
- T1, H1
- T2, H2

🟡 Pendiente / Próximos pasos
- Integrar módulo de relés (luces por horario + calefacción por setpoint/histéresis)
- Hora real por NTP (para horarios)
- Seguridad: watchdog, anti-ciclado relés, alarmas de lectura inválida
- Humidificación por microaspersores (bomba + flotante de nivel)

---

## Hardware

- ESP32: **NodeMCU-32S (ESP32-WROOM)**
- Display: **OLED 1.3" I²C SH1106 (128×64)**
- Sensores: **DHT22 / AM2302** (x2 por ahora)
- Alimentación: 5V (recomendado), con distribución a 3.3V para sensores si aplica

---

## Conexiones

### OLED SH1106 (I²C)
> Pines típicos en ESP32

- **VCC → 3V3**
- **GND → GND**
- **SDA → GPIO 21**
- **SCL → GPIO 22**

### DHT22 #1
- **DATA (S) → GPIO 4**
- **VCC (+) → 3V3**
- **GND (–) → GND**

### DHT22 #2
- **DATA (S) → GPIO 16**
- **VCC (+) → 3V3**
- **GND (–) → GND**

#### Pull-up (importante)
Algunos DHT22 requieren resistencia **4.7kΩ** entre **DATA y VCC** (pull-up).
- Si el DHT22 viene montado en **módulo con plaquita**, normalmente ya la incluye.
- Si viene suelto (sensor + cable), agregar pull-up es recomendado.

---

## Requisitos de software

- Ubuntu + VSCode
- Extensión **PlatformIO IDE**
- Board: `nodemcu-32s`
- Framework: Arduino (via PlatformIO)

### Dependencias (PlatformIO)
El proyecto usa:

- Adafruit SH110X
- Adafruit GFX
- DHT sensor library
- Adafruit Unified Sensor

Ya están definidas en `platformio.ini` (o agregar si faltan):

```ini
lib_deps =
  adafruit/Adafruit SH110X@^2.1.10
  adafruit/Adafruit GFX Library@^1.11.9
  adafruit/DHT sensor library@^1.4.6
  adafruit/Adafruit Unified Sensor@^1.1.14
Compilar y flashear
En VSCode + PlatformIO:

Conectar el ESP32 por USB

Build: PlatformIO: Build

Upload: PlatformIO: Upload

Monitor serie: PlatformIO: Monitor (115200)

Si el upload falla por permisos (/dev/ttyUSB0 permission denied):

bash
Copiar código
sudo usermod -aG dialout $USER
# cerrar sesión y entrar de nuevo
UI actual (OLED)
Muestra:

Header: hora mock (por ahora) + estado D1/D2

Bloque 1: T1 grande + H1

Bloque 2: T2 grande + H2

Símbolo de grados ° habilitado con CP437 en Adafruit_GFX

Archivo principal
src/main.cpp contiene:

init OLED

init DHT22 x2

lectura cada 2000ms

UI refresco cada ~250ms

Próximos pasos sugeridos
Hora real por NTP

WiFi + configTime() / configTzTime()

Relés

Control luces por horario

Control calefacción por setpoint + histéresis + anti-ciclado

Humidificación

Bomba (ideal diafragma 12V) + microaspersores

Sensor de nivel (flotante) para protección

Notas
Evitar usar el 5V del ESP32 para alimentar relés (se recomienda fuente externa).

Mantener GND común en todos los módulos (salvo aislamiento bien implementado).

DHT22: no leer más rápido que cada 2 segundos.