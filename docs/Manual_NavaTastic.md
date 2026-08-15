# Manual de Administración Remota NavaTastic (v4.2.1)

> **ADENDA 14/08/2026 — REPO UNIFICADO (V2)**: manual **VIGENTE**. Ronda V2 (14/08, tras
> el snapshot baseline): nuevos comandos/mensajes `sleepmsg` y avisos [Sueño]/[Vivo]/[Listo],
> `status` con favoritos Auto/Manual reales, `power` con ADC + INA cargando/descargando,
> `fav ls` etiquetado, fragmentación por línea. El código de referencia vive en
> `src/modules/NavaCLIModule.h/.cpp` del repo único. Los 12 envs compilan este mismo
> módulo (diferencia solo `set_txpower` 0-12/0-22 por `NAVARICO_RADIO_*`).
>
> **ADENDA 15/08/2026 — CIERRE V2.3 + FRENTES A/B VERIFICADOS EN BANCO**:
> - **Ciclo sueño/despertar PROBADO punta a punta** (Promicro R2IG): [Sueño] 3375 mV → dormido →
>   despertar LPCOMP 3710 mV → [Listo] 3772 mV. Los avisos salen por el **canal Navadmin
>   (slot 1)**; verificar que el canal está materializado para recibirlos.
> - **`sleepmsg` reparado (V2.3)**: el gate nunca se activaba por comando (bug de parseo);
>   ahora persiste correctamente en `/resilience.bin` (84 B con versión; los ficheros
>   antiguos de 80 B se migran solos: `sleepMsgs=1`, rol sin fijar).
> - **Rol semi-permanente en AMBAS ramas (V2.1)**: `set_role` persiste en `/resilience.bin`
>   y sobrevive al factory reset también en Rama 2.
> - **Acreditación admin persistente (F16a, 15/08)**: tras validar el PKI de un admin, el
>   repetidor guarda la acreditación (bitfield+favorito) en disco → el admin sigue
>   respondiendo tras un reboot sin necesidad de re-anunciar nodeinfo.
> - **Fix de entrega (Frente A, 15/08)**: los avisos [Sueño]/[Vivo]/[Listo] se encolaban con
>   destino inválido (nadie los entregaba); ahora se difunden correctamente al canal Navadmin.

Documento **3 de 3** del proyecto Navarrico. Manual de operación de los comandos `/nava` (módulo `NavaCLIModule`) y código fuente de referencia para auditoría con otro agente.

> El manual de usuario final del firmware (resiliencia, montaje, protocolo de rescate) está en el historial: ver `OLD_CONTEXT/Manual_Navarrico_4.2.md`.

---

## 🛡️ Nivel de Seguridad de los Comandos

- **Canal Abierto (Navadmin)**: Únicamente comandos de consulta y diagnóstico (solo lectura). Se ejecutan en lote sobre toda la flota con retraso de respuesta aleatorio (jitter) anticolisión. Los emisores NO acreditados como administrador no reciben respuesta alguna (silencio total). **Rate-limit: máximo 1 comando por nodo cada 30s** (el exceso se ignora en silencio) para evitar agotamiento de batería/airtime por abuso del canal público.
- **Solo por DM Privado Cifrado**: Comandos críticos de configuración, reinicio, base de datos, bloqueos, favoritos y energía. Requieren firma criptográfica PKI obligatoria. Si un nodo conocido envía un DM sin estar acreditado, responde una sola vez: `NO AUTORIZADO COMO ADMINISTRADOR`.

---

## 📊 1. Diagnóstico y Telemetría (Permitidos en Canal Abierto y DM)

- **`/nava ping`** — Respuesta de latencia con uptime y piso de ruido. Ej: `PONG: RN1 | SNR: 3.5 dB | Bat: 4120 mV | UP: 32d 4h | RUIDO: -120 dBm`. Rate-limit: 1 respuesta cada 10s por nodo.
- **`/nava status`** — Salud de memoria: nodos RAM/80, favoritos **Manual/Auto reales** (persisten tras reinicio), huérfanos, estado Auto-Fav, tiempo activo y línea de energía (ADC + INA si presente).
- **`/nava env`** — Batería, heap, temperatura CPU nRF52 y sensor ambiental I2C.
- **`/nava channel`** — Uso de espectro (airtime % y TX %).
- **`/nava peers`** — Vecinos directos a 0 saltos (ID, rol, SNR, tiempo desde último contacto).
- **`/nava rxlog`** — Metadatos de los últimos 5 paquetes recibidos (ID, PortNum, SNR, RSSI).
- **`/nava afc`** — Deriva de frecuencia del TCXO en Hz del último paquete.
- **`/nava reset_reason`** — Motivo del último reinicio (registro RESETREAS).
- **`/nava noise`** — Piso de ruido instantáneo del chip LoRa en dBm.
- **`/nava bat`** — Química activa, voltaje mV, % OCV y estado TX.
- **`/nava route !ID`** — Saltos y SNR con que escucha al nodo. Si no está en la BD, lanza un TraceRoute automáticamente.
- **`/nava trace !ID`** — Lanza un TraceRoute nativo hacia el nodo.
- **`/nava help`** — Glosario corto de comandos.
- **`/nava help <comando>`** tambén: **`/nava <comando> ?`** / **`/nava <comando> help`** interroga a CUALQUIER comando (excepto `msg`) — Ayuda breve de un comando concreto. Ej: `/nava help fav`, `/nava help storm`.

## 🚫 2. Gestión de Bloqueos (SOLO DM PRIVADO CIFRADO)

- **`/nava ign ls`** — Lista los nodos bloqueados.
- **`/nava ign add !ID`** — Bloquea y silencia al nodo, purgando su clave pública.
  - Salvaguardas: `ERR: NO SE PUEDE IGNORAR A UN ADMIN`, `ERR: NO PUEDES IGNORARTE A TI MISMO`, `ERR: NODO DESCONOCIDO, NO SE PUEDE VERIFICAR ADMIN`.
- **`/nava ign rm !ID`** — Desbloquea al nodo (DM para evitar auto-desbloqueo del bloqueado).

## ⭐️ 3. Gestión de Favoritos (SOLO DM PRIVADO CIFRADO)

- **`/nava fav add !ID`** — Añade a favoritos con bypass de saltos; crea slot huérfano en RAM si no se ha oído (máx. 10).
- **`/nava fav rm !ID`** — Elimina de favoritos.
- **`/nava fav ls`** — Lista los favoritos, etiquetados `[AUTO]` (auto-favoriteo) o `[MAN]` (manuales).
- **`/nava fav auto [on|off]`** — Activa/desactiva el auto-favoriteo de routers directos (0 saltos). Por defecto: ACTIVADO. Con OFF no se marcan nuevos auto-favoritos ni se registran routers para bypass de saltos; los favoritos ya existentes se conservan. `/nava fav auto` sin argumento muestra el estado. Persiste en `/resilience.bin` (sobrevive a factory reset). El listado Auto/Manual persistido se reconstruye tras reinicio.

## ⚙️ 4. Configuración del Nodo en Caliente (SOLO DM PRIVADO CIFRADO)

- **`/nava set_name "[Largo]" "[Corto]"`** — Cambia el nombre (soporta comillas).
- **`/nava set_role [client/mute/router]`** — Cambia el rol de hardware. **SEMI-PERMANENTE en AMBAS ramas (V2.1)**: se guarda en `/resilience.bin` y sobrevive al factory reset (un cliente convertido en router sigue siéndolo tras un rescate; se revierte con `set_role client`). Solo un `nrf erase` restaura el rol del perfil del env.
- **`/nava set_mqtt [on/off]`** — Activa/desactiva MQTT.
- **`/nava set_tz [tz_POSIX]`** — Zona horaria POSIX.
- **`/nava set_hops [1-7]`** — Límite de saltos LoRa.
- **`/nava set_txpower [0-12]`** — Potencia TX (Promicro/E22P). En la **Faketec HT-RA62 es [0-22]**.

## 🧹 5. Mantenimiento y Reinicio (SOLO DM PRIVADO CIFRADO)

- **`/nava db_purge`** — Expulsa nodos temporales conservando favoritos y admins.
- **`/nava db_clear`** — Vacía la base de nodos (nuclear). **Importante**: `db_clear` borra también tu propia entrada del repetidor; el DM PKI solo se descifra con tu entrada en su base de datos. Para re-acreditar: **fuerza desde tu mando el reenvío de tu propio NodeInfo (broadcast, NO DM)** antes de enviar cualquier `/nava`. Sin ese paso, el repetidor no responderá hasta el próximo NodeInfo periódico de tu mando (hasta 72 h).
- **`/nava reboot`** — Reinicio diferido a 3s (el ACK sale antes).
- **`/nava factory_reset`** — Reset de fábrica de emergencia restaurando canales de rescate (el ACK sale antes de ejecutarse).

## 🔋 6. Energía y Resiliencia (SOLO DM PRIVADO CIFRADO)

- **`/nava set_chem [lipo/nimh/sodium/lifepo4]`** — Cambia química de batería y ajusta corte/OCV/LPCOMP. Persiste en `/resilience.bin`. Cortes: lipo 3500, nimh 3400, sodium 2600, **lifepo4 2800**. **Sin argumento** responde la química actual + tabla de cortes/despertar (+ qué químicas no aplican en esa placa). **AVISO**: si algo falla, rollback SOLO con `nrf erase`.
  - ⚠️ **En Seed Solar P1, Xiao Kit i2c, Xiao E22P y Heltec T114** la química **`lifepo4` está rechazada** (responde `ERR: LIFEPO4 NO COMPATIBLE, UMBRAL LPCOMP FIJO`): su LPCOMP es fijo por hardware y el umbral (~3.67V–4.04V) supera el voltaje máximo físico de una celda LiFePO4 (~3.65V); si se aceptara, un nodo apagado por batería baja jamás despertaría por solar. Solo `lipo`, `nimh` y `sodium` en esas variantes. En Promicro y Faketec (LPCOMP dinámico) las 4 químicas siguen disponibles.
- **`/nava set_vbat [2400-3600]`** — Corte de apagado por batería en mV. **Sin argumento**: muestra el corte actual. **AVISO**: rollback SOLO con `nrf erase`.
- **`/nava set_vwake [1-5]`** — Nivel LPCOMP de reencendido solar. Voltajes reales (Promicro/Faketec, divisor 0.5): 1=2.1V, 2=2.5V, **3=3.7V (LiPo/NiMH/Sodio)**, 4=4.5V, **5=3.3V (LiFePO4)**. **Sin argumento**: muestra el nivel actual. **AVISO**: rollback SOLO con `nrf erase`.
  - ⚠️ **En Seed Solar P1, Xiao Kit i2c y Xiao E22P** el umbral real es **fijo `3_8` (~3.67V)** (divisor no es 0.5); `set_vwake` no cambia el voltaje de despertar.
  - ⚠️ **En Heltec T114** el umbral real es **fijo `2_8` (~4.04V)** (divisor 100/490); `set_vwake` tampoco lo cambia.
  - Ver `transfer_context.md` sección 6 (tabla de divisores reales).
- **`/nava storm [1-720]`** — Hibernación con radio apagada (RTC2), despierta por temporizador y reinicia. Responde "MODO TORMENTA ACTIVADO..." y espera 15s antes de dormir.
- **`/nava storm test1` / `test2`** — Prueba rápida: 60s / 120s.
- **`/nava txoff`** — Apaga TX tras 3s (mantiene RX). **AVISO**: persiste; rollback SOLO con `nrf erase`.
- **`/nava txon`** — Reactiva TX.
- **`/nava ble [on/off]`** — Apaga/enciende Bluetooth; programa reinicio real (se lee al boot). **AVISO**: persiste; rollback SOLO con `nrf erase`.

## 📡 7. Transmisión de Datos (SOLO DM PRIVADO CIFRADO)

- **`/nava msg "[TEXTO]"`** — Difunde mensaje en Canal 0 firmado por el repetidor. Valida texto vacío y responde si no hay memoria.
- **`/nava pos`** — Fuerza emisión de posición.
- **`/nava nodeinfo`** — Baliza NodeInfo sin pedir respuesta.
- **`/nava sendtel`** — Telemetrías ambientales inmediatas.
- **`/nava power`** — Métricas de energía: ADC interno (mV) + sensor de potencia I2C (INA219/260) con **V, ±mA y CARGANDO/DESCARGANDO** y potencia en mW.

## 💤 8. Avisos de Sueño/Despertar (v4.3 V2)

El nodo puede anunciar **por el canal Navadmin (slot 1)** su ciclo de batería (ON por defecto;
NO afecta al comportamiento energético, solo a los avisos). **Verificado en banco 15/08/2026**
(ciclo completo: [Sueño] 3375 mV → dormido → LPCOMP 3710 mV → [Listo] 3772 mV):

- **`/nava sleepmsg [on|off]`** — Activa/desactiva los avisos. **Sin argumento**: estado actual. Persiste en `/resilience.bin` (sobrevive a factory reset). Reparado en V2.3 (el gate nunca se activaba por comando).
- **`[Sueño]`** — Antes de dormir por batería baja: nombre, id, ADC mV (+ INA si presente) y tensión de despertar por LPCOMP.
- **`[Vivo]`** — Despertado por reset externo (p. ej. ATtiny13A) con batería en rango seguro (**entre el corte OCV y el umbral LPCOMP**): aviso "sigo vivo, esperando recuperar carga" y re-sueño tras el envío.
- **`[Listo]`** — Despertar real por LPCOMP (solar) con V ≥ umbral LPCOMP: "despierto, cargando, listo para trabajar".
- Regla de silencio: si la batería está por debajo del corte OCV (3400/3500 mV según env) NO se envía nada — re-sueño directo (protección anti-brownout).
- **Para recibirlos**: el observador/mando debe tener materializado el canal Navadmin (PSK pública `{0x01}`, slot 1) y estar en la misma frecuencia/parametros LoRa.

## 🔔 9. Utilidades (SOLO DM PRIVADO CIFRADO)

- **`/nava bell`** — Alarma acústica para localización.
- **`/nava admin_ls`** — Muestra las 3 claves criptográficas de admin en **base64** (para verificar contra lo configurado).

---

## 🎯 Sintaxis de Direccionamiento de Lote (Prefijos)

1. **Por ID**: `/nava !a7c43b2f ping` (solo responde ese nodo).
2. **Por Rol**: `/nava @router status` (solo Routers).
3. **Por Nombre**: `/nava @name:Navarra env` (solo nombres que empiecen por "Navarra").
4. **A Todos**: `/nava env` (todos los repetidores al alcance, secuencial).

---

## ⚠️ Notas de Despliegue (v4.2.1

- **Ayuda y consulta (v4.3)**: cualquier comando de configuración responde información al llamarse sin argumentos: `/nava set_chem` muestra la química actual, la tabla de cortes/despertar y (si aplica) qué no está disponible; `/nava set_vbat`, `set_vwake`, `set_txpower`, `set_hops`, `set_role`, `set_mqtt`, `set_tz`, `set_name` y `ble` muestran el valor actual. También: `/nava <comando> ?` o `/nava <comando> help` (no válido para `msg`: `/nava msg help` sigue difundiendo el texto). Los comandos que persisten configurán (`set_chem`, `set_vbat`, `set_vwake`, `txoff`, `ble`) avisan: un fallo de configuración solo se revierte con `nrf erase`.)
- **Fragmentación (V2)**: las respuestas multilínea (`help`, `status`, `env`, `fav ls`…) se cortan en saltos de línea (máx. 190 chars/fragmento) — ya no se parte una línea de índice entre dos mensajes.

- La PSK del canal Navadmin es la pública de Meshtastic: cualquiera puede escuchar. Por eso el canal solo admite lectura y NUNCA responde a no-admins.
- El canal Navadmin se identifica por slot (índice 1), no por nombre: no reordenar canales.
- **Despliegue (nodos nuevos o reflasheados)**: el flasheo conserva los `/prefs`; un nodo nuevo de fábrica (o con firmware sin canal Navadmin) necesita **un factory reset tras flashear** para materializar el canal 1. Sin él, los avisos [Sueño]/[Vivo]/[Listo] y los comandos de consulta por canal abierto no llegarán. (El factory reset borra `debug_log_api_enabled` y las claves admin añadidas por app — re-aplicarlas si hacían falta.)
- Las respuestas largas se fragmentan a 190 caracteres con retardo entre fragmentos (MTU LoRa SFNarrow).
- `/resilience.bin` en la raíz del disco sobrevive a los resets de fábrica (solo se borra `/prefs`).
- **Rotación de clave del mando (fix 2026-08-10)**: si un mando aparece con una clave pública distinta a la que el repetidor guarda en su DB, el repetidor acepta el cambio SIEMPRE que la nueva clave coincida con una clave de admin configurada, y lo re-marca como favorito. Esto permite re-acreditar a un mando que se registró con una clave no autorizada (p.ej. tras `db_clear` o `ign rm`): basta con que el mando reenvíe su NodeInfo con la clave correcta; el siguiente DM PKI `/nava` ya se descifra y valida. Si la clave nueva NO es admin, el NodeInfo se descarta como antes.
- **Acreditación admin persistente (15/08)**: la acreditación (bitfield criptográfico + favorito) se guarda en disco en el momento de validar el PKI → el admin responde tras reboot sin esperar su próximo NodeInfo. Un `nrf erase` SIEMPRE regenera las claves del nodo: los peers con la clave vieja fallarán el DM PKI (`PKI_SEND_FAIL_PUBLIC_KEY`) — limpiar sus entradas (`--remove-node` en el peer) y reaprender.
- **`/resilience.bin` v2 (15/08)**: fichero de 84 B con versión interna; los ficheros antiguos (80 B) o corruptos se migran solos al arrancar (defaults: `sleepMsgs=1`, rol sin fijar → rol del perfil). Un fichero corrupto ya no puede dejar el nodo en CLIENT con avisos OFF.
- **Pruebas en banco**: con fuente de laboratorio, el E22P del Promicro es inestable en TX a potencias altas (picos de corriente) — usar **TX 1 dBm** para pruebas; en campo se usa la potencia del env (12 dBm E22P / 22 dBm HT-RA62). El USB conectado desactiva la detección de batería baja (`getHasUSB`) — para probar el ciclo de sueño, alimentar SOLO por fuente.

---

## 💻 Código Fuente de Referencia (para auditoría)

> El código canónico desplegado y compilado vive en el **repo único** (`src/modules/NavaCLIModule.h/.cpp`); los 12 envs compilan este mismo módulo (diferencia solo `set_txpower` 0-12/0-22 por `NAVARICO_RADIO_*` y el rol semi-permanente por `NAVARICO_RAMA_1`). Este bloque es la referencia de auditoría; si difiere del repo, **el repo es la fuente de verdad**.

### Estructura clave del módulo (v4.2.1)

- **Clase**: `NavaCLIModule` hereda de `SinglePortModule` + `concurrency::OSThread`.
- **Métodos**:
  - `wantPacket()` — acepta `/nava` si DM hacia nosotros o canal 1; registra rxLog; olfatea telemetría local.
  - `handleReceived()` — autenticación: DM exige PKI; nodo no en DB -> responde una vez `NODO NO REGISTRADO EN NODEDB`; nodo conocido no-admin -> una vez `NO AUTORIZADO COMO ADMINISTRADOR` (rate-limit con `std::set<NodeNum> unauthorizedReplied`); canal 1 solo admins.
  - `executeCommand()` — normaliza a minúsculas ANTES del filtro de canal; whitelist canal 1; despacha comandos (con guards de longitud en `substr()`).
  - `helpForCommand(topic)` — ayuda por comando en español.
  - `runOnce()` — drena `responseQueue` (fragmentos de 190 chars, retardo 12s entre fragmentos), envía [Vivo]/[Listo] en el primer tick, ejecuta `txoff`/`reboot`/`factory_reset`/`storm` diferidos.
- **Flags internos**: `rebootScheduled`, `factoryResetPending`, `stormPending`/`stormSeconds`, `txOffScheduled`.
- **Persistencia**: `ResiliencePrefs` en `/resilience.bin` (química, vbat, vwake, tx, ble, auto-fav, `sleepMsgs`, rol —ambas ramas—, marcador `version` 84 B). **F15**: se recrea el fichero antes de escribir (el `FILE_O_WRITE` de InternalFS no trunca) y se migran ficheros antiguos/corruptos.
- **Avisos por canal**: los mensajes [Sueño]/[Vivo]/[Listo] se encolan SIEMPRE con `NODENUM_BROADCAST` (nunca `to=0`: no es broadcast y nadie lo entrega — fix Frente A 15/08).

### Dependencias externas del módulo
- `extern float lastRxFrequencyError;` (RadioLibInterface).
- `extern uint32_t rawResetReason;` (main-nrf52).
- `extern void timedSystemSleepSeconds(uint32_t);` (main-nrf52, storm RTC2).
- `extern void setBleForceDisabled(bool);` (main-nrf52).
- `router->getInterface()` (Router.h), `environmentTelemetryModule->sendTelemetry()` (EnvironmentTelemetry public).
