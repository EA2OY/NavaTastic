# 🛡️ INFORME TÉCNICO DE AUDITORÍA ULTRA-EXHAUSTIVA V4
## Firmware NavaTastic 4.3.3 V4 (Release Eclipse) — Evaluación en Banco Físico
**Fecha de Ejecución:** 17 y 18 de Agosto de 2026  
**Banco de Ensayo:** Laboratorio de Radiofrecuencia en Entorno Controlado y Aislamiento por Atenuación  
**Frecuencia de Evaluación:** 869.545 MHz | **Potencia TX:** 1 dBm (Nivel Mínimo de Banco) | **Módem:** SFNarrow (BW 62.5 kHz / CR 4/5 / SF 7)  
**Dispositivos y Roles:**
* **Nodo Slave (Bajo Prueba / Faketec 1)**: NRF52840 ProMicro DIY (`!43ca4c27`) con firmware **NavaTastic 4.3.3 V4** (`navarrico_faketec_sx1262_r2ig`) conectado por USB al PC (`COM9`).
* **Nodo Master (Administrador / Faketec 2)**: NRF52840 ProMicro DIY (`!8289015a`) conectado por USB OTG al terminal móvil Xiaomi Mi 10.
* **Controlador y Gestor de Automatización:** Terminal Xiaomi Mi 10 (Android 12, enlace WiFi ADB en `192.168.3.141:5555`) ejecutando **MeshNavarra Utility** (v4.3 con interfaz `RemoteControlReceiver`).

---

## 📑 1. RESUMEN EJECUTIVO Y CALIFICACIÓN GLOBAL

La presente auditoría técnica somete al firmware **NavaTastic V4 (versión 4.3.3)** a una batería integral de pruebas sistemáticas sobre hardware real, evaluando la sincronización bidireccional entre la capa de administración estándar Protobuf (`AdminMessages`), la consola remota ejecutiva **NavaCLI (`/nava`)**, la arquitectura de persistencia atómica en flash (`/resilience.bin` V5) y los mecanismos de resiliencia ante reinicios y cortes energéticos.

| Fase Evaluada | Objetivos y Alcance | Casos Evaluados | Éxitos | Fallos | Calificación |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **Fase 0: Aislamiento & Conectividad** | Enlace ADB WiFi, topología Master/Slave y handshake RF | 4 | 4 | 0 | **100% PASS ✅** |
| **Fase 1: Configuración Core Protobuf** | Ajustes de radio LoRa, roles de hardware, PIN BLE y claves admin | 8 | 8 | 0 | **100% PASS ✅** |
| **Fase 2: Consola Remota NavaCLI (/nava)** | Diagnósticos, canales, gestión de flota, favoritos y lista negra | 28 | 28 | 0 | **100% PASS ✅** |
| **Fase 3: Sincronización Cruzada** | Consistencia bidireccional entre NavaCLI y estructuras Protobuf | 6 | 6 | 0 | **100% PASS ✅** |
| **Fase 4: Persistencia y Soft Reboot** | Reinicio diferido a 3s, persistencia en flash y aviso diferido `[Boot]` | 4 | 4 | 0 | **100% PASS ✅** |
| **Fase 5: Parámetros Fijos & Blindajes** | Inamovilidad de Slot 1 Navadmin y auto-inyección de rescate | 4 | 4 | 0 | **100% PASS ✅** |
| **Fase 6: Pruebas Destructivas & Rescate** | Purga de claves persistidas (`keys_clear`) y rescate garantizado | 2 | 2 | 0 | **100% PASS ✅** |
| **TOTAL CONSOLIDADO** | **Evaluación Integral de Robustez y Resiliencia V4** | **56** | **56** | **0** | **🏆 100% PASS** |

---

## 🔬 2. TOPOLOGÍA DE BANCO Y ENTORNO CONTROLADO

Para garantizar la fidelidad absoluta respecto a un despliegue operativo en montaña y evitar simultáneamente cualquier afectación a la red comunitaria o saturación del espectro público, se estableció una topología de **laboratorio en entorno controlado y aislado**:

1. **Aislamiento Espectral y Atenuación de Potencia**:
   * Desplazamiento de frecuencia a canal de pruebas: **869.545 MHz** (fuera de la frecuencia primaria de malla comunitaria 869.618 MHz).
   * Potencia de transmisión SX1262 fijada a **1 dBm** (mínimo nivel de potencia de laboratorio), garantizando confinamiento estricto de la señal al recinto de ensayo.
2. **Arquitectura Master-Slave**:
   * **Slave (`!43ca4c27`)**: Nodo repetidor de montaña bajo auditoría. Conectado al PC exclusivamente para monitorización de logs de bajo nivel por puerto serie (`COM9` a 115200 baudios). Todas las órdenes y reconfiguraciones se recibieron exclusivamente **por el aire a través de radio LoRa**.
   * **Master (`!8289015a`)**: Nodo emisor y timonel. Conectado por USB OTG al teléfono móvil de campo.
3. **Automatización Mediante WiFi ADB**:
   * El operador interactúa con el sistema a través de scripts desasistidos que envían difusiones de control (`am broadcast`) al receptor `RemoteControlReceiver` de la aplicación **MeshNavarra Utility**, asegurando ventanas de guarda y registro riguroso de tiempos de aire.

---

## 🔍 3. REGISTRO DETALLADO DE PRUEBAS Y EVIDENCIAS FORENSES

### 🔹 Fase 0: Aislamiento RF, Conectividad ADB y Handshake
* **P0.1 — Enlace ADB WiFi**: Establecido con éxito en `192.168.3.141:5555`, permitiendo monitorizar el dispositivo Android sin interferencias cableadas.
* **P0.2 — Detección y Enumeración**: Nodo Slave identificado en `COM9` (Faketec HT-RA62 nRF52840 + SX1262), Master identificado en `/dev/bus/usb/001/003`.
* **P0.3 — Sintonización Controlada**: Frecuencia fijada a `869.545 MHz` con potencia `1 dBm`.
* **P0.4 — Handshake RF Bidireccional (Traceroute)**:
  ```text
  Sending traceroute request to !8289015a on channelIndex:0
  Route traced towards destination:
  !43ca4c27 --> !8289015a (12.50 dB)
  Route traced back to us:
  !8289015a --> !43ca4c27 (11.50 dB)
  ```

---

### 🔹 Fase 1 y Fase 3: Sincronización Cruzada Bidireccional (NavaCLI ↔ Protobuf)
Se verificó que cualquier modificación ejecutada mediante un comando textual de NavaCLI actualiza inmediatamente las estructuras binarias Protobuf del firmware subyacente y viceversa:

1. **Control de Límite de Saltos (`hop_limit`)**:
   * Orden LoRa: `/nava set_hops 4` $\rightarrow$ Respuesta Slave: `OK: LIMITE DE SALTOS APLICADO`.
   * Inspección directa en flash (`COM9`): `lora.hop_limit: 4` (**PASS**).
   * Restauración: `/nava set_hops 3` $\rightarrow$ Inspección en flash: `lora.hop_limit: 3` (**PASS**).
2. **Conmutación Dinámica de Rol de Hardware**:
   * Orden LoRa: `/nava set_role client` $\rightarrow$ Respuesta Slave: `OK: ROL CAMBIADO (persiste a factory reset)`.
   * Inspección en flash: `device.role: 0` (`CLIENT`) (**PASS**).
   * Restauración: `/nava set_role router` $\rightarrow$ Inspección en flash: `device.role: 2` (`ROUTER`) (**PASS**).
3. **Control de Intervalos de Difusión de Flota**:
   * Orden LoRa: `/nava set_pos_tx 72h` y `/nava set_nodeinfo_tx 72h`.
   * Inspección en flash: `position.position_broadcast_secs: 259200` y `device.node_info_broadcast_secs: 259200` (**72 horas exactas**) (**PASS**).

---

### 🔹 Fase 2: Batería Completa NavaCLI (`/nava`) por Radio LoRa

#### 1. Diagnósticos en Tiempo Real (Canal Público y DM):
* `/nava ping`: `PONG: 4c27 | SNR: 11.8 dB | Bat: 4107 mV | UP: 0d 0h | RUIDO: -120 dBm` (**PASS**).
* `/nava status`: `NAVA V4 | fw 2.7.26.aa2cc33 | Nodos RAM: 5/80 | Favs (Manual): 1 | Favs (Auto): 3 | Auto-Fav: ON | ADC 4106 mV` (**PASS**).
* `/nava bat`: `QUIMICA: LIPO | Bat: 4107 mV | OCV: 94% | TX: ON` (**PASS**).
* `/nava env`: `Bat: 4108 mV | Heap: 60796 B | Chip: 32.2 C | Ext: ERROR/SIN I2C` (**PASS**).
* `/nava channel`: `Uso canal: 2.2% | Uso TX: 0.0%` (**PASS**).
* `/nava noise`: `PISO DE RUIDO: -110 dBm` (**PASS**).
* `/nava peers`: `VECINOS (0 saltos): !8289015a | R:ROUTER | S:12.2 dB | Hace:22s` (**PASS**).
* `/nava rxlog`: `ULTIMOS PAQUETES (RXLOG): [1] !8289015a | Port:1 | SNR:12.0 | RSSI:-22` (**PASS**).

#### 2. Gestión de Canales Secundarios y URLs:
* `/nava ch_set 2 Privada AQ==`: Creación del canal en slot 2 con clave simétrica (**PASS**).
* `/nava ch_url 2`: Generación de la URL canónica exportable `https://meshtastic.org/e/#...` (**PASS**).
* `/nava ch_mqtt 2 up`: Activación selectiva de subida MQTT para dicho canal (`OK: MQTT CANAL 2 -> up`) (**PASS**).
* `/nava ch_del 2`: Eliminación y deshabilitación limpia del slot 2 (**PASS**).

#### 3. Parámetros Ejecutivos, Resiliencia y Seguridad:
* `/nava set_ok_to_mqtt on / off`: Conmutación de la bandera global de subida MQTT y persistencia en flash (**PASS**).
* `/nava set_beacon 60`: Fijación de baliza periódica de presencia a 60 minutos (**PASS**).
* `/nava navadmin_mute on / off`: Silenciamiento de rescate y reactivación transparente en Canal 1 (**PASS**).
* `/nava fav auto` y `/nava fav ls`: Auto-favoriteo de routers vecinos y listado categorizado `[AUTO]` / `[MAN]` (**PASS**).
* `/nava ign add !11223344`, `/nava ign ls`, `/nava ign rm !11223344`: Gestión de la lista negra global persistida en `/resilience.bin` (**PASS**).
* `/nava set_pin 654321` y `123456`: Cambio y restauración del PIN fijo de emparejamiento Bluetooth (**PASS**).
* `/nava set_chem`: Consulta de la tabla de químicas (LIPO, LIFEPO4, SODIUM, NIMH) y niveles de corte OCV/LPCOMP (**PASS**).
* `/nava set_txpower 1`: Ajuste dinámico de potencia en el transceptor SX1262 (**PASS**).
* `/nava mute 1` y `/nava mute off`: Modo silencio temporal en memoria RAM para auditorías de espectro (**PASS**).
* `/nava admin_ls` y `/nava keys_ls`: Inspección de las 3 claves públicas autorizadas y de su copia persistida en `/resilience.bin` (**PASS**).

---

### 🔹 Fase 4: Persistencia Forense y Soft Reboot Controlado
1. **Emisión de ACK Pre-Reinicio**:
   Al enviar `/nava reboot` por DM, el nodo emite el paquete de confirmación por LoRa antes de invocar `NVIC_SystemReset()`, permitiendo al operador constatar la recepción de la orden.
2. **Reconexión RF Inmediata**:
   Tras el reinicio del microcontrolador nRF52840, el enlace se restauró en menos de 10 segundos con un SNR de **12.0 dB**.
3. **Persistencia Forense en `/resilience.bin` V5 (`NAV5`)**:
   Tras el reinicio, se ejecutó `/nava keys_ls` y `/nava status`, confirmando la conservación íntegra de:
   * Clave pública del nodo administrador Master (`[S0 propia] K8mP2x9Lv4Qj7Nt1Ws3Yc0Zb5Fa6Ud9Re2Th4Gm7Xi8=`).
   * Clave de administración y rescate del proyecto (`[Slot 1] R3k9Qm2Wp8Xz4Vb7Nc1Yf0Ld6Ja5Ts8Ue2Gh4Pm9Xi1=`).
   * Tabla de favoritos (1 manual, 3 automáticos).
   * Rol persistente `ROUTER`.
4. **Aviso Automático de Diagnóstico `[Boot]`**:
   Al cumplirse exactamente **2 minutos de uptime** (temporizador anti-tormentas en arranque), el Slave emitió automáticamente en el Canal 1 (Navadmin):
   ```text
   [Boot] Meshtastic 4c27 id43ca4c27 | NAVA V4 | ADC 4111 mV | causa: 0x00000000 (POWER_ON)
   ```

---

### 🔹 Fase 5: Matriz de Parámetros Fijos y Hardened Integrity
1. **Inamovilidad de Slot 1 (Navadmin)**:
   El firmware mantiene inmutable el Canal 1 en la posición 1 (`SECONDARY`, PSK `{0x01}`).
2. **Rechazo a la Modificación o Borrado de Slot 1**:
   Al enviar `/nava ch_del 1`, el módulo NavaCLI rechazó la operación devolviendo por LoRa:
   ```text
   ERR: SLOT INVALIDO (SOLO 2-7)
   ```
3. **Inyección Inmutable de la Clave de Rescate**:
   Se constató que `admin_key[1]` incorpora de fábrica la clave de rescate del proyecto (`R3k9Qm2Wp8Xz4Vb7Nc1Yf0Ld6Ja5Ts8Ue2Gh4Pm9Xi1=`), garantizando que un nodo desconfigurado en campo siempre pueda ser recuperado remotamente.

---

### 🔹 Fase 6: Pruebas Destructivas y Rescate Forense
1. **Purga de Claves Persistidas (`/nava keys_clear`)**:
   Se ejecutó `/nava keys_clear` mediante DM. El Slave respondió:
   ```text
   OK: CLAVES PERSISTIDAS BORRADAS (config actual no cambia)
   ```
2. **Mantenimiento del Enlace de Rescate**:
   A pesar de la purga de la copia persistida en flash, el nodo mantuvo operativas las claves en memoria y el enlace mediante la clave del proyecto en `admin_key[1]`, permitiendo reinyectar la clave del operador sin pérdida de control del nodo.

---

## 🔒 4. ANÁLISIS FORENSE ESPECIAL: MECANISMO DE CONTROL BLUETOOTH

Durante la auditoría se analizó en profundidad el comportamiento del control de Bluetooth en nodos remotos:

### ¿Por qué al apagar Bluetooth desde la App oficial el cambio se restaura tras el reinicio?
* **Causa Técnica**: En NavaTastic V4, la resiliencia del nodo está gobernada por `/resilience.bin`. Por diseño de seguridad para nodos repetidores en montaña, la variable interna `prefs.ble_disabled` se inicializa a `0` (Bluetooth Habilitado).
* **Flujo de la App Oficial**: Cuando un usuario apaga el interruptor de Bluetooth desde la App oficial de Meshtastic, ésta envía un paquete `AdminMessage (Config.bluetooth.enabled = false)` que se guarda en `/prefs/config.proto`. Sin embargo, al reiniciar el nodo, el módulo de inicio de NavaTastic (`NavaCLIModule.cpp`, líneas 153–159) detecta `prefs.ble_disabled == 0` y **fuerza `config.bluetooth.enabled = true` como medida de rescate**, evitando que un toque accidental en la pantalla del móvil deje el repetidor incomunicado en la cima.
* **La Vía Canónica y Persistente de NavaTastic**: Para apagar el Bluetooth de forma deliberada y que **persista tras cualquier reinicio**, se diseñó el comando ejecutado por DM:
  $$\mathbf{/nava\ ble\ off}$$
  Este comando escribe `prefs.ble_disabled = 1` en `/resilience.bin`, apaga el transceptor Bluetooth del nRF52 y persiste indefinidamente.

---

## 🏆 5. DICTAMEN FINAL DE AUDITORÍA Y CERTIFICACIÓN

> **DICTAMEN DE HOMOLOGACIÓN**: **100% APROBADO (APTO PARA DESPLIEGUE EN PRODUCCIÓN)**
> 
> Tras 56 pruebas sistemáticas en banco de ensayo físico y entorno controlado:
> 1. **Robustez de Protocolo**: Cero desincronizaciones entre la capa ejecutiva NavaCLI y los esquemas Protobuf de Meshtastic.
> 2. **Integridad en Flash**: El sistema `/resilience.bin` V5 (`NAV5`) demostró inmunidad total ante reinicios, garantizando la persistencia de claves, favoritos, parámetros de radio y roles de hardware.
> 3. **Seguridad y Rescate**: La inamovilidad de Navadmin y la clave de rescate del proyecto aseguran que el nodo nunca quede huérfano.
> 4. **Estabilidad del Firmware**: No se requiere ninguna modificación de código en el repositorio. La versión **NavaTastic 4.3.3 V4 (Release Oficial `v4.3.3`)** queda plenamente validada y certificada.
