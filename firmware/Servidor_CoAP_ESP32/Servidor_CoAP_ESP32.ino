// ============================================================
// Servidor CoAP en ESP32 - Práctica de laboratorio + Reto 1
// Recursos:
//   /sensor  GET          -> temperatura simulada (2.05 Content)
//   /led     POST ON/OFF  -> enciende/apaga el LED del GPIO 21 (2.04 Changed)
//   /led     GET          -> devuelve el estado real del LED (2.05 Content)
// Transporte: UDP, puerto 5683
// Librería: CoAPSimple, de hirotakaster
// Placa: ESP32 en Arduino IDE 2.x (LED externo en GPIO 21)
//
// Cambios respecto al código original:
//   1) Todas las respuestas regresan el Token de la petición
//      (packet.token, packet.tokenlen). Sin él, clientes como
//      aiocoap descartan la respuesta y marcan timeout.
//   2) Reto 1: /led controla un LED real en el GPIO 21 y responde
//      con el estado real.
//   3) Payload no reconocido en /led -> 4.00 Bad Request.
// ============================================================

#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>

// --- Credenciales de la red (solo 2.4 GHz) ------------------
const char* ssid     = "TU_SSID_WIFI";
const char* password = "TU_PASSWORD_WIFI";

// --- Pin del LED --------------------------------------------
// LED externo conectado al GPIO 21:
//   GPIO 21 -> resistencia 220 ohm -> ánodo (pata larga) del LED
//   cátodo (pata corta) -> GND
const int PIN_LED = 21;
bool ledEncendido = false;   // Estado real del LED

WiFiUDP udp;
Coap coap(udp);

// ------------------------------------------------------------
// Función auxiliar: responde regresando SIEMPRE el Token.
// code: COAP_CONTENT (2.05), COAP_CHANGED (2.04), COAP_BAD_REQUEST (4.00)...
// ------------------------------------------------------------
void responder(CoapPacket &packet, IPAddress ip, int port,
               const char* texto, COAP_RESPONSE_CODE code) {
  coap.sendResponse(ip, port, packet.messageid,
                    texto, strlen(texto),
                    code, COAP_TEXT_PLAIN,
                    packet.token, packet.tokenlen);
}

// ============================================================
// CALLBACK DEL RECURSO /led
// ============================================================
void callback_led(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("Petición CoAP recibida en /led");

  // --- Leer el payload como texto ---------------------------
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = '\0';

  String orden(p);
  orden.trim();          // quita espacios y saltos de línea
  orden.toUpperCase();   // acepta "on", "On", "ON"...

  Serial.print("Payload recibido: ");
  Serial.println(orden);

  // --- GET /led: solo consultar el estado -------------------
  if (packet.code == COAP_GET) {
    const char* estado = ledEncendido ? "LED: ENCENDIDO" : "LED: APAGADO";
    Serial.println(estado);
    responder(packet, ip, port, estado, COAP_CONTENT);
    return;
  }

  // --- POST /led: ejecutar la orden -------------------------
  if (orden == "ON") {
    digitalWrite(PIN_LED, HIGH);
    ledEncendido = true;
  } else if (orden == "OFF") {
    digitalWrite(PIN_LED, LOW);
    ledEncendido = false;
  } else {
    Serial.println("Orden no válida (usa ON u OFF)");
    responder(packet, ip, port, "Error: usa ON u OFF", COAP_BAD_REQUEST);
    return;
  }

  // Respondemos con el estado REAL leído del pin, no con un
  // texto fijo: así el cliente sabe qué pasó de verdad.
  ledEncendido = (digitalRead(PIN_LED) == HIGH);
  const char* estado = ledEncendido ? "LED: ENCENDIDO" : "LED: APAGADO";
  Serial.println(estado);

  // 2.04 Changed: el código correcto cuando un POST modifica
  // el estado del recurso.
  responder(packet, ip, port, estado, COAP_CHANGED);
}

// ============================================================
// CALLBACK DEL RECURSO /sensor
// ============================================================
void callback_sensor(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("Petición CoAP recibida en /sensor");

  // 25.4 °C + 0.0 a 4.9 °C de variación aleatoria
  float temperaturaSimulada = 25.4 + (random(0, 50) / 10.0);

  // snprintf con un decimal: "Temp: 27.1 C"
  char respuesta[24];
  snprintf(respuesta, sizeof(respuesta), "Temp: %.1f C", temperaturaSimulada);
  Serial.println(respuesta);

  responder(packet, ip, port, respuesta, COAP_CONTENT);
}

// ============================================================
// setup()
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // LED como salida y apagado al arrancar
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  // --- Conexión Wi-Fi ---------------------------------------
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConectado a la red");
  Serial.print("Dirección IP del Servidor CoAP: ");
  Serial.println(WiFi.localIP());

  // --- Registrar recursos -----------------------------------
  coap.server(callback_led, "led");
  coap.server(callback_sensor, "sensor");

  // --- Arrancar el servidor (UDP 5683) ----------------------
  coap.start();
  Serial.println("Servidor CoAP escuchando en el puerto UDP 5683");
}

// ============================================================
// loop(): solo escuchar. Nada de delays largos aquí.
// ============================================================
void loop() {
  coap.loop();
}
