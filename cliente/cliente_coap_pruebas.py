# cliente_coap_pruebas.py - Cliente CoAP para la práctica + Reto 1
#
# Instalación (una sola vez):   pip install aiocoap
#
# Uso:
#   python cliente_coap_pruebas.py todo          -> corre P1, P2, P3, P4 y el Reto 1
#   python cliente_coap_pruebas.py sensor        -> GET /sensor (P1)
#   python cliente_coap_pruebas.py on            -> POST /led "ON"
#   python cliente_coap_pruebas.py off           -> POST /led "OFF"
#   python cliente_coap_pruebas.py estado        -> GET /led (estado del LED)
#   python cliente_coap_pruebas.py p3            -> GET /temperatura (ruta inexistente)
#   python cliente_coap_pruebas.py p4            -> 5 lecturas de /sensor en tabla
#   python cliente_coap_pruebas.py invalido      -> POST /led "HOLA" (espera 4.00)
#
# Cambia IP_ESP32 por la IP que mostró tu placa en el Monitor Serie.

import asyncio
import sys
import time

from aiocoap import Context, Message, GET, POST

IP_ESP32 = "192.168.1.85"   # <-- IP asignada a nuestra ESP32 (cámbiala si es otra)
TIEMPO_MAX = 10             # segundos antes de dar la petición por perdida


async def pedir(ctx, metodo, ruta, payload=b""):
    """Envía una petición y muestra código, payload y tiempo."""
    uri = f"coap://{IP_ESP32}/{ruta}"
    nombre = "GET" if metodo == GET else "POST"
    extra = f'  payload="{payload.decode()}"' if payload else ""
    print(f"\n-> {nombre} {uri}{extra}")

    inicio = time.perf_counter()
    try:
        msg = Message(code=metodo, uri=uri, payload=payload)
        resp = await asyncio.wait_for(ctx.request(msg).response, TIEMPO_MAX)
    except asyncio.TimeoutError:
        print(f"   Sin respuesta en {TIEMPO_MAX} s (timeout)")
        return None
    except Exception as e:
        print(f"   Error: {type(e).__name__}: {e}")
        return None

    ms = (time.perf_counter() - inicio) * 1000
    print(f"   Código : {resp.code}")
    print(f"   Payload: {resp.payload.decode(errors='replace')}")
    print(f"   Tiempo : {ms:.1f} ms")
    return resp


async def p4(ctx):
    print("\n=== P4: 5 lecturas de /sensor ===")
    valores = []
    for i in range(1, 6):
        r = await pedir(ctx, GET, "sensor")
        valores.append(r.payload.decode() if r else "sin respuesta")
        await asyncio.sleep(0.5)
    print("\n| Lectura | Respuesta |")
    print("| :---: | :--- |")
    for i, v in enumerate(valores, 1):
        print(f"| {i} | {v} |")


async def main():
    accion = sys.argv[1].lower() if len(sys.argv) > 1 else "todo"
    ctx = await Context.create_client_context()

    if accion == "sensor":
        await pedir(ctx, GET, "sensor")
    elif accion == "on":
        await pedir(ctx, POST, "led", b"ON")
    elif accion == "off":
        await pedir(ctx, POST, "led", b"OFF")
    elif accion == "estado":
        await pedir(ctx, GET, "led")
    elif accion == "p3":
        await pedir(ctx, GET, "temperatura")
    elif accion == "p4":
        await p4(ctx)
    elif accion == "invalido":
        await pedir(ctx, POST, "led", b"HOLA")
    elif accion == "todo":
        print("=== P1: GET /sensor ===")
        await pedir(ctx, GET, "sensor")

        print("\n=== P2 + Reto 1: POST /led ON, consultar, OFF, consultar ===")
        await pedir(ctx, POST, "led", b"ON")
        await pedir(ctx, GET, "led")
        await asyncio.sleep(2)            # 2 s para ver el LED encendido
        await pedir(ctx, POST, "led", b"OFF")
        await pedir(ctx, GET, "led")

        print("\n=== Reto 1: orden inválida (se espera 4.00 Bad Request) ===")
        await pedir(ctx, POST, "led", b"HOLA")

        print("\n=== P3: ruta inexistente /temperatura ===")
        print("   (CoAPSimple manda el 4.04 sin Token; aiocoap puede marcar")
        print("    timeout. Verifica el 4.04 en Wireshark.)")
        await pedir(ctx, GET, "temperatura")

        await p4(ctx)
    else:
        print("Acción no reconocida. Usa: todo, sensor, on, off, estado, p3, p4, invalido")

    await ctx.shutdown()


asyncio.run(main())
