---
title: "Manual de Administracion Remota /nava"
subtitle: "NavaTastic Eclipse V4 - comandos completos de gestion remota del nodo y flota"
author: "NavaTastic - EA2OY"
date: "Agosto 2026"
colorlinks: true
toc: true
toc-title: "Indice"
---

# Manual de Administración Remota NavaTastic — NavaTastic Eclipse V4

> **ADENDA 17/08/2026 — FRENTE F21/F22 (Gestión de Flota en Lote, Mitigación Anti-Tormentas y Control de Difusión)**:
> - **Consola Privada de Gestión de Flota**: Al redirigir la CLI a un canal secundario privado (`set_cli_chan <2-7>`), el administrador puede lanzar órdenes en lote a toda la red con un solo mensaje (`set_ok_to_mqtt`, `set_pos_tx`, `set_nodeinfo_tx`, `set_telem_tx`, `ign add/del`, `set_beacon`, `set_chem`, `mute`, `test_tx`, `db_purge`).
> - **Blindaje Anti-Tormentas en Canal Público Navadmin**: En broadcast abierto solo responden los 7 comandos ligeros de sondeo (`ping`, `status`, `bat`, `power`, `env`, `channel`, `noise`) en 1 línea con jitter escalonado. `/nava help` general y comandos no autorizados se silencian en broadcast para evitar saturaciones de radio.
> - **Gestión de Canales y Difusión**: Nuevos comandos `set_pos_tx`, `set_nodeinfo_tx`, `set_telem_tx` y `pos_clear` para control exhaustivo del tráfico LoRa.
> - **Lista Negra Persistente**: `ign add/del/ls/clear` ahora se respalda en `/resilience.bin` V5 (NAV5) y descarta paquetes a nivel router.
> - **Generación NAVA V4**: Visible en `/nava status` y en el aviso `[Boot]`.

> **ADENDA 16/08/2026 — "NavaTastic Eclipse V3/V4" (4.3.3)**: el nodo muestra su generación en `/nava status` y en el aviso [Boot] (`NAVA V4`).
> **ADENDA 14/08/2026 — REPO UNIFICADO (V2)**: manual **VIGENTE**. Ronda V2: nuevos comandos/mensajes `sleepmsg` y avisos [Sueño]/[Vivo]/[Listo], `status` con favoritos Auto/Manual reales, `power` con ADC + INA cargando/descargando, `fav ls` etiquetado, fragmentación por línea.

Documento **3 de 3** del proyecto Navarrico. Manual de operación de los comandos `/nava` (módulo `NavaCLIModule`) y código fuente de referencia para auditoría con otro agente.

---

## 🛡️ Nivel de Seguridad de los Comandos

- **Canal Abierto Público (Navadmin / Slot 1)**:
  - **Broadcast Masivo (sin `!ID`)**: Únicamente los 7 comandos ligeros de sondeo de 1 línea (`ping`, `status`, `bat`, `power`, `env`, `channel`, `noise`). Cero emisión de menús pesados o errores masivos.
  - **Broadcast Dirigido (con `!ID` o `@grupo`)**: Permite además diagnósticos individuales (`stats`, `log`, `ch_ls`, `help`, `peers`, `rxlog`, `afc`, `reset_reason`, `route`, `trace`).
- **Canal Privado de Flota (Slots 2..7 con CLI redirigida)**: Permite órdenes en lote de gestión de red a toda la flota simultáneamente (`set_ok_to_mqtt`, `set_pos_tx`, `set_nodeinfo_tx`, `set_telem_tx`, `ign`, `set_beacon`, `set_chem`, `mute`, `test_tx`, `db_purge`, `nodeinfo`, `pos`, `sendtel`). Comandos individuales geográficos (`set_pos`, `set_name`, `set_pin`) y destructivos nucleares (`wipe`, `factory_reset`, `reboot`) exigen `!ID` o DM.
- **Solo por DM Privado Cifrado (PKI Curve25519)**: Acceso al 100% de la funcionalidad, configuración de canales (`ch_*`), claves admin y comandos destructivos. Exige firma criptográfica de `admin_key[0..2]`.

---

## 📊 1. Diagnóstico y Telemetría (Permitidos en Canal Abierto y DM)

- **`/nava ping`** — Respuesta de latencia con uptime y piso de ruido. Ej: `PONG: RN1 | SNR: 3.5 dB | Bat: 4120 mV | UP: 32d 4h | RUIDO: -120 dBm`. Rate-limit: 1 respuesta cada 10s por nodo.
- **`/nava status`** — Salud de memoria: etiqueta de build NavaTastic (`NAVA V4 | fw 2.7.26…`), nodos RAM/80, favoritos **Manual/Auto reales** (persisten tras reinicio), huérfanos, estado Auto-Fav, tiempo activo y línea de energía (ADC + INA si presente).
- **`/nava power`** — **[F22 en abierto]** Métricas de energía: ADC interno (mV) + sensor de potencia I2C (INA219: V, ±mA, CARGANDO/DESCARGANDO, mW).
- **`/nava env`** — Batería, heap, temperatura CPU nRF52 y sensor ambiental I2C.
- **`/nava channel`** — Uso de espectro (airtime % y TX %).
- **`/nava peers`** — Vecinos directos a 0 saltos (ID, rol, SNR, tiempo desde último contacto).
- **`/nava rxlog`** — Metadatos de los últimos 5 paquetes recibidos (ID, PortNum, SNR, RSSI).
- **`/nava afc`** — Deriva de frecuencia del TCXO en Hz del último paquete.
- **`/nava reset_reason`** — Motivo del último reinicio (registro RESETREAS).
- **`/nava noise`** — Piso de ruido instantáneo del chip LoRa en dBm.
- **`/nava bat`** — Química activa, voltaje mV, % OCV y estado TX.
- **`/nava stats`** — **[F21, 100% RAM]** Informe forense de extremos y tráfico: temperaturas CPU mín/máx/act, voltaje mín/act, paquetes RX/TX/Enrutados y conteo Auto-Fav. (Requiere `!ID` o DM).
- **`/nava log [lineas]`** — **[F21, 100% RAM]** Muestra las últimas 1-15 entradas del buffer circular de eventos en memoria (boot, cortes, transiciones de ciclo solar y comandos ejecutados). (Requiere `!ID` o DM).
- **`/nava route !ID`** — Saltos y SNR con que escucha al nodo. Si no está en la BD, lanza un TraceRoute automáticamente.
- **`/nava trace !ID`** — Lanza un TraceRoute nativo hacia el nodo.
- **`/nava help`** — Glosario corto de comandos. (En DM o con `!ID`).
- **`/nava help <comando>`** también: **`/nava <comando> ?`** / **`/nava <comando> help`** interroga a CUALQUIER comando (excepto `msg`) — Ayuda breve de un comando concreto. Ej: `/nava help fav`, `/nava help set_pos_tx`.

---

## 📻 2. Gestión de Canales Secundarios y Redirección (SOLO DM PRIVADO CIFRADO)

- **`/nava ch_ls`** — Lista los 8 slots de canales (0-7), indicando rol (`PRI`, `SEC`, `DIS`), marca de canal CLI activo (`*`), nombre, tipo de clave (`AES128`, `AES256`, `#1`, `DEF_PRI`) y compuerta MQTT (`U/D`, `U`, `D`, `-`).
- **`/nava ch_set <slot 2-7> <nombre> <psk_base64>`** — Configura y activa un canal secundario privado. La clave se decodifica en Base64 (1 byte índice, 16 bytes AES-128 o 32 bytes AES-256). Se guarda en `/prefs` y se respalda en `/resilience.bin` para sobrevivir a resets de configuración.
  - *Ejemplo AES-128*: `/nava ch_set 2 Privada 1b4D8...==`
  - *Ejemplo por defecto*: `/nava ch_set 3 Malla AQ==`
- **`/nava ch_del <slot 2-7>`** — Deshabilita el canal del slot indicado y limpia su respaldo. Si el slot albergaba la escucha de NavaCLI, se redirige automáticamente al Slot 1.
- **`/nava ch_url [slot 0-7]`** — Genera la URL canónica de Meshtastic (`https://meshtastic.org/e/#...`) para importar el canal directamente mediante código QR o enlace en la aplicación móvil.
- **`/nava set_cli_chan <slot 1-7>`** — Redirige la escucha de comandos `/nava` y la emisión de los avisos solares (`[Listo]`, `[Vivo]`, `[Critico]`, `[Sueño]`, `[Boot]`) al slot seleccionado (1-7).
- **`/nava navadmin_mute [on|off]`** — Silencia o reactiva el canal público de rescate (Slot 1 Navadmin). Permite operar de forma 100% invisible en frecuencias compartidas.
- **`/nava ch_reset`** — Restaura la tabla de canales al estado de fábrica (Slot 0 SFNarrow, Slot 1 Navadmin, Slots 2-7 deshabilitados, CLI en Slot 1 y Mute OFF).

---

## 🌐 3. Pasarelas MQTT y Gestión de Infraestructura (DM o Canal Privado de Flota)

- **`/nava ch_mqtt <slot 0-7> [up|down|both|off]`** — Configura individualmente el reenvío MQTT para el canal indicado (subida `up`, bajada `down`, bidireccional `both` o apagado `off`).
- **`/nava set_ok_to_mqtt [on|off]`** — Activa o desactiva la bandera global `config_ok_to_mqtt` en los paquetes del nodo, autorizando a pasarelas ajenas a subirlos a servidores MQTT públicos o privados.
- **`/nava set_pos <lat> <lon> [alt]`** — Fija coordenadas geográficas estáticas en el nodo (sin necesidad de módulo GPS físico). Persiste a resets de fábrica. (Individual con `!ID` o DM).
- **`/nava pos_clear`** — Borra las coordenadas fijas guardadas, dejando el repetidor sin posición.
- **`/nava set_pos_tx [on|off|minutos]`** — Controla la difusión periódica espontánea de posición de flota (por defecto 72h). Con `off` se apaga por completo para ahorro de airtime y privacidad.
- **`/nava set_nodeinfo_tx [on|off|minutos]`** — Controla la difusión periódica de NodeInfo/nombres en la flota (por defecto 72h).
- **`/nava set_telem_tx [on|off|minutos]`** — Regula el intervalo de reporte de telemetría de batería y sensores en la red (1-1440 min).
- **`/nava set_beacon [minutos]`** — Configura el intervalo de emisión periódica de las balizas `NodeInfo` y `Position` (1 a 1440 minutos).
- **`/nava mute [minutos|off]`** — **[100% RAM]** Activa el modo silencioso temporal en el repetidor. Cancela el reenvío LoRa de paquetes de terceros durante el tiempo indicado para realizar auditorías limpias de espectro sin tráfico parásito.
- **`/nava set_pin <6_digitos>`** — Configura un PIN fijo personalizado de 6 dígitos para el emparejamiento Bluetooth BLE. Persiste en `/resilience.bin`. (Individual con `!ID` o DM).
- **`/nava test_tx [segundos 5-30]`** — **[100% RAM]** Emite una ráfaga periódica de balizas de prueba a razón de 1 paquete/segundo para medir niveles de cobertura y SNR en campo.

---

## 🚫 4. Gestión de Bloqueos y Lista Negra Persistente (DM o Canal Privado de Flota)

- **`/nava ign ls`** — Lista los nodos bloqueados persistentes en `/resilience.bin`.
- **`/nava ign add !ID`** — Bloquea y silencia al nodo, descartando sus paquetes en el router y persistiendo en disco (hasta 8 nodos).
- **`/nava ign rm !ID`** (o `ign del !ID`) — Desbloquea al nodo de la lista negra.
- **`/nava ign clear`** — Borra la lista negra por completo.

---

## ⭐️ 5. Gestión de Favoritos (SOLO DM PRIVADO CIFRADO)

- **`/nava fav add !ID`** — Añade a favoritos con bypass de saltos; crea slot huérfano en RAM si no se ha oído (máx. 10).
- **`/nava fav rm !ID`** — Elimina de favoritos.
- **`/nava fav ls`** — Lista los favoritos, etiquetados `[AUTO]` (auto-favoriteo) o `[MAN]` (manuales).
- **`/nava fav auto [on|off]`** — Activa/desactiva el auto-favoriteo de routers directos (0 saltos). Por defecto: ACTIVADO. Con OFF no se marcan nuevos auto-favoritos ni se registran routers para bypass de saltos; los favoritos ya existentes se conservan. `/nava fav auto` sin argumento muestra el estado. Persiste en `/resilience.bin` (sobrevive a factory reset). El listado Auto/Manual persistido se reconstruye tras reinicio.

---

## ⚙️ 6. Configuración del Nodo en Caliente (SOLO DM PRIVADO CIFRADO)

- **`/nava set_name "[Largo]" "[Corto]"`** — Cambia el nombre (soporta comillas).
- **`/nava set_role [client/mute/router]`** — Cambia el rol de hardware. **SEMI-PERMANENTE en AMBAS ramas (V2.1)**: se guarda en `/resilience.bin` y sobrevive al factory reset (un cliente convertido en router sigue siéndolo tras un rescate; se revierte con `set_role client`). Solo un `nrf erase` restaura el rol del perfil del env.
- **`/nava set_mqtt [on/off]`** — Activa/desactiva MQTT global.
- **`/nava set_tz [tz_POSIX]`** — Zona horaria POSIX.
- **`/nava set_hops [1-7]`** — Límite de saltos LoRa.
- **`/nava set_txpower [0-12]`** — Potencia TX (Promicro/E22P). En la **Faketec HT-RA62 es [0-22]**.

---

## 🧹 7. Mantenimiento y Reinicio (SOLO DM PRIVADO CIFRADO)

- **`/nava db_purge`** — Expulsa nodos temporales conservando favoritos y admins.
- **`/nava db_clear`** — Vacía la base de nodos (nuclear). **Importante**: `db_clear` borra también tu propia entrada del repetidor; el DM PKI solo se descifra con tu entrada en su base de datos. Para re-acreditar: **fuerza desde tu mando el reenvío de tu propio NodeInfo (broadcast, NO DM)** antes de enviar cualquier `/nava`.
- **`/nava reboot`** — Reinicio diferido a 3s (el ACK sale antes).
- **`/nava factory_reset`** — Reset de fábrica de emergencia restaurando canales de rescate (el ACK sale antes de ejecutarse). **Regenera el par PKI**. Las claves admin del usuario y canales secundarios se conservan en `/resilience.bin` y vuelven tras el reset.
- **`/nava full_reset`** — Reset completo: configuración + semi-persistentes (`/resilience.bin`) a defaults **conservando el par PKI, los bonds BLE y las claves admin del usuario (F20)** → revert remoto sin PC y sin romper la malla.
- **`/nava wipe`** — Purga de compromiso: erase total + **par PKI NUEVO** + bonds BLE + **claves admin persistidas borradas** (queda solo la del proyecto — rescate garantizado).

---

## 🔋 8. Energía y Resiliencia (SOLO DM PRIVADO CIFRADO)

- **`/nava set_chem [lipo/nimh/sodium/lifepo4]`** — Cambia química de batería y ajusta corte/OCV/LPCOMP. Persiste en `/resilience.bin`. Cortes: lipo 3500, nimh 3400, sodium 2600, **lifepo4 2800**. **Sin argumento** responde la química actual + tabla de cortes/despertar.
- **`/nava set_vbat [2400-3600]`** — Corte de apagado por batería en mV.
- **`/nava set_vwake [1-5]`** — Nivel LPCOMP de reencendido solar.
- **`/nava storm [1-720]`** — Hibernación con radio apagada (RTC2), despierta por temporizador y reinicia.
- **`/nava storm test1` / `test2`** — Prueba rápida: 60s / 120s.
- **`/nava txoff`** — Apaga TX tras 3s (mantiene RX).
- **`/nava txon`** — Reactiva TX.
- **`/nava ble [on/off]`** — Apaga/enciende Bluetooth (requiere reinicio).

---

## 📡 9. Transmisión de Datos (SOLO DM PRIVADO CIFRADO)

- **`/nava msg "[TEXTO]"`** — Difunde mensaje en Canal 0 firmado por el repetidor.
- **`/nava pos`** — Fuerza emisión de posición.
- **`/nava nodeinfo`** — Baliza NodeInfo sin pedir respuesta.
- **`/nava sendtel`** — Telemetrías ambientales inmediatas.
- **`/nava power`** — Métricas de energía: ADC interno (mV) + sensor de potencia I2C (INA219).

---

## 💤 10. Avisos de Sueño/Despertar y Modo de Resiliencia Solar

El nodo anuncia por su canal CLI asignado (`prefs.cliChannelSlot`, por defecto Canal 1 Navadmin) su estado de batería y ciclo solar:

| Aviso | Nivel / Banda | Comportamiento en Red |
|---|---|---|
| **`[Listo]`** | **Normal** ($V \ge \text{corte}$) | **Despertar por recuperación solar**: El sol ha recargado el banco $\rightarrow$ "despierto, cargando, listo para trabajar". |
| **`[Vivo]`** | **Nivel 1** ($3.30\text{V} - 3.40\text{V}$) | **Límite de corte**: Despertado por reset externo $\rightarrow$ "sigo vivo, al limite de carga" (opera 160s). |
| **`[Critico]`** | **Nivel 2** ($< 3.30\text{V}$) | **Capacidad crítica**: Despertado por reset externo $\rightarrow$ "bateria en capacidad critica, operando 160s". |
| **`[Sueño]`** | **Corte de Batería** ($V < \text{corte}$) | **Entrada en Sueño Profundo**: Tras 8 lecturas consecutivas bajo el corte (~160s) $\rightarrow$ Emite aviso y duerme a **0.4 mA**. |
| **`[Boot]`** | **Arranque General** (diferido 2 min) | **Diagnóstico de Reinicio**: Reporta causa hardware (`RESETREAS`) y versión `NAVA V3`. |

- **`/nava sleepmsg [on|off]`** — Activa/desactiva los avisos solares.

---

## 🔔 11. Utilidades (SOLO DM PRIVADO CIFRADO)

- **`/nava bell`** — Alarma acústica para localización.
- **`/nava admin_ls`** — Muestra las 3 claves criptográficas de admin en **base64**.
- **`/nava keys_ls`** — Muestra las claves admin **persistidas** en `/resilience.bin`.
- **`/nava keys_clear`** — Borra SOLO la copia **persistida** de las claves admin.

---

## ⚠️ 12. Coexistencia y Diferencias con la App Oficial de Meshtastic

NavaTastic incorpora un motor de resiliencia (`/resilience.bin`) diseñado para evitar que un repetidor solar de montaña quede aislado, huérfano o averiado por degradación de flash. Esto introduce comportamientos intencionados que difieren de la App oficial:

| Función / Interruptor | Comportamiento en App Oficial | Comportamiento en NavaTastic | Razón de Seguridad / Resiliencia | Solución / Manejo Canónico |
| :--- | :--- | :--- | :--- | :--- |
| **Interruptor Bluetooth (BLE)** | Al apagar el switch se guarda en flash. | Al reiniciar, el firmware **vuelve a encender BLE** si no se usó `/nava`. | **Protección Anti-Huérfano**: Evita que un toque accidental en el móvil deje el repetidor incomunicado sin Bluetooth en la cumbre. | Para apagarlo de forma permanente: enviar por DM `/nava ble off` (guarda `prefs.ble_disabled = 1`). |
| **Canal 1 (Navadmin)** | La App permite borrarlo o editarlo. | **Inamovible y protegido**: Rechaza `/nava ch_del 1` y restaura PSK `AQ==`. | **Canal de Rescate Vital**: Garantiza que el nodo siempre emita avisos de ciclo solar, telemetría y diagnósticos. | Para silenciarlo en abierto: `/nava navadmin_mute on` o mover la consola con `/nava set_cli_chan <2-7>`. |
| **Claves Admin tras Reset** | Un Factory Reset borra todas las claves. | **Persistencia Criptográfica**: Las claves de usuario y rescate se restauran solas. | **Mantenimiento Remoto Seguro**: Evita perder el control administrativo tras un reset de configuración. | Consultar con `/nava admin_ls` o purgar copia persistida con `/nava keys_clear`. |
| **Rol de Hardware** | Un reset vuelve al rol del binario original. | **Rol Semi-Permanente**: El rol modificado persiste tras Factory Reset. | **Supervivencia de Malla**: Evita que un router reconvertido vuelva a cliente tras una tormenta eléctrica. | Conmutar con `/nava set_role router` o `/nava set_role client`. |
| **Base de Datos de Nodos** | La App espera que los nodos se guarden en flash. | **100% RAM-Only**: Nodos de paso viven en RAM y no se escriben en disco. | **Cero Desgaste de Flash**: Multiplica por 10 la vida útil del microcontrolador al no quemar las celdas flash. | Los favoritos manuales y automáticos se respaldan en `/resilience.bin`. |
| **Cadencia de Balizas en Routers** | Suele emitir cada 15 a 30 minutos. | Fijada por defecto a **72 horas** en routers de infraestructura. | **Anti-Saturación LoRa**: Protege el canal de spam de posición innecesario en repetidores fijos. | Ajustar intervalos de flota con `/nava set_pos_tx` y `/nava set_nodeinfo_tx`. |

---

## 🎯 Sintaxis de Direccionamiento de Lote (Prefijos)

1. **Por ID**: `/nava !a7c43b2f ping` (solo responde ese nodo).
2. **Por Rol**: `/nava @router status` (solo Routers).
3. **Por Nombre**: `/nava @name:Navarra env` (solo nombres que empiecen por "Navarra").
4. **A Todos**: `/nava env` (todos los repetidores al alcance, secuencial).

---

# English Condensed Reference: Remote Administration /nava

## 🛡️ Security Levels

- **Open Channel (Navadmin / CLI Active Channel)**: Read-only diagnostic queries (`ping`, `status`, `env`, `channel`, `peers`, `rxlog`, `afc`, `reset_reason`, `noise`, `bat`, `stats`, `log`, `route`, `trace`). Admins only. Generic 30s rate-limit.
- **Encrypted DM (PKI)**: Critical configuration, reboots, database management, channels, power, and infrastructure.

## Command Glossary (F21 Complete)

1. **Diagnostics**: `ping`, `status`, `env`, `channel`, `peers`, `rxlog`, `afc`, `reset_reason`, `noise`, `bat`, `stats` (RAM-only), `log` (RAM-only events), `route !ID`, `trace !ID`, `help [cmd]`.
2. **Channels (F21)**: `ch_ls`, `ch_set <slot> <name> <psk_b64>`, `ch_del <slot>`, `ch_url [slot]`, `set_cli_chan <slot>`, `navadmin_mute [on|off]`, `ch_reset`.
3. **Infrastructure (F21)**: `ch_mqtt <slot> [up|down|both|off]`, `set_ok_to_mqtt [on|off]`, `set_pos <lat> <lon> [alt]`, `set_beacon [mins]`, `mute [mins|off]`, `set_pin <6_digits>`, `test_tx [secs]`.
4. **Blacklist / Favorites**: `ign add/rm/ls`, `fav add/rm/ls/auto [on|off]`.
5. **Node Config**: `set_name`, `set_role [client/mute/router]`, `set_mqtt [on|off]`, `set_tz`, `set_hops`, `set_txpower`.
6. **Maintenance & Resets**: `db_purge`, `db_clear`, `reboot`, `factory_reset`, `full_reset`, `wipe`.
7. **Power & Solar Resilience**: `set_chem`, `set_vbat`, `set_vwake`, `storm [hours]`, `txoff`, `txon`, `ble [on|off]`, `sleepmsg [on|off]`.
8. **Utilities & Admin Keys**: `bell`, `admin_ls`, `keys_ls`, `keys_clear`, `power`, `msg "[TEXT]"`, `pos`, `nodeinfo`, `sendtel`.

## ⚠️ Coexistence & Differences with Official Meshtastic App

- **Bluetooth BLE Switch**: Disabling BLE from the official app will be overridden upon reboot to prevent orphaned mountain repeaters. To permanently disable BLE, send DM command `/nava ble off`.
- **Channel 1 (Navadmin)**: Locked and protected with default PSK `AQ==` (`0x01`). Cannot be deleted via app to guarantee a persistent remote rescue channel. Mute with `/nava navadmin_mute on` or relocate CLI with `/nava set_cli_chan <slot>`.
- **Admin Keys Persistence**: Admin keys survive factory resets via `/resilience.bin` backup.
- **Semi-Permanent Role**: Role changes (`set_role router/client`) persist across factory resets.
- **Zero Flash Wear (RAM-Only NodeDB)**: Node discovery database lives in RAM to protect flash cells from burnout. Favorite nodes are backed up in `/resilience.bin`.
