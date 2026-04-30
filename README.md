# 🌊 LED Visualizer para Impresoras 3D con Klipper

**Versión estable para ELEGOO Neptune 4 Max: 1.3.4**  
*Autor: Israel Garcia Armas con DeepSeek*

---

## 📖 Origen del proyecto

Este proyecto nace como una **evolución natural** de un sistema de visualización LED que desarrollé originalmente para mi **impresora Snapmaker U1**. 

La Snapmaker U1 venía de fábrica con un **firmware Klipper cerrado y fuertemente modificado por el fabricante**, lo que impedía acceder a la API estándar de Moonraker y personalizar su comportamiento. Para poder implementar un sistema de retroalimentación visual, **modifiqué el firmware original y lo extendí con Klipper Extended**, lo que me permitió abrir la comunicación y acceder a los datos de la impresora en tiempo real. Aquella primera versión del visualizador LED funcionó con éxito en la Snapmaker U1, demostrando la viabilidad del concepto.

Posteriormente, apliqué los conocimientos adquiridos y adapté el sistema para **Klipper estándar** en mi **ELEGOO Neptune 4 Max** (impresora de gran formato que ya tenía en mi taller). El resultado es este proyecto totalmente funcional, robusto y fácilmente **portable a cualquier otra impresora que ejecute Klipper + Moonraker**.

Mi objetivo es **adaptar este sistema a todas mis impresoras basadas en Klipper**, independientemente del fabricante, siempre que expongan la API de Moonraker.

---

## 🎯 Objetivos principales

- **Monitorizar en tiempo real** el estado de la impresora: reposo, calentando, imprimiendo (con barra de progreso), pausa, finalizado, error y calibración de cama.
- **Interfaz web legible y responsive** desde cualquier dispositivo (móvil, tablet, PC) que muestre temperaturas, progreso y permita configurar efectos de color.
- **Personalización completa de efectos LED** para cada estado (color fijo, respiración, parpadeo, arcoíris, wave) con almacenamiento en SPIFFS.
- **Detección automática de la calibración de cama** (nivelación), algo que no es trivial en Klipper, mediante una lógica heurística de movimiento de ejes y temperatura del extrusor a 140 °C.
- **Animación de arranque atractiva** (dos serpientes azules que chocan en el centro + destello de color por fase) que informa del estado del sistema (WiFi, SPIFFS, Moonraker, éxito final).

---

## 🧩 Componentes y tecnologías

| Área | Componente / Tecnología |
|------|--------------------------|
| **Microcontrolador** | ESP32 (NodeMCU-32S o similar) |
| **Tira LED** | NeoPixel (WS2812B) – 21 LEDs (configurable) |
| **Firmware de la impresora** | Klipper + Moonraker (estándar en Neptune 4 Max) |
| **Librerías Arduino** | WiFiManager, ArduinoJson, Adafruit_NeoPixel, WebServer, SPIFFS |
| **Entorno de desarrollo** | Arduino IDE 2.x |
| **Comunicación** | HTTP (JSON sobre Moonraker) |
| **Interfaz web** | HTML5, CSS3, JavaScript (fetch, manipulación dinámica) |

---

## ✨ Características destacadas

### 📡 WiFiManager – Configuración WiFi sencilla y persistente

El sistema utiliza la librería **WiFiManager** para gestionar la conexión a la red WiFi de forma totalmente autónoma y sin necesidad de hardcodear credenciales en el código.

**¿Cómo funciona?**
- **Primer arranque (o si no hay credenciales guardadas):**  
  El ESP32 crea un punto de acceso (AP) llamado **`Neptune4-Lights`** (sin contraseña). Desde cualquier dispositivo te conectas a esa red y se abre un portal cautivo donde seleccionas tu WiFi doméstica e introduces la contraseña.
- **Almacenamiento de credenciales:**  
  Las credenciales se guardan en la memoria flash del ESP32. En reinicios posteriores, el ESP32 se conecta automáticamente a la red guardada.
- **Fallo de conexión:**  
  Si no puede conectar (por ejemplo, cambio de contraseña), vuelve a crear el AP para reconfigurarlo.

**Ventajas:**  
No hay que modificar el código para cambiar de red; es fácil de usar en diferentes ubicaciones.

### 🚀 Arranque (boot) visual de 4 fases
1. **WiFi** – serpientes azules + destello azul en el LED central.
2. **SPIFFS** – serpientes azules + destello amarillo.
3. **Moonraker** – serpientes azules + destello magenta.
4. **Éxito final** – 4 parpadeos verdes en toda la tira.

### 🤖 Estados automáticos de la impresora
| Estado | Efecto LED por defecto | Descripción |
|--------|------------------------|-------------|
| `idle` | respiración verde tenue | Impresora en reposo, sin archivo cargado. |
| `heating` | naranja respirando | Calentando cama o extrusor. |
| `printing` | barra de progreso azul con respiración lenta (4 s) | Impresión en curso, la barra se llena según el porcentaje real. |
| `paused` | amarillo parpadeante | Impresión en pausa. |
| `finished` | arcoíris (persiste 2 min) | Impresión finalizada. Se puede cancelar desde la web. |
| `error` | rojo parpadeante | Error o cancelación (`error` / `cancelled`). |
| `calibrating` | wave verde/azul | Calibración de cama detectada automáticamente. |

### 🔧 Detección inteligente de calibración de cama
- Se activa cuando `extruderTarget == 140.0` (valor exacto que alcanza la impresora durante la nivelación).
- Monitorea **movimiento en los ejes X, Y y Z** (umbral > 2 mm).
- Finaliza cuando:
  - No hay movimiento durante **5 segundos** consecutivos.
  - `extruderTarget` deja de ser 140 °C (la impresora apaga el calentador).
  - **Timeout de seguridad** de 5 minutos (por si el movimiento nunca cesa).

### 🌐 Interfaz web
- **Diseño claro y contrastado** (texto claro sobre fondo oscuro, sin fuentes oscuras ilegibles).
- **Visualización en tiempo real**: estado actual, barra de progreso, nombre del archivo, temperaturas actuales y objetivo (extrusor y cama).
- **Control de brillo** (0–255).
- **Botones manuales** para forzar estados (modo manual) y volver a modo automático.
- **Panel de configuración de efectos**:
  - Selector de tipo de efecto (color fijo, respiración, parpadeo, arcoíris, wave).
  - Selector de color (primario y secundario para wave/fondo de barra).
  - Ajuste de velocidad (ms por ciclo).
  - Preview en tiempo real, guardado y restauración de valores por defecto.
- **Información de versión**: número de versión, etiqueta “Beta”, créditos del autor.

### 🔄 Finished persistente
- Después de una impresión, el estado `finished` se mantiene **2 minutos**.
- Se puede cancelar manualmente desde la web con los botones **"REPOSO"** o **"MODO AUTO"**.
- También se cancela automáticamente si se inicia una nueva impresión.

### 🛠️ Almacenamiento de configuración
- Los efectos personalizados se guardan en SPIFFS (`/config.json`).
- Al reiniciar el ESP32, se recupera la última configuración.

---

## ✅ Ventajas y pros

- **No requiere modificar el firmware de la impresora** (funciona con Moonraker estándar en cualquier impresora Klipper).
- **Totalmente configurable** (colores, efectos, velocidades) desde una interfaz web sencilla.
- **Respuesta en tiempo real** (actualización cada 500 ms).
- **Fácil instalación** (conexión directa a los pines del ESP32, sin componentes adicionales).
- **Bajo coste** (aprox. 15–30 €).
- **Escalable** (se puede cambiar el número de LEDs sin más que ajustar una constante).
- **Portable** (el ESP32 puede alimentarse desde cualquier puerto USB, incluso desde la propia impresora).
- **Compatibilidad** con cualquier impresora que ejecute Klipper + Moonraker (no solo Neptune 4 Max).

---

## 📦 Hardware necesario y coste estimado

| Componente | Coste aprox. |
|------------|--------------|
| ESP32 (NodeMCU-32S) | 5–8 € |
| Tira de LEDs NeoPixel (21 LEDs, WS2812B) | 5–10 € |
| Cables y conectores | 1–2 € |
| Fuente de alimentación 5V (opcional si no se usa USB) | 5–10 € |
| **Total** | **15–30 €** |

---

## 🔧 Limitaciones y cosas a mejorar

- **Detección de calibración heurística**: podrían darse falsos positivos en movimientos atípicos (aunque umbrales ajustados).
- **Web no es PWA** (no instalable como app, pero totalmente funcional en navegador).
- **Sin timestamps reales** (el ESP32 no tiene RTC; se usan los del monitor serie).
- **Configuración en SPIFFS**: podría corromperse en cortes de energía extremos (se restaura a valores por defecto).

---

## 🔮 Posibles futuras mejoras

- **Soporte para más de 7 estados** (personalizables).
- **Detección directa del comando `BED_MESH_CALIBRATE`** (si Moonraker lo expone).
- **Notificaciones push** (Telegram, Blynk, etc.).
- **Modo “simulación”** para pruebas sin impresora real.
- **Integración con Home Assistant** (MQTT o API REST).
- **Ampliación de efectos** (estela, fuego, relámpago) sin sobrecargar el ESP32.
- **Traducción al inglés** de la interfaz web.
- **Control por botón físico** para alternar modo automático/manual.
- **Adaptación automática a diferentes impresoras** (detección dinámica de ejes, límites).

---

## ⚙️ Instalación y configuración

### 1. Instalar las librerías necesarias (Arduino IDE)
- `WiFiManager` (de tzapu)
- `ArduinoJson` (versión 6)
- `Adafruit_NeoPixel`
- `WebServer` (incluida con el núcleo ESP32)
- `SPIFFS` (incluida)

### 2. Configurar la placa
- Seleccionar **ESP32 Dev Module** en el Arduino IDE.
- Puerto COM correspondiente.

### 3. Subir el código
- Copiar el código completo (versión 1.3.4) en un nuevo sketch.
- Compilar y subir al ESP32.

### 4. Primer inicio (configuración WiFi)
- El ESP32 creará un punto de acceso llamado **`Neptune4-Lights`** (sin contraseña).
- Conectarse desde un móvil o PC a esa red.
- Se abrirá un portal cautivo; seleccionar la red WiFi doméstica y escribir su contraseña.
- El ESP32 se reiniciará y se conectará a la red doméstica.

### 5. Acceder a la interfaz web
- Abrir el monitor serie (115200 baudios) para ver la IP del ESP32 (ej. `192.168.1.39`).
- En un navegador, entrar a `http://[IP_del_ESP32]`.
- ¡Ya se puede usar!

---

## 📄 Licencia

Este proyecto se distribuye bajo licencia **MIT**, lo que permite su uso, copia, modificación y distribución libre, siempre que se incluya el aviso de copyright y la licencia original.

---

## 🙏 Agradecimientos

- A la comunidad de **Klipper y Moonraker** por proporcionar una API documentada y estable.
- A los desarrolladores de las librerías `WiFiManager`, `ArduinoJson` y `Adafruit_NeoPixel`.
- A **DeepSeek** por la asistencia en la depuración y el desarrollo del código.

---

**¡Disfruta de tu visualizador LED y que tus impresiones sean siempre un éxito!** 🌊🖨️✨
