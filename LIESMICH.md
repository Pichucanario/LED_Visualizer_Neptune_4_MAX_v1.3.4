# 🔱 LED Visualizer für ELEGOO Neptune 4 Max

**Stabile Version: 1.3.5 (DE / EN / ES / FR)**  
*Autor: Israel Garcia Armas mit DeepSeek*

![LED Visualizer Vorschau](images/demo.gif)

---

## 📖 Projektursprung

Dieses Projekt ist die natürliche Weiterentwicklung eines LED-Visualisierungssystems, das ursprünglich für meinen **Snapmaker U1**-Drucker entwickelt wurde. Der Snapmaker U1 wurde ab Werk mit einer **geschlossenen, stark modifizierten Klipper-Firmware** ausgeliefert, die den Zugriff auf die Standard-Moonraker-API verhinderte. Ich habe die **ursprüngliche Firmware modifiziert und mit Klipper Extended erweitert**, um die Kommunikation zu öffnen und auf Echtzeitdaten des Druckers zuzugreifen. Diese erste Version war ein Erfolg.

Aufbauend auf dieser Erfahrung habe ich das System für **Standard-Klipper** auf meinem **ELEGOO Neptune 4 Max** (ein Großformatdrucker, den ich bereits in meiner Werkstatt hatte) angepasst. Das Ergebnis ist dieses voll funktionsfähige, robuste und leicht **auf jeden Klipper + Moonraker Drucker übertragbare System**.

---

## 🎯 Hauptziele

- **Echtzeit-Überwachung** der Druckerzustände: Leerlauf, Aufheizen, Drucken (mit Fortschrittsbalken), Pause, Fertig, Fehler und Betttablett-Kalibrierung.
- **Übersichtliche, responsive Weboberfläche** (mobil, Tablet, PC) zur Anzeige von Temperaturen, Fortschritt und zur Konfiguration der Effekte.
- **Vollständig anpassbare LED-Effekte** für jeden Zustand (Feste Farbe, Atmung, Blinken, Regenbogen, Welle) mit SPIFFS-Speicherung.
- **Automatische Betttablett-Kalibrierungserkennung** mittels Extrudertemperatur (140°C) und Achsenbewegung.
- **Attraktive Startanimation** (zwei blaue Schlangen, die sich in der Mitte treffen + Farbblitz pro Phase).

---

## 🧩 Komponenten & Technologien

| Bereich | Komponente / Technologie |
| :--- | :--- |
| **Mikrocontroller** | ESP32 (NodeMCU-32S oder ähnlich) |
| **LED-Streifen** | NeoPixel (WS2812B) – 21 LEDs (konfigurierbar) |
| **Drucker-Firmware** | Klipper + Moonraker (Standard auf Neptune 4 Max) |
| **Arduino-Bibliotheken** | WiFiManager, ArduinoJson, Adafruit_NeoPixel, WebServer, SPIFFS |
| **Entwicklungsumgebung** | Arduino IDE 2.x |
| **Kommunikation** | HTTP (JSON über Moonraker) |
| **Weboberfläche** | HTML5, CSS3, JavaScript (fetch, dynamisches DOM) |

---

## ✨ Hauptmerkmale

### 📡 WiFiManager – Einfache & dauerhafte WiFi-Einrichtung

Das System verwendet **WiFiManager** für eine autonome WiFi-Verbindung, ohne Credentials fest zu codieren.

- **Erster Start** (oder keine gespeicherten Credentials): Der ESP32 erstellt einen Access Point mit dem Namen `Neptune4-Lights` (kein Passwort). Verbinde dich damit, und ein gefangenes Portal erscheint, um dein heimisches WLAN auszuwählen und das Passwort einzugeben.
- **Credential-Speicherung**: Gespeichert im Flash-Speicher des ESP32. Bei späteren Starts verbindet er sich automatisch.
- **Fehlerbehandlung**: Wenn die Verbindung fehlschlägt (z. B. geändertes Passwort), wird der AP zur erneuten Konfiguration neu erstellt.

### 🚀 4‑Phasen‑Startanimation

1. **WiFi** – blaue Schlangen + blauer Blitz in der Mitte.
2. **SPIFFS** – blaue Schlangen + gelber Blitz.
3. **Moonraker** – blaue Schlangen + magenta Blitz.
4. **Erfolg** – 4 grüne Blinks auf dem gesamten LED-Streifen.

### 🤖 Automatische Druckerzustände

| Zustand | Standard-LED-Effekt | Beschreibung |
| :--- | :--- | :--- |
| `idle` (Leerlauf) | sanftes grünes Atmen | Drucker in Ruhe, keine Datei geladen. |
| `heating` (Aufheizen) | oranges Atmen | Heizt Bett oder Düse auf. |
| `printing` (Drucken) | blauer Fortschrittsbalken mit langsamem Atmen (4s Zyklus) | Druck läuft, Balken füllt sich entsprechend dem tatsächlichen Prozentsatz. |
| `paused` (Pause) | gelbes Blinken | Druck pausiert. |
| `finished` (Fertig) | Regenbogen (bleibt 2 Minuten) | Druck abgeschlossen. Kann über das Web (LEERLAUF oder AUTO-MODUS) abgebrochen werden. |
| `error` (Fehler) | rotes Blinken | Fehler oder Abbruch (`error` / `cancelled`). |
| `calibrating` (Kalibrieren) | grün/blaue Welle | Automatische Betttablett-Kalibrierung erkannt (Düse bei 140°C + Achsenbewegung). |

### 🔧 Intelligente Kalibrierungserkennung

- **Auslöser**: Die Zieltemperatur der Düse erreicht genau 140°C (der Wert, der während des Betttablett-Nivellierens verwendet wird).
- **Überwachung**: Erkennt Bewegung auf den Achsen X, Y, Z (Schwelle > 2 mm).
- **Beendigungsbedingungen**:
  - Keine Bewegung für **5 aufeinanderfolgende Sekunden**.
  - Die Zieltemperatur der Düse ändert sich von 140°C.
  - **Sicherheits-Timeout** von 5 Minuten.

### 🌐 Weboberfläche

- **Sauberes, kontrastreiches Design** (helle Schrift auf dunklem Hintergrund, keine dunkel‑auf‑dunkel‑Probleme).
- **Echtzeitanzeige**: aktueller Zustand, Fortschrittsbalken, Dateiname, aktuelle und Zieltemperaturen (Düse und Bett).
- **Helligkeitsregelung** (0–255).
- **Manuelle Modus-Tasten** zum Erzwingen eines beliebigen Zustands; kehrt nach 5 Sekunden in den Automatikmodus zurück.
- **Effektkonfigurationsbereich** (standardmäßig ausgeblendet, durch einen Button einblendbar):
  - Dropdown zur Auswahl des Effekttyps (feste Farbe, Atmung, Blinken, Regenbogen, Welle).
  - Farbwähler (primäre und sekundäre Farbe für Welle oder Fortschrittsbalken-Hintergrund).
  - Geschwindigkeitseinstellung (ms pro Zyklus).
  - Live-Vorschau, Speichern und Wiederherstellen der Standardeinstellungen.
- **Versionsinfo**: Versionsnummer, "Beta"-Tag, Autorenangaben.

### 🔄 Dauerhafter `finished`-Zustand

- Nach einem Druck bleibt `finished` für **2 Minuten** aktiv.
- Manuell abbrechbar über die Web-Buttons **LEERLAUF** oder **AUTO-MODUS**.
- Wird automatisch beendet, wenn ein neuer Druck startet.

### 🛠️ Konfigurationsspeicherung

- Benutzerdefinierte Effekte werden in SPIFFS (`/config.json`) gespeichert.
- Nach einem ESP32-Neustart wird die letzte Konfiguration automatisch wiederhergestellt.

---

## ✅ Vorteile & Pluspunkte

- **Keine Änderung der Drucker-Firmware nötig** – funktioniert mit der standardmäßigen Moonraker-API.
- **Vollständig anpassbar** (Farben, Effekte, Geschwindigkeiten) über eine einfache Weboberfläche.
- **Echtzeit-Reaktionsfähigkeit** (Aktualisierung alle 500 ms).
- **Einfache Installation** – direkter Anschluss an die ESP32-Pins, keine zusätzlichen Komponenten.
- **Sehr niedrige Kosten** (ca. 15–30 € an Hardware).
- **Skalierbar** – die Anzahl der LEDs ist durch Ändern einer Konstanten einstellbar.
- **Portabel** – der ESP32 kann über jeden USB-Port mit Strom versorgt werden, auch über den des Druckers.
- **Kompatibel** mit jedem Klipper + Moonraker Drucker (nicht nur Neptune 4 Max).

---

## 📦 Benötigte Hardware & geschätzte Kosten

| Komponente | Ungefährer Preis |
| :--- | :--- |
| ESP32 (NodeMCU-32S) | 5–8 € |
| NeoPixel LED-Streifen (21 LEDs, WS2812B) | 5–10 € |
| Kabel & Verbinder | 1–2 € |
| 5V Netzteil (optional, falls nicht per USB betrieben) | 5–10 € |
| **Gesamt** | **15–30 €** |

---

## 🔧 Wichtig: Den Daten-Pin ändern

Der Code verwendet standardmäßig **GPIO 21** als Daten-Pin für den NeoPixel-Streifen (`DATA_PIN`). Wenn du deinen LED-Streifen an einen anderen Pin angeschlossen hast, musst du diese Konstante **vor dem Kompilieren ändern**:

1. Öffne den Code in der Arduino IDE.
2. Suche diese Zeile am Anfang:
   ```cpp
   #define DATA_PIN 21