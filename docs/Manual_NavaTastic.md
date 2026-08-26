---
title: "Manual de Administracion Remota /nava"
subtitle: "NavaTastic V5 (v4.3.4) - Gestion remota integral de nodo y flota provincial"
author: "NavaTastic - EA2OY"
date: "Agosto 2026"
colorlinks: true
toc: true
toc-title: "Indice"
---

# Manual de Administración Remota NavaTastic — NavaTastic V5 (v4.3.4)

> **ADENDA 25/08/2026 — NAVATASTIC V5 (v4.3.4)**:
> - **Sincronización Bidireccional Transparente de la App Oficial**: Los 12 ajustes cotidianos modificados desde la App Oficial de Meshtastic (rol, OK to MQTT, intervalos de telemetría/nodeinfo/posición, posición fija, canales 0-7, LoRa preset/frecuencia, PIN BLE, ignorados y admin_keys) se sincronizan automáticamente y sin fricción hacia `/resilience.bin` V6 (`NAV6`), eliminando cualquier reversión no deseada al reiniciar.
> - **Hop-Aware Timing y Desacople de Traceroute**: Jitter adaptativo escalonado en DM (300ms a 0 saltos, 1.5s a 1 salto, 3.5s a $\ge 2$ saltos), True Random Jitter en Navadmin (5 a 13s) y sonda RF de `traceroute` desacoplada 8s tras el acuse de texto.
> - **Ventana de Gracia Pre-Reboot de 6s**: Margen de seguridad asegurado tras vaciar la cola de transmisión (`responseQueue.empty()`) antes de cualquier reinicio o reset.
> - **Persistencia LoRa (Estándar/Custom) y Canal 0 Primario**: Gestión integral en `/resilience.bin` con los comandos `set_preset`, `set_lora`, `set_freq` y `ch_set 0`.
> - **Protocolo "Botón del Pánico"**: Evacuación de emergencia simultánea de la malla con `panic` y consolidación con `panic_ok`.
> - **Capacidad Ampliada de Auto-Favoritos**: Soporte para hasta 32 nodos directos (`autoFavIds[32]`).

Documento oficial del proyecto NavaTastic. Manual de operación de los comandos `/nava` (módulo `NavaCLIModule`) y guía de administración de red.

---

## 🛡️ Nivel de Seguridad de los Comandos

- **Canal Abierto Público (Navadmin / Slot 1)**:
  - **Broadcast Masivo (sin `!ID`)**: Únicamente los 7 comandos ligeros de sondeo de 1 línea (`ping`, `status`, `bat`, `power`, `env`, `channel`, `noise`) con True Random Jitter (5 a 13s). Menús pesados y respuestas largas se silencian para evitar colisiones.
  - **Broadcast Dirigido (con `!ID` o `@grupo`)**: Permite diagnósticos individuales (`stats`, `log`, `ch_ls`, `help`, `peers`, `rxlog`, `afc`, `reset_reason`, `route`, `trace`).
- **Canal Privado de Flota (Slots 2..7 con CLI redirigida)**: Permite órdenes en lote de gestión de red a toda la flota simultáneamente (`set_ok_to_mqtt`, `set_pos_tx`, `set_nodeinfo_tx`, `set_telem_tx`, `ign`, `set_beacon`, `set_chem`, `mute`, `test_tx`, `db_purge`, `nodeinfo`, `pos`, `sendtel`). Comandos individuales geográficos (`set_pos`, `set_name`, `set_pin`) y destructivos nucleares (`wipe`, `factory_reset`, `full_reset`, `panic`) exigen `!ID` o DM.
- **Solo por DM Privado Cifrado (PKI Curve25519)**: Acceso al 100% de la funcionalidad, configuración de canales (`ch_*`), capa física LoRa (`set_preset`, `set_lora`, `set_freq`), claves admin y evacuación de pánico. Exige firma criptográfica de `admin_key[0..2]`.

---

## 📊 1. Diagnóstico y Telemetría (Permitidos en Canal Abierto y DM)

- **`/nava ping`** — Respuesta de latencia con uptime y piso de ruido. Ej: `PONG: RN1 | SNR: 3.5 dB | Bat: 4120 mV | UP: 32d 4h | RUIDO: -120 dBm`. Rate-limit: 1 respuesta cada 10s por nodo.
- **`/nava status`** — Salud de memoria: etiqueta de build NavaTastic (`NAVA V5 | fw 2.7.26…`), nodos RAM/80, favoritos **Manual/Auto reales** (hasta 32 auto-favoritos, persisten tras reinicio), huérfanos, estado Auto-Fav, tiempo activo y línea de energía (ADC + INA si presente).
- **`/nava power`** — Métricas de energía: ADC interno (mV) + sensor de potencia I2C (INA219: V, ±mA, CARGANDO/DESCARGANDO, mW).
- **`/nava env`** — Batería, heap, temperatura CPU nRF52 y sensor ambiental I2C.
- **`/nava channel`** — Uso de espectro (airtime % y TX %).
- **`/nava peers`** — Vecinos directos a 0 saltos (ID, rol, SNR, tiempo desde último contacto).
- **`/nava rxlog`** — Metadatos de los últimos 5 paquetes recibidos (ID, PortNum, SNR, RSSI).
- **`/nava afc`** — Deriva de frecuencia del TCXO en Hz del último paquete.
- **`/nava reset_reason`** — Motivo del último reinicio (registro RESETREAS).
- **`/nava noise`** — Piso de ruido instantáneo del chip LoRa en dBm.
- **`/nava bat`** — Química activa, voltaje mV, % OCV y estado TX.
- **`/nava stats`** — **[100% RAM]** Informe forense de extremos y tráfico: temperaturas CPU mín/máx/act, voltaje mín/act, paquetes RX/TX/Enrutados y conteo Auto-Fav. (Requiere `!ID` o DM).
- **`/nava log [lineas]`** — **[100% RAM]** Muestra las últimas 1-15 entradas del buffer circular de eventos en memoria (boot, cortes, transiciones de ciclo solar y comandos ejecutados). (Requiere `!ID` o DM).
- **`/nava route !ID`** — Saltos y SNR con que escucha al nodo. Si no está en la BD, lanza un TraceRoute automáticamente.
- **`/nava trace !ID`** — **[Desacoplado en V5]** Responde inmediatamente con acuse de texto `OK: TRACEROUTE ENCOLADO. SONDA RF EN 8s...` y lanza la sonda RF 8 segundos después para evitar colisiones en mallas de alta latencia.
- **`/nava help`** — Glosario corto de comandos. (En DM o con `!ID`).
- **`/nava help <comando>`** también: **`/nava <comando> ?`** / **`/nava <comando> help`** — Ayuda interactiva y estado actual del parámetro. Ej: `/nava help set_preset`, `/nava set_lora ?`.

---

## 📻 2. Gestión de Canales, Frecuencias y Radio LoRa (SOLO DM PRIVADO CIFRADO)

### 2.1 Capa Física LoRa y Presets (Novedad V5)
- **`/nava set_preset <nombre>`** — Aplica un preset LoRa estándar y asigna su slot de frecuencia por defecto. Aplica reinicio suave en 6 segundos para recalibrar el módem SX1262.
  - *Presets soportados*: `long_fast`, `medium_fast`, `short_fast`, `long_slow`, `short_slow`, `medium_slow`, `long_moderate`, `short_turbo`.
- **`/nava set_lora <bw> <sf> <cr> <freq_mhz> <slot> [txpower]`** — Configuración integral de preset Custom para el módem LoRa:
  - `bw`: Ancho de banda (31, 62, 125, 200, 250, 500 kHz).
  - `sf`: Spreading Factor (5 a 12).
  - `cr`: Coding Rate (4 a 8).
  - `freq_mhz`: Frecuencia en MHz (863.0 a 873.3 MHz en EU868).
  - `slot`: Número de slot de canal (ej. 4 en ShortFast Narrow España).
  - `txpower`: Potencia de transmisión opcional en dBm (1 a 22 dBm).
  - *Ejemplo SFNarrow España*: `/nava set_lora 62 7 5 869.618 4 22`
- **`/nava set_freq <freq_mhz> [slot]`** — Ajusta atómicamente la frecuencia central de emisión y su slot asociado.

### 2.2 Botón del Pánico (Cambio masivo de frecuencia o preset)

Permite cambiar de canal, preset o velocidad a todos los repetidores de la montaña a la vez sin tener que subir físicamente a pie.

- **`/nava panic <preset|sfnarrow> [minutos_aviso=10] [minutos_prueba=0]`** — Inicia la migración de toda la flota:
  - `<preset|sfnarrow>`: Preset o velocidad destino (ej: `medium_fast`, `long_fast`, `sfnarrow`, etc.).
  - `[minutos_aviso]` *(Opcional, por defecto 10)*: Tiempo que da a los repetidores para avisarse entre ellos por la montaña antes de cambiar a la vez. Durante el último minuto la red se silencia para que el salto sea limpio.
  - `[minutos_prueba]` *(Opcional, por defecto 0)*: Tiempo de prueba con vuelta automática de seguridad:
    - **Si pones `0` (Definitivo)**: Cambian y se quedan fijos en el nuevo preset para siempre.
    - **Si pones minutos (ej. `120` = 2 horas de prueba)**: Cambian de forma temporal. Si en 2 horas nadie confirma que todo va bien, **los repetidores vuelven solos automáticamente a la frecuencia de fábrica (SFNarrow)**.

- **`/nava panic_ok`** — Confirma que la migración ha sido un éxito y cancela la vuelta atrás.
  - *Cómo usarlo*: Cambia tu teléfono/mando a la nueva frecuencia y manda `/nava panic_ok` por el canal de administración para que todos los repetidores queden fijados definitivamente.

#### 💡 Ejemplos claros de uso:
- **Probar un preset nuevo con 2 horas de red de seguridad**:  
  `/nava panic medium_fast 15 120`  
  *(Avisa durante 15 minutos, salta a MediumFast y te da 2 horas para probarlo y mandar `/nava panic_ok`. Si no lo mandas o no hay cobertura, los repetidores vuelven solos a SFNarrow)*.
- **Volver definitivamente a la frecuencia oficial SFNarrow**:  
  `/nava panic sfnarrow 10 0`  
  *(Avisa durante 10 minutos y se queda fijado para siempre en SFNarrow)*.

> ⚠️ **Aviso de seguridad**: Si un repetidor sufre un reinicio eléctrico o de watchdog durante el tiempo de prueba, seguirá funcionando en el nuevo preset y renovará el tiempo de espera, dándote margen para mandar el `panic_ok`.



### 2.3 Canales Lógicos (Primario y Secundarios)
- **`/nava ch_ls`** — Lista los 8 slots de canales (0-7), indicando rol (`PRI`, `SEC`, `DIS`), marca de canal CLI activo (`*`), nombre, tipo de clave (`AES128`, `AES256`, `#1`, `DEF_PRI`) y compuerta MQTT (`U/D`, `U`, `D`, `-`).
- **`/nava ch_set 0 <nombre> <psk_base64>`** — **[Novedad V5]** Configura de forma persistente el Canal 0 Primario (nombre y clave PSK) sin alterar la modulación física de la radio.
- **`/nava ch_set <slot 2-7> <nombre> <psk_base64>`** — Configura y activa un canal secundario privado. La clave se decodifica en Base64 (1 byte índice, 16 bytes AES-128 o 32 bytes AES-256).
  - *Ejemplo AES-128*: `/nava ch_set 2 Privada 1b4D8...==`
  - *Ejemplo por defecto*: `/nava ch_set 3 Malla AQ==`
- **`/nava ch_del <slot 2-7>`** — Deshabilita el canal del slot indicado y limpia su respaldo.
- **`/nava ch_url [slot 0-7]`** — Genera la URL canónica de Meshtastic (`https://meshtastic.org/e/#...`) para importar el canal directamente mediante código QR o enlace en la aplicación móvil.
- **`/nava set_cli_chan <slot 1-7>`** — Redirige la escucha de comandos `/nava` y la emisión de los avisos solares (`[Listo]`, `[Vivo]`, `[Critico]`, `[Sueño]`, `[Boot]`) al slot seleccionado (1-7).
- **`/nava navadmin_mute [on|off]`** — Silencia o reactiva el canal público de rescate (Slot 1 Navadmin).
- **`/nava ch_reset`** — Restaura la tabla de canales al estado de fábrica (Slot 0 SFNarrow, Slot 1 Navadmin, Slots 2-7 deshabilitados, CLI en Slot 1 y Mute OFF).

---

## 🌐 3. Pasarelas MQTT y Gestión de Infraestructura (DM o Canal Privado de Flota)

- **`/nava ch_mqtt <slot 0-7> [up|down|both|off]`** — Configura individualmente el reenvío MQTT para el canal indicado (subida `up`, bajada `down`, bidireccional `both` o apagado `off`).
- **`/nava set_ok_to_mqtt [on|off]`** — Activa o desactiva la bandera global `config_ok_to_mqtt` en los paquetes del nodo, autorizando a pasarelas ajenas a subirlos a servidores MQTT públicos o privados.
- **`/nava set_pos <lat> <lon> [alt]`** — Fija coordenadas geográficas estáticas en el nodo (sin GPS físico). Persiste a resets de fábrica y dispara emisión inmediata de posición a los mapas.
- **`/nava pos_clear`** — Borra las coordenadas fijas guardadas, dejando el repetidor sin posición fija.
- **`/nava set_pos_tx [on|off|minutos]`** — Controla la difusión periódica espontánea de posición de flota (por defecto 72h). Con `off` se apaga por completo para ahorro de airtime y privacidad.
- **`/nava set_nodeinfo_tx [on|off|minutos]`** — Controla la difusión periódica de NodeInfo/nombres en la flota (por defecto 72h).
- **`/nava set_telem_tx [on|off|minutos]`** — Regula el intervalo de reporte de telemetría de batería, energía y sensores de clima/ambiente (por defecto: **12 horas** = 720 min; configurable de 1 a 1440 min). Persiste en `/resilience.bin` y se sincroniza con la App Oficial.
- **`/nava set_beacon [minutos]`** — Configura el intervalo de emisión periódica de las balizas `NodeInfo` y `Position` (1 a 1440 minutos).
- **`/nava mute [minutos|off]`** — **[100% RAM]** Activa el modo silencioso temporal en el repetidor. Cancela el reenvío LoRa de paquetes de terceros durante el tiempo indicado para realizar auditorías limpias de espectro.
- **`/nava set_pin <6_digitos>`** — Configura un PIN fijo personalizado de 6 dígitos para el emparejamiento Bluetooth BLE. Persiste en `/resilience.bin`.
- **`/nava test_tx [segundos 5-30]`** — **[100% RAM]** Emite una ráfaga periódica de balizas de prueba a razón de 1 paquete/segundo para medir niveles de cobertura y SNR en campo.

---

## 🚫 4. Gestión de Bloqueos y Lista Negra Persistente (DM o Canal Privado de Flota)

- **`/nava ign ls`** — Lista los nodos bloqueados persistentes en `/resilience.bin`.
- **`/nava ign add !ID`** — Bloquea y silencia al nodo, descartando sus paquetes en el router y persistiendo en disco (hasta 8 nodos).
- **`/nava ign rm !ID`** (o `ign del !ID`) — Desbloquea al nodo de la lista negra.
- **`/nava ign clear`** — Borra la lista negra por completo.

---

## ⭐️ 5. Gestión de Favoritos (SOLO DM PRIVADO CIFRADO)

- **`/nava fav add !ID`** — Añade a favoritos manuales con bypass de saltos (máx. 10).
- **`/nava fav rm !ID`** — Elimina de favoritos.
- **`/nava fav ls`** — Lista los favoritos, etiquetados `[AUTO]` (auto-favoriteo hasta 32 nodos) o `[MAN]` (manuales).
- **`/nava fav auto [on|off]`** — Activa/desactiva el auto-favoriteo de routers directos (0 saltos). Por defecto: ACTIVADO. Con OFF no se marcan nuevos auto-favoritos; los favoritos existentes se conservan. Persiste en `/resilience.bin`.

---

## ⚙️ 6. Configuración del Nodo en Caliente (SOLO DM PRIVADO CIFRADO)

- **`/nava set_name "[Largo]" "[Corto]"`** — Cambia el nombre y lo **hardcodea como persistente en `/resilience.bin`** (sobrevive a cualquier reset de fábrica). Sincroniza en `NodeDB` local, persiste en `SEGMENT_DEVICESTATE` y emite `NodeInfo` actualizado de inmediato a la malla.
- **`/nava set_name flush`** — Elimina el nombre hardcodeado de `/resilience.bin`, devolviendo el nodo al comportamiento natural de la app.
- **`/nava set_role [client/mute/router]`** — Cambia el rol de hardware. **SEMI-PERMANENTE**: se guarda en `/resilience.bin`, sincroniza `owner.role` y sobrevive al factory reset.
- **`/nava set_mqtt [on/off]`** — Activa/desactiva MQTT global.
- **`/nava set_tz [tz_POSIX]`** — Zona horaria POSIX.
- **`/nava set_hops [1-7]`** — Límite de saltos LoRa.
- **`/nava set_txpower [0-12]`** — Potencia TX (Promicro/E22P). En la **Faketec HT-RA62 es [0-22]**.

---

## 🧹 7. Mantenimiento y Reinicio con Ventana de Gracia (SOLO DM)

- **`/nava db_purge`** — Expulsa nodos temporales conservando favoritos y admins.
- **`/nava db_clear`** — Vacía la base de nodos (nuclear).
- **`/nava reboot`** — Reinicio con ventana de gracia de 6 segundos tras vaciar la cola de transmisión del paquete de acuse.
- **`/nava factory_reset`** — Reset de fábrica de emergencia conservando claves admin de usuario y canales secundarios en `/resilience.bin`.
- **`/nava full_reset CONFIRM`** — Reset completo a defaults conservando el par PKI, los bonds BLE y las claves admin del usuario (`NAV7`). Requiere confirmación explícita.
- **`/nava wipe CONFIRM`** — Purga total de compromiso: erase total + par PKI NUEVO + bonds BLE + purga de claves admin persistidas. Requiere confirmación explícita.

---

## 🔋 8. Energía y Resiliencia (SOLO DM PRIVADO CIFRADO)

- **`/nava set_chem [lipo/nimh/sodium/lifepo4]`** — Cambia química de batería y ajusta corte/OCV/LPCOMP. Persiste en `/resilience.bin`. Cortes: lipo 3500, nimh 3400, sodium 2600, **lifepo4 2800**.
- **`/nava set_vbat [2400-3600]`** — Corte de apagado por batería en mV.
- **`/nava set_vwake [1-5]`** — Nivel LPCOMP de reencendido solar (debe ser estrictamente superior al corte `vbat_cutoff`).
- **`/nava panic <preset|params> [minutos=10] [rollback=0]`** — Salto coordinado de evacuación por pánico de toda la red.
- **`/nava panic_ok`** — Consolida permanentemente el salto de evacuación en toda la red, emitiendo pulso `POK!` y cancelando el rollback.
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
| **`[Boot]`** | **Arranque General** (diferido 2 min) | **Diagnóstico de Reinicio**: Reporta causa hardware (`RESETREAS`) y versión `NAVA V5`. |

- **`/nava sleepmsg [on|off]`** — Activa/desactiva los avisos solares.

---

## 🔔 11. Utilidades y Claves Admin (SOLO DM)

- **`/nava bell`** — Alarma acústica para localización.
- **`/nava admin_ls`** — Muestra las 3 claves criptográficas de admin en **base64**.
- **`/nava keys_ls`** — Muestra las claves admin **persistidas** en `/resilience.bin`.
- **`/nava keys_clear`** — Borra SOLO la copia **persistida** de las claves admin.

---

## 🔄 12. Sincronización Bidireccional con la App Oficial de Meshtastic

A partir de **NavaTastic V5**, la interacción entre la App Oficial de Meshtastic y el motor de resiliencia `/resilience.bin` es **completamente transparente y bidireccional**:

| Parámetro en App Oficial | Comportamiento en NavaTastic V5 |
| :--- | :--- |
| **Rol del Dispositivo** | Al cambiar el rol en la App, se sincroniza en `/resilience.bin` y se actualiza `owner.role` en tiempo real. |
| **OK to MQTT** | Se sincroniza automáticamente hacia la flash de resiliencia. |
| **Intervalos de Telemetría, NodeInfo y Posición** | Se sincronizan en `/resilience.bin` evitando reversiones en el boot. |
| **Posición Fija y Coordenadas GPS** | Se persisten atómicamente y se difunden de inmediato a la malla. |
| **Canales 0 al 7** | Cualquier canal añadido o modificado en la App se respalda en `/resilience.bin`. |
| **Preset LoRa y Frecuencia** | Se validan y guardan en el bloque de radio física persistente. |
| **PIN Bluetooth** | Si se define en la App, queda protegido contra reinicios. |
| **Lista Negra / Nodos Ignorados** | Se sincroniza con el filtro del router. |
| **Canal 1 Navadmin** | Protegido como canal de rescate y telemetría de ciclo solar. |

---

## 🎯 Sintaxis de Direccionamiento de Lote (Prefijos)

1. **Por ID**: `/nava !a7c43b2f ping` (solo responde ese nodo).
2. **Por Rol**: `/nava @router status` (solo Routers).
3. **Por Nombre**: `/nava @name:Navarra env` (solo nombres que empiecen por "Navarra").
4. **A Todos**: `/nava env` (todos los repetidores al alcance, secuencial con jitter anti-colisión).

---

# English Condensed Reference: Remote Administration /nava (V5)

## 🛡️ Security Levels

- **Open Channel (Navadmin / CLI Active Channel)**: 1-line queries (`ping`, `status`, `env`, `channel`, `peers`, `rxlog`, `afc`, `reset_reason`, `noise`, `bat`, `stats`, `log`, `route`, `trace`). True random jitter (5-13s).
- **Encrypted DM (PKI)**: Critical configuration, LoRa PHY settings, panic protocol, reboots, database management, channels, power, and infrastructure.

## Command Glossary (V5 Complete)

1. **Diagnostics**: `ping`, `status`, `env`, `channel`, `peers`, `rxlog`, `afc`, `reset_reason`, `noise`, `bat`, `stats` (RAM-only), `log` (RAM-only events), `route !ID`, `trace !ID` (8s RF decoupled), `help [cmd]`.
2. **LoRa PHY & Panic Protocol (V5)**: `set_preset <name>`, `set_lora <bw> <sf> <cr> <freq_mhz> <slot> [txpower]`, `set_freq <freq_mhz> [slot]`, `panic <preset> [mins] [rollback_mins]`, `panic_ok`.
3. **Channels**: `ch_ls`, `ch_set 0 <name> <psk_b64>` (Primary Channel persistent), `ch_set <slot 2-7> <name> <psk_b64>`, `ch_del <slot>`, `ch_url [slot]`, `set_cli_chan <slot>`, `navadmin_mute [on|off]`, `ch_reset`.
4. **Infrastructure**: `ch_mqtt <slot> [up|down|both|off]`, `set_ok_to_mqtt [on|off]`, `set_pos <lat> <lon> [alt]`, `pos_clear`, `set_pos_tx [mins|off]`, `set_nodeinfo_tx [mins|off]`, `set_telem_tx [mins|off]`, `set_beacon [mins]`, `mute [mins|off]`, `set_pin <6_digits>`, `test_tx [secs]`.
5. **Blacklist / Favorites**: `ign add/rm/ls/clear`, `fav add/rm/ls/auto [on|off]` (up to 32 auto-favorites).
6. **Node Config**: `set_name "[Long]" "[Short]" | set_name flush` (persistent override in `/resilience.bin` vs natural mode), `set_role [client/mute/router]` (semi-permanent, synchronized with `owner.role`), `set_mqtt [on|off]`, `set_tz`, `set_hops`, `set_txpower`.
7. **Maintenance & Resets (6s Grace Period)**: `db_purge`, `db_clear`, `reboot`, `factory_reset`, `full_reset`, `wipe`.
8. **Power & Solar Resilience**: `set_chem`, `set_vbat`, `set_vwake`, `storm [hours]`, `txoff`, `txon`, `ble [on|off]`, `sleepmsg [on|off]`.
9. **Utilities & Admin Keys**: `bell`, `admin_ls`, `keys_ls`, `keys_clear`, `power`, `msg "[TEXT]"`, `pos`, `nodeinfo`, `sendtel`.
