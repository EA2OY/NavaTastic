---
title: "Manual de Administracion Remota /nava"
subtitle: "NavaTastic Eclipse V3 - comandos completos de gestion remota del nodo"
author: "NavaTastic - EA2OY"
date: "Agosto 2026"
colorlinks: true
toc: true
toc-title: "Indice"
---

# Manual de Administración Remota NavaTastic — NavaTastic Eclipse V3

> **ADENDA 16/08/2026 — "NavaTastic Eclipse V3" (4.3.2)**: este manual describe la versión
> actual. El nodo muestra su generación en `/nava status` y en el aviso [Boot] (`NAVA V3`).
> **Para saber qué comandos admite un nodo concreto (según la versión que lleve cargada),
> basta mandarle `/nava help`** — la lista se adapta a su release.

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
- **`/nava status`** — Salud de memoria: etiqueta de build NavaTastic (`NAVA V3 | fw 2.7.26…`), nodos RAM/80, favoritos **Manual/Auto reales** (persisten tras reinicio), huérfanos, estado Auto-Fav, tiempo activo y línea de energía (ADC + INA si presente).
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
- **`/nava factory_reset`** — Reset de fábrica de emergencia restaurando canales de rescate (el ACK sale antes de ejecutarse). **Regenera el par PKI** (los peers fallan DM hasta re-aprender; L11). **F20**: las claves admin del usuario se conservan en `/resilience.bin` y vuelven tras el reset.
- **`/nava full_reset`** — Reset completo: configuración + semi-persistentes (`/resilience.bin`) a defaults **conservando el par PKI, los bonds BLE y las claves admin del usuario (F20)** → revert remoto sin PC y sin romper la malla (el ACK sale antes de ejecutarse). **Verificado en banco 15/08 (7/7)**: slot 0 vuelve con la clave propia del usuario si había desautorizado la de fábrica; rol/avisos/semi-persistentes vuelven a defaults del perfil.
- **`/nava wipe`** — Purga de compromiso: erase total + **par PKI NUEVO** + bonds BLE + **claves admin persistidas borradas** (queda solo la del proyecto — rescate garantizado) (equivalente remoto del `nrf erase`). **AVISO**: los peers fallarán el DM PKI hasta re-aprender la clave nueva (quitar la entrada stale con `--remove-node` y forzar NodeInfo del nodo). El NodeNum NO cambia (deriva de la MAC). Escalera de resets: `factory_reset` → `full_reset` → `wipe`.

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
NO afecta al comportamiento energético, solo a los avisos). Contenido: `ADC X mV | CPU X.X C`
(solo sensores internos del nRF52 — los sensores I2C no están disponibles en estos momentos).
**Verificado en banco 15/08/2026** (ciclo completo: [Vivo] → operación ~100s → [Sueño] → dormido
~1 mA → LPCOMP ~3.7-3.8V → [Listo] → [Boot] a los 2 min). **V3**: el monitor runtime usa **8
lecturas (~160s)** para todas las placas.

- **`/nava sleepmsg [on|off]`** — Activa/desactiva los avisos. **Sin argumento**: estado actual. Persiste en `/resilience.bin` (sobrevive a factory reset). Reparado en V2.3 (el gate nunca se activaba por comando).
- **`[Sueño]`** — Antes de dormir por batería baja: nombre, id, ADC + temperatura del chip y tensión de despertar por LPCOMP. Se dispara tras **8 lecturas bajas del monitor (~160s)** — el filtro evita dormirse por lecturas ADC puntuales erróneas (RF/temperatura). Después duerme **TODO** (radio, GPS, pantalla, LED) → ~1 mA.
- **`[Vivo]`** — Despertado por reset externo (p. ej. ATtiny13A) con batería en la **banda [corte−100 mV, corte)** (E22P: 3400-3500; SX1262: 3300-3400): aviso "sigo vivo, al límite de carga" y el nodo **sigue operando con normalidad** — el monitor decidirá dormir tras sus 8 lecturas si la baja persiste.
- **`[Listo]`** — Despertar con **V ≥ corte OCV** (por LPCOMP solar o reset externo): "despierto, cargando, listo para trabajar" — el nodo sigue operando con normalidad.
- **`[Boot]` (V2.4)** — Aviso de arranque **diferido 2 minutos** (anti-bucle: un nodo en ciclo de reinicios nunca llega a enviarlo). Solo en arranques que NO vienen del ciclo de sueño: power-on, reset externo, **watchdog**, brownout, flasheo, `/nava reboot`. Incluye la **causa del reset** (registro RESETREAS: `WDT` = watchdog del firmware, `RESETPIN` = ATtiny/botón, `SOFT` = reboot/storm/flash, `LOCKUP`, `LPCOMP`, `VBUS`) y la **etiqueta de build** (`NAVA V3`). Todos gated por `sleepmsg`.
- Regla de silencio: si la batería está por debajo de corte−100 mV NO se envía nada — re-sueño directo (protección anti-brownout).
- **Para recibirlos**: el observador/mando debe tener materializado el canal Navadmin (PSK pública `{0x01}`, slot 1) y estar en la misma frecuencia/parametros LoRa.

## 🔔 9. Utilidades (SOLO DM PRIVADO CIFRADO)

- **`/nava bell`** — Alarma acústica para localización.
- **`/nava admin_ls`** — Muestra las 3 claves criptográficas de admin en **base64** (para verificar contra lo configurado).
- **`/nava keys_ls`** — Muestra las claves admin **persistidas** en `/resilience.bin` (base64, mismo formato que `admin_ls`): son las que volverán tras un factory/full reset.
- **`/nava keys_clear`** — Borra SOLO la copia **persistida** de las claves admin (NO toca la configuración actual ni reinicia; el ACK sale antes de ejecutarse). Tras un `keys_clear`, el próximo factory/full reset volverá a dejar solo la clave del proyecto.
  - ⚠️ **Regla (F20, merge)**: quitar una clave en la app NO la purga del nodo — la copia persistida se mantiene y reaparecerá tras el próximo reset. Para purgar de verdad una clave: `keys_clear` (borra todas las persistidas), `wipe` (purga total) o `nrf erase`.

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
- **Despliegue (nodos nuevos o reflasheados)**: el flasheo conserva los `/prefs`; un nodo nuevo de fábrica (o con firmware sin canal Navadmin) necesita **un factory reset tras flashear** para materializar el canal 1. Sin él, los avisos [Sueño]/[Vivo]/[Listo] y los comandos de consulta por canal abierto no llegarán. (El factory reset borra `debug_log_api_enabled` — re-activarlo si hacía falta. Las **claves admin del usuario sobreviven a los resets (F20)**: quedan en `/resilience.bin` y se re-aplican al arrancar.)
- Las respuestas largas se fragmentan a 190 caracteres con retardo entre fragmentos (MTU LoRa SFNarrow).
- `/resilience.bin` en la raíz del disco sobrevive a los resets de fábrica (solo se borra `/prefs`).
- **Rotación de clave del mando (fix 2026-08-10)**: si un mando aparece con una clave pública distinta a la que el repetidor guarda en su DB, el repetidor acepta el cambio SIEMPRE que la nueva clave coincida con una clave de admin configurada, y lo re-marca como favorito. Esto permite re-acreditar a un mando que se registró con una clave no autorizada (p.ej. tras `db_clear` o `ign rm`): basta con que el mando reenvíe su NodeInfo con la clave correcta; el siguiente DM PKI `/nava` ya se descifra y valida. Si la clave nueva NO es admin, el NodeInfo se descarta como antes.
- **Acreditación admin persistente (15/08)**: la acreditación (bitfield criptográfico + favorito) se guarda en disco en el momento de validar el PKI → el admin responde tras reboot sin esperar su próximo NodeInfo. Un `nrf erase` SIEMPRE regenera las claves del nodo: los peers con la clave vieja fallarán el DM PKI (`PKI_SEND_FAIL_PUBLIC_KEY`) — limpiar sus entradas (`--remove-node` en el peer) y reaprender.
- **`/resilience.bin` v2 (15/08)**: fichero de 84 B con versión interna; los ficheros antiguos (80 B) o corruptos se migran solos al arrancar (defaults: `sleepMsgs=1`, rol sin fijar → rol del perfil). Un fichero corrupto ya no puede dejar el nodo en CLIENT con avisos OFF.
- **`/resilience.bin` v3 + claves admin (15/08, F20)**: fichero de 180 B (marcador "NAV3") que además guarda las claves admin PÚBLICAS del usuario para que sobrevivan a los resets de fábrica. **Regla de slot 0**: "slot 0 = estado previo del usuario" — si el dueño puso su clave en slot 0 (desautorizando la de fábrica), tras un reset vuelve SU clave (sin ventana de secuestro); si nunca la desautorizó, slot 0 = clave del proyecto. Las claves del proyecto NUNCA se persisten como de usuario (dedupe). Tras `wipe`/`nrf erase` (fichero sin estado previo) queda solo la del proyecto: canal de rescate garantizado.
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
  - `runOnce()` — drena `responseQueue` (fragmentos de 190 chars, retardo 12s entre fragmentos), envía [Vivo]/[Listo] en el primer tick, ejecuta `txoff`/`reboot`/`factory_reset`/`full_reset`/`wipe`/`storm` diferidos.
- **Flags internos**: `rebootScheduled`, `factoryResetPending`, `fullResetPending`, `wipePending`, `stormPending`/`stormSeconds`, `txOffScheduled`.
- **Persistencia**: `ResiliencePrefs` en `/resilience.bin` (química, vbat, vwake, tx, ble, auto-fav,
  `sleepMsgs`, rol —ambas ramas—, **claves admin del usuario (F20)**, marcador `version` 180 B "NAV3"). **F15**: se recrea el fichero antes de escribir
  (el `FILE_O_WRITE` de InternalFS no trunca) y se migran ficheros antiguos/corruptos.
- **Avisos por canal**: los mensajes [Sueño]/[Vivo]/[Listo] se encolan SIEMPRE con `NODENUM_BROADCAST` (nunca `to=0`: no es broadcast y nadie lo entrega — fix Frente A 15/08).

### Dependencias externas del módulo
- `extern float lastRxFrequencyError;` (RadioLibInterface).
- `extern uint32_t rawResetReason;` (main-nrf52).
- `extern void timedSystemSleepSeconds(uint32_t);` (main-nrf52, storm RTC2).
- `extern void setBleForceDisabled(bool);` (main-nrf52).
- `router->getInterface()` (Router.h), `environmentTelemetryModule->sendTelemetry()` (EnvironmentTelemetry public).

---

# NavaTastic Remote Administration Manual — Eclipse V3 (English)

> **ADDENDUM 16/08/2026 — "NavaTastic Eclipse V3" (4.3.2)**: this manual describes the current
> version. The node shows its generation in `/nava status` and in the [Boot] notice (`NAVA V3`).
> **To know which commands a given node supports (depending on the version it runs), just send
> it `/nava help`** — the list adapts to its release.

> English translation of the Spanish manual above. **The Spanish original is the authoritative
> version**; this section carries the same information for international readers. Canonical code:
> `src/modules/NavaCLIModule.h/.cpp` in the unified repo (all 12 envs compile this same module;
> only `set_txpower` differs, 0-12/0-22 by `NAVARICO_RADIO_*`).

## Command security levels

- **Open channel (Navadmin)**: read-only query/diagnostic commands only. Batch execution over the
  fleet with anti-collision response jitter; non-accredited senders get **no reply at all**
  (total silence). **Rate-limit: max 1 command per node every 30 s** (excess ignored silently) to
  avoid battery/airtime exhaustion through the public channel.
- **Encrypted DM only**: critical configuration, reboot, database, blocklist, favorites and energy
  commands. Mandatory PKI signature. A known node sending a DM without being accredited gets a
  single reply: `NO AUTORIZADO COMO ADMINISTRADOR`.

## 1. Diagnostics & telemetry (open channel and DM)

- **`/nava ping`** — Latency reply with uptime and noise floor. Rate-limit: 1 reply every 10 s per node.
- **`/nava status`** — Memory health: NavaTastic build tag (`NAVA V3 | fw 2.7.26…`), nodes RAM/80, **real Manual/Auto favorites** (persist across
  reboots), orphans, auto-fav state, uptime and energy line (ADC + INA when present).
- **`/nava env`** — Battery, heap, nRF52 CPU temperature and I2C environmental sensor.
- **`/nava channel`** — Spectrum usage (airtime % and TX %).
- **`/nava peers`** — Direct 0-hop neighbors (ID, role, SNR, last-heard).
- **`/nava rxlog`** — Metadata of the last 5 received packets.
- **`/nava afc`** — TCXO frequency drift in Hz of the last packet.
- **`/nava reset_reason`** — Last reset cause (RESETREAS register).
- **`/nava noise`** — Instantaneous LoRa chip noise floor in dBm.
- **`/nava bat`** — Active chemistry, voltage mV, OCV % and TX state.
- **`/nava route !ID`** — Hops and SNR toward a node; launches a TraceRoute automatically if unknown.
- **`/nava trace !ID`** — Native TraceRoute toward the node.
- **`/nava help`** — Short command glossary. **`/nava help <command>`** / **`<command> ?`** /
  **`<command> help`** — brief help for ANY command (except `msg`).

## 2. Blocklist management (ENCRYPTED DM ONLY)

- **`/nava ign ls`** — Lists blocked nodes.
- **`/nava ign add !ID`** — Blocks and silences a node, purging its public key. Safeguards:
  `ERR: NO SE PUEDE IGNORAR A UN ADMIN`, `ERR: NO PUEDES IGNORARTE A TI MISMO`,
  `ERR: NODO DESCONOCIDO, NO SE PUEDE VERIFICAR ADMIN`.
- **`/nava ign rm !ID`** — Unblocks (DM, to prevent the blocked node from unblocking itself).

## 3. Favorites management (ENCRYPTED DM ONLY)

- **`/nava fav add !ID`** — Adds to favorites with hop bypass; creates a RAM orphan slot if never
  heard (max 10).
- **`/nava fav rm !ID`** — Removes from favorites.
- **`/nava fav ls`** — Lists favorites tagged `[AUTO]` / `[MAN]`.
- **`/nava fav auto [on|off]`** — Enables/disables auto-favoriting of direct (0-hop) routers.
  Default ON; persists in `/resilience.bin` (survives factory reset). No argument shows state.

## 4. Hot node configuration (ENCRYPTED DM ONLY)

- **`/nava set_name "[Long]" "[Short]"`** — Renames the node (quotes supported).
- **`/nava set_role [client/mute/router]`** — Hardware role. **SEMI-PERMANENT on BOTH branches
  (V2.1)**: saved in `/resilience.bin`, survives factory reset; only `nrf erase` restores the env
  profile role.
- **`/nava set_mqtt [on/off]`** · **`/nava set_tz [POSIX]`** · **`/nava set_hops [1-7]`**
- **`/nava set_txpower [0-12]`** — TX power (Promicro/E22P). **Faketec HT-RA62: [0-22]**.

## 5. Maintenance & reboot (ENCRYPTED DM ONLY)

- **`/nava db_purge`** — Evicts non-favorite, non-admin nodes.
- **`/nava db_clear`** — Empties the node database (nuclear). **Important**: it also removes YOUR
  entry from the repeater; re-accredit by **forcing your own NodeInfo broadcast (broadcast, NOT
  DM)** before sending any `/nava` — otherwise the repeater stays silent until your next periodic
  NodeInfo (up to 72 h).
- **`/nava reboot`** — Deferred reboot at 3 s (ACK goes out first).
- **`/nava factory_reset`** — Emergency factory reset restoring rescue channels (ACK first). **Regenerates the PKI keypair** (peers fail DM until they re-learn; L11). **F20**: the user's admin keys survive in `/resilience.bin` and come back after the reset.
- **`/nava full_reset`** — Full reset: config + semi-persistent (`/resilience.bin`) to defaults **keeping the PKI keypair, BLE bonds and the user's admin keys (F20)** → remote revert without a PC and without breaking the mesh (ACK first). **Bench-verified 15/08 (7/7)**: slot 0 returns the user's own key if they had deauthorized the factory one; role/notices/semi-persistent values return to profile defaults.
- **`/nava wipe`** — Compromise purge: total erase + **NEW PKI keypair** + BLE bonds + **persisted admin keys deleted** (only the project key remains — guaranteed rescue) (remote equivalent of `nrf erase`). **WARNING**: peers will fail PKI DM until they re-learn the new key (remove the stale entry with `--remove-node` and force the node's NodeInfo). The NodeNum does NOT change (derived from the MAC). Reset ladder: `factory_reset` → `full_reset` → `wipe`.

## 6. Energy & resilience (ENCRYPTED DM ONLY)

- **`/nava set_chem [lipo/nimh/sodium/lifepo4]`** — Switches chemistry and adjusts cutoff/OCV/LPCOMP.
  Persists in `/resilience.bin`. Cutoffs: lipo 3500, nimh 3400, sodium 2600, **lifepo4 2800**.
  No argument → current chemistry + cutoffs/wake table. **Rollback ONLY via `nrf erase`.**
  - ⚠️ On **Seed Solar P1, Xiao Kit i2c, Xiao E22P and Heltec T114** `lifepo4` is **rejected**
    (`ERR: LIFEPO4 NO COMPATIBLE, UMBRAL LPCOMP FIJO`): their LPCOMP is hardware-fixed and the
    wake threshold (~3.67 V–4.04 V) exceeds the physical maximum of a LiFePO4 cell (~3.65 V) — an
    accepted LiFePO4 would never wake on solar. Only `lipo`, `nimh`, `sodium` there. Promicro and
    Faketec (dynamic LPCOMP) keep all 4 chemistries.
- **`/nava set_vbat [2400-3600]`** — Battery shutdown cutoff in mV. No argument shows current.
  **Rollback ONLY via `nrf erase`.**
- **`/nava set_vwake [1-5]`** — Solar wake LPCOMP level. Real voltages (Promicro/Faketec, 0.5
  divider): 1=2.1 V, 2=2.5 V, **3=3.7 V (LiPo/NiMH/Sodium)**, 4=4.5 V, **5=3.3 V (LiFePO4)**.
  No argument shows current. **Rollback ONLY via `nrf erase`.**
  - ⚠️ Seed/Xiao×2: fixed `3_8` (~3.67 V) — `set_vwake` does not change the wake voltage.
  - ⚠️ Heltec T114: fixed `2_8` (~4.04 V) — `set_vwake` does not change it either.
- **`/nava storm [1-720]`** — Hibernation with radio off (RTC2); wakes by timer and reboots.
  ACKs "MODO TORMENTA ACTIVADO..." and waits 15 s before sleeping. `storm test1`/`test2` = 60 s/120 s.
- **`/nava txoff`** — Disables TX after 3 s (RX stays). Persists; rollback ONLY via `nrf erase`.
  **`/nava txon`** — Re-enables TX.
- **`/nava ble [on/off]`** — Disables/enables Bluetooth (schedules a real reboot). Persists;
  rollback ONLY via `nrf erase`.

## 7. Data transmission (ENCRYPTED DM ONLY)

- **`/nava msg "[TEXT]"`** — Broadcasts on channel 0 signed by the repeater.
- **`/nava pos`** — Forces a position broadcast. **`/nava nodeinfo`** — NodeInfo beacon without
  asking for replies. **`/nava sendtel`** — Immediate environmental telemetry.
- **`/nava power`** — Energy metrics: internal ADC (mV) + I2C power sensor (INA219/260) with
  V, ±mA CHARGING/DISCHARGING and power in mW. **DM only** (SOLO DM SEGURO).

## 8. Sleep/wake notices (v4.3 V2)

The node can announce its battery cycle **on the Navadmin channel (slot 1)** (ON by default;
does NOT affect energy behavior, only the notices). Content: `ADC X mV | CPU X.X C` (internal
nRF52 sensors only — I2C sensors are unavailable at those moments). **Bench-verified 15/08/2026**
(full cycle: [Vivo] → ~100 s operation → [Sueño] → ~1 mA sleep → LPCOMP ~3.7-3.8 V → [Listo] →
[Boot] at 2 min). **V3**: the runtime monitor uses **8 readings (~160 s)** on all boards.

- **`/nava sleepmsg [on|off]`** — Enables/disables notices. Persists in `/resilience.bin`.
  Fixed in V2.3 (the gate never activated by command before).
- **`[Sueño]`** — Before low-battery sleep: name, id, ADC + chip temperature and LPCOMP wake
  voltage. Fires after **8 low monitor readings (~160 s)** — the filter prevents sleeping on
  spurious ADC readings (RF/temperature). Then sleeps EVERYTHING (radio, GPS, screen, LED) → ~1 mA.
- **`[Vivo]`** — Woken by external reset (e.g. ATtiny13A) with battery in the band
  **[cutoff−100 mV, cutoff)** (E22P: 3400-3500; SX1262: 3300-3400): "alive, at the charge limit"
  and the node **keeps operating normally** — the monitor decides after its 8 readings.
- **`[Listo]`** — Wake with **V ≥ OCV cutoff** (solar LPCOMP or external reset): "awake, charging,
  ready" — normal operation continues.
- **`[Boot]` (V2.4)** — Startup notice **delayed 2 minutes** (anti-loop: a node in a reset cycle
  never sends it). Only on boots NOT coming from the sleep cycle: power-on, external reset,
  **watchdog**, brownout, flash, `/nava reboot`. Includes the **reset cause** (RESETREAS: `WDT`,
  `RESETPIN`, `SOFT`, `LOCKUP`, `LPCOMP`, `VBUS`) and the **build tag** (`NAVA V3`). All gated by `sleepmsg`.
- Silence rule: below cutoff−100 mV nothing is sent — direct re-sleep (anti-brownout protection).
- **To receive them**: the observer/control node must have the Navadmin channel materialized
  (public PSK `{0x01}`, slot 1) and use the same frequency/LoRa parameters.

## 9. Utilities (ENCRYPTED DM ONLY)

- **`/nava bell`** — Acoustic alarm for localization.
- **`/nava admin_ls`** — Shows the 3 admin keys in **base64** (to verify against configuration).
- **`/nava keys_ls`** — Shows the **persisted** admin keys in `/resilience.bin` (base64, same format as `admin_ls`): the ones that will return after a factory/full reset.
- **`/nava keys_clear`** — Clears ONLY the **persisted** copy of the admin keys (does NOT touch the current configuration and does not reboot; ACK first). After `keys_clear`, the next factory/full reset will leave only the project key.
  - ⚠️ **Rule (F20, merge)**: removing a key in the app does NOT purge it from the node — the persisted copy remains and will reappear after the next reset. To truly purge a key: `keys_clear` (clears all persisted), `wipe` (total purge) or `nrf erase`.

## Batch addressing syntax (prefixes)

1. **By ID**: `/nava !a7c43b2f ping` (only that node answers).
2. **By role**: `/nava @router status` (Routers only).
3. **By name**: `/nava @name:Navarra env` (names starting with "Navarra").
4. **All**: `/nava env` (every repeater in range, sequential).

## Deployment notes (condensed)

- Any configuration command answers with state when called with no arguments; `?` / `help`
  interrogation works for all except `msg`. Persistent commands (`set_chem`, `set_vbat`,
  `set_vwake`, `txoff`, `ble`) warn: rollback only via `nrf erase`.
- Multi-line replies are fragmented at line breaks (max 190 chars per fragment).
- Navadmin channel uses Meshtastic's **public PSK**: anyone can listen — read-only only, never
  replies to non-admins. It is identified by **slot (index 1)**, not by name: do not reorder
  channels.
- **Deployment (new or reflashed nodes)**: flashing keeps `/prefs`; a factory-new node needs
  **one factory reset after flashing** to materialize channel 1 — without it the [Sueño]/[Vivo]/
  [Listo] notices and open-channel queries will not arrive. (Factory reset also clears
  `debug_log_api_enabled` — re-enable it. The **user's admin keys survive resets (F20)**: they
  live in `/resilience.bin` and are re-applied at boot.)
- `/resilience.bin` survives factory resets (only `/prefs` is removed).
- **`/resilience.bin` v3 + admin keys (15/08, F20)**: 180 B file ("NAV3" marker) that also
  stores the user's PUBLIC admin keys so they survive factory resets. **Slot 0 rule**:
  "slot 0 = the user's previous state" — if the owner put their own key in slot 0
  (deauthorizing the factory key), after a reset THEIR key returns (no hijack window); if they
  never deauthorized it, slot 0 = project key. Project keys are NEVER persisted as user keys
  (dedupe). After `wipe`/`nrf erase` (no previous state) only the project key remains:
  guaranteed rescue channel.
- **Control-key rotation (fix 2026-08-10)**: a control node presenting a new public key is
  accepted whenever the new key matches a configured admin key (and re-favorited).
- **Persistent admin accreditation (15/08)**: accreditation is saved to disk at PKI validation —
  the admin answers after reboot without waiting for its next NodeInfo. An `nrf erase` ALWAYS
  regenerates the node keys: peers holding the old key fail DM PKI (`PKI_SEND_FAIL_PUBLIC_KEY`) —
  remove their entries and re-learn.
- **Bench testing**: the Promicro E22P TX is unstable at high power on a lab supply (current
  spikes) — use **1 dBm TX** for tests; USB disables low-battery detection (`getHasUSB`) — power
  from the supply only to test the sleep cycle.

## Source code reference (for audit)

> The deployed code lives in the unified repo (`src/modules/NavaCLIModule.h/.cpp`); the repo is
> the source of truth. Class `NavaCLIModule` (SinglePortModule + OSThread): `wantPacket()` (DM or
> channel 1), `handleReceived()` (PKI auth, one-time replies to non-admins), `executeCommand()`
> (lowercases before the channel whitelist), `helpForCommand()`, `runOnce()` (drains the fragment
> queue, deferred txoff/reboot/factory_reset/full_reset/wipe/storm). Persistence: `ResiliencePrefs` in
> `/resilience.bin` (180 B "NAV3" with version marker + persisted admin keys; legacy/corrupt files auto-migrate). The
> [Sueño]/[Vivo]/[Listo] notices always enqueue with `NODENUM_BROADCAST` (never `to=0`).
