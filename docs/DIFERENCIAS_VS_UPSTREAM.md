# NavaTastic V5 — Documento Maestro de Arquitectura y Diferencias contra Upstream

> **Base de partida**: [Meshtastic Firmware v2.7.26](https://github.com/meshtastic/firmware) (Commit base `54e0d8d`).  
> **Ámbito de este documento**: Inventario exhaustivo y anatómico de todas las modificaciones, extensiones y módulos exclusivos introducidos en NavaTastic respecto al firmware estándar de Meshtastic.

---

## 🧭 Índice General

1. [Visión General y Filosofía de Diseño](#1-visión-general-y-filosofía-de-diseño)
2. [Inventario Global de Cambios (Métricas Git)](#2-inventario-global-de-cambios-métricas-git)
3. [Subsistema 1: Resiliencia de Energía y Corte Solar (LPCOMP)](#3-subsistema-1-resiliencia-de-energía-y-corte-solar-lpcomp)
4. [Subsistema 2: Base de Datos en RAM y Cero Desgaste de Flash (`NodeDB RAM-Only`)](#4-subsistema-2-base-de-datos-en-ram-y-cero-desgaste-de-flash-nodedb-ram-only)
5. [Subsistema 3: Motor de Resiliencia y Comando Remoto (`NavaCLI` & `NAV6`)](#5-subsistema-3-motor-de-resiliencia-y-comando-remoto-navacli--nav6)
6. [Subsistema 4: Red, Enrutamiento Malla y Sincronización Bidireccional](#6-subsistema-4-red-enrutamiento-malla-y-sincronización-bidireccional)
7. [Subsistema 5: Blindaje de Seguridad, Bluetooth y Canales de Fábrica](#7-subsistema-5-blindaje-de-seguridad-bluetooth-y-canales-de-fábrica)
8. [Subsistema 6: Matriz de Hardware, Divisores ADC y los 12 Entornos](#8-subsistema-6-matriz-de-hardware-divisores-adc-y-los-12-entornos)
9. [Subsistema 7: Scripts de Automatización, Compilación y Distribución](#9-subsistema-7-scripts-de-automatización-compilación-y-distribución)
10. [Tabla Resumen de Archivos Modificados](#10-tabla-resumen-de-archivos-modificados)

---

## 1. Visión General y Filosofía de Diseño

El firmware estándar de Meshtastic está concebido principalmente como una red de mensajería para usuarios móviles con smartphones emparejados por Bluetooth. En escenarios de **repetidores solares fijos instalados en alta montaña** o ubicaciones remotas e inaccesibles, el firmware oficial adolece de problemas críticos:
- Bloqueos de encendido por subida lenta de tensión solar (*brownout*).
- Desgaste prematuro de la memoria flash por escritura constante de nodos de paso.
- Tormentas de tráfico (*NodeInfo storms*) que colapsan la frecuencia LoRa.
- Pérdida de canales y claves de rescate tras un reinicio de configuración (*factory reset*).
- Imposibilidad de gestión remota completa sin desplazamiento físico con ordenador y cable.

**NavaTastic V5** reescribe y complementa la arquitectura de Meshtastic para convertir el hardware Nordic nRF52840 en un repetidor autónomo de grado de supervivencia extrema, administrable al 100% por radio mediante mensajes cifrados o desde la App oficial sin degradar la memoria ni la frecuencia.

---

## 2. Inventario Global de Cambios (Métricas Git)

Comparando el árbol de NavaTastic V5 contra la base limpia `54e0d8d` de Meshtastic:
- **Archivos modificados / añadidos**: 104 archivos.
- **Líneas añadidas**: +15.402 líneas de código C++, headers, configuraciones PlatformIO y scripts.
- **Líneas upstream adaptadas**: 159 líneas.

---

## 3. Subsistema 1: Resiliencia de Energía y Corte Solar (LPCOMP)

### Archivos Modificados:
- `src/Power.cpp`
- `src/platform/nrf52/main-nrf52.cpp`
- `src/power.h`

### Modificaciones y Funcionalidades:
1. **Despertar Solar por Comparador Analógico de Hardware (`LPCOMP`)**:
   - En lugar de depender de la CPU en bucles de sondeo activos, el microcontrolador entra en modo de reposo profundo (*System OFF*, consumo **0.4 mA**).
   - El módulo LPCOMP del nRF52 monitoriza la subida de tensión del panel solar a través del divisor resistivo y solo genera una interrupción de arranque cuando la batería alcanza de forma estable el umbral seguro de recuperación ($\ge 3.77\text{ V}$).
2. **Ciclo Solar de 5 Estados con Avisos Automáticos**:
   - `[Listo]` ($\ge 3.77\text{ V}$): Recuperación completada, operación continua.
   - `[Vivo]` ($3.30\text{ V} - 3.40\text{ V}$): Aviso de límite de carga, ventana de operación de 160s.
   - `[Critico]` ($< 3.30\text{ V}$): Batería crítica, apagado limpio tras despedida por radio.
   - `[Sueño]`: Corte inminente con telemetría de tensión y temperatura antes de desconectar la radio LoRa por bus SPI.
   - `[Boot]`: Diagnóstico diferido a los 2 minutos de uptime exactos tras el arranque, indicando el motivo hardware de reset (`RESETREAS`: `WDT`, `LPCOMP`, `RESETPIN`, `SOFT`, `VBUS`).
3. **Soporte Multiquímica sin Recompilar**:
   - Manejo de 4 perfiles químicos conmutables por comando `/nava set_chem`: LiPo/Li-Ion (3500 mV), NiMH (3400 mV), Sodio Na-Ion (2600 mV) y LiFePO4 (2800 mV).

---

## 4. Subsistema 2: Base de Datos en RAM y Cero Desgaste de Flash (`NodeDB RAM-Only`)

### Archivos Modificados:
- `src/mesh/NodeDB.cpp`
- `src/mesh/NodeDB.h`
- `src/modules/NodeInfoModule.cpp`

### Modificaciones y Funcionalidades:
1. **`NodeDB RAM-Only` (Cero Desgaste de Memoria Flash)**:
   - Activado mediante la macro `USERPREFS_NODEDB_RAM_ONLY=true`.
   - La tabla de nodos de paso, métricas de señal y rutas se almacena exclusivamente en memoria RAM. Se eliminan por completo las escrituras continuas en el sistema de ficheros flash de la partición `/prefs`, protegiendo la vida útil del microcontrolador durante años.
2. **Auto-Favoritos Inteligentes Zero-Hop (hasta 32 Nodos)**:
   - El repetidor detecta los routers vecinos directos (0 saltos) y los incluye automáticamente en la lista de favoritos de retransmisión rápida.
   - Capacidad ampliada en V5 a 32 nodos directos persistidos en `/resilience.bin`.
3. **Escudo Anti-Tormentas de Balizas (*Anti-Storm*)**:
   - En `NodeInfoModule.cpp`, se fuerza `want_response = false` en los anuncios periódicos del nodo.
   - El repetidor publica su presencia y nombre en la red sin solicitar que todos los nodos al alcance respondan al unísono, erradicando los colapsos de frecuencia LoRa en zonas con alta densidad de nodos.

---

## 5. Subsistema 3: Motor de Resiliencia y Comando Remoto (`NavaCLI` & `NAV6`)

### Archivos Modificados:
- `src/modules/NavaCLIModule.cpp` (~3.750 líneas)
- `src/modules/NavaCLIModule.h` (~320 líneas)

### Modificaciones y Funcionalidades:
1. **Estructura Criptográfica de Supervivencia (`struct ResiliencePrefs` / `NAV6`)**:
   - Almacenamiento seguro en `/resilience.bin` (Magic `0x4E415636`):
     - **Bloque A (Capa Física LoRa)**: Preset módem, ancho de banda, factor de dispersión (SF), coding rate (CR), canal y frecuencia de override.
     - **Bloque B (Canal 0 Primario)**: Nombre del canal, clave PSK (1-32 bytes) y longitud.
     - **Bloque C (Protocolo de Pánico)**: Temporizador monotónico, canal prioritario de evacuación y rollback automático.
     - **Bloque D (Nombre Persistente)**: `custom_long_name[40]` y `custom_short_name[5]`.
     - **Slots de Claves Admin**: 3 ranuras públicas de 32 bytes (`keySlot0`, `keySlot1`, `keySlot2`).
     - **Parámetros de Red**: Intervalos de telemetría (12h), posición GPS y nodeinfo.
2. **Catálogo de más de 50 Comandos Remotos por DM Cifrado**:
   - Métricas y Estado: `ping`, `status`, `bat`, `power`, `env`, `noise`, `channel`, `peers`, `rxlog`, `afc`, `stats`, `log`, `reset_reason`.
   - Control de Malla y Rutas: `route`, `trace` (desacoplado a 8s), `fav` (`add`/`rm`/`ls`), `mute` (`add`/`rm`/`ls`), `txoff`, `storm`.
   - Configuración Física y Lógica: `set_preset`, `set_lora`, `set_freq`, `ch_set`, `set_name` (con subcomando `flush`), `set_pos` (con `pos_clear`), `set_role`, `set_telem_tx`, `set_pin`.
   - Operaciones Ejecutivas y Rescate: `reboot`, `factory_reset`, `full_reset`, `wipe`, `panic`, `panic_ok`, `keys_add`, `keys_rm`, `keys_clear`.
3. **Ventana de Gracia Pre-Reboot (6 Segundos)**:
   - Antes de ejecutar un reinicio o reseteo diferido, el módulo espera a que la cola de transmisión de radio esté vacía (`responseQueue.empty()`) y garantiza un margen de 6.000 ms para que el paquete de confirmación (ACK) viaje por la malla antes de reiniciar la CPU.

---

## 6. Subsistema 4: Red, Enrutamiento Malla y Sincronización Bidireccional

### Archivos Modificados:
- `src/mesh/Router.cpp`
- `src/mesh/Router.h`
- `src/mesh/Default.h`
- `src/modules/AdminModule.cpp`

### Modificaciones y Funcionalidades:
1. **Hop-Aware Adaptive Timing & Jitter Aleatorio Real**:
   - En canales de difusión (Navadmin): dispersión aleatoria uniforme de 5 a 13 segundos (`5000 + random(0, 8000)` ms).
   - En respuestas por Mensaje Directo (DM): retardo adaptativo según los saltos recorridos por el paquete de entrada:
     - 0 saltos (vecino directo): `300 ms`
     - 1 salto: `1.500 ms`
     - $\ge 2$ saltos: `3.500 ms`
2. **Desacople Asíncrono de `traceroute`**:
   - Al recibir `/nava trace !ID`, el nodo emite un acuse de texto inmediato al operador y programa el envío de la sonda física de radiofrecuencia a los 8 segundos exactos, evitando colisiones con la respuesta del DM.
3. **Cadencia por Defecto de Telemetría a 12 Horas (43.200s)**:
   - Modificado en `src/mesh/Default.h` (`min_default_telemetry_interval_secs = 43200`).
   - Propagación automática con `/nava set_telem_tx` a todos los sensores: batería, energía (INA219), ambiente (BME280/BMP280), calidad de aire y salud.
4. **Sincronización Bidireccional Transparente de la App Oficial**:
   - En `AdminModule.cpp`, se interceptan los 12 ajustes cotidianos cuando el usuario los modifica desde la App oficial de Meshtastic en su teléfono:
     - Rol del nodo (`syncDeviceRoleFromConfig`)
     - Habilitación MQTT (`syncOkToMqttFromConfig`)
     - Intervalos de Telemetría, NodeInfo y Posición GPS
     - Posición fija y coordenadas manuales
     - Canal 0 Primario y Canales Secundarios 2 al 7
     - Capa Física LoRa (Preset y Frecuencia)
     - PIN de emparejamiento Bluetooth
     - Lista de nodos ignorados (*blacklist*)
     - Claves públicas de administración
   - Todas las sincronizaciones cuentan con guarda `if (navaCLIModule)` y guardado condicional (`if changed`, cero escrituras parásitas).

---

## 7. Subsistema 5: Blindaje de Seguridad, Bluetooth y Canales de Fábrica

### Archivos Modificados:
- `src/mesh/Channels.cpp`
- `userPrefs.jsonc`
- `profiles/*.jsonc`

### Modificaciones y Funcionalidades:
1. **Canal Secundario `Navadmin` Inamovible (Slot 1)**:
   - Inyección forzada en el Slot 1 con nombre `Navadmin`, clave PSK estándar `{ 0x01 }` (`AQ==`), precisión de posición 0 y enlace uplink/downlink desactivado.
   - Actúa como canal de telemetría de supervivencia y rescate público.
2. **PIN Fijo de Bluetooth (`654321`)**:
   - Configurado en modo `FIXED_PIN` para evitar la necesidad de pantallas OLED en nodos de montaña para leer códigos PIN aleatorios al emparejar el teléfono.
3. **Blindaje de Claves de Administración**:
   - Las claves públicas de los dispositivos de mando del operador se almacenan en los slots de resiliencia y se reinyectan automáticamente en la memoria del nodo incluso si este sufre un reseteo de fábrica accidental o intencionado.

---

## 8. Subsistema 6: Matriz de Hardware, Divisores ADC y los 12 Entornos

### Archivos Modificados:
- `variants/nrf52840/navarrico.ini`
- `variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h`
- `variants/nrf52840/diy/nrf52_promicro_diy_tcxo/platformio.ini`
- `variants/nrf52840/seeed_solar_node/variant.h`
- `variants/nrf52840/seeed_solar_node/platformio.ini`
- `variants/nrf52840/heltec_mesh_node_t114/variant.h`
- `variants/nrf52840/heltec_mesh_node_t114/platformio.ini`
- `variants/nrf52840/seeed_xiao_nrf52840_kit/variant.h`
- `profiles/R1IG_*.jsonc` (6 perfiles para Clientes)
- `profiles/R2IG_*.jsonc` (6 perfiles para Routers)

### Matriz de los 12 Entornos Unificados:

| Nombre de Entorno en PlatformIO | Placa Base | Módulo de Radio | Potencia TX | Rol de Fábrica |
| :--- | :--- | :--- | :--- | :--- |
| `navarrico_promicro_e22p_r2ig` | Promicro nRF52840 | Ebyte E22-900M22S (E22P) | 12 dBm | ROUTER |
| `navarrico_faketec_sx1262_r2ig` | Faketec HT-RA62 | Semtech SX1262 | 22 dBm | ROUTER |
| `navarrico_seed_sx1262_r2ig` | Seeed Solar Node P1 | Semtech SX1262 | 22 dBm | ROUTER |
| `navarrico_t114_sx1262_r2ig` | Heltec Mesh Node T114 | Semtech SX1262 | 22 dBm | ROUTER |
| `navarrico_xiao_kit_sx1262_r2ig` | Seeed Xiao BLE Sense Kit | Semtech SX1262 | 22 dBm | ROUTER |
| `navarrico_xiao_e22p_r2ig` | Seeed Xiao BLE + E22P | Ebyte E22-900M22S (E22P) | 12 dBm | ROUTER |
| `navarrico_promicro_e22p_r1ig` | Promicro nRF52840 | Ebyte E22-900M22S (E22P) | 12 dBm | CLIENT |
| `navarrico_faketec_sx1262_r1ig` | Faketec HT-RA62 | Semtech SX1262 | 22 dBm | CLIENT |
| `navarrico_seed_sx1262_r1ig` | Seeed Solar Node P1 | Semtech SX1262 | 22 dBm | CLIENT |
| `navarrico_t114_sx1262_r1ig` | Heltec Mesh Node T114 | Semtech SX1262 | 22 dBm | CLIENT |
| `navarrico_xiao_kit_sx1262_r1ig` | Seeed Xiao BLE Sense Kit | Semtech SX1262 | 22 dBm | CLIENT |
| `navarrico_xiao_e22p_r1ig` | Seeed Xiao BLE + E22P | Ebyte E22-900M22S (E22P) | 12 dBm | CLIENT |

### Calibración del Divisor ADC:
- En las placas DIY Promicro y Faketec se estandariza el divisor de batería resistivo **1 MΩ + 1 MΩ (factor 2.0)** para garantizar lecturas ADC lineales y concordancia milivoltio a milivoltio con los comparadores LPCOMP.

---

## 9. Subsistema 7: Scripts de Automatización, Compilación y Distribución

### Archivos Modificados / Creados:
- `build.ps1`: Script de compilación desatendida de la matriz completa de los 12 firmwares mediante PlatformIO.
- `distribuir.ps1`: Empaquetado automático con cálculo de hashes MD5 hacia `distribucion\` y hacia el Escritorio (`Desktop\NavaTastic V5 4.3.4`) con sufijo canónico `*_V5_4.3.4` (Norma 13).
- `verificar_paridad.ps1`: Validador de paridad byte a byte contra el baseline histórico de compilación.
- `bin/platformio-custom.py`: Script de post-procesamiento de PlatformIO con inyección de metadatos de compilación deterministas (`BUILD_EPOCH`, remapeo de prefijos `-ffile-prefix-map` para evitar fugas de rutas locales).
- `herramientas/generar_pdf.ps1`: Generador automatizado de manuales técnicos en formato PDF con Pandoc y XeLaTeX.
- `herramientas/subir_assets_release.ps1`: Automatización de subida de binarios UF2, OTA ZIPs y PDFs a las Releases de GitHub con codificación UTF-8 estricta.

---

## 10. Tabla Resumen de Archivos Modificados

| Ruta del Archivo | Tipo de Cambio | Propósito Principal |
| :--- | :--- | :--- |
| `src/modules/NavaCLIModule.cpp` | **Nuevo Módulo** (~3.750 líneas) | Motor principal de comandos remotos, persistencia `NAV6`, pánico y auditoría. |
| `src/modules/NavaCLIModule.h` | **Nuevo Header** (~320 líneas) | Declaración de `struct ResiliencePrefs`, firmas de comandos y estados. |
| `src/Power.cpp` | Modificación | Integración del comparador LPCOMP, umbrales solares y 5 estados de energía. |
| `src/platform/nrf52/main-nrf52.cpp` | Modificación | Arranque de bajo consumo, recuperación solar y diagnóstico diferido `[Boot]`. |
| `src/mesh/NodeDB.cpp` / `.h` | Modificación | Operación `RAM-Only` (cero escrituras en flash) y auto-favoritos Zero-Hop (32 nodos). |
| `src/mesh/Router.cpp` | Modificación | Hop-Aware timing adaptativo, modo túnel de pánico y desacople de traceroute. |
| `src/mesh/Default.h` | Modificación | Cadencia por defecto de telemetría a 12 horas (43.200s). |
| `src/modules/AdminModule.cpp` | Modificación | Sincronización bidireccional continua e inteligente desde la App oficial. |
| `src/mesh/Channels.cpp` | Modificación | Creación y blindaje del canal Navadmin (Slot 1, `{0x01}`). |
| `src/modules/NodeInfoModule.cpp` | Modificación | Desactivación de `want_response` (escudo anti-tormentas de balizas). |
| `variants/nrf52840/navarrico.ini` | **Nuevo Archivo** | Definición de los 12 entornos de compilación de hardware. |
| `profiles/*.jsonc` (12 archivos) | **Nuevos Archivos** | Inyección de perfiles de inicio y configuración de radio por placa. |
| `build.ps1` / `distribuir.ps1` | **Nuevos Scripts** | Automatización del ciclo de vida de compilación y distribución versionada V5. |
| `README.md` | Modificación | Portada oficial bilingüe (ES/EN) con enlaces de descarga directa verificados. |
| `docs/Manual_NavaTastic.md` | Modificación | Manual exhaustivo de comandos remotos `/nava` y administración. |
| `docs/Manual_uso_NavaTastic.md` | Modificación | Manual de montaje, requisitos de hardware y protocolo de rescate. |
| `docs/PLAN_DE_TRABAJO.md` | Modificación | Plan maestro y registro de hitos de desarrollo. |
| `docs/cerebro/cerebro.md` | Modificación | Índice global y registro vivo de decisiones de arquitectura. |
| `docs/BITACORA_TECNICA.md` | Modificación | Cuaderno de bitácora técnica de fallos, fixes y lecciones aprendidas (L1-L35). |
