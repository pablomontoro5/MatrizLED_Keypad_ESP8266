/**
* @file matriz_8x8_oled_esp8266.ino
* @brief Control de una matriz NeoPixel 8x8, una pantalla OLED I2C
* y un keypad 4x4 desde una web en un ESP8266.
*
* Funcionalidades:
* - Servidor web en el ESP8266.
* - Página web con una matriz 8x8 clickable.
* - Selección de color en la web.
* - Cada clic en una casilla: pinta; segundo clic en la misma: borra.
* - Matriz física NeoPixel 8x8 sincronizada con la web.
* - Pantalla OLED I2C mostrando el color seleccionado.
* - Al reiniciar la matriz, la OLED muestra "Reiniciando... / Matriz...".
* - Keypad 4x4 con un cursor que:
* - Empieza en (0,0).
* - Se mueve con 2 (arriba), 4 (izquierda), 6 (derecha), 8 (abajo).
* - Pinta/borra con 5.
* - Cambia de color con la tecla F3-C3 (tecla 'C').
* - El cursor no puede salirse de (0,0)–(7,7) y parpadea en gris.
* - La tecla 7 (fila 3, columna 1) reinicia la matriz.
* - La web se sincroniza periódicamente con el estado real de la matriz
* (para ver lo que se pinta con el keypad).
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -----------------------------------------------------------------------------
// CONFIGURACIÓN DE LA MATRIZ NEOPIXEL
// -----------------------------------------------------------------------------

const int PIN_MATRIX = 5;  // D1 -> GPIO5
#define NUM_LEDS 64        // 8x8

Adafruit_NeoPixel strip(NUM_LEDS, PIN_MATRIX, NEO_GRB + NEO_KHZ800);

uint32_t cellColor[NUM_LEDS];  // Color lógico de cada LED (0 = apagado)
String cellName[NUM_LEDS];     // Nombre lógico del color ("rojo","none", etc.)

// -----------------------------------------------------------------------------
// CONFIGURACIÓN DE LA PANTALLA OLED I2C (SSD1306)
// -----------------------------------------------------------------------------

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int I2C_SDA = 4;  // D2
const int I2C_SCL = 0;  // D3

// Color actual seleccionado (texto)
String colorActual = "rojo";

// Lista de colores para ciclar con el keypad
const char* listaColores[] = { "rojo", "verde", "azul", "amarillo",
                               "magenta", "cian", "blanco" };
const int NUM_COLORES = 7;

// -----------------------------------------------------------------------------
// CONFIGURACIÓN DEL KEYPAD 4x4
// -----------------------------------------------------------------------------

// Filas y columnas usadas (según tu conexión):
// F1 -> D8, F2 -> D7, F3 -> D6
// C1 -> D5, C2 -> D0, C3 -> D4

const int KP_ROW1 = 15;
const int KP_ROW2 = 13;
const int KP_ROW3 = 12;

const int KP_COL1 = 14;
const int KP_COL2 = 16;
const int KP_COL3 = 2;

// Cursor (0..7, 0..7)
int cursorX = 0;
int cursorY = 0;

// Parpadeo del cursor
bool cursorVisible = true;
const unsigned long CURSOR_BLINK_INTERVAL = 500;  // ms
unsigned long lastBlinkMillis = 0;

// Para evitar repeticiones continuas entre lecturas
char lastKeyPressed = 0;

// -----------------------------------------------------------------------------
// CONFIGURACIÓN WIFI
// -----------------------------------------------------------------------------

const char* ssid = "iPhone de Pablo";
const char* password = "pablo2004";

ESP8266WebServer server(80);

// -----------------------------------------------------------------------------
// HTML DE LA PÁGINA WEB
// -----------------------------------------------------------------------------

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Matriz 8x8</title>
<style>
body {
font-family: Arial, sans-serif;
margin: 0;
padding: 0;
display: flex;
flex-direction: column;
align-items: center;
justify-content: flex-start;
min-height: 100vh;
background-color: #f5f5f5;
}
h2 {
text-align: center;
margin: 15px 10px 5px;
}
.contenedor {
margin-top: 10px;
display: flex;
flex-direction: column;
align-items: center;
gap: 15px;
width: 100%;
}
#grid {
display: grid;
grid-template-columns: repeat(8, 35px);
grid-template-rows: repeat(8, 35px);
gap: 2px;
margin: 0 auto;
}
.cell {
border: 1px solid #444;
width: 35px;
height: 35px;
font-size: 10px;
display: flex;
align-items: center;
justify-content: center;
cursor: pointer;
user-select: none;
background-color: #eee;
}
.panel-derecho {
display: flex;
flex-direction: column;
align-items: center;
gap: 10px;
width: 100%;
max-width: 320px;
padding: 0 10px 20px;
box-sizing: border-box;
}
.panel-derecho select {
width: 100%;
padding: 6px;
font-size: 14px;
}
#posiciones {
margin-top: 5px;
border: 1px solid #ccc;
padding: 5px;
min-height: 40px;
width: 100%;
font-size: 12px;
white-space: pre-wrap;
background-color: #fff;
box-sizing: border-box;
}
#btnReset {
margin-top: 5px;
padding: 8px 12px;
font-size: 14px;
border: none;
border-radius: 4px;
background-color: #007bff;
color: #fff;
cursor: pointer;
}
#btnReset:active {
transform: scale(0.98);
}
.creditos {
text-align: center;
margin: 10px 10px 8px;
font-size: 11px;
color: #555;
}
@media (min-width: 600px) {
#grid {
grid-template-columns: repeat(8, 40px);
grid-template-rows: repeat(8, 40px);
}
.cell {
width: 40px;
height: 40px;
}
}
</style>
</head>
<body>
<h2>Matriz LED 8x8 programable</h2>

<div class="contenedor">
<div id="grid"></div>

<div class="panel-derecho">
<div><strong>Selecciona un color:</strong></div>
<select id="colorSelect">
<option value="rojo">Rojo</option>
<option value="verde">Verde</option>
<option value="azul">Azul</option>
<option value="amarillo">Amarillo</option>
<option value="magenta">Magenta</option>
<option value="cian">Cian</option>
<option value="blanco">Blanco</option>
</select>

<div id="posiciones">Posiciones pintadas/borradas:</div>

<button id="btnReset">Reiniciar matriz</button>
</div>
</div>

<p class="creditos">Hecho por David Lorente Wagner y Pablo Javier
Montoro Bermúdez</p>

<script>
const size = 8;
const grid = document.getElementById('grid');
const posicionesDiv = document.getElementById('posiciones');
const colorSelect = document.getElementById('colorSelect');
const btnReset = document.getElementById('btnReset');

// Estado de cada celda para el historial (solo cambios desde la web)
const estados = {};

// Crear la matriz 8x8 en la web (solo divs)
for (let y = 0; y < size; y++) {
for (let x = 0; x < size; x++) {
const cell = document.createElement('div');
cell.className = 'cell';
cell.dataset.x = x;
cell.dataset.y = y;
cell.title = "(" + x + "," + y + ")";
cell.addEventListener('click', onCellClick);
grid.appendChild(cell);
}
}

// NodeList de todas las celdas (orden y*8+x)
const allCells = document.querySelectorAll('.cell');

// Pasar nombre de color (rojo, verde...) a color CSS
function cssColorFromName(name) {
switch (name) {
case 'rojo': return 'red';
case 'verde': return 'green';
case 'azul': return 'blue';
case 'amarillo': return 'yellow';
case 'magenta': return 'magenta';
case 'cian': return 'cyan';
case 'blanco': return 'white';
default: return '#eee';
}
}

let historial = [];

// Cuando se cambia el color en el desplegable, avisamos al ESP8266
colorSelect.addEventListener('change', () => {
const color = colorSelect.value;
fetch(`/color?color=${color}`).catch(err => console.error(err));
});

// Función que se ejecuta al hacer clic en una celda
function onCellClick(e) {
const cell = e.currentTarget;
const x = cell.dataset.x;
const y = cell.dataset.y;
const key = x + "," + y;
const estado = estados[key];

if (estado && estado.activo) {
// Estaba pintada -> borrar
fetch(`/set?x=${x}&y=${y}&color=borrar`)
.then(r => r.text())
.then(t => {
cell.style.backgroundColor = '#eee';
estados[key] = {activo: false};

const texto = `Borrada (${x},${y})`;
historial.push(texto);
if (historial.length > 10) historial.shift();
posicionesDiv.textContent =
'Posiciones pintadas/borradas:\n' + historial.join('\n');
})
.catch(err => {
console.error(err);
alert('Error al enviar al ESP8266');
});
} else {
// Estaba vacía -> pintar
const color = colorSelect.value;
fetch(`/set?x=${x}&y=${y}&color=${color}`)
.then(r => r.text())
.then(t => {
cell.style.backgroundColor = cssColorFromName(color);
estados[key] = {activo: true, color: color};

const texto = `Pintada (${x},${y}) como ${color}`;
historial.push(texto);
if (historial.length > 10) historial.shift();
posicionesDiv.textContent =
'Posiciones pintadas/borradas:\n' + historial.join('\n');
})
.catch(err => {
console.error(err);
alert('Error al enviar al ESP8266');
});
}
}

// Botón REINICIAR matriz
btnReset.addEventListener('click', () => {
fetch('/clear')
.then(r => r.text())
.then(t => {
allCells.forEach(c => c.style.backgroundColor = '#eee');
historial = [];
posicionesDiv.textContent = 'Posiciones pintadas/borradas:';
for (const k in estados) {
if (Object.prototype.hasOwnProperty.call(estados, k)) {
estados[k].activo = false;
}
}
})
.catch(err => {
console.error(err);
alert('Error al reiniciar la matriz');
});
});

// --- Sincronización con el estado real de la matriz (keypad, etc.) ---

function syncFromESP() {
  fetch('/state')
  .then(r => r.json())
  .then(data => {

    // ---- Actualiza color del dropdown ----
    if (colorSelect.value !== data.color) {
        colorSelect.value = data.color;
    }

    const cells = data.cells;
    if (!cells || cells.length !== 64) return;

    for (let idx = 0; idx < 64; idx++) {
      const name = cells[idx];
      const cell = allCells[idx];
      if (!cell) continue;

      if (!name || name === "none" || name === "borrar") {
        cell.style.backgroundColor = '#eee';
      } else {
        cell.style.backgroundColor = cssColorFromName(name);
      }
    }
  })
  .catch(err => {
    console.error('Error al sincronizar /state', err);
  });
}

// Sincroniza cada 0.5 segundos (igual que el parpadeo del cursor)
setInterval(syncFromESP, 500);
</script>
</body>
</html>
)rawliteral";

// -----------------------------------------------------------------------------
// FUNCIONES AUXILIARES (C++)
// -----------------------------------------------------------------------------

uint32_t colorToNeo(String c) {
  c.toLowerCase();
  if (c == "rojo") return strip.Color(255, 0, 0);
  if (c == "verde") return strip.Color(0, 255, 0);
  if (c == "azul") return strip.Color(0, 0, 255);
  if (c == "amarillo") return strip.Color(255, 255, 0);
  if (c == "magenta") return strip.Color(255, 0, 255);
  if (c == "cian") return strip.Color(0, 255, 255);
  if (c == "blanco") return strip.Color(255, 255, 255);
  if (c == "borrar") return strip.Color(0, 0, 0);
  return strip.Color(0, 0, 0);
}

void drawCenteredText(int16_t y, const String& text, uint8_t textSize) {
  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(textSize);
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);

  int16_t x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, y);
  display.print(text);
}

String capitalizarPrimera(const String& txt) {
  if (txt.length() == 0) return "";
  String r = txt;
  r.toLowerCase();
  r[0] = toupper(r[0]);
  return r;
}

String siguienteColor(const String& actual) {
  String a = actual;
  a.toLowerCase();
  for (int i = 0; i < NUM_COLORES; i++) {
    if (a == listaColores[i]) {
      int siguiente = (i + 1) % NUM_COLORES;
      return String(listaColores[siguiente]);
    }
  }
  return String("rojo");
}

void showColorOnOLED(String color) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawCenteredText(0, "Color:", 2);

  String line;
  if (color.length() == 0 || color == "none" || color == "borrar") {
    line = "Ninguno";
  } else {
    line = capitalizarPrimera(color);
  }

  drawCenteredText(32, line, 2);
  display.display();
}

void showReiniciandoOLED() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawCenteredText(8, "Reiniciando...", 1);
  drawCenteredText(32, "Matriz...", 1);

  display.display();
}

int coordToIndex(int x, int y) {
  return y * 8 + x;
}

void renderMatrix() {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, cellColor[i]);
  }

  int idxCursor = coordToIndex(cursorX, cursorY);
  if (cursorVisible) {
    strip.setPixelColor(idxCursor, strip.Color(40, 40, 40));  // gris
  }

  strip.show();
}

// -----------------------------------------------------------------------------
// KEYPAD
// -----------------------------------------------------------------------------

void initKeypad() {
  pinMode(KP_ROW1, OUTPUT);
  pinMode(KP_ROW2, OUTPUT);
  pinMode(KP_ROW3, OUTPUT);

  digitalWrite(KP_ROW1, HIGH);
  digitalWrite(KP_ROW2, HIGH);
  digitalWrite(KP_ROW3, HIGH);

  pinMode(KP_COL1, INPUT_PULLUP);
  pinMode(KP_COL2, INPUT_PULLUP);
  pinMode(KP_COL3, INPUT_PULLUP);
}

/**
* @brief Escanea el keypad y devuelve:
* '2','4','5','6','8','7' o 'C' (cambio de color), o 0 si nada.
*
* - F1C2 -> '2'
* - F2C1 -> '4'
* - F2C2 -> '5'
* - F2C3 -> '6'
* - F3C1 -> '7' (reinicio matriz)
* - F3C2 -> '8'
* - F3C3 -> 'C' (cambio de color)
*/
char scanKeypad() {
  digitalWrite(KP_ROW1, HIGH);
  digitalWrite(KP_ROW2, HIGH);
  digitalWrite(KP_ROW3, HIGH);

  // Fila 1: tecla '2' (C2)
  digitalWrite(KP_ROW1, LOW);
  delayMicroseconds(5);
  if (digitalRead(KP_COL2) == LOW) {
    digitalWrite(KP_ROW1, HIGH);
    return '2';
  }
  digitalWrite(KP_ROW1, HIGH);

  // Fila 2: '4','5','6'
  digitalWrite(KP_ROW2, LOW);
  delayMicroseconds(5);
  if (digitalRead(KP_COL1) == LOW) {
    digitalWrite(KP_ROW2, HIGH);
    return '4';
  }
  if (digitalRead(KP_COL2) == LOW) {
    digitalWrite(KP_ROW2, HIGH);
    return '5';
  }
  if (digitalRead(KP_COL3) == LOW) {
    digitalWrite(KP_ROW2, HIGH);
    return '6';
  }
  digitalWrite(KP_ROW2, HIGH);

  // Fila 3: '7', '8' y 'C'
  digitalWrite(KP_ROW3, LOW);
  delayMicroseconds(5);
  if (digitalRead(KP_COL1) == LOW) {  // NUEVO: tecla 7
    digitalWrite(KP_ROW3, HIGH);
    return '7';  // reinicio matriz
  }
  if (digitalRead(KP_COL2) == LOW) {
    digitalWrite(KP_ROW3, HIGH);
    return '8';
  }
  if (digitalRead(KP_COL3) == LOW) {
    digitalWrite(KP_ROW3, HIGH);
    return 'C';  // cambio de color
  }
  digitalWrite(KP_ROW3, HIGH);

  return 0;
}

void handleKey(char key) {
  if (key == 0) return;

  static unsigned long lastPress = 0;
  static bool bloquePintar = false;

  unsigned long now = millis();

  // Si venimos de pintar, bloqueamos el keypad 40ms
  if (bloquePintar && (now - lastPress < 40)) {
    return;
  }
  bloquePintar = false;

  // Debounce general
  if (now - lastPress < 120) {
    return;
  }
  lastPress = now;

  switch (key) {

    case '2':  // arriba
      cursorY = max(0, cursorY - 1);
      break;

    case '8':  // abajo
      cursorY = min(7, cursorY + 1);
      break;

    case '4':  // izquierda
      cursorX = max(0, cursorX - 1);
      break;

    case '6':  // derecha
      cursorX = min(7, cursorX + 1);
      break;

    case '5':
      {  // pintar sin mover cursor
        int idx = coordToIndex(cursorX, cursorY);

        if (cellColor[idx] == 0) {
          cellColor[idx] = colorToNeo(colorActual);
          cellName[idx] = colorActual;
        } else {
          cellColor[idx] = 0;
          cellName[idx] = "none";
        }

        // BLOQUEA keypad 40 ms para evitar movimientos por rebote
        bloquePintar = true;
        break;
      }

    case '7':  // reiniciar matriz
      showReiniciandoOLED();
      for (int i = 0; i < NUM_LEDS; i++) {
        cellColor[i] = 0;
        cellName[i] = "none";
      }
      renderMatrix();
      delay(500);
      showColorOnOLED(colorActual);
      break;

    case 'C':  // cambiar color
      colorActual = siguienteColor(colorActual);
      showColorOnOLED(colorActual);
      break;
  }

  renderMatrix();
}


// -----------------------------------------------------------------------------
// HANDLERS DEL SERVIDOR WEB
// -----------------------------------------------------------------------------

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleSet() {
  if (!server.hasArg("x") || !server.hasArg("y") || !server.hasArg("color")) {
    server.send(400, "text/plain", "Faltan parametros");
    return;
  }

  int x = server.arg("x").toInt();
  int y = server.arg("y").toInt();
  String color = server.arg("color");

  if (x < 0 || x >= 8 || y < 0 || y >= 8) {
    server.send(400, "text/plain", "Coordenadas fuera de rango");
    return;
  }

  if (color != "borrar") {
    colorActual = color;
  }

  int index = coordToIndex(x, y);

  if (color == "borrar") {
    cellColor[index] = 0;
    cellName[index] = "none";
  } else {
    cellColor[index] = colorToNeo(color);
    cellName[index] = color;
  }

  renderMatrix();

  String respuesta = "OK (" + String(x) + "," + String(y) + ") = " + color;
  server.send(200, "text/plain", respuesta);
}

void handleClear() {
  showReiniciandoOLED();

  for (int i = 0; i < NUM_LEDS; i++) {
    cellColor[i] = 0;
    cellName[i] = "none";
  }
  renderMatrix();

  delay(1000);

  showColorOnOLED(colorActual);

  server.send(200, "text/plain", "CLEARED");
}

void handleColor() {
  if (!server.hasArg("color")) {
    server.send(400, "text/plain", "Falta parametro color");
    return;
  }

  String color = server.arg("color");
  colorActual = color;
  showColorOnOLED(colorActual);

  server.send(200, "text/plain", "COLOR SET");
}

/**
* @brief Devuelve el estado de la matriz (nombres de color) en JSON.
* Formato: {"cells":["rojo","none",...]} tamaño 64.
*/
void handleState() {
  String json = "{\"color\":\"" + colorActual + "\",\"cells\":[";
  for (int i = 0; i < NUM_LEDS; i++) {
    json += "\"";
    json += cellName[i];
    json += "\"";
    if (i < NUM_LEDS - 1) json += ",";
  }
  json += "]}";

  server.send(200, "application/json", json);
}


// -----------------------------------------------------------------------------
// SETUP Y LOOP
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  // OLED
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;) { delay(10); }
  }
  display.setRotation(0);
  showColorOnOLED("rojo");

  // Matriz
  strip.begin();
  strip.setBrightness(20);

  for (int i = 0; i < NUM_LEDS; i++) {
    cellColor[i] = 0;
    cellName[i] = "none";
  }
  renderMatrix();

  // Keypad
  initKeypad();

  // WiFi
  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado. IP: ");
  Serial.println(WiFi.localIP());

  // Rutas
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/clear", handleClear);
  server.on("/color", handleColor);
  server.on("/state", handleState);  // estado matriz para la web

  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();

  // --- LECTURA KEYPAD ---
  static unsigned long lastKeyTime = 0;
  const unsigned long keyDelay = 140;

  char key = scanKeypad();

  unsigned long now = millis();
  if (key != 0 && (now - lastKeyTime) > keyDelay && key != lastKeyPressed) {
    handleKey(key);
    lastKeyPressed = key;
    lastKeyTime = now;
  }

  if (key == 0) {
    lastKeyPressed = 0;
  }

  // --- PARPADEO DEL CURSOR ---
  if (now - lastBlinkMillis >= CURSOR_BLINK_INTERVAL) {
    lastBlinkMillis = now;
    cursorVisible = !cursorVisible;
    renderMatrix();
  }
}
