Matriz LED 8x8 con ESP8266 - Control Web, Keypad y Pantalla OLED

Proyecto completo que integra una matriz NeoPixel 8x8, un ESP8266, un keypad físico 4×4 y una pantalla OLED I2C, todo sincronizado con una interfaz web en tiempo real.

El sistema permite controlar la matriz desde el navegador o desde el keypad, mostrando siempre el estado actualizado tanto en la web como en el dispositivo.

✨ Características principales
🔵 Control desde la web

Matriz 8×8 clicable.

Selección de color.

Pintado y borrado.

Reinicio completo.

Sincronización cada 0,5 s con la matriz real.

🟢 Control desde el keypad físico

2 → Mover cursor arriba

8 → Mover cursor abajo

4 → Mover cursor izquierda

6 → Mover cursor derecha

5 → Pintar / borrar sin mover el cursor

9 → Cambiar color

7 → Reiniciar matriz

Cursor parpadeante en gris

Movimiento con debounce para evitar pulsaciones dobles

🟣 Pantalla OLED SSD1306

Muestra siempre el color actual.

Mensaje de reinicio.

Actualización automática al cambiar color desde la web o keypad.

🧩 Hardware utilizado

ESP8266 (NodeMCU)

Matriz NeoPixel 8×8 WS2812B

Pantalla OLED I2C SSD1306

Keypad 4×4

Fuente de alimentación 5V

Cables Dupont

🚀 Cómo desplegar el proyecto

  1.Instalar Arduino IDE.

  2.Añadir soporte para ESP8266.

  3.Instalar las librerías:

    Adafruit NeoPixel

    Adafruit GFX

    Adafruit SSD1306

  4.Configurar tu SSID y contraseña WiFi.

  5.Subir el sketch al ESP8266.

  6.Conectar a la IP mostrada por el ESP8266.

¡Listo!

💡 Autores

David Lorente Wagner
Pablo Javier Montoro Bermúdez
