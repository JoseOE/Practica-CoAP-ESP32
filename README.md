# 📡 Práctica de Laboratorio: Servidor CoAP en ESP32 con Arduino IDE

**Institución:** Instituto Tecnológico Superior del Occidente del Estado de Hidalgo (ITSOEH)
**División:** Ingeniería en Tecnologías de la Información y Comunicaciones
**Docente:** M.C. Saúl Isaí Soto Ortiz
**Práctica original:** [ssoto123/Practica-de-Laboratorio-Servidor-CoAP-en-ESP32-con-Arduino-IDE](https://github.com/ssoto123/Practica-de-Laboratorio-Servidor-CoAP-en-ESP32-con-Arduino-IDE)

### 👥 Equipo

| Integrante | GitHub |
| :--- | :--- |
| José María Ortiz Escamilla | [@JoseOE](https://github.com/JoseOE) |
| Valeria Soto Hernández | [@ValeSot0](https://github.com/ValeSot0) |
| Edwin Salvador Soto Hernández | [@EdwinSotoHz](https://github.com/EdwinSotoHz) |
| Yael Antonio Chávez Atanacio | [@YaellCh](https://github.com/YaellCh) |
| Eduardo Gómez Mejía | [@EduardoGomezTics](https://github.com/EduardoGomezTics) |

---

## 📑 Contenido

1. [Resumen](#-1-resumen) · [Video](#-video-de-funcionamiento)
2. [Estructura del repositorio](#-2-estructura-del-repositorio)
3. [Materiales](#-3-materiales)
4. [Marco teórico](#-4-marco-teórico-coap-en-pocas-palabras)
5. [Arquitectura y conexiones](#-5-arquitectura-y-conexiones)
6. [Configuración del entorno](#%EF%B8%8F-6-configuración-del-entorno)
7. [Código del servidor y mejoras](#-7-código-del-servidor-y-mejoras)
8. [Cliente CoAP en Python](#-8-cliente-coap-en-python)
9. [Pruebas y resultados](#-9-pruebas-y-resultados)
10. [Análisis con Wireshark](#-10-análisis-con-wireshark)
11. [Hallazgos técnicos](#-11-hallazgos-técnicos)
12. [Preguntas de análisis](#-12-preguntas-de-análisis)
13. [Solución de problemas](#%EF%B8%8F-13-solución-de-problemas)
14. [Conclusiones](#-14-conclusiones)
15. [Referencias](#-referencias)

---

## 📖 1. Resumen

Convertimos una ESP32 en un **servidor CoAP** (Constrained Application Protocol, RFC 7252) que expone dos recursos por UDP en el puerto **5683**:

| Recurso | Método | Comportamiento | Código de respuesta |
| :--- | :--- | :--- | :---: |
| `/sensor` | `GET` | Temperatura simulada entre 25.4 y 30.3 °C, p. ej. `Temp: 27.5 C` | 2.05 Content |
| `/led` | `POST` `ON` / `OFF` | **Enciende o apaga un LED real en el GPIO 21** y responde con el estado leído del pin | 2.04 Changed |
| `/led` | `GET` | Devuelve el estado actual del LED | 2.05 Content |
| `/led` | `POST` (otro texto) | Rechaza la orden | 4.00 Bad Request |

Además de la práctica base (pruebas P1–P4), realizamos el **Reto 1** (LED físico) y corregimos un detalle de compatibilidad del servidor con el cliente de Python (ver [sección 11](#-11-hallazgos-técnicos)).

**Resultado:** todas las pruebas funcionaron con la ESP32 en la IP `192.168.1.85`. La salida completa está en [`resultados/salida_pruebas.txt`](resultados/salida_pruebas.txt).

### 🎥 Video de funcionamiento

El video con la demostración del prototipo está en Google Drive:

**[▶️ Ver el video del funcionamiento](https://drive.google.com/drive/folders/1MVohdBMKIYfqj2gRCWGgWuZ6j4Wyt1i4)**

Muestra el arranque del servidor y la IP asignada, la lectura de `/sensor` (P1), el encendido y apagado del LED en el GPIO 21 (P2 y Reto 1), la orden inválida (4.00), la ruta inexistente (P3) y las cinco lecturas de P4.

---

## 📁 2. Estructura del repositorio

```
Practica-CoAP-ESP32/
├── README.md                              <- esta documentación
├── firmware/
│   └── Servidor_CoAP_ESP32/
│       └── Servidor_CoAP_ESP32.ino        <- servidor CoAP + Reto 1 (Arduino IDE)
├── cliente/
│   ├── cliente_coap_pruebas.py            <- cliente de pruebas (aiocoap)
│   └── requirements.txt
└── resultados/
    └── salida_pruebas.txt                 <- salida real de las pruebas
```

---

## 🛠 3. Materiales

**Hardware**

- 1 tarjeta ESP32
- 1 cable USB de datos
- 1 LED
- 1 resistencia de 220 Ω
- Cables Dupont y protoboard
- Computadora con Wi-Fi
- Red Wi-Fi de **2.4 GHz** compartida por la ESP32 y la computadora

**Software**

- Arduino IDE 2.x con el paquete de placas **esp32** (Espressif Systems)
- Librería **CoAP simple library** de hirotakaster
- Python 3 con la librería `aiocoap`
- Wireshark (para el análisis de paquetes)

---

## 🧠 4. Marco teórico: CoAP en pocas palabras

CoAP conserva el modelo REST de la web (recursos con URI y métodos `GET`, `POST`, `PUT`, `DELETE`), pero viaja sobre **UDP** en lugar de TCP. No hay *handshake* ni conexión abierta: cada petición y su respuesta caben en un datagrama cada una.

| Aspecto | HTTP sobre TCP | CoAP sobre UDP |
| :--- | :--- | :--- |
| Conexión previa | Sí (*handshake* de 3 vías, más TLS si es HTTPS) | No: el primer datagrama ya es la petición |
| Formato | Texto, cabeceras de cientos de bytes | Binario, cabecera fija de 4 bytes |
| Fiabilidad | Siempre la da TCP | Opcional, por mensaje (CON + ACK) |
| Consumo | Mayor | Menor: la radio puede dormir entre mensajes |

**Estructura de un mensaje CoAP**

```
| Ver | T | TKL | Code | Message ID | Token (0-8 B) | Opciones | 0xFF | Payload |
   2b  2b   4b     8b       16b
```

- **Message ID:** detecta duplicados y empareja un CON con su ACK.
- **Token:** empareja la *respuesta* con la *petición*.
- **Opciones:** por ejemplo `Uri-Path = "sensor"` o `Content-Format = text/plain`.

**Tipos de mensaje**

| Tipo | Significado |
| :--- | :--- |
| **CON** | Confirmable: exige un ACK y se reintenta si no llega |
| **NON** | No confirmable: se envía y se olvida |
| **ACK** | Confirma un CON; puede llevar la respuesta "a cuestas" (*piggyback*) |
| **RST** | El mensaje no pudo procesarse |

**Códigos usados en esta práctica:** 2.04 Changed, 2.05 Content, 4.00 Bad Request y 4.04 Not Found.

---

## 🔌 5. Arquitectura y conexiones

```
  PC (cliente)                    Router Wi-Fi 2.4 GHz              ESP32 (servidor CoAP)
  cliente_coap_pruebas.py  ──UDP──>  solo reenvía      ──UDP──>  192.168.1.85 : 5683
  aiocoap                  <──────   datagramas        <──────   coap.loop()
                                                                   ├── /sensor -> callback_sensor()
                                                                   └── /led    -> callback_led() -> GPIO 21
```

**Conexión del LED (Reto 1)**

```
ESP32 GPIO 21 ──── resistencia 220 Ω ──── ánodo (+, pata larga) del LED
                                          cátodo (−, pata corta) ──── GND
```

> 🎥 Se puede ver en el [video de funcionamiento](#-video-de-funcionamiento).

---

## ⚙️ 6. Configuración del entorno

### 6.1 Arduino IDE y ESP32

1. En *Herramientas → Placa → Gestor de tarjetas*, instalar **esp32** de Espressif Systems.
2. En *Herramientas → Administrar bibliotecas*, instalar **CoAP simple library** de hirotakaster.
3. Abrir `firmware/Servidor_CoAP_ESP32/Servidor_CoAP_ESP32.ino`.
4. Escribir el SSID y la contraseña de la red de 2.4 GHz:
   ```cpp
   const char* ssid     = "TU_SSID_WIFI";
   const char* password = "TU_PASSWORD_WIFI";
   ```
5. Seleccionar la placa ESP32 y el puerto, y subir el sketch.
6. Abrir el Monitor Serie a **115200** baudios y presionar **RST**. La placa muestra su IP:
   ```
   Conectando a Wi-Fi.....
   Conectado a la red
   Dirección IP del Servidor CoAP: 192.168.1.85
   Servidor CoAP escuchando en el puerto UDP 5683
   ```

> 🎥 Se puede ver en el [video de funcionamiento](#-video-de-funcionamiento).

### 6.2 Computadora (cliente)

1. Conectar la computadora a la **misma red Wi-Fi** que la ESP32.
2. Instalar la librería del cliente:
   ```
   pip install -r cliente/requirements.txt
   ```
3. Revisar que `IP_ESP32` en `cliente/cliente_coap_pruebas.py` coincida con la IP de la placa (`192.168.1.85`).
4. Si Windows pregunta, permitir que Python use la red.

---

## 💻 7. Código del servidor y mejoras

El sketch completo está en [`firmware/Servidor_CoAP_ESP32/Servidor_CoAP_ESP32.ino`](firmware/Servidor_CoAP_ESP32/Servidor_CoAP_ESP32.ino) y está comentado línea por línea.

**Flujo general**

- **`setup()`:** inicia el Monitor Serie, configura el GPIO 21 como salida (LED apagado), conecta al Wi-Fi, imprime la IP, registra los recursos con `coap.server(...)` y arranca el servidor con `coap.start()`.
- **`loop()`:** solo llama a `coap.loop()`. No tiene `delay()`, para no perder peticiones.
- **`callback_sensor()`:** genera `25.4 + random(0, 50) / 10.0` y responde `Temp: XX.X C` (formateado con `snprintf` a un decimal).
- **`callback_led()`:** lee el payload, lo limpia (`trim()` y `toUpperCase()`) y actúa según el método:

```cpp
if (packet.code == COAP_GET) {            // GET /led -> solo consulta
  responder(..., estado, COAP_CONTENT);   // 2.05
  return;
}
if (orden == "ON")       digitalWrite(PIN_LED, HIGH);
else if (orden == "OFF") digitalWrite(PIN_LED, LOW);
else { responder(..., "Error: usa ON u OFF", COAP_BAD_REQUEST); return; }  // 4.00

ledEncendido = (digitalRead(PIN_LED) == HIGH);   // estado REAL del pin
responder(..., estado, COAP_CHANGED);            // 2.04
```

**Cambios respecto al código original**

| # | Cambio | Motivo |
| :---: | :--- | :--- |
| 1 | Todas las respuestas regresan el **Token** de la petición, mediante la función `responder()` | Sin el Token, el cliente `aiocoap` descarta la respuesta y marca *timeout* (ver [sección 11.1](#111-la-respuesta-original-no-incluía-el-token)) |
| 2 | **Reto 1:** `/led` controla un LED real en el **GPIO 21** con `pinMode`, `digitalWrite` y `digitalRead` | Pasar de una respuesta fija a un actuador real |
| 3 | La respuesta de `/led` refleja el **estado leído del pin** (`LED: ENCENDIDO` / `LED: APAGADO`) | El cliente se entera de lo que realmente pasó |
| 4 | `POST /led` responde **2.04 Changed** en lugar de 2.05 | 2.04 es el código correcto cuando una petición modifica el recurso |
| 5 | Una orden distinta de `ON`/`OFF` responde **4.00 Bad Request** | Validar la entrada en lugar de ignorarla |
| 6 | `GET /led` consulta el estado sin cambiarlo | Separar lectura y escritura, como en REST |
| 7 | Temperatura formateada con `snprintf("%.1f")` en lugar de `String` | `String(float)` imprime dos decimales y fragmenta la memoria |

---

## 🐍 8. Cliente CoAP en Python

[`cliente/cliente_coap_pruebas.py`](cliente/cliente_coap_pruebas.py) usa `aiocoap` y muestra, por cada petición, el código, el payload y el tiempo de respuesta. Espera hasta 10 s por respuesta.

| Comando | Qué hace |
| :--- | :--- |
| `python cliente_coap_pruebas.py todo` | Corre todas las pruebas en orden |
| `python cliente_coap_pruebas.py sensor` | P1: `GET /sensor` |
| `python cliente_coap_pruebas.py on` | P2 / Reto 1: `POST /led` con `ON` |
| `python cliente_coap_pruebas.py off` | Reto 1: `POST /led` con `OFF` |
| `python cliente_coap_pruebas.py estado` | Reto 1: `GET /led` |
| `python cliente_coap_pruebas.py invalido` | Reto 1: `POST /led` con `HOLA` (se espera 4.00) |
| `python cliente_coap_pruebas.py p3` | P3: `GET /temperatura` (ruta inexistente) |
| `python cliente_coap_pruebas.py p4` | P4: 5 lecturas de `/sensor` con tabla |

---

## ✅ 9. Pruebas y resultados

Todas las pruebas se hicieron contra la ESP32 en `coap://192.168.1.85`.

### 9.1 Resumen

| # | Petición | Resultado esperado | Resultado obtenido | Estado |
| :---: | :--- | :--- | :--- | :---: |
| **P1** | `GET /sensor` | 2.05 con `Temp: XX.X C` | 2.05 Content · `Temp: 30.3 C` · 104.6 ms | ✅ |
| **P2** | `POST /led` `ON` | Respuesta de confirmación y payload en el Monitor Serie | 2.04 Changed · `LED: ENCENDIDO` · 4.7 ms | ✅ |
| **P3** | `GET /temperatura` | 4.04 Not Found | *Timeout* en el cliente; el 4.04 se verifica en Wireshark (ver 11.2) | ✅* |
| **P4** | `GET /sensor` × 5 | 5 valores distintos entre 25.4 y 30.3 °C | 28.0, 29.7, 27.5, 27.6, 29.8 °C | ✅ |
| **R1** | `POST /led` `ON` / `OFF` y `GET /led` | El LED físico enciende y apaga; el estado se puede consultar | 2.04 / 2.05 con el estado correcto | ✅ |
| **R1** | `POST /led` `HOLA` | 4.00 Bad Request | 4.00 Bad Request · `Error: usa ON u OFF` | ✅ |

\* P3 se comporta como se espera del servidor. La diferencia está en cómo responde la librería, explicada en la sección 11.2.

> 🎥 Las pruebas completas se pueden ver en el [video de funcionamiento](#-video-de-funcionamiento).

### 9.2 P1: GET /sensor

```
-> GET coap://192.168.1.85/sensor
   Código : 2.05 Content
   Payload: Temp: 30.3 C
   Tiempo : 104.6 ms
```

El servidor respondió con 2.05 Content y una temperatura dentro del rango esperado.

### 9.3 P2 y Reto 1: control del LED en GPIO 21

| Paso | Petición | Código | Payload | Tiempo |
| :---: | :--- | :---: | :--- | ---: |
| 1 | `POST /led` `ON` | 2.04 Changed | `LED: ENCENDIDO` | 4.7 ms |
| 2 | `GET /led` | 2.05 Content | `LED: ENCENDIDO` | 6.8 ms |
| 3 | `POST /led` `OFF` | 2.04 Changed | `LED: APAGADO` | 32.9 ms |
| 4 | `GET /led` | 2.05 Content | `LED: APAGADO` | 8.4 ms |
| 5 | `POST /led` `HOLA` | 4.00 Bad Request | `Error: usa ON u OFF` | 4.5 ms |

El LED físico encendió con `ON` y se apagó con `OFF`. La consulta con `GET` devolvió el estado real en ambos casos, y la orden inválida fue rechazada sin modificar el LED.

> 🎥 El LED encendiéndose y apagándose se puede ver en el [video de funcionamiento](#-video-de-funcionamiento).

### 9.4 P3: ruta inexistente

```
-> GET coap://192.168.1.85/temperatura
   Sin respuesta en 10 s (timeout)
```

La ruta `/temperatura` no está registrada en `setup()`, así que la librería responde **4.04 Not Found**. El cliente no lo muestra porque esa respuesta llega sin Token y `aiocoap` la descarta (ver 11.2). La respuesta 4.04 se puede comprobar con Wireshark usando el filtro `coap.code == 132` (sección 10).

### 9.5 P4: cinco lecturas de /sensor

| Lectura | Temperatura | Tiempo |
| :---: | :---: | ---: |
| 1 | 28.0 °C | 19.2 ms |
| 2 | 29.7 °C | 102.7 ms |
| 3 | 27.5 °C | 99.0 ms |
| 4 | 27.6 °C | 101.6 ms |
| 5 | 29.8 °C | 103.6 ms |

Los cinco valores son distintos y están dentro del rango de 25.4 a 30.3 °C. Esto confirma que estamos hablando con nuestra placa y no con la de otro equipo.

---

## 🦈 10. Análisis con Wireshark

**Procedimiento**

1. Abrir Wireshark sobre la interfaz Wi-Fi de la computadora.
2. Aplicar el filtro `coap` (o `udp.port == 5683`).
3. Ejecutar `python cliente_coap_pruebas.py sensor` y luego `python cliente_coap_pruebas.py p3`.
4. Revisar cada paquete en el panel de detalle.

**Campos a identificar**

| Campo | Petición P1 | Respuesta P1 | Respuesta P3 |
| :--- | :--- | :--- | :--- |
| Tipo | CON | ACK | ACK |
| Código | 0.01 GET | 2.05 Content | 4.04 Not Found |
| Message ID | Asignado por el cliente | **El mismo** de la petición | El mismo de su petición |
| Token | 2 bytes generados por `aiocoap` | **El mismo** de la petición | Vacío (lo envía la librería) |
| Uri-Path | `sensor` | — | — |
| Payload | — | `Temp: XX.X C` | — |

**Filtros útiles**

| Filtro | Muestra |
| :--- | :--- |
| `coap` | Todo el tráfico CoAP |
| `coap.code == 69` | Solo respuestas 2.05 Content |
| `coap.code == 68` | Solo respuestas 2.04 Changed |
| `coap.code == 132` | Solo el 4.04 Not Found de P3 |

Con el filtro `coap.code == 132` se ve que la ESP32 sí responde 4.04 a la ruta `/temperatura`, aunque el cliente marque *timeout*: la respuesta llega con el Token vacío (sección 11.2).

---

## 🔍 11. Hallazgos técnicos

### 11.1 La respuesta original no incluía el Token

En el código original, las respuestas se envían con:

```cpp
coap.sendResponse(ip, port, packet.messageid, "texto");
```

Revisando el código fuente de CoAPSimple, esa versión corta de `sendResponse()` llama internamente a la versión completa con `token = NULL` y `tokenlen = 0`. Es decir, la respuesta sale **sin Token**.

`aiocoap` envía siempre un Token (de 2 bytes) y lo usa para emparejar la respuesta con la petición. Si la respuesta no trae el mismo Token, la descarta y la petición termina en *timeout*, aunque la ESP32 sí haya respondido.

Lo comprobamos simulando el servidor:

| Respuesta del servidor | Resultado en aiocoap |
| :--- | :--- |
| ACK 2.05 sin Token (como el código original) | `TimeoutError` |
| ACK 2.05 regresando el Token de la petición | `2.05 Content b'Temp: 27.1 C'` |

**Corrección:** usar la versión completa de `sendResponse()` y regresar `packet.token` y `packet.tokenlen`. En el sketch esto quedó en una función auxiliar:

```cpp
void responder(CoapPacket &packet, IPAddress ip, int port,
               const char* texto, COAP_RESPONSE_CODE code) {
  coap.sendResponse(ip, port, packet.messageid,
                    texto, strlen(texto),
                    code, COAP_TEXT_PLAIN,
                    packet.token, packet.tokenlen);
}
```

### 11.2 El 4.04 de la librería tampoco lleva Token

Cuando llega una ruta no registrada, la propia librería responde 4.04 Not Found, pero también con `token = NULL`. Eso ocurre dentro de la librería, no en nuestro sketch, así que no se puede corregir desde el `.ino`. Por eso en P3 el cliente marca *timeout*: la respuesta sí sale de la ESP32 (se ve en Wireshark), pero `aiocoap` no la puede emparejar con la petición.

### 11.3 Latencia de ~5 ms frente a ~100 ms

Algunas respuestas tardaron entre 4 y 8 ms y otras alrededor de 100 ms. La explicación más probable es el **ahorro de energía del Wi-Fi** de la ESP32, que viene activado por defecto: entre mensajes, la radio se duerme y despierta periódicamente para revisar si hay paquetes pendientes.

- Las peticiones que llegaron justo después de otra (P2: 4.7, 6.8 y 8.4 ms) encontraron la radio despierta.
- Las de P4, separadas por medio segundo, tardaron cerca de 100 ms porque la radio ya se había dormido.

No es un error: es el intercambio entre consumo de energía y latencia del que trata CoAP. Se puede comprobar agregando `WiFi.setSleep(false);` después de `WiFi.begin(...)`; con eso las respuestas deberían bajar a unos pocos milisegundos, a cambio de mayor consumo.

---

## ❓ 12. Preguntas de análisis

**1. ¿Por qué CoAP puede usar UDP si UDP no garantiza la entrega? ¿Qué mecanismo compensa esa falta y cuándo no se usa?**

Porque CoAP pone su propia fiabilidad encima de UDP, y solo cuando se necesita. Los mensajes **CON** exigen un ACK con el mismo Message ID; si el ACK no llega, el cliente retransmite con tiempos de espera crecientes, y el Message ID permite al servidor detectar duplicados. Los mensajes **NON** no usan este mecanismo: se envían una vez y, si se pierden, no pasa nada. Son útiles para lecturas periódicas donde la siguiente lectura llegará pronto. Así, cada mensaje paga el costo de la fiabilidad solo si lo necesita, en lugar de que TCP lo imponga siempre.

**2. ¿La respuesta de `/sensor` fue un ACK independiente o viajó "a cuestas" con el payload?**

Viajó **a cuestas** (*piggybacked*). La librería responde con un único mensaje de tipo **ACK** que ya incluye el código **2.05 Content** y el payload `Temp: XX.X C`, con el mismo Message ID de la petición CON. Si fuera una respuesta separada, veríamos primero un ACK vacío (código 0.00) y después otro mensaje CON con el 2.05. Con una respuesta a cuestas, el intercambio completo son solo dos paquetes: la petición CON y el ACK con el payload.

**3. ¿Qué campo permitió saber que la respuesta correspondía a nuestra petición y no a la de otro equipo?**

Dos campos de la cabecera:

- El **Message ID** empareja la petición CON con su ACK.
- El **Token** empareja la respuesta con la petición, y es el que usa `aiocoap` para aceptarla.

Lo comprobamos durante la práctica: una respuesta con el Message ID correcto pero sin Token es descartada por el cliente (sección 11.1). Además, la respuesta va dirigida a la IP y el puerto UDP de nuestra computadora, así que otro equipo no la recibe.

**4. ¿Cuántos bytes ocupa el mensaje CoAP de respuesta y cómo se compara con HTTP?**

Para la respuesta `Temp: 30.3 C` de nuestro servidor:

| Parte | Bytes |
| :--- | ---: |
| Cabecera fija (Ver, T, TKL, Code, Message ID) | 4 |
| Token (devuelto por el servidor) | 2 |
| Opción Content-Format (1 byte de encabezado + 2 de valor) | 3 |
| Marcador de payload `0xFF` | 1 |
| Payload `Temp: 30.3 C` | 12 |
| **Total del mensaje CoAP** | **22** |
| + UDP (8) + IPv4 (20) | 50 |

Una respuesta HTTP/1.1 equivalente necesita al menos la línea de estado y cabeceras como `Content-Type` y `Content-Length`, unos 60–70 bytes antes del payload, y normalmente cientos de bytes con las cabeceras habituales de un servidor. A eso se suma el *handshake* de TCP (3 paquetes) y el cierre de la conexión. CoAP resuelve el intercambio completo en **dos datagramas**.

**5. Si el servidor controlara una bomba de riego con batería, ¿CON o NON para reportar la humedad cada minuto?**

**NON** para el reporte de humedad. Si se pierde una lectura, llega otra un minuto después, así que no vale la pena gastar energía en esperar un ACK ni en retransmitir: cada CON obliga a mantener la radio encendida hasta recibir la confirmación. En cambio, las **órdenes a la bomba** (encender o apagar) sí deberían ser **CON**, porque perder una puede dejar la bomba encendida.

Nuestra medición de latencia (sección 11.3) muestra el otro lado del intercambio: con el ahorro de energía activo, la radio duerme y las respuestas tardan ~100 ms en lugar de ~5 ms. En un nodo con batería conviene aceptar esa latencia y enviar pocos mensajes, cortos y sin confirmación.

**6. ¿Qué riesgo implica un servidor CoAP sin DTLS en una red compartida y quién podría explotarlo?**

Sin DTLS no hay **cifrado** ni **autenticación**:

- Cualquier dispositivo en la misma red puede **leer** las temperaturas con Wireshark, igual que lo hicimos nosotros.
- Cualquiera puede **enviar órdenes**: un `POST coap://192.168.1.85/led OFF` desde otra computadora apaga nuestro LED, y el servidor no tiene forma de saber quién lo envió.
- También es posible suplantar al servidor o repetir mensajes capturados.

En el laboratorio, cualquier compañero conectado a la misma red Wi-Fi podría hacerlo con el mismo script de Python, solo cambiando la IP. Con un actuador real (una cerradura, una bomba, un motor) el riesgo sería mucho mayor. La solución es usar **CoAP sobre DTLS** (`coaps://`, puerto 5684), o al menos aislar la red de los dispositivos IoT.

---

## 🛠️ 13. Solución de problemas

| Síntoma | Causa probable | Solución |
| :--- | :--- | :--- |
| Puntos sin fin en el Monitor Serie | SSID o contraseña incorrectos, o red de 5 GHz | Revisar credenciales y usar una red de 2.4 GHz |
| La placa tiene IP pero todo da *timeout* | PC y placa en redes distintas, o aislamiento entre clientes | Conectar ambas a la misma red (por ejemplo, el hotspot de un teléfono) |
| *Timeout* con todo en la misma red | Firewall de Windows bloqueando a Python | Permitir Python en redes privadas y públicas |
| *Timeout* solo con el código original | Respuestas sin Token | Usar el sketch de este repositorio (sección 11.1) |
| El LED no enciende pero la respuesta dice `ENCENDIDO` | LED invertido o mal conectado | Revisar polaridad: pata larga hacia el GPIO 21 (vía la resistencia) |
| Error de compilación en `coap-simple.h` | Librería no instalada o equivocada | Instalar *CoAP simple library* de hirotakaster |

---

## 📝 14. Conclusiones

- La ESP32 funcionó como servidor CoAP con tres comportamientos sobre dos recursos, y cada intercambio se resolvió en un par de datagramas UDP, sin conexión previa.
- Con el Reto 1, el recurso `/led` dejó de devolver un texto fijo y pasó a controlar un actuador real en el GPIO 21, respondiendo con el estado leído del pin y con los códigos CoAP adecuados (2.04, 2.05 y 4.00).
- El Token no es un detalle opcional: un servidor que no lo devuelve es incompatible con clientes estándar como `aiocoap`. Lo detectamos, lo comprobamos y lo corregimos.
- Medimos en la práctica el intercambio entre energía y latencia: con el ahorro de energía del Wi-Fi activo, la respuesta pasa de ~5 ms a ~100 ms.
- Sin DTLS, cualquier dispositivo de la red puede leer los datos y controlar el LED. Para un despliegue real, la seguridad tendría que ser el siguiente paso.

---

## 📚 Referencias

- Shelby, Z., Hartke, K. y Bormann, C. (2014). *The Constrained Application Protocol (CoAP)*, RFC 7252, IETF.
- Soto Ortiz, S. I. *Práctica de Laboratorio: Implementación de un Servidor CoAP en ESP32 con Arduino IDE*. ITSOEH. [GitHub](https://github.com/ssoto123/Practica-de-Laboratorio-Servidor-CoAP-en-ESP32-con-Arduino-IDE)
- hirotakaster. *CoAP simple library for Arduino*. [GitHub](https://github.com/hirotakaster/CoAP-simple-library)
- aiocoap: The Python CoAP library. [Documentación](https://aiocoap.readthedocs.io/)
