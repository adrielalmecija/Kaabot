# Terrario Controller (ESP32 + OLED + DHT22 + Relés)

Control y monitoreo de un terrario con **ESP32 NodeMCU-32S**, **OLED 1.3" SH1106 (I²C)**, **2× DHT22** y **módulo de relés** para calefacción e iluminación.

Actualmente el OLED muestra:
- **T1 / H1** (sensor 1)
- **T2 / H2** (sensor 2)

Y el ESP32 ya controla relés (base lista para agregar lógica por setpoint/horario y Wi-Fi/NTP).

---

## Wi-Fi (credenciales)

IMPORTANTE Este repo no versiona credenciales.

1. Copiar `include/secrets.example.h` a `include/secrets.h`
2. Editar `include/secrets.h` y completar `WIFI_SSID_SECRET` y `WIFI_PASS_SECRET`

> `include/secrets.h` está ignorado por Git (`.gitignore`).


## Estado

- ✅ OLED SH1106 funcionando (UI sin solapamientos)
- ✅ 2× DHT22 funcionando (T1/H1 y T2/H2)
- ✅ Módulo de relés funcionando (control desde ESP32)
- 🟡 Pendiente: Wi-Fi/NTP, lógica de luces/calefacción, humidificación/aspersores

---

## Hardware

- ESP32: **NodeMCU-32S (ESP32-WROOM)**
- Display: **OLED 1.3" SH1106 128×64 (I²C)**
- Sensores: **DHT22 / AM2302** (x2)
- Relés: **módulo de 6 canales** (con pines **VCC / JD-VCC**)

---

## Conexiones

### OLED SH1106 (I²C)
- VCC → **3V3**
- GND → **GND**
- SDA → **GPIO 21**
- SCL → **GPIO 22**

### DHT22 #1
- DATA → **GPIO 4**
- VCC → **3V3**
- GND → **GND**

### DHT22 #2
- DATA → **GPIO 16**
- VCC → **3V3**
- GND → **GND**

#### Pull-up (DATA)
Algunos DHT22 requieren una resistencia **4.7 kΩ** entre **DATA y VCC** (pull-up).  
Si el DHT22 viene en **módulo con plaquita**, normalmente ya la incluye.

---

## Módulo de relés (6 canales)

### Pines de control (ESP32 → relé)
- IN1 → **GPIO 25**
- IN2 → **GPIO 26**
- IN3 → **GPIO 27**
- IN4 → **GPIO 32**
- IN5 → **GPIO 33**
- IN6 → **GPIO 23**

### Mapeo de salidas (uso actual)
- **Relay 1 (IN1 / GPIO25):** lámpara cerámica
- **Relay 2 (IN2 / GPIO26):** cama de calor
- **Relay 3 (IN3 / GPIO27):** lámpara calor
- **Relay 4 (IN4 / GPIO32):** iluminación
- Relay 5 (IN5 / GPIO33): libre
- Relay 6 (IN6 / GPIO23): libre

### Alimentación recomendada (IMPORTANTE)
Este tipo de módulo tiene dos alimentaciones:
- **VCC**: lógica/opto (entradas IN)
- **JD-VCC**: bobinas de los relés (parte “pesada”)

**Configuración óptima para ESP32 (3.3V):**
1. **Quitar el jumper** que une **VCC ↔ JD-VCC**
2. **VCC (lógica) → 3V3 del ESP32**
3. **JD-VCC (bobinas) → 5V externo** (ej. MB102 o fuente 5V)
4. **GND común:** unir **GND ESP32 ↔ GND relé ↔ GND fuente**

> Sin GND común, el módulo no puede interpretar correctamente los niveles HIGH/LOW de IN1..IN6.

---

## Software

- Ubuntu + VSCode
- Extensión: **PlatformIO IDE**
- Board: `nodemcu-32s`
- Framework: Arduino

### Dependencias (PlatformIO)
En `platformio.ini`:

```ini
lib_deps =
  adafruit/Adafruit SH110X@^2.1.10
  adafruit/Adafruit GFX Library@^1.11.9
  adafruit/DHT sensor library@^1.4.6
  adafruit/Adafruit Unified Sensor@^1.1.14
Build / Upload / Monitor
Desde VSCode + PlatformIO:

Build: PlatformIO: Build

Upload: PlatformIO: Upload

Monitor: PlatformIO: Monitor (115200)

Si aparece Permission denied: /dev/ttyUSB0:

bash
Copiar código
sudo usermod -aG dialout $USER
# cerrar sesión y volver a entrar
Archivos
src/main.cpp: OLED + 2×DHT22 + control de relés (y/o test de relés según versión).

Notas importantes
Evitar alimentar bobinas de relés desde el ESP32: usar 5V externo (JD-VCC).

Mantener GND común entre ESP32 y relés para que IN funcione correctamente.

El DHT22 no debe leerse más rápido que cada ~2 segundos.

Para cargas 220V: cortar siempre la fase, usar caja/borneras, y mantener separación física del cableado de baja tensión.

Próximos pasos
Conectar Wi-Fi y obtener hora real por NTP

Luces por horario + calefacción por setpoint/histéresis + anti-ciclado

Humidificación por microaspersores (bomba + flotante de nivel)