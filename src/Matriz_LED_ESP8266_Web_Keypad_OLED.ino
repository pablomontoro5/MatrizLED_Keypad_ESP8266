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
*   (para ver lo que se pinta con el keypad).
*/

#include <ESP8266WiFi.h>          // Manejo de WiFi en ESP8266
#include <ESP8266WebServer.h>     // Servidor HTTP incorporado
#include <Adafruit_NeoPixel.h>    // Biblioteca para controlar LEDs NeoPixel

#include <Wire.h>                 // I2C (usado para OLED)
#include <Adafruit_GFX.h>         // Librería gráfica base
#include <Adafruit_SSD1306.h>     // Controlador de pantallas OLED SSD1306

// -----------------------------------------------------------------------------
// CONFIGURACIÓN DE LA MATRIZ NEOPIXEL
// -----------------------------------------------------------------------------

const int PIN_MATRIX = 5;  // Pin D1 (GPIO5) conectado a la entrada DIN de la matriz
#define NUM_LEDS 64        // Matriz 8x8 → 64 LEDs

// Objeto principal que controla la matriz
Adafruit_NeoPixel strip(NUM_LEDS, PIN_MATRIX, NEO_GRB + NEO_KHZ800);

// Arrays para almacenar el estado lógico de cada LED
uint32_t cellColor[NUM_LEDS];  // Color real en formato NeoPixel
String cellName[NUM_LEDS];     // Nombre textual del color para la web y JSON

// -----------------------------------------------------------------------------
// CONFIGURACIÓN DE LA PANTALLA OLED I2C (SSD1306)
// -----------------------------------------------------------------------------

#define SCREEN_WIDTH 128     // Ancho del OLED
#define SCREEN_HEIGHT 64     // Alto del OLED
#define OLED_RESET -1        // SSD1306 sin pin RESET (se usa el reset interno)

// Objeto de la pantalla OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pines I2C para ESP8266 (coinciden con tu cableado físico)
const int I2C_SDA = 4;  // D2 (SDA)
const int I2C_SCL = 0;  // D3 (SCL)

// Color actualmente seleccionado (tanto web como keypad)
String colorActual = "rojo";

// Lista de colores disponibles para rotar con el keypad
const char* listaColores[] = { "rojo", "verde", "azul", "amarillo",
                               "magenta", "cian", "blanco" };
const int NUM_COLORES = 7;   // Total de colores disponibles

// -----------------------------------------------------------------------------
// CONFIGURACIÓN DEL KEYPAD 4x4
// -----------------------------------------------------------------------------

// Los pines corresponden a tu conexión real del teclado matricial
const int KP_ROW1 = 15;   // Fila 1 → D8
const int KP_ROW2 = 13;   // Fila 2 → D7
const int KP_ROW3 = 12;   // Fila 3 → D6

const int KP_COL1 = 14;   // Columna 1 → D5
const int KP_COL2 = 16;   // Columna 2 → D0
const int KP_COL3 = 2;    // Columna 3 → D4

// Posición del cursor en la matriz (0–7)
int cursorX = 0;
int cursorY = 0;

// Variables de parpadeo del cursor
bool cursorVisible = true;
const unsigned long CURSOR_BLINK_INTERVAL = 500;  // Intervalo en ms
unsigned long lastBlinkMillis = 0;

// Control interno para evitar repeticiones por rebotes
char lastKeyPressed = 0;

// -----------------------------------------------------------------------------
// CONFIGURACIÓN WIFI
// -----------------------------------------------------------------------------

// Credenciales WiFi (hardcoded)
const char* ssid = "iPhone de Pablo";
const char* password = "pablo2004";

// Servidor HTTP en el puerto 80
ESP8266WebServer server(80);

// -----------------------------------------------------------------------------
// HTML (se envía al navegador)
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// HTML DE LA PÁGINA WEB (se envía al navegador al entrar en "/")
// Comentarios incluidos dentro del contenido para explicar su estructura.
// -----------------------------------------------------------------------------

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Matriz 8x8</title>

<style>
/* ---------- ESTILOS GENERALES DEL CUERPO ---------- */
body {
    font-family: Arial, sans-serif;    /* Fuente */
    margin: 0;
    padding: 0;
    display: flex;                    /* Centrado vertical */
    flex-direction: column;
    align-items: center;              /* Centrado horizontal */
    justify-content: flex-start;
    min-height: 100vh;                /* Alto completo pantalla */
    background-color: #f5f5f5;
}

h2 {
    text-align: center;
    margin: 15px 10px 5px;
}

/* Contenedor principal */
.contenedor {
    margin-top: 10px;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 15px;                        /* Espacio entre elementos */
    width: 100%;
}

/* ---------- MATRIZ DE 8x8 CLICABLE ---------- */
#grid {
    display: grid;
    grid-template-columns: repeat(8, 35px);
    grid-template-rows: repeat(8, 35px);
    gap: 2px;
    margin: 0 auto;
}

/* Cada celda de la matriz web */
.cell {
    border: 1px solid #444;
    width: 35px;
    height: 35px;
    font-size: 10px;
    display: flex;
    align-items: center;
    justify-content: center;
    cursor: pointer;                 /* Indica que es clicable */
    user-select: none;               /* Evita selección de texto */
    background-color: #eee;          /* Color por defecto */
}

/* Panel lateral: color, historial, reset */
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

/* Selector de color */
.panel-derecho select {
    width: 100%;
    padding: 6px;
    font-size: 14px;
}

/* Área donde se muestra historial de acciones */
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

/* Botón de reiniciar matriz */
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

/* Nota inferior */
.creditos {
    text-align: center;
    margin: 10px 10px 8px;
    font-size: 11px;
    color: #555;
}

/* Adaptación para pantallas más grandes */
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

<!-- Contenedor principal de la matriz y controles -->
<div class="contenedor">

    <!-- MATRIZ 8x8 donde cada celda se puede pulsar -->
    <div id="grid"></div>

    <!-- Panel con selector de color, historial y botón reset -->
    <div class="panel-derecho">

        <div><strong>Selecciona un color:</strong></div>

        <!-- Selector desplegable de color -->
        <select id="colorSelect">
            <option value="rojo">Rojo</option>
            <option value="verde">Verde</option>
            <option value="azul">Azul</option>
            <option value="amarillo">Amarillo</option>
            <option value="magenta">Magenta</option>
            <option value="cian">Cian</option>
            <option value="blanco">Blanco</option>
        </select>

        <!-- Historial de posiciones pintadas -->
        <div id="posiciones">Posiciones pintadas/borradas:</div>

        <!-- Botón que pide al ESP8266 reiniciar la matriz -->
        <button id="btnReset">Reiniciar matriz</button>
    </div>
</div>

<p class="creditos">Hecho por David Lorente Wagner y Pablo Javier Montoro Bermúdez</p>

<script>
// ----------- VARIABLES PRINCIPALES DE LA WEB -----------

const size = 8;
const grid = document.getElementById('grid');
const posicionesDiv = document.getElementById('posiciones');
const colorSelect = document.getElementById('colorSelect');
const btnReset = document.getElementById('btnReset');

// Objeto para almacenar estado de celdas cambiadas desde la web
const estados = {};

// ----------- CREACIÓN DE MATRIZ 8x8 EN EL DOM -----------

for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {

        // Se crea un div por cada LED físico
        const cell = document.createElement('div');
        cell.className = 'cell';

        // Se guardan coordenadas como atributos del HTML
        cell.dataset.x = x;
        cell.dataset.y = y;

        // Tooltip mostrando coordenadas
        cell.title = "(" + x + "," + y + ")";

        // Evento click → pintado o borrado
        cell.addEventListener('click', onCellClick);

        grid.appendChild(cell);
    }
}

// Lista directa de todas las celdas (orden y*8+x)
const allCells = document.querySelectorAll('.cell');

// Convierte nombre de color (texto) a color CSS real
function cssColorFromName(name) {
    switch (name) {
        case 'rojo': return 'red';
        case 'verde': return 'green';
        case 'azul': return 'blue';
        case 'amarillo': return 'yellow';
        case 'magenta': return 'magenta';
        case 'cian': return 'cyan';
        case 'blanco': return 'white';
        default: return '#eee';   // fondo por defecto
    }
}

let historial = [];  // Guarda últimas 10 acciones del usuario

// ----------- EVENTO CAMBIO DE COLOR DESPLEGABLE -----------

colorSelect.addEventListener('change', () => {
    const color = colorSelect.value;

    // Notifica al ESP8266 para cambiar colorActual global
    fetch(`/color?color=${color}`).catch(err => console.error(err));
});

// ----------- FUNCIÓN AL CLICAR UNA CELDA -----------

function onCellClick(e) {

    const cell = e.currentTarget;
    const x = cell.dataset.x;
    const y = cell.dataset.y;
    const key = x + "," + y;

    const estado = estados[key]; // Último estado conocido

    // Si estaba pintada ⇒ ahora borra
    if (estado && estado.activo) {

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
        // Si estaba vacía ⇒ pintar con el color seleccionado

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

// ----------- BOTÓN RESET (limpia toda la matriz) -----------

btnReset.addEventListener('click', () => {

    fetch('/clear')
        .then(r => r.text())
        .then(t => {

            // Limpia visualmente la matriz web
            allCells.forEach(c => c.style.backgroundColor = '#eee');

            // Limpia historial
            historial = [];
            posicionesDiv.textContent = 'Posiciones pintadas/borradas:';

            // Marca todo como vacío
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

// ----------- SINCRONIZACIÓN CON HARDWARE (keypad / web) -----------

function syncFromESP() {

    fetch('/state')

        .then(r => r.json())
        .then(data => {

            // Sincroniza colorActual del navegador con ESP8266
            if (colorSelect.value !== data.color) {
                colorSelect.value = data.color;
            }

            // Aplica colores de la matriz física a la matriz web
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

// Se sincroniza cada 500 ms (coincide con el parpadeo del cursor)
setInterval(syncFromESP, 500);

</script>
</body>
</html>
)rawliteral";

// -----------------------------------------------------------------------------
// FUNCIONES AUXILIARES (C++)
// -----------------------------------------------------------------------------


/**
 * Convierte un nombre de color (texto) a su valor RGB para NeoPixel.
 * Se usa tanto al pintar desde la web como desde el keypad.
 */
uint32_t colorToNeo(String c) {
  c.toLowerCase();  // Asegura comparación coherente

  if (c == "rojo")     return strip.Color(255, 0, 0);
  if (c == "verde")    return strip.Color(0, 255, 0);
  if (c == "azul")     return strip.Color(0, 0, 255);
  if (c == "amarillo") return strip.Color(255, 255, 0);
  if (c == "magenta")  return strip.Color(255, 0, 255);
  if (c == "cian")     return strip.Color(0, 255, 255);
  if (c == "blanco")   return strip.Color(255, 255, 255);
  if (c == "borrar")   return strip.Color(0, 0, 0);  // LED apagado

  // Color por defecto si no coincide con ninguno
  return strip.Color(0, 0, 0);
}


/**
 * Dibuja texto centrado horizontalmente en la pantalla OLED.
 * Se usa para mostrar "Color:" y el color actual.
 *
 * @param y        Posición vertical del texto
 * @param text     Texto a dibujar
 * @param textSize Tamaño de letra (1–2 en este proyecto)
 */
void drawCenteredText(int16_t y, const String& text, uint8_t textSize) {
  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(textSize);
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);

  int16_t x = (SCREEN_WIDTH - w) / 2;  // Cálculo de centrado
  display.setCursor(x, y);
  display.print(text);
}


/**
 * Devuelve una cadena con la primera letra mayúscula y el resto minúscula.
 * Es sólo un formateador para mostrar mejor los colores en la OLED.
 */
String capitalizarPrimera(const String& txt) {
  if (txt.length() == 0) return "";
  String r = txt;
  r.toLowerCase();
  r[0] = toupper(r[0]);
  return r;
}


/**
 * Devuelve el color siguiente en la lista de colores.
 * Se usa únicamente cuando el keypad pulsa la tecla 'C'.
 */
String siguienteColor(const String& actual) {
  String a = actual;
  a.toLowerCase();

  for (int i = 0; i < NUM_COLORES; i++) {
    if (a == listaColores[i]) {
      int siguiente = (i + 1) % NUM_COLORES;  // Cicla al principio
      return String(listaColores[siguiente]);
    }
  }
  return String("rojo");  // Valor por defecto
}


/**
 * Muestra en la OLED el color actualmente seleccionado.
 * Si el color está vacío o es "none", muestra "Ninguno".
 */
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


/**
 * Dibuja el mensaje "Reiniciando..." en la pantalla OLED.
 * Se llama cuando el usuario reinicia desde la web o desde el keypad.
 */
void showReiniciandoOLED() {

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawCenteredText(8,  "Reiniciando...", 1);
  drawCenteredText(32, "Matriz...", 1);

  display.display();
}


/**
 * Convierte coordenadas (x,y) de la matriz 8x8 a un índice lineal (0–63).
 * Fórmula: índice = fila * 8 + columna.
 */
int coordToIndex(int x, int y) {
  return y * 8 + x;
}


/**
 * Redibuja la matriz NeoPixel según cellColor[] y añade la posición del cursor.
 * Si cursorVisible es true, el cursor parpadea mostrando un LED gris.
 *
 * Se llama:
 * - tras pintar
 * - tras mover cursor
 * - tras reiniciar
 * - en el parpadeo del cursor
 */
void renderMatrix() {

  // Escribe cada uno de los 64 LED según su color almacenado
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, cellColor[i]);
  }

  // LED del cursor
  int idxCursor = coordToIndex(cursorX, cursorY);
  if (cursorVisible) {
    strip.setPixelColor(idxCursor, strip.Color(40, 40, 40));  // Gris suave
  }

  strip.show();  // Actualiza matriz física
}

// -----------------------------------------------------------------------------
// KEYPAD
// -----------------------------------------------------------------------------

/**
 * Inicializa los pines del keypad.
 * Las filas se configuran como salidas (HIGH por defecto).
 * Las columnas se configuran como entradas con pull-up.
 *
 * El funcionamiento es:
 * - Se baja a LOW una fila (una a una)
 * - Se leen las columnas: si una columna está LOW → tecla pulsada
 */
void initKeypad() {
  pinMode(KP_ROW1, OUTPUT);
  pinMode(KP_ROW2, OUTPUT);
  pinMode(KP_ROW3, OUTPUT);

  // Todas las filas inicialmente en HIGH (no activas)
  digitalWrite(KP_ROW1, HIGH);
  digitalWrite(KP_ROW2, HIGH);
  digitalWrite(KP_ROW3, HIGH);

  // Columnas como entradas con pull-up (esperan HIGH siempre)
  pinMode(KP_COL1, INPUT_PULLUP);
  pinMode(KP_COL2, INPUT_PULLUP);
  pinMode(KP_COL3, INPUT_PULLUP);
}


/**
 * Escanea el keypad devolviendo el carácter correspondiente a la tecla
 * detectada o '0' si no se ha pulsado ninguna.
 *
 * Mapa implementado:
 *
 *   F1-C2 → '2'   (arriba)
 *   F2-C1 → '4'   (izquierda)
 *   F2-C2 → '5'   (pintar/borrar)
 *   F2-C3 → '6'   (derecha)
 *   F3-C1 → '7'   (reiniciar matriz)
 *   F3-C2 → '8'   (abajo)
 *   F3-C3 → 'C'   (cambiar color)
 *
 * El método:
 * 1. Se pone una fila LOW
 * 2. Si alguna columna lee LOW, esa tecla está pulsada
 * 3. Se devuelve el carácter asociado a la combinación fila-columna
 */
char scanKeypad() {

  // --- Fila 1 (busca tecla '2') ---
  digitalWrite(KP_ROW1, HIGH);
  digitalWrite(KP_ROW2, HIGH);
  digitalWrite(KP_ROW3, HIGH);

  digitalWrite(KP_ROW1, LOW);
  delayMicroseconds(5);
  if (digitalRead(KP_COL2) == LOW) {  // F1C2
    digitalWrite(KP_ROW1, HIGH);
    return '2';
  }
  digitalWrite(KP_ROW1, HIGH);

  // --- Fila 2 (busca '4','5','6') ---
  digitalWrite(KP_ROW2, LOW);
  delayMicroseconds(5);

  if (digitalRead(KP_COL1) == LOW) {  // F2C1
    digitalWrite(KP_ROW2, HIGH);
    return '4';
  }
  if (digitalRead(KP_COL2) == LOW) {  // F2C2
    digitalWrite(KP_ROW2, HIGH);
    return '5';
  }
  if (digitalRead(KP_COL3) == LOW) {  // F2C3
    digitalWrite(KP_ROW2, HIGH);
    return '6';
  }
  digitalWrite(KP_ROW2, HIGH);

  // --- Fila 3 (busca '7','8','C') ---
  digitalWrite(KP_ROW3, LOW);
  delayMicroseconds(5);

  if (digitalRead(KP_COL1) == LOW) {  // F3C1
    digitalWrite(KP_ROW3, HIGH);
    return '7';   // reinicio matriz
  }
  if (digitalRead(KP_COL2) == LOW) {  // F3C2
    digitalWrite(KP_ROW3, HIGH);
    return '8';   // abajo
  }
  if (digitalRead(KP_COL3) == LOW) {  // F3C3
    digitalWrite(KP_ROW3, HIGH);
    return 'C';   // cambio de color
  }
  digitalWrite(KP_ROW3, HIGH);

  return 0;  // ninguna tecla pulsada
}


/**
 * Maneja la acción de una tecla pulsada.
 * Aquí se realizan los movimientos del cursor, pintura, cambios de color
 * y el reinicio de la matriz.
 *
 * Además incluye:
 * - Debounce general para evitar dobles pulsaciones
 * - Un bloqueo especial después de pintar para evitar movimiento accidental
 */
void handleKey(char key) {

  if (key == 0) return;  // No hay tecla → salir

  static unsigned long lastPress = 0;    // Última vez que se procesó una pulsación
  static bool bloquePintar = false;      // Bloqueo corto tras pintar

  unsigned long now = millis();

  // Si venimos de pintar, aplicamos un bloqueo de 40ms
  if (bloquePintar && (now - lastPress < 40)) {
    return;
  }
  bloquePintar = false;


  // Debounce general para evitar rebotes del keypad
  if (now - lastPress < 120) {
    return;
  }
  lastPress = now;


  // ---- Acciones según la tecla ----
  switch (key) {

    case '2':  // MOVER ARRIBA
      cursorY = max(0, cursorY - 1);
      break;

    case '8':  // MOVER ABAJO
      cursorY = min(7, cursorY + 1);
      break;

    case '4':  // MOVER IZQUIERDA
      cursorX = max(0, cursorX - 1);
      break;

    case '6':  // MOVER DERECHA
      cursorX = min(7, cursorX + 1);
      break;

    case '5':
      {
        // Pintar o borrar sin mover el cursor
        int idx = coordToIndex(cursorX, cursorY);

        if (cellColor[idx] == 0) {
          // Pintar con el color actual
          cellColor[idx] = colorToNeo(colorActual);
          cellName[idx] = colorActual;
        } else {
          // Borrar la casilla
          cellColor[idx] = 0;
          cellName[idx] = "none";
        }

        // Bloqueo leve para evitar movimientos involuntarios
        bloquePintar = true;
        break;
      }

    case '7':  // RESETEAR MATRIZ COMPLETA
      showReiniciandoOLED();

      for (int i = 0; i < NUM_LEDS; i++) {
        cellColor[i] = 0;
        cellName[i] = "none";
      }

      renderMatrix();
      delay(500);
      showColorOnOLED(colorActual);
      break;

    case 'C':  // CAMBIAR COLOR
      colorActual = siguienteColor(colorActual);
      showColorOnOLED(colorActual);
      break;
  }

  // Siempre que algo cambia por keypad, actualizamos matriz
  renderMatrix();
}

// -----------------------------------------------------------------------------
// HANDLERS DEL SERVIDOR WEB
// -----------------------------------------------------------------------------

/**
 * Envía la página web principal almacenada en PROGMEM.
 * Esta es la interfaz que permite pintar la matriz desde el navegador.
 */
void handleRoot() {
  server.send_P(200, "text/html", index_html);
}


/**
 * Handler encargado de pintar o borrar una celda solicitado por la web.
 * Parámetros GET esperados:
 *   x = columna [0..7]
 *   y = fila    [0..7]
 *   color = nombre del color o "borrar"
 *
 * Si el color no es "borrar", también actualiza el colorActual global
 * para sincronizar keypad + OLED + web.
 */
void handleSet() {

  // Verifica que los parámetros existen
  if (!server.hasArg("x") || !server.hasArg("y") || !server.hasArg("color")) {
    server.send(400, "text/plain", "Faltan parametros");
    return;
  }

  // Obtiene los parámetros
  int x = server.arg("x").toInt();
  int y = server.arg("y").toInt();
  String color = server.arg("color");

  // Comprueba que están dentro del rango permitido
  if (x < 0 || x >= 8 || y < 0 || y >= 8) {
    server.send(400, "text/plain", "Coordenadas fuera de rango");
    return;
  }

  // Si el color proviene de la web y no es borrar, se actualiza colorActual
  if (color != "borrar") {
    colorActual = color;
  }

  // Calcula índice en el array lineal
  int index = coordToIndex(x, y);

  // Pintar o borrar celda según parámetro recibido
  if (color == "borrar") {
    cellColor[index] = 0;
    cellName[index] = "none";
  } else {
    cellColor[index] = colorToNeo(color);
    cellName[index] = color;
  }

  // Refresca matriz física
  renderMatrix();

  // Respuesta al navegador
  String respuesta = "OK (" + String(x) + "," + String(y) + ") = " + color;
  server.send(200, "text/plain", respuesta);
}


/**
 * Reinicio completo de la matriz solicitado desde la web.
 *
 * Borra todos los LED, deja colorActual intacto,
 * muestra animación en la OLED y devuelve respuesta HTTP.
 */
void handleClear() {

  showReiniciandoOLED();

  // Borra matriz lógica y visual
  for (int i = 0; i < NUM_LEDS; i++) {
    cellColor[i] = 0;
    cellName[i] = "none";
  }
  renderMatrix();

  delay(1000);  // pequeña pausa de UX

  // Muestra el color actual después del reinicio
  showColorOnOLED(colorActual);

  server.send(200, "text/plain", "CLEARED");
}


/**
 * Cambia el color activo desde la web.
 * La web envía: /color?color=rojo
 *
 * Este valor afecta:
 * - Pintado físico
 * - Pintado desde keypad
 * - Web
 * - OLED
 */
void handleColor() {

  if (!server.hasArg("color")) {
    server.send(400, "text/plain", "Falta parametro color");
    return;
  }

  // Actualiza color actual y OLED
  String color = server.arg("color");
  colorActual = color;
  showColorOnOLED(colorActual);

  server.send(200, "text/plain", "COLOR SET");
}


/**
 * Devuelve el estado COMPLETO de la matriz en formato JSON.
 *
 * Ejemplo:
 * {
 *   "color": "rojo",
 *   "cells": ["none","none","rojo", ... ]
 * }
 *
 * Este JSON es leído por JavaScript cada 0.5s para sincronizar:
 * - La web con el keypad
 * - El dropdown con el colorActual
 * - Los colores de las 64 celdas
 */
void handleState() {

  // Añade colorActual para sincronizar dropdown
  String json = "{\"color\":\"" + colorActual + "\",\"cells\":[";

  // Inserta los 64 valores de colorName[]
  for (int i = 0; i < NUM_LEDS; i++) {
    json += "\"";
    json += cellName[i];
    json += "\"";
    if (i < NUM_LEDS - 1) json += ",";
  }

  json += "]}";

  // Devuelve JSON al navegador
  server.send(200, "application/json", json);
}
// -----------------------------------------------------------------------------
// SETUP Y LOOP
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);   // Inicia consola serie para debug

  // --------------------------
  //  INICIALIZACIÓN OLED
  // --------------------------
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // Si la pantalla no se inicializa, se queda en bucle
    Serial.println("SSD1306 allocation failed");
    for (;;) { delay(10); }
  }
  display.setRotation(0);          // Orientación normal
  showColorOnOLED("rojo");         // Muestra el color inicial (“rojo”)

  // --------------------------
  //  INICIALIZACIÓN MATRIZ LED
  // --------------------------
  strip.begin();
  strip.setBrightness(20);         // Brillo moderado para NeoPixels

  // Limpia la matriz lógica y visual
  for (int i = 0; i < NUM_LEDS; i++) {
    cellColor[i] = 0;
    cellName[i] = "none";
  }
  renderMatrix();

  // --------------------------
  //  INICIALIZACIÓN KEYPAD
  // --------------------------
  initKeypad();  // Configura las filas como OUTPUT y columnas como INPUT_PULLUP

  // --------------------------
  //  INICIALIZACIÓN WIFI
  // --------------------------
  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);

  // Espera hasta que se conecte al punto de acceso
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Conectado. IP: ");
  Serial.println(WiFi.localIP());   // Muestra la IP del servidor

  // --------------------------
  //  RUTAS DEL SERVIDOR HTTP
  // --------------------------
  server.on("/", handleRoot);         // Página principal
  server.on("/set", handleSet);       // Pintar/borrar desde web
  server.on("/clear", handleClear);   // Reiniciar matriz
  server.on("/color", handleColor);   // Cambiar color actual
  server.on("/state", handleState);   // Obtener estado completo en JSON

  server.begin();                     // Inicia servidor web
  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();  // Atiende peticiones HTTP continuamente

  // -----------------------------------------------------------------
  // LECTURA DEL KEYPAD CON DEBOUNCE
  // -----------------------------------------------------------------
  static unsigned long lastKeyTime = 0;
  const unsigned long keyDelay = 140;   // Anti-rebote general

  char key = scanKeypad();  // Lee tecla pulsada (o 0 si no hay nada)

  unsigned long now = millis();

  // Se acepta una tecla si:
  // - es distinta de 0
  // - ha pasado suficiente tiempo desde la última
  // - no es repetición inmediata (lastKeyPressed)
  if (key != 0 && (now - lastKeyTime) > keyDelay && key != lastKeyPressed) {
    handleKey(key);         // Ejecuta acción del keypad (mover / pintar / color)
    lastKeyPressed = key;   // Evita repetir tecla automáticamente
    lastKeyTime = now;
  }

  // Si no hay tecla pulsada, resetea lastKeyPressed
  if (key == 0) {
    lastKeyPressed = 0;
  }

  // -----------------------------------------------------------------
  // PARPADEO DEL CURSOR (gris) CADA 500 ms
  // -----------------------------------------------------------------
  if (now - lastBlinkMillis >= CURSOR_BLINK_INTERVAL) {
    lastBlinkMillis = now;
    cursorVisible = !cursorVisible;   // Alterna visibilidad
    renderMatrix();                   // Redibuja estado real + cursor
  }
}
