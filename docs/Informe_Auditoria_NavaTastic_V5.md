---
title: "Informe de Auditoría - NavaTastic Eclipse V5"
---

# Informe de Auditoría — NavaTastic Eclipse V5 (ALPHA)

**Fecha de la auditoría**: 27-29/08/2026 (tres tandas: con la aplicación, con el mando por USB y
sesión nocturna del Botón del Pánico)
**Versión auditada**: NavaTastic Eclipse V5 (basada en Meshtastic 2.7.26)
**Alcance**: gestión remota (`/nava`), canal de administración, seguridad, persistencia,
Botón del Pánico (evacuación de emergencia de frecuencia) y comandos de configuración.

---

## 1. Resumen ejecutivo

Se han auditado en banco físico **todos los comandos de gestión remota de la versión**, en
varias tandas (aplicación MeshNavarra + mando por USB/SDK) y en escenarios reales (malla con
tráfico ajeno). El resultado general es **positivo**: los comandos funcionan como se espera, la
evacuación de emergencia se verificó de extremo a extremo (aviso, salto coordinado, confirmación
y consolidación) y las correcciones de esta versión se comprobaron en hardware. Los pendientes
quedan listados al final, en lenguaje claro y sin referencias internas.

---

## 2. Cómo se realizó la auditoría (montaje y método)

- **Nodo de mando**: un nodo conectado al PC por USB (herramienta oficial de Meshtastic: CLI y
  SDK Python). En una tanda también se usó un **smartphone con la aplicación MeshNavarra** como
  mando por radio, con su batería de pruebas personalizada.
- **Dos nodos simulando nodos de montaña** (placas Faketec con radio SX1262), configurados en la
  frecuencia **SFNarrow estándar de la red** (869.618 MHz, banda estrecha, potencia reducida),
  con el **canal de administración** (Navadmin) y un **canal privado de flota**.
- **Fases del método**:
  1. *Calentamiento del enlace cifrado*: dos mensajes de prueba a cada nodo (el primero
     establece el cifrado, el segundo ya se procesa).
  2. *Batería de consultas*: comandos ligeros, dirigidos y consultas con `?`.
  3. *Batería de comportamiento y configuración*: con verificación cruzada del estado real.
  4. *Batería del Botón del Pánico*: aviso, salto, confirmación, vuelta atrás y reinicios.
  5. *Persistencia*: reinicios y resets.
- **Nota sobre el entorno**: durante las pruebas los nodos permanecieron en la frecuencia de la
  **malla real**, donde circula tráfico ajeno constante: las respuestas llegan más despacio
  (35-45 s) y las bases de datos se llenan de nodos reales (normal e inofensivo). Para pruebas
  aisladas se usó una frecuencia de laboratorio.

---

## 3. Batería de pruebas por bloques

> Leyenda de la columna *Pendiente*: **—** = sin pendiente · **Revisar** = requiere seguimiento ·
> **Pendiente** = falta probar o decidir.

### Bloque 1 — Enlace y cifrado

| Comando probado | Cómo se probó | Resultado observado | Esperado | Pendiente |
|---|---|---|---|---|
| `/nava ping` (mensaje directo) | Dos mensajes a cada nodo, repetidos tras reinicios | Respuesta (PONG) con señal 11-13 dB en todos los nodos | El primer DM establece el cifrado; el segundo se procesa | — |
| Enlace con la aplicación | Batería de la app MeshNavarra desde el smartphone | Comandos ejecutados; algunos TIMEOUT de la app por plazo corto | Respuestas dentro del plazo | La app debe subir su plazo a 45-50 s |

### Bloque 2 — Consultas y estado (canal de administración)

| Comando probado | Cómo se probó | Resultado observado | Esperado | Pendiente |
|---|---|---|---|---|
| `/nava ping`, `status`, `bat`, `power`, `env`, `channel`, `noise` | Difusión por el canal de administración | Respuestas de una línea en los 7 ligeros | Solo los ligeros responden en difusión | — |
| `/nava peers`, `rxlog`, `afc`, `reset_reason`, `stats`, `log`, `ch_ls`, `help`, `route`, `trace` | Dirigidos con `!ID` por el canal de administración | Respuestas correctas en los 10 | Los diagnósticos responden dirigidos | — |
| `/nava set_telem_tx ?`, `set_lora ?`, `set_preset ?`, `help <comando>` | Consulta con `?` por mensaje directo | Muestran el valor actual y las opciones (24 consultas) | Interrogación universal | — |
| `/nava admin_ls` | Mensaje directo | Lista las claves de administración | — | — |

### Bloque 3 — Canales

| Comando probado | Cómo se probó | Resultado observado | Esperado | Pendiente |
|---|---|---|---|---|
| `/nava ch_ls` | Mensaje directo | Tabla de canales correcta | — | — |
| `/nava ch_set 2 <nombre> <clave>` | Crear canal secundario con clave propia | Canal creado y operativo (tras recrearlo; la primera creación dejó respuestas mudas) | Canal secundario activo | **Revisar**: la primera creación con clave propia puede dejar respuestas mudas hasta recrearlo |
| `/nava ch_del 2` | Borrar el canal | Canal deshabilitado y respaldo limpio | — | — |
| `/nava ch_url 0` / `ch_url 2` | Compartir canales | URL válida de canal | — | — |
| `/nava ch_mqtt 2 off/up` | Compuerta MQTT por canal | Estado actualizado en la lista | — | — |
| `/nava ch_reset` | Restablecer tabla de canales | Canales de fábrica y MQTT del canal público cerrado | — | — |
| `/nava set_cli_chan 2` / `1` | Redirigir la consola | Los comandos pasan al canal privado; vuelta al slot 1 | Consola privada de flota | — |
| `/nava ch_set 0 SFNarrow AQ==` | Escribir el canal principal | **El nombre se conserva exacto** (mayúsculas respetadas — corregido) | El nombre forma parte de la identidad del canal | — |

### Bloque 4 — Comportamiento y lista de nodos

| Comando probado | Cómo se probó | Resultado observado | Esperado | Pendiente |
|---|---|---|---|---|
| `/nava navadmin_mute on/off` | Silenciar/reactivar el canal público | Canal 1 mudo con DM vivo; reactivación correcta | — | — |
| `/nava mute 1` / `off` | Silencio temporal | Ventana de 60 s y cancelación | — | — |
| `/nava ign add/ls/rm/clear` | Lista negra | Añadir, listar, quitar y borrar correctos; persiste | — | — |
| `/nava fav add/rm/ls` y `fav auto on/off` | Favoritos | Altas/bajas correctas; auto-favorito conserva los manuales | — | — |
| `/nava txoff` / `txon` | Apagar/reactivar TX | Pings se silencian y vuelven | TX apagado con RX viva | — |
| `/nava storm test1` / `test2` | Hibernación de prueba | Hiberna y vuelve a operar | — | En placas Heltec (ESP32) desactivado a propósito (específico de nRF52) |
| `/nava test_tx 10/15` | Ráfaga de prueba RF | ACK y balizas; la cadencia real se espacia por el uso del canal | Ráfaga de señal | — |
| `/nava bell` | Tono de alarma | OK | — | — |
| `/nava msg "texto"` | Mensaje a la malla | ACK y difusión oída por el mando | — | — |
| `/nava nodeinfo` / `pos` | Reenvío de presencia/posición | TX emitido | — | — |
| `/nava sendtel` | Telemetría | Rechazado con aviso (sin sensor de clima en el nodo de banco) | Rechazo informativo | Probar con sensor real |
| `/nava keys_ls` | Claves persistidas | Lista las claves de administración | — | — |

### Bloque 5 — Configuración funcional (con verificación del estado real)

| Comando probado | Cómo se probó | Resultado observado | Esperado | Pendiente |
|---|---|---|---|---|
| `/nava set_hops 4` | Cambiar límite de saltos | Aplicado y verificado | — | — |
| `/nava set_ok_to_mqtt on` | Compuerta MQTT | Aplicado y verificado | — | — |
| `/nava set_pin 123456` | PIN Bluetooth | Aplicado y verificado | — | — |
| `/nava set_telem_tx 60` y `3600` | Cadencia de telemetría | Aplicado (los 5 tipos) y verificado | — | — |
| `/nava set_beacon 30` y `1800` | Cadencia de presencia/posición | Aplicado y verificado | — | — |
| `/nava set_pos_tx off/120` | Difusión de posición | **Corregido en esta versión** (antes no funcionaba); aplicado y verificado | — | — |
| `/nava set_rebroadcast known` (y otros modos) | Modo de retransmisión | Aplicado, persiste y se sincroniza | — | — |
| `/nava set_role client` / `router` | Cambio de rol | Conversión en caliente sin pisar otros ajustes | — | — |
| Apagado de posición/presencia desde la app | OFF persistente | Sobrevive al reinicio | — | — |
| `/nava reboot` | Reinicio remoto | ACK y vuelta a responder | — | — |

### Bloque 6 — Botón del Pánico (evacuación de emergencia)

| Comando probado | Cómo se probó | Resultado observado | Esperado | Pendiente |
|---|---|---|---|---|
| `/nava panic medium_fast 2 <min>` | Orden a un nodo (mensaje directo) | Aviso por el canal privado + propagación en cascada al otro nodo | Aviso + cascada | — |
| Salto coordinado (T=0) | Tras la cuenta atrás | **Salto sincronizado** de ambos nodos con todos los parámetros de radio aplicados (modulación, frecuencia y canal alineados con el mando) | Migración completa y coordinada | — |
| `/nava panic long_fast` | Evacuación a LONG_FAST | **Aplica la modulación LONG_FAST real** (antes aplicaba ajustes vacíos — corregido) | Salto correcto a cualquier preset | — |
| `/nava panic sfnarrow` | Vuelta a la frecuencia estándar | Salto correcto al SFNarrow canónico | — | — |
| Comunicación tras el salto | Mensaje de prueba al nodo migrado | **El nodo sigue escuchando y responde** (antes podía quedar sordo — corregido) | El nodo mantiene la recepción | **Revisar**: comportamiento intermitente del pasado, confirmar en más sesiones |
| `/nava panic_ok` (confirmación) | Durante el tiempo de prueba | **El nodo confirma y consolida el cambio** (antes el mensaje se perdía — corregido); pasados los plazos no hay vuelta atrás | Confirmación operativa | Probar desde la aplicación MeshNavarra |
| Vuelta atrás automática | Dejar expirar el plazo sin confirmar | Ambos nodos **vuelven solos** a la configuración de fábrica y restauran canales y ajustes | Rollback automático de seguridad | — |
| Reinicio durante el aviso | Reiniciar un nodo a mitad de la cuenta atrás | **El nodo se reincorpora a la evacuación** y salta con el resto (antes se quedaba fuera — corregido) | El reinicio no deja nodos aislados | El reincorporado salta ~1 min después que el resto (sincronización fina, no bloqueante) |
| Reinicio durante el tiempo de prueba | Reiniciar un nodo ya migrado | Sigue en la nueva frecuencia y renueva su plazo | Margen extra para confirmar | — |

### Bloque 7 — Persistencia y recuperación

| Comando probado | Cómo se probó | Resultado observado | Esperado | Pendiente |
|---|---|---|---|---|
| Ajustes tras reinicio | Cambiar ajustes y reiniciar | Los ajustes se mantienen | Persistencia | — |
| Tras la vuelta atrás del pánico | Comprobar el nodo tras el rollback | Vuelve a la configuración recomendada, **incluido el modo de retransmisión** (antes quedaba en un modo distinto — corregido) | Estado de fábrica limpio | — |
| Instalación sobre otro firmware | Flashear la versión nueva sobre nodos ya usados | El nodo **se configura solo** al primer arranque, sin reset de fábrica manual, respetando las claves existentes | Despliegue automático | — |
| Avisos de estado | Reinicios y ciclos | Avisos de arranque (con causa) y de sueño/despertar recibidos por el canal de administración | — | — |

### Bloque 8 — Seguridad

| Comando probado | Cómo se probó | Resultado observado | Esperado | Pendiente |
|---|---|---|---|---|
| Canal de administración público | Enviar comandos de configuración por el canal público | **Bloqueados** (solo lectura; el pánico y los destructivos exigen mensaje cifrado o canal privado) | Protección del canal público | — |
| Acreditación de administrador | Primer mensaje cifrado de un mando con la clave correcta | Se concede el estado de administrador | Acreditación por clave | — |
| Nodo sin clave de administrador | Mensaje de un nodo no autorizado | **Silencio total** | No revelar información | — |
| `/nava panic` por el canal público | Orden de evacuación por difusión y dirigida | **Silencio** (bloqueado) y rechazo con aviso | El pánico solo por canales seguros | — |
| Comandos destructivos | Confirmación exigida | Se exige confirmación explícita | — | — |

---

## 4. Resultados y pendientes

### Corregido y verificado en esta versión

- La **confirmación de la evacuación** (`panic_ok`) llega durante el tiempo de prueba y consolida
  el cambio en la flota.
- Tras el salto de frecuencia, el nodo **mantiene la recepción** (antes podía quedar sordo).
- El salto a **LONG_FAST** aplica la modulación real (antes aplicaba ajustes vacíos).
- Un nodo que se **reinicia durante el aviso** se reincorpora a la evacuación.
- Tras la vuelta atrás automática, el **modo de retransmisión** vuelve al valor recomendado.
- Los **nombres de canal** se conservan exactos (mayúsculas incluidas).
- La migración de frecuencia escribe **todos** los parámetros de radio a la vez.
- **`set_pos_tx`** vuelve a funcionar (regresión corregida).

### Pendientes y recomendaciones

- **Verificación en hardware real** de las nuevas placas Heltec V3 y V4 (la auditoría se ha
  realizado con las placas nRF52840 de banco).
- Probar la **confirmación de la evacuación desde la aplicación** MeshNavarra.
- La **primera creación de un canal con clave propia** puede dejar respuestas mudas hasta
  recrear el canal (revisar).
- Sincronización fina del nodo reincorporado tras un reinicio durante el aviso (salta algo más
  tarde que el resto; no bloqueante).
- Vigilancia del comportamiento intermitente de recepción tras el salto (corregido, pero por ser
  intermitente conviene confirmarlo en más sesiones).
- **Aplicación MeshNavarra**: subir el plazo de respuesta del canal de administración a 45-50 s.
- En las placas Heltec, la hibernación por tormenta queda desactivada (específica de nRF52); el
  resto de la gestión remota funciona igual.

---

## 5. Estado final de los nodos auditados

Tras la auditoría, los tres nodos quedaron en su estado normal: frecuencia SFNarrow estándar,
potencia reducida, canal de administración y canal privado de flota configurados.

---

*Informe generado siguiendo el formato estándar de auditoría de NavaTastic (bloques de pruebas,
comando probado, resultado observado vs esperado y pendientes). Para la reproducción del banco,
ver la guía de método de auditoría del proyecto.*
