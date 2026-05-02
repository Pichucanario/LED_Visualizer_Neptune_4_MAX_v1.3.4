/*
  LED Visualizer pour ELEGOO Neptune 4 Max
  Version: 1.3.5 FR - Trident + Correction calibration
  - Correction: retour à REPOS après 5s sans mouvement des axes (calibration)
  - Emoji trident 🔱 dans le titre
  - 5 effets par état (Couleur fixe, Respiration, Clignotement, Arc-en-ciel, Vague)
  - Menu déroulant pour la configuration
  Crédits: Israel Garcia Armas avec DeepSeek
*/

#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <WebServer.h>
#include <SPIFFS.h>

// ========== CONFIGURATION ==========
#define NUM_LEDS 21
#define DATA_PIN 21
#define VERSION "1.3.5 FR"
#define DEBUG_JSON false

Adafruit_NeoPixel strip(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

// ========== IP DE L'IMPRIMANTE ==========
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

bool finishedPersistent = false;
unsigned long finishedStartTime = 0;
const unsigned long finishedDisplayDuration = 120000;
bool wasPrinting = false;
unsigned long lastProgressChangeTime = 0;
const unsigned long progressStableDelay = 2000;

// Variables pour la calibration avec retour à idle après 5s sans mouvement
float lastPosX = 0.0, lastPosY = 0.0, lastPosZ = 0.0;
bool movementDetected = false;
unsigned long movementStopTime = 0;
const unsigned long CALIBRATION_END_DELAY = 5000;   // 5 secondes sans mouvement
const float MOVEMENT_THRESHOLD = 2.0;               // 2 mm de changement
bool isCalibrating = false;
unsigned long calibrationStartTime = 0;
const unsigned long MAX_CALIBRATION_TIME = 300000;  // 5 minutes de sécurité
bool lastMovementState = false;
unsigned long noMovementStartTime = 0;

// ========== EFFETS CONFIGURABLES (5 TYPES) ==========
const int NUM_STATES = 7;
const char* stateNames[NUM_STATES] = {"idle", "heating", "printing", "paused", "finished", "error", "calibrating"};
const char* stateLabels[NUM_STATES] = {"REPOS", "CHAUFFAGE", "IMPRESSION", "PAUSE", "TERMINÉ", "ERREUR", "CALIBRATION"};

struct Effect {
  uint8_t type;      // 0=fixe, 1=respiration, 2=clignotement, 3=arc-en-ciel, 4=vague
  uint8_t r, g, b;
  uint8_t r2, g2, b2;
  int speed;
  int hueStep;
};

Effect effects[NUM_STATES];

const Effect defaultEffects[NUM_STATES] = {
  {1, 0, 80, 60,   0,0,0,   3000, 0},
  {1, 255, 140, 0,  0,0,0,   2000, 0},
  {0, 0, 255, 0,    0,0,50,  4000, 0},
  {2, 255, 200, 0,  0,0,0,   400,  0},
  {3, 0, 0, 0,      0,0,0,   20,   10},
  {2, 255, 0, 0,    0,0,0,   200,  0},
  {4, 0, 255, 0,    0,0,255, 80,   0}
};

static unsigned long lastEffectUpdate = 0;
static int hueOffset = 0;
static int wavePos = 0;
static int waveDir = 1;

// ========== PROTOTYPES ==========
void bootPhase(int phase, bool success);
void blinkAll(uint32_t color, int times, int delayMs);
void loadConfig();
void saveConfig();
void applyLedEffect();
void updatePrinterStatus();
void setupWebServer();

// ========== ANIMATION DE DÉMARRAGE ==========
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

// ========== CONFIGURATION SPIFFS ==========
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

// ========== EFFETS LED ==========
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
    for (int i = 