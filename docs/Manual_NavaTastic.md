---
title: "Manual de Administracion Remota /nava"
subtitle: "NavaTastic Eclipse V5.2 (v4.3.9) - Gestion remota integral de nodo y flota provincial"
author: "NavaTastic - EA2OY"
date: "Agosto 2026"
colorlinks: true
toc: true
toc-title: "Indice"
---

# Manual de Administración Remota NavaTastic — NavaTastic Eclipse V5.2 (v4.3.9)

> **ADENDA 15/09/2026 — PRIORIDAD DE CLAVES ADMIN: MANDA LA CONFIGURACIÓN (regla del operador)**:
> si la configuración del nodo tiene **alguna clave de dueño válida** (32 B, correcta y
> distinta de la del proyecto), **manda ella y el respaldo interno NO toca nada** en el arranque. El
> respaldo interno entra **solo para rescatar**,
> cuando la configuración se queda **sin ninguna clave de dueño** (reset de fábrica, flasheo,
> catástrofe). Consecuencias: **borrar** una clave en la App **funciona** (ya no se resucita al
> reiniciar), **no puede quedar la misma clave en dos campos** reinyectándose dos veces y
> **`keys_clear` antes de configurar deja de ser un paso obligatorio**. A cambio, restaurar una
> **copia de seguridad antigua** del móvil con claves viejas **no se corrige sola**: esas claves son
> válidas y mandan hasta que se reponga la propia desde la App. El **rescate automático de fábrica** no cambia
> (configuración sin ninguna clave válida → se reinyecta la del proyecto, **sin aviso por radio ni
> por ningún otro medio**).
>
> **ADENDA 11/09/2026 — NAVATASTIC ECLIPSE V5.1 (v4.3.8)**: **el nombre que pongas con la App se recuerda** (sobrevive a reinicios y al reset de fábrica; `set_name` sigue mandando hasta `flush`); **contador `FR`** de restablecimientos de fábrica en `status`, `reset_reason` y `[Boot]` (solo si >0); **`/nava trace` devuelve el resultado** por el mismo canal (ruta IDA/VUELTA con SNR por tramo; aviso si el destino no responde); un rol avanzado puesto desde la App ya no restablece el nodo; el aviso `[Boot]` pasa a los 3 minutos.
>
> **ADENDA 28/08/2026 — NAVATASTIC ECLIPSE V5 (v4.3.7): EL USUARIO MANDA**:
> - **Nuevo comando `/nava set_rebroadcast <all|local|known|core|none>`**: elige cómo
>   retransmite el nodo los mensajes ajenos. Se guarda y sobrevive a los resets (también se
>   puede cambiar desde la App oficial).
> - **El rol y la retransmisión ya no se revierten solos**: al cambiar el rol (App o NavaCLI)
>   ya no se re-aplican los valores por defecto del rol (72 h, LOCAL_ONLY, vecindario) — esos
>   valores son de rescate y solo se reinyectan en la instalación de fábrica o si el respaldo
>   interno se pierde o corrompe. **Todo lo que configures persiste**.
> - **Apagar también persiste**: desactivar posición/presencia/telemetría (OFF) sobrevive a
>   reinicios y a resets mientras el respaldo esté sano.
> - **Instalación automática**: al instalar sobre cualquier firmware oficial, el primer
>   arranque aplica solo los valores recomendados (posición y presencia cada 72 h, sensores
>   cada 12 h, canal Navadmin, SFNarrow) **respetando tus claves de administración** (si no
>   tienes ninguna, se inyecta la de rescate del proyecto). Sin factory reset manual.

> **ADENDA 26/08/2026 — NAVATASTIC V5.2 (v4.3.5, Auditoría de seguridad)**:
> - **Acreditación de admin por mensaje cifrado**: el título de administrador solo se concede tras el primer mensaje directo cifrado descifrado con una clave de administración (un anuncio público ya no basta).
> - **Navadmin solo-lectura y solo admins**: `set_lora`, `set_freq`, `set_preset`, `panic`, `panic_ok` y destructivos bloqueados en el canal público; silencio total ante no-admins.
> - **Pánico por mensaje cifrado o canal privado**: los pulsos de propagación viajan cifrados por el canal privado de flota (Slots 2-7); solo la flota puede emitirlos o falsificarlos.
> - **Respaldo de claves sagrado**: borrar una clave en la app no purga el respaldo interno; el nodo nunca se queda sin administrador.
> - **`storm`/`mute` con ventana de gracia de 60s**; `CONFIRM` sin distinguir mayúsculas; respaldo interno escrito de forma segura (comprobación de integridad y acceso protegido).

> **ADENDA 25/08/2026 — NAVATASTIC V5 (v4.3.4)**:
> - **Sincronización Bidireccional Transparente de la App Oficial**: Los 12 ajustes cotidianos modificados desde la App Oficial de Meshtastic (rol, OK to MQTT, intervalos de telemetría/presencia/posición, posición fija, canales 0-7, LoRa preset/frecuencia, PIN BLE, ignorados y claves admin) se sincronizan automáticamente y sin fricción con el respaldo interno del nodo, eliminando cualquier reversión no deseada al reiniciar.
> - **Respuestas adaptadas a la distancia y Traceroute desacoplado**: espera proporcional a los saltos en mensajes directos (0,3 s a 3,5 s) con dispersión aleatoria en el canal público (5 a 13 s), y sonda de `traceroute` lanzada 8 s después del acuse de texto.
> - **Ventana de Gracia Pre-Reinicio de 6s**: margen de seguridad tras vaciar la cola de transmisión antes de cualquier reinicio o reset.
> - **Persistencia LoRa (Estándar/Custom) y Canal 0 Primario**: gestión integral con los comandos `set_preset`, `set_lora`, `set_freq` y `ch_set 0`.
> - **Protocolo "Botón del Pánico"**: evacuación de emergencia simultánea de la malla con `panic` y consolidación con `panic_ok`.
> - **Capacidad Ampliada de Auto-Favoritos**: soporte para hasta 32 nodos directos.

Documento oficial del proyecto NavaTastic. Manual de operación de los comandos `/nava` y guía de administración de red.

---

## 🛡️ Nivel de Seguridad de los Comandos

- **Canal Abierto Público (Navadmin / Slot 1)** — SOLO LECTURA y SOLO para administradores verificados (ante no-admins: silencio total):
  - **Broadcast Masivo (sin `!ID`)**: Únicamente los 7 comandos ligeros de sondeo de 1 línea (`ping`, `status`, `bat`, `power`, `env`, `channel`, `noise`) con True Random Jitter (5 a 13s). Menús pesados y respuestas largas se silencian para evitar colisiones.
  - **Broadcast Dirigido (con `!ID` o `@grupo`)**: Permite diagnósticos individuales (`stats`, `log`, `ch_ls`, `help`, `peers`, `rxlog`, `afc`, `reset_reason`, `route`, `trace`).
  - **Prohibido en Navadmin**: cualquier comando de configuración (`set_lora`, `set_freq`, `set_preset`, `set_pin`, `set_role`...), el pánico (`panic`, `panic_ok`) y los destructivos (`wipe`, `factory_reset`, `full_reset`, `reboot`...). Todo eso solo por DM cifrado o canal privado.
- **Canal Privado de Flota (Slots 2..7 con CLI redirigida y clave propia)**: Permite órdenes en lote seguras (el cifrado del canal es la credencial) para gestión uniforme de la flota: `set_pin`, `set_mqtt`, `set_ok_to_mqtt`, `set_telem_tx`, `set_nodeinfo_tx`, `set_pos_tx`, `set_tz`, `set_hops`, `sleepmsg`, `ign add/del`, `db_purge`, `db_clear`, `panic`, `panic_ok`. **NO en lote** (exigen `!ID` o DM): configuración física por nodo (`set_lora`, `set_freq`, `set_preset` — desalineación de malla —, `set_txpower`, `set_chem`, `set_vbat`, `set_vwake`), identidad/topología (`set_name`, `set_role`, `set_pos`, `fav`, `set_cli_chan`, `ch_*`), los que cortan la propagación (`mute`, `storm`, `txoff`, `txon`) y los nucleares (`wipe`, `factory_reset`, `full_reset`, `keys_clear`).
- **Solo por DM Privado Cifrado (PKI Curve25519)**: Acceso al 100% de la funcionalidad, configuración de canales (`ch_*`), capa física LoRa (`set_preset`, `set_lora`, `set_freq`), claves admin y evacuación de pánico. Exige firma criptográfica de `admin_key[0..2]` (el bit de admin solo se concede tras el primer DM descifrado con éxito).

---

## 📊 1. Diagnóstico y Telemetría (Permitidos en Canal Abierto y DM)

- **`/nava ping`** — Respuesta de latencia con uptime y piso de ruido. Ej: `PONG: RN1 | SNR: 3.5 dB | Bat: 4120 mV | UP: 32d 4h | RUIDO: -120 dBm`. Rate-limit: 1 respuesta cada 10s por nodo.
- **`/nava status`** — Salud de memoria: etiqueta de build NavaTastic (`NAVA V5.2 | fw 2.7.26…`), nodos RAM/80, favoritos **Manual/Auto reales** (hasta 32 auto-favoritos, persisten tras reinicio), huérfanos, estado Auto-Fav, tiempo activo y línea de energía (ADC + INA si presente). Si el nodo ha sufrido algún restablecimiento de fábrica, se indica al final como `FR:n` (n veces).
- **`/nava power`** — Métricas de energía: ADC interno (mV) + sensor de potencia I2C (INA219: V, ±mA, CARGANDO/DESCARGANDO, mW).
- **`/nava env`** — Batería, heap, temperatura CPU nRF52 y sensor ambiental I2C.
- **`/nava channel`** — Uso de espectro (airtime % y TX %).
- **`/nava peers`** — Vecinos directos a 0 saltos (ID, rol, SNR, tiempo desde último contacto).
- **`/nava rxlog`** — Metadatos de los últimos 5 paquetes recibidos (ID, PortNum, SNR, RSSI).
- **`/nava afc`** — Deriva de frecuencia del TCXO en Hz del último paquete.
- **`/nava reset_reason`** — Motivo del último reinicio (lo registra el propio microcontrolador). Si el nodo ha sufrido restablecimientos de fábrica, se indica como `FR:n`.
- **`/nava noise`** — Piso de ruido instantáneo del chip LoRa en dBm.
- **`/nava bat`** — Química activa, voltaje mV, % OCV y estado TX.
- **`/nava stats`** — **[100% RAM]** Informe forense de extremos y tráfico: temperaturas CPU mín/máx/act, voltaje mín/act, paquetes RX/TX/Enrutados y conteo Auto-Fav. (Requiere `!ID` o DM).
- **`/nava log [lineas]`** — **[100% RAM]** Muestra las últimas 1-15 entradas del buffer circular de eventos en memoria (boot, cortes, transiciones de ciclo solar y comandos ejecutados). (Requiere `!ID` o DM).
- **`/nava route !ID`** — Saltos y SNR con que escucha al nodo. Si no está en la BD, lanza un TraceRoute automáticamente.
- **`/nava trace !ID`** — **[Desacoplado en V5]** Responde inmediatamente con acuse de texto `OK: TRACEROUTE ENCOLADO. SONDA RF EN 8s...` y lanza la sonda RF 8 segundos después para evitar colisiones en mallas de alta latencia. **Desde V5.1 el resultado vuelve por el mismo canal por el que preguntaste**: ruta completa con los nodos recorridos y la señal de cada tramo, en ida (`IDA:`) y vuelta (`VUELTA:`), con nombres o `!xxxx`; si el destino no contesta en 60 s, recibes el aviso `TRACE: SIN RESPUESTA DEL DESTINO`.
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

> 🛡️ **Seguridad (4.3.5)**: `panic` y `panic_ok` **solo funcionan por DM cifrado (PKI)** o por el **canal privado de flota** (Slots 2-7 con clave propia). Están **bloqueados en el canal público Navadmin** — solo quien posee la clave del canal privado o la clave privada de admin puede disparar una evacuación. La propagación entre repetidores se hace con pulsos cifrados por ese canal privado (los nodos sin la clave no pueden ni leerlos ni falsificarlos).

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
- **`/nava set_ok_to_mqtt [on|off]`** — Activa o desactiva la marca global de permiso MQTT que viaja en los paquetes del nodo, autorizando a pasarelas ajenas a subirlos a servidores MQTT públicos o privados.
- **`/nava set_pos <lat> <lon> [alt]`** — Fija coordenadas geográficas estáticas en el nodo (sin GPS físico). Persiste a resets de fábrica y dispara emisión inmediata de posición a los mapas.
- **`/nava pos_clear`** — Borra las coordenadas fijas guardadas, dejando el repetidor sin posición fija.
- **`/nava set_pos_tx [on|off|minutos]`** — Controla la difusión periódica espontánea de posición de flota (por defecto 72h). Con `off` se apaga por completo para ahorro de airtime y privacidad.
- **`/nava set_nodeinfo_tx [on|off|minutos]`** — Controla la difusión periódica de NodeInfo/nombres en la flota (por defecto 72h).
- **`/nava set_telem_tx [on|off|minutos]`** — Regula el intervalo de reporte de telemetría de batería, energía y sensores de clima/ambiente (por defecto: **12 horas** = 720 min; configurable de 1 a 1440 min). Persiste en el respaldo interno del nodo y se sincroniza con la App Oficial. **Este comando cambia los 5 tipos a la vez**; si quieres tiempos distintos por tipo (batería, clima, energía, aire, salud), configúralos desde la App oficial: ahora cada tipo se guarda por separado. `/nava set_telem_tx ?` muestra los 5 intervalos actuales.
- **`/nava set_nodeinfo_tx [minutos|off]`** — Configura el intervalo de emisión periódica del aviso `NodeInfo` (1 a 1440 minutos, o `off`). Para la cadencia de `Position` está `set_pos_tx`. *(Sustituye a `set_beacon`, retirado el 15/09/2026: escribía el mismo ajuste por un camino aparte y al arrancar no se leía, así que respondía OK y el nodo hacía otra cosa.)*
- **`/nava mute [minutos|off]`** — **[100% RAM]** Activa el modo silencioso temporal en el repetidor. Cancela el reenvío LoRa de paquetes de terceros durante el tiempo indicado para realizar auditorías limpias de espectro.
- **`/nava set_pin <6_digitos>`** — Configura un PIN fijo personalizado de 6 dígitos para el emparejamiento Bluetooth BLE. Persiste en `/resilience.bin`.
- **`/nava test_tx [segundos 5-30]`** — **[100% RAM]** Emite una ráfaga periódica de balizas de prueba a razón de 1 paquete/segundo para medir niveles de cobertura y SNR en campo.

---

## 🚫 4. Gestión de Bloqueos y Lista Negra Persistente (DM o Canal Privado de Flota)

- **`/nava ign ls`** — Lista los nodos bloqueados persistentes en el respaldo interno del nodo.
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

- **`/nava set_name "[Largo]" "[Corto]"`** — Cambia el nombre y lo **fija como persistente en el respaldo interno del nodo** (sobrevive a cualquier reset de fábrica). Sincroniza en el nodo local, persiste en la memoria del nodo y emite la identidad actualizada de inmediato a la malla. Mientras el nombre esté fijado, los cambios hechos desde la App se respetan en el momento pero el fijado vuelve al reiniciar.
- **`/nava set_name flush`** — Elimina el nombre fijado del respaldo interno y devuelve el nodo al comportamiento natural de la App. **Desde V5.1**, en ese modo natural el nombre que pongas con la App **se recuerda solo** (sobrevive a reinicios y a un restablecimiento de fábrica); al actualizar desde otro firmware también se conserva tu nombre.
- **`/nava set_role [client/mute/router]`** — Cambia el rol de hardware. **SEMI-PERMANENTE**: se guarda en el respaldo interno, sincroniza la identidad pública y sobrevive al factory reset. **Revisión 28/08**: al cambiar el rol NO se re-aplican los valores por defecto del rol — tus intervalos y tu modo de retransmisión se mantienen.
- **`/nava set_rebroadcast [all|local|known|core|none]`** — **[Nuevo]** Cambia el modo de retransmisión LoRa del nodo (cómo reenvía paquetes ajenos): `all` (todo), `local` (solo nodos conocidos, el valor de fábrica), `known` (solo emisores conocidos), `core` (solo portnums oficiales), `none` (sin reenvío — rechazado en rol ROUTER). **Persiste y sobrevive a resets**; se sincroniza con la App oficial en ambas direcciones. `all`/`known`/`core` pueden reducir la cobertura del NavaCLI multi-salto (canal privado, avisos y pánico) — elegir con criterio.
- **`/nava set_mqtt [on/off]`** — Activa/desactiva MQTT global.
- **`/nava set_tz [tz_POSIX]`** — Zona horaria POSIX.
- **`/nava set_hops [1-7]`** — Límite de saltos LoRa.
- **`/nava set_txpower [0-12]`** — Potencia TX (Promicro/E22P). En la **Faketec HT-RA62 es [0-22]**.

---

## 🧹 7. Mantenimiento y Reinicio con Ventana de Gracia (SOLO DM)

- **`/nava db_purge`** — Expulsa nodos temporales conservando favoritos y admins.
- **`/nava db_clear`** — Vacía la base de nodos (nuclear).
- **`/nava reboot`** — Reinicio con ventana de gracia de 6 segundos tras vaciar la cola de transmisión del paquete de acuse.
- **`/nava factory_reset`** — Reset de fábrica de emergencia conservando claves admin de usuario y canales secundarios en el respaldo interno.
- **`/nava full_reset CONFIRM`** — Reset completo a defaults conservando el par PKI, los bonds BLE y las claves admin del usuario. Requiere confirmación explícita.
- **`/nava wipe CONFIRM`** — Purga total de compromiso: erase total + par PKI NUEVO + bonds BLE + purga de claves admin persistidas. Requiere confirmación explícita.

---

## 🔋 8. Energía y Resiliencia (SOLO DM PRIVADO CIFRADO)

- **`/nava set_chem [lipo/nimh/sodium/lifepo4]`** — Cambia la química de la batería y ajusta el corte de apagado y el umbral de reencendido solar. Persiste en el respaldo interno. Cortes: lipo 3500, nimh 3400, sodium 2600, **lifepo4 2800**.
- **`/nava set_vbat [2400-3600]`** — Corte de apagado por batería en mV.
- **`/nava set_vwake [1-5]`** — Nivel de tensión de reencendido solar (debe ser estrictamente superior al corte `vbat_cutoff`).
- **`/nava panic <preset|params> [minutos=10] [rollback=0]`** — Salto coordinado de evacuación por pánico de toda la red. **Solo DM cifrado o canal privado de flota** (bloqueado en Navadmin).
- **`/nava panic_ok`** — Consolida permanentemente el salto de evacuación en toda la red, cancelando el rollback. **Solo DM cifrado o canal privado de flota**.
- **`/nava storm [1-720]`** — Hibernación con radio apagada (RTC2), despierta por temporizador y reinicia.
- **`/nava storm test1` / `test2`** — Prueba rápida: 60s / 120s.
- **`/nava txoff`** — Apaga TX tras 3s (mantiene RX).
- **`/nava txon`** — Reactiva TX.
- **`/nava ble [on/off]`** — Apaga/enciende Bluetooth (requiere reinicio).

> **Nota (placas Heltec V3/V4, ESP32-S3)**: la gestión de batería profunda (comparador de bajo consumo, químicas,
> hibernación por tormenta) es específica de las placas nRF52840. En las placas Heltec el comando
> `storm` responde "solo disponible en nRF52" y la energía la gestiona Meshtastic estándar; el
> resto de comandos `/nava` funcionan igual.

---

## 📡 9. Transmisión de Datos (SOLO DM PRIVADO CIFRADO)

- **`/nava msg "[TEXTO]"`** — Difunde mensaje en Canal 0 firmado por el repetidor.
- **`/nava pos`** — Fuerza emisión de posición.
- **`/nava nodeinfo`** — Baliza NodeInfo sin pedir respuesta.
- **`/nava sendtel`** — Telemetrías ambientales inmediatas.
- **`/nava power`** — Métricas de energía: ADC interno (mV) + sensor de potencia I2C (INA219).

---

## 💤 10. Avisos de Sueño/Despertar y Modo de Resiliencia Solar

El nodo anuncia por su canal CLI asignado (por defecto, Canal 1 Navadmin) su estado de batería y ciclo solar:

| Aviso | Nivel / Banda | Comportamiento en Red |
|---|---|---|
| **`[Listo]`** | **Normal** ($V \ge \text{corte}$) | **Despertar por recuperación solar**: El sol ha recargado el banco $\rightarrow$ "despierto, cargando, listo para trabajar". |
| **`[Vivo]`** | **Nivel 1** ($3.30\text{V} - 3.40\text{V}$) | **Límite de corte**: Despertado por reset externo $\rightarrow$ "sigo vivo, al limite de carga" (opera 160s). |
| **`[Critico]`** | **Nivel 2** ($< 3.30\text{V}$) | **Capacidad crítica**: Despertado por reset externo $\rightarrow$ "bateria en capacidad critica, operando 160s". |
| **`[Sueño]`** | **Corte de Batería** ($V < \text{corte}$) | **Entrada en Sueño Profundo**: Tras 8 lecturas consecutivas bajo el corte (~160s) $\rightarrow$ Emite aviso y duerme a **0.4 mA**. |
| **`[Boot]`** | **Arranque General** (diferido 3 min) | **Diagnóstico de Reinicio**: Reporta la causa del reinicio (hardware) y la versión del firmware. |

- **`/nava sleepmsg [on|off]`** — Activa/desactiva los avisos solares.

---

## 🔔 11. Utilidades y Claves Admin (SOLO DM)

- **`/nava bell`** — Alarma acústica para localización.
- **`/nava admin_ls`** — Muestra las 3 claves criptográficas de admin activas (`admin_key[0..2]`, las que autorizan) en **base64**.
- **`/nava keys_ls`** — Muestra las claves admin **persistidas** en el respaldo interno del nodo.
- **`/nava keys_clear`** — Borra SOLO la copia **persistida** de las claves admin; **no toca la configuración activa ni reinicia**.

> **Prioridad configuración ↔ respaldo (regla 15/09/2026)**: el nodo **sale sin
> tocar nada** si la configuración tiene alguna clave de dueño válida; el respaldo se aplica **solo**
> cuando la configuración se queda sin ninguna (reset de fábrica, flasheo, catástrofe). Por eso
> **borrar una clave en la App ya se respeta al reiniciar** y `keys_clear` deja de ser el paso previo
> obligatorio. La sincronización con la App sigue **sin purgar** el respaldo (la App solo añade o
> actualiza claves), así que una clave borrada continúa en la copia interna como material de rescate:
> volvería a aplicarse tras un reset **solo si para entonces la configuración no tiene ninguna clave
> de dueño**. Si no queda ninguna clave válida, se reinyecta la del proyecto (**rescate
> silencioso**: sin aviso por radio ni por ningún otro medio; se comprueba con
> `/nava admin_ls`).

---

## 🔄 12. Sincronización Bidireccional con la App Oficial de Meshtastic

A partir de **NavaTastic V5**, la interacción entre la App Oficial de Meshtastic y el motor de resiliencia del nodo es **completamente transparente y bidireccional**, con una excepción: las **claves de administración**, donde la prioridad es de la configuración (ver la fila **Claves de Administración** y la regla del 15/09/2026):

| Parámetro en App Oficial | Comportamiento en NavaTastic V5 |
| :--- | :--- |
| **Rol del Dispositivo** | Al cambiar el rol en la App, se sincroniza en el respaldo interno y se actualiza la identidad pública en tiempo real. |
| **OK to MQTT** | Se sincroniza automáticamente hacia el respaldo interno. |
| **Intervalos de Telemetría, NodeInfo y Posición** | Se sincronizan en el respaldo interno evitando reversiones en el boot. |
| **Posición Fija y Coordenadas GPS** | Se persisten de forma atómica y se difunden de inmediato a la malla. |
| **Canales 0 al 7** | Cualquier canal añadido o modificado en la App se respalda en el respaldo interno. |
| **Preset LoRa y Frecuencia** | Se validan y guardan en el bloque de radio física persistente. |
| **PIN Bluetooth** | Si se define en la App, queda protegido contra reinicios. |
| **Lista Negra / Nodos Ignorados** | Se sincroniza con el filtro del router. |
| **Claves de Administración (slots 0-2)** | La App solo **añade o actualiza** claves en el respaldo. Regla 15/09/2026: si la configuración tiene alguna clave de dueño válida, **manda ella** y el respaldo no la pisa; borrar una clave en la App se respeta al reiniciar. |
| **Canal 1 Navadmin** | Protegido como canal de rescate y telemetría de ciclo solar. |

---

## 🎯 Sintaxis de Direccionamiento de Lote (Prefijos)

1. **Por ID**: `/nava !a7c43b2f ping` (solo responde ese nodo).
2. **Por Rol**: `/nava @router status` (solo Routers).
3. **Por Nombre**: `/nava @name:Navarra env` (solo nombres que empiecen por "Navarra").
4. **A Todos**: `/nava env` (todos los repetidores al alcance, secuencial con jitter anti-colisión).

---

# English Condensed Reference: Remote Administration /nava (V5.1)

## 🛡️ Security Levels

- **Open Public Channel (Navadmin / Slot 1)** — READ-ONLY and ONLY for verified administrators (non-admins get total silence): undirected broadcast allows the 7 light 1-line queries (`ping`, `status`, `bat`, `power`, `env`, `channel`, `noise`); directed broadcast (`!ID` or `@group`) adds diagnostics (`stats`, `log`, `ch_ls`, `help`, `peers`, `rxlog`, `afc`, `reset_reason`, `route`, `trace`). Configuration commands (`set_lora`, `set_freq`, `set_preset`, `set_pin`, `set_role`...), the panic protocol (`panic`, `panic_ok`) and destructive commands (`wipe`, `factory_reset`, `full_reset`, `reboot`...) are **forbidden** here — DM or private channel only.
- **Private Fleet Channel (Slots 2-7 with redirected CLI and own key)**: encrypted batch fleet management for uniform commands (`set_pin`, `set_mqtt`, `set_hops`, `db_purge`, `db_clear`, telemetry cadences, `panic`, `panic_ok`...). Per-node physical config (`set_lora`, `set_freq`, `set_preset`, `set_txpower`, chemistry/voltages), mesh-breaking commands (`mute`, `storm`, `txoff`, `txon`) and nuclear resets require `!ID` or DM.
- **Encrypted DM (PKI)**: Critical configuration, LoRa PHY settings, panic protocol, reboots, database management, channels, power, and infrastructure. Admin rights are granted on the first successfully decrypted PKI DM whose public key matches `admin_key[]`.

## Command Glossary (V5.2 Eclipse — 4.3.9)

1. **Diagnostics**: `ping`, `status`, `env`, `channel`, `peers`, `rxlog`, `afc`, `reset_reason`, `noise`, `bat`, `stats` (RAM-only), `log` (RAM-only events), `route !ID`, `trace !ID` (8s RF decoupled), `help [cmd]`.
2. **LoRa PHY & Panic Protocol (V5)**: `set_preset <name>`, `set_lora <bw> <sf> <cr> <freq_mhz> <slot> [txpower]`, `set_freq <freq_mhz> [slot]`, `panic <preset> [mins] [rollback_mins]`, `panic_ok`.
3. **Channels**: `ch_ls`, `ch_set 0 <name> <psk_b64>` (Primary Channel persistent), `ch_set <slot 2-7> <name> <psk_b64>`, `ch_del <slot>`, `ch_url [slot]`, `set_cli_chan <slot>`, `navadmin_mute [on|off]`, `ch_reset`.
4. **Infrastructure**: `ch_mqtt <slot> [up|down|both|off]`, `set_ok_to_mqtt [on|off]`, `set_pos <lat> <lon> [alt]`, `pos_clear`, `set_pos_tx [mins|off]`, `set_nodeinfo_tx [mins|off]`, `set_telem_tx [mins|off]`, `mute [mins|off]`, `set_pin <6_digits>`, `test_tx [secs]`.
5. **Blacklist / Favorites**: `ign add/rm/ls/clear`, `fav add/rm/ls/auto [on|off]` (up to 32 auto-favorites).
6. **Node Config**: `set_name "[Long]" "[Short]" | set_name flush` (persistent override in the node's internal backup) vs natural mode), `set_role [client/mute/router]` (semi-permanent, synchronized with `owner.role`), `set_mqtt [on|off]`, `set_tz`, `set_hops`, `set_txpower`.
7. **Maintenance & Resets (6s Grace Period)**: `db_purge`, `db_clear`, `reboot`, `factory_reset`, `full_reset`, `wipe`.
8. **Power & Solar Resilience**: `set_chem`, `set_vbat`, `set_vwake`, `storm [hours]`, `txoff`, `txon`, `ble [on|off]`, `sleepmsg [on|off]`.
9. **Utilities & Admin Keys**: `bell`, `admin_ls`, `keys_ls`, `keys_clear`, `power`, `msg "[TEXT]"`, `pos`, `nodeinfo`, `sendtel`.
