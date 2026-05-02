/*
  Proyecto: LED Visualizer para ELEGOO Neptune 4 Max
  Versión: 1.3.4 - Calibración robusta con temporizador de seguridad
  - Salida de calibrating por falta de movimiento (5s, umbral 2mm)
  - Salida por cambio de extruderTarget != 140
  - Temporizador máximo de 5 minutos (300s) para forzar idle
  - Web idéntica a 1.3.3 (sin cambios)
  - DEBUG_JSON activado por defecto (cambiar a false si no se necesita)
  Créditos: Israel Garcia Armas con DeepSeek
*/

#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <WebServer.h>
#include <SPIFFS.h>

// ========== CONFIGURACIÓN ==========
#define NUM_LEDS 21
#define DATA_PIN 21
#define VERSION "1.3.4"
#define DEBUG_JSON true   // ← Activar para ver volcado JSON y mensajes de movimiento

Adafruit_NeoPixel strip(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

// ========== IP DE LA IMPRESORA ==========
const char* printerIP = "192.168.1.56";

// ========== VARIABLES GLOBALES ==========
String currentState = "idle";
float progress = 0.0;
uint8_t globalBrightness = 130;
bool autoMode = true;
String forcedState = "";
String currentFilename = "";
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 500;

float extruderTemp = 0.0, extruderTarget = 0.0;
float bedTemp = 0.0, bedTarget = 0.0;

bool errorActive = false;
unsigned long errorStartMs = 0;
const unsigned long errorDuration = 15000;
int consecutiveHttpErrors = 0;
const int maxHttpErrors = 5;

// Finished persistente (2 minutos)
bool finishedPersistent = false;
unsigned long finishedStartTime = 0;
const unsigned long finishedDisplayDuration = 120000;
bool wasPrinting = false;
unsigned long lastProgressChangeTime = 0;
const unsigned long progressStableDelay = 2000;

// ========== Detección de calibración (con temporizador de seguridad) ==========
float lastPosX = 0.0, lastPosY = 0.0, lastPosZ = 0.0;
bool movementDetected = false;
unsigned long movementStopTime = 0;
const unsigned long CALIBRATION_END_DELAY = 5000;   // 5 segundos sin movimiento
const float MOVEMENT_THRESHOLD = 2.0;               // 2 mm (más tolerante)
bool isCalibrating = false;
unsigned long calibrationStartTime = 0;             // Para temporizador máximo
const unsigned long MAX_CALIBRATION_TIME = 300000;   // 5 minutos (300 segundos)

// ========== EFECTOS CONFIGURABLES ==========
const int NUM_STATES = 7;
const char* stateNames[NUM_STATES] = {"idle", "heating", "printing", "paused", "finished", "error", "calibrating"};
const char* stateLabels[NUM_STATES] = {"REPOSO", "CALENTANDO", "IMPRIMIENDO", "PAUSA", "FINALIZADO", "ERROR", "CALIBRACIONES"};

struct Effect {
  uint8_t type;      // 0=fijo, 1=respiración, 2=parpadeo, 3=arcoíris, 4=wave
  uint8_t r, g, b;
  uint8_t r2, g2, b2;
  int speed;
  int hueStep;
};

Effect effects[NUM_STATES];

const Effect defaultEffects[NUM_STATES] = {
  {1, 0, 80, 60,   0,0,0,   3000, 0},   // idle
  {1, 255, 140, 0,  0,0,0,   2000, 0},  // heating
  {0, 0, 255, 0,    0,0,50,  4000, 0},  // printing
  {2, 255, 200, 0,  0,0,0,   400,  0},  // paused
  {3, 0, 0, 0,      0,0,0,   20,   10}, // finished
  {2, 255, 0, 0,    0,0,0,   200,  0},  // error
  {4, 0, 255, 0,    0,0,255, 80,   0}   // calibrating
};

static unsigned long lastEffectUpdate = 0;
static int hueOffset = 0;
static int wavePos = 0;
static int waveDir = 1;

// ========== PROTOTIPOS ==========
void bootPhase(int phase, bool success);
void blinkAll(uint32_t color, int times, int delayMs);
void loadConfig();
void saveConfig();
void applyLedEffect();
void updatePrinterStatus();
void setupWebServer();

// ========== ANIMACIÓN DE ARRANQUE (4 fases) ==========
void bootPhase(int phase, bool success) {
  const int snakeLen = 10;
  const int center = NUM_LEDS / 2;
  const uint32_t snakeColor = strip.Color(0, 100, 255);
  uint32_t flashColor;
  switch(phase) {
    case 1: flashColor = strip.Color(0, 100, 255); break;
    case 2: flashColor = strip.Color(255, 200, 0); break;
    case 3: flashColor = strip.Color(255, 0, 255); break;
    default: flashColor = strip.Color(0, 255, 0);
  }
  if (!success) flashColor = strip.Color(255, 0, 0);

  for (int step = 1; step <= center + snakeLen; step++) {
    strip.clear();
    for (int i = 0; i < step && i < NUM_LEDS; i++) {
      int brillo = 255 - (step - i) * 25;
      if (brillo < 50) brillo = 50;
      uint8_t r = ((snakeColor >> 16) & 0xFF) * brillo / 255;
      uint8_t g = ((snakeColor >> 8) & 0xFF) * brillo / 255;
      uint8_t b = (snakeColor & 0xFF) * brillo / 255;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    for (int i = 0; i < step && (NUM_LEDS - 1 - i) >= 0; i++) {
      int led = NUM_LEDS - 1 - i;
      int brillo = 255 - (step - i) * 25;
      if (brillo < 50) brillo = 50;
      uint8_t r = ((snakeColor >> 16) & 0xFF) * brillo / 255;
      uint8_t g = ((snakeColor >> 8) & 0xFF) * brillo / 255;
      uint8_t b = (snakeColor & 0xFF) * brillo / 255;
      strip.setPixelColor(led, strip.Color(r, g, b));
    }
    strip.show();
    delay(15);
  }
  delay(50);
  strip.clear();
  strip.setPixelColor(center, flashColor);
  strip.show();
  delay(200);
  strip.clear();
  strip.show();
  delay(100);
}

void blinkAll(uint32_t color, int times, int delayMs) {
  for (int t = 0; t < times; t++) {
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, color);
    strip.show();
    delay(delayMs);
    strip.clear();
    strip.show();
    if (t < times - 1) delay(delayMs);
  }
}

// ========== CONFIGURACIÓN SPIFFS ==========
void loadConfig() {
  if (!SPIFFS.begin(true)) {
    for (int i = 0; i < NUM_STATES; i++) effects[i] = defaultEffects[i];
    return;
  }
  if (!SPIFFS.exists("/config.json")) {
    for (int i = 0; i < NUM_STATES; i++) effects[i] = defaultEffects[i];
    return;
  }
  File f = SPIFFS.open("/config.json", "r");
  if (!f) {
    for (int i = 0; i < NUM_STATES; i++) effects[i] = defaultEffects[i];
    return;
  }
  DynamicJsonDocument doc(4096);
  deserializeJson(doc, f);
  f.close();
  for (int i = 0; i < NUM_STATES; i++) {
    JsonObject obj = doc[stateNames[i]];
    if (!obj.isNull()) {
      effects[i].type   = obj["type"]   | defaultEffects[i].type;
      effects[i].r      = obj["r"]      | defaultEffects[i].r;
      effects[i].g      = obj["g"]      | defaultEffects[i].g;
      effects[i].b      = obj["b"]      | defaultEffects[i].b;
      effects[i].r2     = obj["r2"]     | defaultEffects[i].r2;
      effects[i].g2     = obj["g2"]     | defaultEffects[i].g2;
      effects[i].b2     = obj["b2"]     | defaultEffects[i].b2;
      effects[i].speed  = obj["speed"]  | defaultEffects[i].speed;
      effects[i].hueStep = obj["hueStep"] | defaultEffects[i].hueStep;
    } else {
      effects[i] = defaultEffects[i];
    }
  }
}

void saveConfig() {
  DynamicJsonDocument doc(4096);
  for (int i = 0; i < NUM_STATES; i++) {
    JsonObject obj = doc.createNestedObject(stateNames[i]);
    obj["type"]    = effects[i].type;
    obj["r"]       = effects[i].r;
    obj["g"]       = effects[i].g;
    obj["b"]       = effects[i].b;
    obj["r2"]      = effects[i].r2;
    obj["g2"]      = effects[i].g2;
    obj["b2"]      = effects[i].b2;
    obj["speed"]   = effects[i].speed;
    obj["hueStep"] = effects[i].hueStep;
  }
  File f = SPIFFS.open("/config.json", "w");
  if (f) { serializeJson(doc, f); f.close(); }
}

// ========== EFECTOS LED ==========
void applyWaveEffect(uint32_t color1, uint32_t color2, int speedMs) {
  static unsigned long lastWaveUpdate = 0;
  if (millis() - lastWaveUpdate >= (unsigned long)speedMs) {
    lastWaveUpdate = millis();
    wavePos += waveDir;
    if (wavePos >= NUM_LEDS) {
      wavePos = NUM_LEDS - 1;
      waveDir = -1;
    } else if (wavePos < 0) {
      wavePos = 0;
      waveDir = 1;
    }
  }
  for (int i = 0; i < NUM_LEDS; i++) {
    int distance = abs(i - wavePos);
    float factor = 1.0 - (float)distance / (NUM_LEDS / 2);
    if (factor < 0) factor = 0;
    uint8_t r1 = (color1 >> 16) & 0xFF, g1 = (color1 >> 8) & 0xFF, b1 = color1 & 0xFF;
    uint8_t r2 = (color2 >> 16) & 0xFF, g2 = (color2 >> 8) & 0xFF, b2 = color2 & 0xFF;
    uint8_t r = r1 * (1-factor) + r2 * factor;
    uint8_t g = g1 * (1-factor) + g2 * factor;
    uint8_t b = b1 * (1-factor) + b2 * factor;
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

void applyLedEffect() {
  String state = (autoMode && forcedState == "") ? currentState : forcedState;
  int idx = -1;
  for (int i = 0; i < NUM_STATES; i++) if (state == stateNames[i]) idx = i;
  if (idx < 0) idx = 5;

  Effect e = effects[idx];
  unsigned long now = millis();

  if (idx == 2) {
    int ledsLit = (progress / 100.0) * NUM_LEDS;
    float breath = (sin(now * 2 * PI / e.speed) + 1) / 2;
    float intensity = 0.3 + (breath * 0.7);
    uint8_t rBar = e.r * intensity;
    uint8_t gBar = e.g * (0.5 + breath * 0.5);
    uint8_t bBar = e.b * (0.3 + breath * 0.7);
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i < ledsLit) strip.setPixelColor(i, strip.Color(rBar, gBar, bBar));
      else strip.setPixelColor(i, strip.Color(e.r2, e.g2, e.b2));
    }
    strip.show();
    return;
  }

  if (e.type == 4) {
    applyWaveEffect(strip.Color(e.r, e.g, e.b), strip.Color(e.r2, e.g2, e.b2), e.speed);
    return;
  }

  if (e.type == 0) {
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(e.r, e.g, e.b));
  }
  else if (e.type == 1) {
    float intensity = (sin(now * 2 * PI / e.speed) + 1) / 2;
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(e.r * intensity, e.g * intensity, e.b * intensity));
  }
  else if (e.type == 2) {
    bool on = (now % (e.speed * 2)) < e.speed;
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, on ? strip.Color(e.r, e.g, e.b) : 0);
  }
  else if (e.type == 3 && idx == 4) {
    if (now - lastEffectUpdate >= (unsigned long)e.speed) {
      lastEffectUpdate = now;
      hueOffset = (hueOffset + e.hueStep) % 360;
    }
    for (int i = 0; i < NUM_LEDS; i++) {
      int hue = (hueOffset + i * 360 / NUM_LEDS) % 360;
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue * 182)));
    }
  }

  strip.setBrightness(globalBrightness);
  strip.show();
}

// ========== COMUNICACIÓN CON MOONRAKER (con salidas robustas de calibración) ==========
void updatePrinterStatus() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "http://" + String(printerIP) + "/printer/objects/query?print_stats&heater_bed&extruder&display_status&virtual_sdcard&gcode_move";
  http.begin(url);
  http.setTimeout(4000);
  int code = http.GET();

  if (code != 200) {
    http.end();
    consecutiveHttpErrors++;
    if (consecutiveHttpErrors >= maxHttpErrors && !errorActive) {
      errorActive = true;
      errorStartMs = millis();
      currentState = "error";
      finishedPersistent = false;
      Serial.println("⚠️ Error por fallo HTTP consecutivo");
    }
    return;
  }

  consecutiveHttpErrors = 0;
  DynamicJsonDocument doc(8192);
  String payloadRaw = http.getString();
  if (deserializeJson(doc, payloadRaw) != DeserializationError::Ok) {
    http.end();
    return;
  }
  http.end();

  if (DEBUG_JSON) {
    static unsigned long lastDebugPrint = 0;
    if (millis() - lastDebugPrint >= 5000) {
      Serial.println("=== JSON MOONRAKER ===");
      Serial.println(payloadRaw);
      Serial.println("=== FIN JSON ===\n");
      lastDebugPrint = millis();
    }
  }

  // Leer datos
  String moonState = doc["result"]["status"]["print_stats"]["state"] | "standby";
  currentFilename = doc["result"]["status"]["virtual_sdcard"]["file_path"] | "";
  if (currentFilename == "") currentFilename = doc["result"]["status"]["print_stats"]["filename"] | "";

  extruderTemp = doc["result"]["status"]["extruder"]["temperature"] | 0.0;
  extruderTarget = doc["result"]["status"]["extruder"]["target"] | 0.0;
  bedTemp = doc["result"]["status"]["heater_bed"]["temperature"] | 0.0;
  bedTarget = doc["result"]["status"]["heater_bed"]["target"] | 0.0;

  float prog = doc["result"]["status"]["display_status"]["progress"] | 0.0;
  if (prog < 0 || prog > 1.0) prog = doc["result"]["status"]["print_stats"]["progress"] | 0.0;
  float newProgress = constrain(prog * 100.0, 0.0, 100.0);

  if (newProgress != progress) lastProgressChangeTime = millis();
  progress = newProgress;

  // DETECCIÓN DE MOVIMIENTO (calibración)
  bool currentMovement = false;
  if (extruderTarget == 140.0 && moonState != "printing" && moonState != "paused") {
    JsonArray pos = doc["result"]["status"]["gcode_move"]["position"];
    if (pos.size() >= 3) {
      float curX = pos[0].as<float>();
      float curY = pos[1].as<float>();
      float curZ = pos[2].as<float>();
      static bool firstPos = true;
      if (firstPos) {
        lastPosX = curX; lastPosY = curY; lastPosZ = curZ;
        firstPos = false;
      } else {
        float deltaX = fabs(curX - lastPosX);
        float deltaY = fabs(curY - lastPosY);
        float deltaZ = fabs(curZ - lastPosZ);
        if (deltaX > MOVEMENT_THRESHOLD || deltaY > MOVEMENT_THRESHOLD || deltaZ > MOVEMENT_THRESHOLD) {
          currentMovement = true;
        }
        lastPosX = curX; lastPosY = curY; lastPosZ = curZ;
      }
    }
  } else {
    // No estamos en condiciones de calibración, reiniciamos estado y contadores
    movementDetected = false;
    movementStopTime = 0;
    // No forzamos isCalibrating = false aquí porque puede estar en calibración todavía.
  }

  // Actualizar movimiento detectado y tiempo de parada
  if (currentMovement) {
    movementDetected = true;
    movementStopTime = 0;
  } else if (movementDetected && movementStopTime == 0) {
    movementStopTime = millis();
  }

  // LÓGICA DE CALIBRACIÓN (con múltiples condiciones de salida)
  if (autoMode && !errorActive && !finishedPersistent) {
    // Entrada en calibración
    if (extruderTarget == 140.0 && movementDetected && currentState != "printing" && currentState != "paused") {
      if (!isCalibrating) {
        isCalibrating = true;
        calibrationStartTime = millis();
        currentState = "calibrating";
        Serial.println("🔧 Calibración detectada (extrusor 140°C + movimiento).");
      }
    }

    // Salida de calibración
    if (isCalibrating) {
      bool exitCalibration = false;
      // 1. Por falta de movimiento durante CALIBRATION_END_DELAY
      if (!movementDetected && movementStopTime > 0 && (millis() - movementStopTime >= CALIBRATION_END_DELAY)) {
        exitCalibration = true;
        Serial.println("✅ Salida calibración: sin movimiento durante 5 segundos.");
      }
      // 2. Por cambio de extrusorTarget (deja de ser 140)
      if (extruderTarget != 140.0) {
        exitCalibration = true;
        Serial.println("✅ Salida calibración: extruderTarget ya no es 140.");
      }
      // 3. Por timeout (máximo 5 minutos)
      if (millis() - calibrationStartTime >= MAX_CALIBRATION_TIME) {
        exitCalibration = true;
        Serial.println("⚠️ Salida calibración por timeout (5 minutos).");
      }

      if (exitCalibration) {
        isCalibrating = false;
        currentState = "idle";
        movementDetected = false;
        movementStopTime = 0;
        Serial.println("✅ Calibración finalizada. Estado idle.");
      }
    }
  }

  // MÁQUINA DE ESTADOS (finished, error, normal)
  if (autoMode) {
    if (moonState == "error" || moonState == "cancelled") {
      if (!errorActive) {
        errorActive = true;
        errorStartMs = millis();
        currentState = "error";
        finishedPersistent = false;
        isCalibrating = false;
        Serial.println("⚠️ Error detectado");
      }
    } else {
      if (errorActive && (millis() - errorStartMs >= errorDuration)) {
        errorActive = false;
        currentState = "idle";
        finishedPersistent = false;
        Serial.println("Error resuelto");
      }

      if (!errorActive) {
        // Finished
        bool shouldFinish = false;
        if (wasPrinting && moonState == "standby" && (millis() - lastProgressChangeTime >= progressStableDelay)) {
          shouldFinish = true;
        }
        if (progress >= 99.9 && (moonState == "printing" || wasPrinting)) {
          shouldFinish = true;
        }
        if (currentFilename != "" && moonState == "standby" && bedTarget == 0 && extruderTarget == 0 && progress > 0) {
          shouldFinish = true;
        }

        if (shouldFinish && !finishedPersistent) {
          finishedPersistent = true;
          finishedStartTime = millis();
          currentState = "finished";
          Serial.println("🏁 Impresión finalizada. Estado finished durante 2 minutos.");
        }

        wasPrinting = (moonState == "printing");

        if (finishedPersistent) {
          if (millis() - finishedStartTime >= finishedDisplayDuration) {
            finishedPersistent = false;
            currentState = "idle";
            Serial.println("Fin del tiempo de finished (2 min), volviendo a idle.");
          } else if (moonState == "printing" && progress > 0.0) {
            finishedPersistent = false;
            currentState = "printing";
            Serial.println("Nueva impresión, saliendo de finished.");
          }
        } else if (!isCalibrating) {
          // Estados normales (solo si no estamos en calibración)
          bool isHeating = false;
          if (progress == 0.0) {
            if ((bedTarget > 0 && bedTemp < bedTarget - 2.0) ||
                (extruderTarget > 0 && extruderTemp < extruderTarget - 2.0)) {
              isHeating = true;
            }
          }
          if (isHeating && moonState != "complete") {
            currentState = "heating";
          }
          else if (moonState == "printing") {
            currentState = "printing";
          }
          else if (moonState == "paused") {
            currentState = "paused";
          }
          else if (moonState == "standby") {
            currentState = "idle";
          }
          else {
            currentState = "idle";
          }
        }
      }
    }
  }

  // Depuración cada 5 segundos
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug >= 5000) {
    Serial.printf("Estado: %s | Mov: %s | Ext target: %.0f | StopTime: %lu | Calibrando: %s\n",
                  currentState.c_str(), movementDetected ? "Sí" : "No", extruderTarget, movementStopTime,
                  isCalibrating ? "Sí" : "No");
    lastDebug = millis();
  }
}

// ========== SERVIDOR WEB (idéntico a 1.3.3, no se modifica) ==========
// (Se incluye el mismo HTML que funcionaba en 1.3.3; por brevedad, se usa el mismo de la versión anterior)
// Para evitar duplicar 200 líneas, se mantiene el mismo index_html de la versión 1.3.3.
// Si lo necesitas completo, pídemelo, pero asumo que ya lo tienes funcionando.
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=yes">
  <title>🌊 Neptune 4 Max LED · Beta</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      background: radial-gradient(circle at 20% 30%, #0a0f2a, #03050b);
      font-family: system-ui, -apple-system, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
      padding: 20px;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
    }
    .glass-card {
      backdrop-filter: blur(12px);
      background: rgba(15, 25, 45, 0.85);
      border-radius: 56px;
      padding: 28px 24px;
      width: 100%;
      max-width: 600px;
      border: 1px solid rgba(0, 255, 255, 0.5);
      box-shadow: 0 20px 40px rgba(0,0,0,0.6);
      text-align: center;
      color: #f0f0f0;
    }
    h1 {
      font-size: 1.8rem;
      background: linear-gradient(135deg, #FFFFFF, #00FFFF);
      -webkit-background-clip: text;
      background-clip: text;
      color: transparent;
      margin-bottom: 5px;
    }
    .beta-tag {
      font-size: 0.7rem;
      color: #aac9ff;
      background: rgba(0, 160, 255, 0.2);
      display: inline-block;
      padding: 2px 8px;
      border-radius: 20px;
      margin-left: 8px;
      vertical-align: middle;
    }
    .credit {
      font-size: 0.7rem;
      color: #aac9ff;
      margin-bottom: 20px;
    }
    .status-card {
      background: rgba(0,0,0,0.6);
      border-radius: 40px;
      padding: 20px;
      margin-bottom: 28px;
    }
    .status-label {
      font-size: 0.7rem;
      letter-spacing: 2px;
      color: #bbddff;
      text-transform: uppercase;
    }
    .status-value {
      font-size: 2rem;
      font-weight: 700;
      color: #0ff;
      text-shadow: 0 0 10px #0ff5;
      word-break: break-word;
    }
    .progress-bar-bg {
      background: #1e2a3e;
      border-radius: 60px;
      height: 18px;
      margin-top: 12px;
      overflow: hidden;
    }
    .progress-fill {
      width: 0%;
      height: 100%;
      background: linear-gradient(90deg, #2effb0, #ffcc33);
      border-radius: 60px;
      transition: width 0.3s;
    }
    .progress-text {
      font-size: 0.8rem;
      text-align: right;
      margin-top: 6px;
    }
    .filename {
      font-size: 0.65rem;
      color: #d0e4ff;
      word-break: break-all;
      margin-top: 8px;
      font-style: italic;
    }
    .temp-row {
      display: flex;
      justify-content: center;
      gap: 30px;
      margin: 20px 0;
      background: rgba(0,0,0,0.5);
      border-radius: 40px;
      padding: 12px 16px;
    }
    .temp-item {
      text-align: center;
      font-size: 1rem;
      font-weight: 600;
    }
    .temp-label {
      margin-right: 6px;
      opacity: 0.9;
      color: #ccddff;
    }
    .slider-container {
      margin: 24px 0 20px;
      text-align: left;
    }
    .slider-container label {
      display: flex;
      justify-content: space-between;
      color: #e0f0ff;
      margin-bottom: 6px;
    }
    input[type="range"] {
      width: 100%;
      height: 5px;
      background: linear-gradient(90deg, #0ff, #6a5acd);
      border-radius: 5px;
      -webkit-appearance: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 20px;
      height: 20px;
      background: white;
      border-radius: 50%;
      border: 2px solid #0ff;
      cursor: pointer;
    }
    .button-row {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      justify-content: center;
      margin: 20px 0;
    }
    button {
      background: linear-gradient(145deg, #1f3b6c, #0b1a2f);
      border: none;
      padding: 8px 16px;
      border-radius: 60px;
      color: white;
      font-weight: 600;
      font-size: 0.75rem;
      cursor: pointer;
      border: 1px solid rgba(0,255,255,0.3);
    }
    button:hover { transform: translateY(-2px); }
    .btn-auto { background: linear-gradient(145deg, #2c3e66, #0f1a2e); border-color: #0ff; }
    .config-panel {
      background: rgba(0,0,0,0.6);
      border-radius: 35px;
      padding: 15px;
      margin-top: 20px;
      text-align: left;
      border: 1px solid rgba(0,255,255,0.2);
    }
    .config-title { color: #0ff; margin-bottom: 12px; font-weight: 600; }
    select, input[type="color"], input[type="number"] {
      width: 100%;
      margin: 6px 0;
      padding: 8px;
      border-radius: 40px;
      border: none;
      background: #1e2a3e;
      color: #ffffff;
    }
    .row-flex { display: flex; gap: 10px; align-items: center; }
    .badge {
      font-size: 0.65rem;
      background: rgba(0,166,196,0.6);
      border-radius: 40px;
      padding: 4px 12px;
      display: inline-block;
      margin-top: 12px;
    }
    footer {
      font-size: 0.6rem;
      margin-top: 30px;
      color: #bfd9ff;
      text-align: center;
    }
    #toast {
      visibility: hidden;
      min-width: 250px;
      background-color: #111;
      color: #0ff;
      text-align: center;
      border-radius: 40px;
      padding: 10px 16px;
      position: fixed;
      bottom: 30px;
      left: 50%;
      transform: translateX(-50%);
      font-size: 0.8rem;
      z-index: 1000;
      backdrop-filter: blur(8px);
      border: 1px solid cyan;
    }
    #toast.show { visibility: visible; animation: fadein 0.4s, fadeout 0.5s 2.5s; }
    @keyframes fadein { from {bottom: 0; opacity: 0;} to {bottom: 30px; opacity: 1;} }
    @keyframes fadeout { from {bottom: 30px; opacity: 1;} to {bottom: 0; opacity: 0;} }
  </style>
</head>
<body>
<div class="glass-card">
  <h1>🌊 NEPTUNE 4 MAX <span class="beta-tag">Beta</span></h1>
  <div class="credit">LED Visualizer v1.3.4 · Israel Garcia Armas con DeepSeek</div>
  <div class="status-card">
    <div class="status-label">ESTADO ACTUAL</div>
    <div class="status-value" id="printerState">---</div>
    <div class="progress-bar-bg"><div class="progress-fill" id="progressFill"></div></div>
    <div class="progress-text" id="progressPercent">0%</div>
    <div class="filename" id="filename"></div>
  </div>
  <div class="temp-row">
    <div class="temp-item"><span class="temp-label">Extrusor 🌡️</span> <span id="extruderTemp">0</span>°C / <span id="extruderTarget">0</span>°C</div>
    <div class="temp-item"><span class="temp-label">Cama 🌡️</span> <span id="bedTemp">0</span>°C / <span id="bedTarget">0</span>°C</div>
  </div>
  <div class="slider-container">
    <label>💡 BRILLO GENERAL <span id="brightnessVal">100%</span></label>
    <input type="range" id="brightnessSlider" min="0" max="100" value="100">
  </div>
  <div class="button-row">
    <button id="btnHeat">🔥 CALENTANDO</button>
    <button id="btnPrint">🖨️ IMPRIMIENDO</button>
    <button id="btnPause">⏸️ PAUSA</button>
    <button id="btnFinished">🏁 FINALIZADO</button>
    <button id="btnError">⚠️ ERROR</button>
    <button id="btnIdle">💤 REPOSO</button>
    <button id="btnCalibrating">⚙️ CALIBRACIONES</button>
    <button id="btnAuto" class="btn-auto">🤖 MODO AUTO</button>
  </div>
  <div class="badge" id="modeBadge">Modo automático</div>
  <div class="config-panel" id="configPanel"></div>
  <footer>Efectos configurables · finished 2 min · Calibración con timeout 5 min</footer>
</div>
<div id="toast"></div>
<script>
  const stateNames = ["idle","heating","printing","paused","finished","error","calibrating"];
  const stateLabels = ["REPOSO","CALENTANDO","IMPRIMIENDO","PAUSA","FINALIZADO","ERROR","CALIBRACIONES"];
  let autoMode = true;
  let pruebaTimeout = null;

  function showToast(m) {
    const toast = document.getElementById('toast');
    toast.innerText = m;
    toast.className = "show";
    setTimeout(() => toast.className = "", 3000);
  }

  function updateUI(d) {
    const labels = {idle:"REPOSO", heating:"CALENTANDO", printing:"IMPRIMIENDO", paused:"PAUSA", finished:"FINALIZADO", error:"ERROR", calibrating:"CALIBRACIONES"};
    document.getElementById('printerState').innerText = labels[d.state] || d.state;
    document.getElementById('progressFill').style.width = d.progress+'%';
    document.getElementById('progressPercent').innerText = Math.round(d.progress)+'%';
    if(d.filename) document.getElementById('filename').innerText = d.filename;
    document.getElementById('modeBadge').innerText = autoMode ? '🤖 Modo automático' : '🎮 Modo manual';
    document.getElementById('extruderTemp').innerText = Math.round(d.extruderTemp||0);
    document.getElementById('extruderTarget').innerText = Math.round(d.extruderTarget||0);
    document.getElementById('bedTemp').innerText = Math.round(d.bedTemp||0);
    document.getElementById('bedTarget').innerText = Math.round(d.bedTarget||0);
  }

  function poll() { fetch('/api/status').then(r=>r.json()).then(updateUI).catch(e=>console.log); }

  function previewState(st, wasAuto) {
    fetch('/force?state='+st);
    if(pruebaTimeout) clearTimeout(pruebaTimeout);
    pruebaTimeout = setTimeout(() => {
      if(wasAuto) { fetch('/auto'); autoMode=true; showToast('Modo automático'); }
      else showToast('Fin prueba');
      pruebaTimeout = null;
    }, 5000);
  }

  function loadConfig() {
    fetch('/getEffectsConfig').then(r=>r.json()).then(cfg => {
      const container = document.getElementById('configPanel');
      container.innerHTML = '';
      for(let i=0;i<stateNames.length;i++) {
        let st = stateNames[i], label = stateLabels[i], d = cfg[st];
        let div = document.createElement('div');
        div.style.marginBottom = '20px';
        div.style.borderBottom = '1px solid rgba(0,255,255,0.2)';
        div.innerHTML = `<div style="color:#ffaa00">${label}</div>`;
        if(st == "printing") {
          let barCol = `#${(d.r<16?'0':'')+d.r.toString(16)}${(d.g<16?'0':'')+d.g.toString(16)}${(d.b<16?'0':'')+d.b.toString(16)}`;
          let bgCol = `#${(d.r2<16?'0':'')+d.r2.toString(16)}${(d.g2<16?'0':'')+d.g2.toString(16)}${(d.b2<16?'0':'')+d.b2.toString(16)}`;
          div.innerHTML += `<div>Color barra:</div><input type="color" id="barCol_${i}" value="${barCol}">`;
          div.innerHTML += `<div>Color fondo:</div><input type="color" id="bgCol_${i}" value="${bgCol}">`;
          div.innerHTML += `<div>Velocidad:</div><div class="row-flex"><input type="range" id="spd_${i}" min="100" max="5000" value="${d.speed}"><span>${d.speed}</span></div>`;
        } else {
          let tipo = d.type;
          let col = `#${(d.r<16?'0':'')+d.r.toString(16)}${(d.g<16?'0':'')+d.g.toString(16)}${(d.b<16?'0':'')+d.b.toString(16)}`;
          let opts = `<option value="0" ${tipo==0?'selected':''}>🎨 Color fijo</option>
                      <option value="1" ${tipo==1?'selected':''}>🔄 Respiración</option>
                      <option value="2" ${tipo==2?'selected':''}>⚡ Parpadeo</option>
                      <option value="3" ${tipo==3?'selected':''}>🌈 Arcoíris</option>
                      <option value="4" ${tipo==4?'selected':''}>🌊 Wave</option>`;
          div.innerHTML += `<select id="typ_${i}">${opts}</select>`;
          div.innerHTML += `<input type="color" id="col_${i}" value="${col}">`;
          div.innerHTML += `<div>Color secundario (wave/fondo):</div><input type="color" id="col2_${i}" value="#${(d.r2<16?'0':'')+d.r2.toString(16)}${(d.g2<16?'0':'')+d.g2.toString(16)}${(d.b2<16?'0':'')+d.b2.toString(16)}">`;
          if(st == "finished") {
            div.innerHTML += `<div>Paso tono:</div><input type="number" id="hue_${i}" min="1" max="30" step="1" value="${d.hueStep}" style="width:100%">`;
          }
          div.innerHTML += `<div>Velocidad:</div><div class="row-flex"><input type="range" id="spd_${i}" min="10" max="5000" value="${d.speed}"><span>${d.speed}</span></div>`;
        }
        let btnDiv = document.createElement('div');
        btnDiv.style.display = 'flex';
        btnDiv.style.gap = '8px';
        btnDiv.style.marginTop = '8px';
        let saveBtn = document.createElement('button');
        saveBtn.innerText = '💾 GUARDAR';
        saveBtn.onclick = () => {
          if(st == "printing") {
            fetch(`/saveEffect?state=${st}&color=${document.getElementById('barCol_'+i).value.substring(1)}&bgColor=${document.getElementById('bgCol_'+i).value.substring(1)}&speed=${document.getElementById('spd_'+i).value}`);
          } else {
            let extra = "";
            if(st=="finished") extra = `&hueStep=${document.getElementById('hue_'+i).value}`;
            fetch(`/saveEffect?state=${st}&type=${document.getElementById('typ_'+i).value}&color=${document.getElementById('col_'+i).value.substring(1)}&color2=${document.getElementById('col2_'+i).value.substring(1)}&speed=${document.getElementById('spd_'+i).value}${extra}`);
          }
          setTimeout(()=>loadConfig(),500);
        };
        let resetBtn = document.createElement('button');
        resetBtn.innerText = '↺ RESTAURAR';
        resetBtn.onclick = () => fetch(`/resetEffect?state=${st}`).then(()=>loadConfig());
        let testBtn = document.createElement('button');
        testBtn.innerText = '🎬 PRUEBA';
        testBtn.onclick = () => { let was = autoMode; previewState(st, was); };
        btnDiv.appendChild(saveBtn);
        btnDiv.appendChild(resetBtn);
        btnDiv.appendChild(testBtn);
        div.appendChild(btnDiv);
        container.appendChild(div);

        if(st != "printing") {
          document.getElementById(`typ_${i}`).onchange = () => {
            let tp = document.getElementById(`typ_${i}`).value;
            let col = document.getElementById(`col_${i}`).value.substring(1);
            let col2 = document.getElementById(`col2_${i}`).value.substring(1);
            let sp = document.getElementById(`spd_${i}`).value;
            let extra = (st=="finished") ? `&hueStep=${document.getElementById(`hue_${i}`).value}` : "";
            fetch(`/previewEffect?state=${st}&type=${tp}&color=${col}&color2=${col2}&speed=${sp}${extra}`);
          };
          document.getElementById(`col_${i}`).oninput = () => {
            let tp = document.getElementById(`typ_${i}`).value;
            let col = document.getElementById(`col_${i}`).value.substring(1);
            let col2 = document.getElementById(`col2_${i}`).value.substring(1);
            let sp = document.getElementById(`spd_${i}`).value;
            let extra = (st=="finished") ? `&hueStep=${document.getElementById(`hue_${i}`).value}` : "";
            fetch(`/previewEffect?state=${st}&type=${tp}&color=${col}&color2=${col2}&speed=${sp}${extra}`);
          };
          document.getElementById(`col2_${i}`).oninput = () => {
            let tp = document.getElementById(`typ_${i}`).value;
            let col = document.getElementById(`col_${i}`).value.substring(1);
            let col2 = document.getElementById(`col2_${i}`).value.substring(1);
            let sp = document.getElementById(`spd_${i}`).value;
            let extra = (st=="finished") ? `&hueStep=${document.getElementById(`hue_${i}`).value}` : "";
            fetch(`/previewEffect?state=${st}&type=${tp}&color=${col}&color2=${col2}&speed=${sp}${extra}`);
          };
          document.getElementById(`spd_${i}`).oninput = (e) => {
            e.target.nextSibling.innerText = e.target.value;
            let tp = document.getElementById(`typ_${i}`).value;
            let col = document.getElementById(`col_${i}`).value.substring(1);
            let col2 = document.getElementById(`col2_${i}`).value.substring(1);
            let sp = e.target.value;
            let extra = (st=="finished") ? `&hueStep=${document.getElementById(`hue_${i}`).value}` : "";
            fetch(`/previewEffect?state=${st}&type=${tp}&color=${col}&color2=${col2}&speed=${sp}${extra}`);
          };
          if(st=="finished" && document.getElementById(`hue_${i}`)) {
            document.getElementById(`hue_${i}`).onchange = () => {
              let tp = document.getElementById(`typ_${i}`).value;
              let col = document.getElementById(`col_${i}`).value.substring(1);
              let col2 = document.getElementById(`col2_${i}`).value.substring(1);
              let sp = document.getElementById(`spd_${i}`).value;
              let hs = document.getElementById(`hue_${i}`).value;
              fetch(`/previewEffect?state=${st}&type=${tp}&color=${col}&color2=${col2}&speed=${sp}&hueStep=${hs}`);
            };
          }
        }
      }
    });
  }

  document.getElementById('brightnessSlider').oninput = e => {
    let v = e.target.value;
    document.getElementById('brightnessVal').innerText = v+'%';
    fetch('/brightness?value='+Math.round(v*2.55));
  };
  document.getElementById('btnHeat').onclick = () => { autoMode=false; fetch('/force?state=heating'); };
  document.getElementById('btnPrint').onclick = () => { autoMode=false; fetch('/force?state=printing'); };
  document.getElementById('btnPause').onclick = () => { autoMode=false; fetch('/force?state=paused'); };
  document.getElementById('btnFinished').onclick = () => { autoMode=false; fetch('/force?state=finished'); };
  document.getElementById('btnError').onclick = () => { autoMode=false; fetch('/force?state=error'); };
  document.getElementById('btnIdle').onclick = () => { autoMode=false; fetch('/force?state=idle'); };
  document.getElementById('btnCalibrating').onclick = () => { autoMode=false; fetch('/force?state=calibrating'); };
  document.getElementById('btnAuto').onclick = () => { autoMode=true; fetch('/auto'); };

  poll();
  setInterval(poll, 500);
  loadConfig();
</script>
</body>
</html>
)rawliteral";

void setupWebServer() {
  server.on("/", []() { server.send(200, "text/html", index_html); });
  server.on("/api/status", []() {
    String stateToShow = (errorActive) ? "error" : currentState;
    if (!autoMode && forcedState != "") stateToShow = forcedState;
    String json = "{\"state\":\"" + stateToShow + "\",\"progress\":" + String(progress) + ",\"filename\":\"" + currentFilename + "\"" +
                  ",\"extruderTemp\":" + String(extruderTemp) + ",\"extruderTarget\":" + String(extruderTarget) +
                  ",\"bedTemp\":" + String(bedTemp) + ",\"bedTarget\":" + String(bedTarget) + "}";
    server.send(200, "application/json", json);
  });
  server.on("/brightness", []() { 
    if (server.hasArg("value")) {
      int val = server.arg("value").toInt();
      if (val >= 0 && val <= 255) globalBrightness = (uint8_t)val;
      strip.setBrightness(globalBrightness);
      strip.show();
    }
    server.send(200, "text/plain", "OK"); 
  });
  server.on("/force", []() { 
    if (server.hasArg("state")) { 
      String state = server.arg("state");
      bool valid = false;
      for (int i = 0; i < NUM_STATES; i++) if (state == stateNames[i]) valid = true;
      if (valid) {
        autoMode = false; 
        forcedState = state; 
        errorActive = false;
        finishedPersistent = false;
        isCalibrating = false;
      }
    } 
    server.send(200, "text/plain", "OK"); 
  });
  server.on("/auto", []() { 
    autoMode = true; 
    forcedState = ""; 
    errorActive = false;
    finishedPersistent = false;
    isCalibrating = false;
    server.send(200, "text/plain", "OK"); 
  });
  server.on("/getEffectsConfig", []() { 
    DynamicJsonDocument doc(4096); 
    for (int i=0; i<NUM_STATES; i++) { 
      JsonObject obj = doc.createNestedObject(stateNames[i]); 
      obj["type"] = effects[i].type; 
      obj["r"] = effects[i].r; 
      obj["g"] = effects[i].g; 
      obj["b"] = effects[i].b; 
      obj["r2"] = effects[i].r2; 
      obj["g2"] = effects[i].g2; 
      obj["b2"] = effects[i].b2; 
      obj["speed"] = effects[i].speed; 
      obj["hueStep"] = effects[i].hueStep; 
    } 
    String resp; 
    serializeJson(doc, resp); 
    server.send(200, "application/json", resp); 
  });
  server.on("/previewEffect", []() { 
    if (server.hasArg("state")) { 
      String st = server.arg("state"); 
      int idx = -1; 
      for (int i=0; i<NUM_STATES; i++) if (st == stateNames[i]) idx = i; 
      if (idx >= 0) { 
        Effect e = effects[idx]; 
        if (server.hasArg("type")) e.type = constrain(server.arg("type").toInt(), 0, 4);
        if (server.hasArg("color") && server.arg("color").length() == 6) { 
          long c = strtol(server.arg("color").c_str(), NULL, 16); 
          e.r = (c>>16)&0xFF; e.g = (c>>8)&0xFF; e.b = c&0xFF; 
        } 
        if (server.hasArg("color2") && server.arg("color2").length() == 6) { 
          long c = strtol(server.arg("color2").c_str(), NULL, 16); 
          e.r2 = (c>>16)&0xFF; e.g2 = (c>>8)&0xFF; e.b2 = c&0xFF; 
        } 
        if (server.hasArg("speed")) e.speed = constrain(server.arg("speed").toInt(), 10, 10000);
        if (server.hasArg("hueStep")) e.hueStep = constrain(server.arg("hueStep").toInt(), 1, 360);
        effects[idx] = e; 
        applyLedEffect(); 
      } 
    } 
    server.send(200, "text/plain", "OK"); 
  });
  server.on("/saveEffect", []() { 
    if (server.hasArg("state")) { 
      String st = server.arg("state"); 
      int idx = -1; 
      for (int i=0; i<NUM_STATES; i++) if (st == stateNames[i]) idx = i; 
      if (idx >= 0) { 
        if (server.hasArg("type")) effects[idx].type = constrain(server.arg("type").toInt(), 0, 4);
        if (server.hasArg("color") && server.arg("color").length() == 6) { 
          long c = strtol(server.arg("color").c_str(), NULL, 16); 
          effects[idx].r = (c>>16)&0xFF; effects[idx].g = (c>>8)&0xFF; effects[idx].b = c&0xFF; 
        } 
        if (server.hasArg("color2") && server.arg("color2").length() == 6) { 
          long c = strtol(server.arg("color2").c_str(), NULL, 16); 
          effects[idx].r2 = (c>>16)&0xFF; effects[idx].g2 = (c>>8)&0xFF; effects[idx].b2 = c&0xFF; 
        } 
        if (server.hasArg("speed")) effects[idx].speed = constrain(server.arg("speed").toInt(), 10, 10000);
        if (server.hasArg("hueStep")) effects[idx].hueStep = constrain(server.arg("hueStep").toInt(), 1, 360);
        saveConfig(); 
      } 
    } 
    server.send(200, "text/plain", "OK"); 
  });
  server.on("/resetEffect", []() { 
    if (server.hasArg("state")) { 
      String st = server.arg("state"); 
      int idx = -1; 
      for (int i=0; i<NUM_STATES; i++) if (st == stateNames[i]) idx = i; 
      if (idx >= 0) { 
        effects[idx] = defaultEffects[idx]; 
        saveConfig(); 
      } 
    } 
    server.send(200, "text/plain", "OK"); 
  });
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== Neptune 4 Max LED Visualizer %s ===\n", VERSION);

  strip.begin();
  strip.show();
  strip.setBrightness(globalBrightness);

  bootPhase(1, true);
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.autoConnect("Neptune4-Lights");
  Serial.print("WiFi IP: ");
  Serial.println(WiFi.localIP());

  bootPhase(2, true);
  if (!SPIFFS.begin(true)) {
    Serial.println("Error SPIFFS, usando defaults");
    for (int i = 0; i < NUM_STATES; i++) effects[i] = defaultEffects[i];
  } else {
    loadConfig();
  }

  bootPhase(3, true);
  updatePrinterStatus();

  bootPhase(4, true);
  blinkAll(strip.Color(0, 255, 0), 4, 200);

  setupWebServer();
  server.begin();
  Serial.print("Servidor web: http://");
  Serial.println(WiFi.localIP());
  Serial.println("Sistema listo.\n");
}

void loop() {
  server.handleClient();
  if (millis() - lastUpdate >= updateInterval) {
    if (autoMode) updatePrinterStatus();
    lastUpdate = millis();
  }
  applyLedEffect();
  delay(5);
}
