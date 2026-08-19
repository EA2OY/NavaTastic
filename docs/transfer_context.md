# NavaTastic: Context Transfer & Technical Memory (v4.3 → repo unificado)

> **ADENDA 14/08/2026 — REPO UNIFICADO**: este es el documento histórico 4.3 (estructura
> de 24 carpetas **OBSOLETA**); su contenido técnico sigue siendo válido como memoria
> canónica de comportamiento. La estructura física actual es **`C:\NavaTastic Codigo
> completo`**: 12 envs `navarrico_<placa>_<radio>_<rama>` en `variants/nrf52840/
> navarrico.ini`, perfiles en `profiles/`, scripts `build.ps1`/`distribuir.ps1`/
> `verificar_paridad.ps1`, salida a `distribucion\`, paridad 12/12 byte-idéntica
> (Eclipse Edition R2IG + R1IG). Mapa de equivalencias: `Rama 2 Infraestructura\
> Infraestructura General\<carpeta>` → env `navarrico_<placa>_<radio>_r2ig` + perfil
> `R2IG_<Placa>.jsonc`; `Rama 1 Clientes...R1IG` → `..._r1ig` + `R1IG_*.jsonc`.
> Guías vivas: `docs/Guia_para_agente_sobre_NavaTastic.md`, `docs/BITACORA_TECNICA.md`,
> `docs/PLAN_DE_TRABAJO.md`, `docs/PORTING_NUEVO_FORK.md`. `C:\Firmware Navarrico 4.3` SOLO LECTURA.

## 0. Resumen Ejecutivo de Estado
- **Versión del Proyecto**: NavaTastic 4.3.2 V3 (Fork Unificado de Meshtastic 2.7.26).
- **Doble Auditoría Automatizada (16/08/2026)**: **100% PASS (22/22 pruebas superadas)** en banco de pruebas de radio real con Xiaomi Mi 10 y PC (NavaTastic V3 Firmware + MeshNavarra Utility v4.3). Ver [INFORME_DOBLE_AUDITORIA_NAVATASTIC_Y_MESHNAVARRA.md](./INFORME_DOBLE_AUDITORIA_NAVATASTIC_Y_MESHNAVARRA.md) y [12_auditoria_navatastic.md](./cerebro/12_auditoria_navatastic.md).
- **Repositorio GitHub**: `https://github.com/Anticorpos/NavaTastic` (Rama `main` huérfana limpia con 32 binarios y 2 PDFs canónicos).

Este documento es la **memoria técnica canónica** del proyecto **Firmware Navarrico 4.3**, un fork de Meshtastic (v2.7.26 Beta) optimizado para repetidores solares de infraestructura en la malla SFNarrow (Madrid). Alimenta a cualquier agente de IA para retomar el trabajo sin perder contexto. Es el documento **1 de 3** del proyecto:

| # | Documento | Propósito |
|---|---|---|
| 1 | `transfer_context.md` (este) | Estado del proyecto, arquitectura, variantes, parches y reglas |
| 2 | `guia_integracion_navarrico.md` | Cómo replicar/portar los parches (bloques copy-paste + código) |
| 3 | `Manual_NavaTastic.md` | Manual de comandos `/nava` para operadores |
| — | `Manual_uso_NavaTastic_4.2.md` — el `.md` vive en `Contexto y Manuales\` de 4.3 (corregido 11/08: químicas por placa); original en `C:\Firmware Navarrico 4.2\Manual_uso_NavaTastic_4.2.md` | Manual de uso del firmware (para distribuir en PDF) |
| — | `herramientas\plantilla_navatastic.tex` + `herramientas\generar_pdf.ps1` | Sistema de generación de PDF (Pandoc + MiKTeX) |

> Los documentos históricos y de versiones anteriores se conservan en `OLD_CONTEXT/` en **`C:\Firmware Navarrico 4.2\OLD_CONTEXT`** (raíz 4.2; no copiado a 4.3).

---

## 1. Resumen del Proyecto y Separación de Ramas

*   **Rama 1 Clientes (nueva, 12/08)**: `Rama 1 Clientes en Infraestructura/` — nodos de infraestructura que NO son routers (rol CLIENT), sufijos `R1IG`/`R1IP`. Copiada por el operador desde Rama 2; normas vs R2 (ÚNICAS diferencias): **rol CLIENT** (jsonc + fallback NodeDB) y **rol semi-permanente en `/resilience.bin`** (`set_role` sobrevive a factory reset, aplicado en `loadResiliencePrefs` con `installRoleDefaults`). Todo lo demás idéntico a R2 (filtros de guardado, H3, fav auto, canal Navadmin, energía). Detalle completo en `cerebro/11_rama1_plan.md`. Compilación 12/12 SUCCESS desde cero y distribuida (12/08). ⚠️ MAX_PATH en Promicro×2/E22P×2 (error #13, `libdeps_dir`/`build_dir` cortos) y distribución al Desktop vía `HerramientasPropiasIA\distribuir_desktop.ps1`.
*   **Rama 1 (Resiliencia Física y Energía)**: `Rama 1 General/` — **NO migrada a 4.3**; vive en `C:\Firmware Navarrico 4.2\Rama 1 General`. Seguridad de batería, despertares solares, fixes de reset de hardware, integración de sensores y preservación del sueño de bajo consumo. La base de datos es persistente en Flash (comportamiento estándar de Meshtastic) para que los nodos móviles no pierdan listas de pares al apagarse. Su espíritu energético está integrado en ambas ramas (R1 Clientes y R2).
*   **Rama 2 (Protección de Flash e Infraestructura)**: `Rama 2 Infraestructura/`. Hereda toda Rama 1 y añade protección absoluta contra el desgaste de Flash para nodos de montaña. Base de datos selectiva en RAM (`USERPREFS_NODEDB_RAM_ONLY`), auto-favoritos de routers directos en RAM, bypass de límite de saltos y desalojo híbrido seguro. Se subdivide en **dos ramas de infraestructura**: `Infraestructura General/` (sufijo `R2IG`) e `Infraestructura Propia/` (sufijo `R2IP`). **General ACTIVA desde 2026-08-12** (primera ronda: copia del operador desde Propia + diferenciación según `cerebro/09_general_vs_propia.md`: K0=Master Node, 1 sola clave, BT 654321; canal Navadmin homogeneizado en las 12). Propia es la rama de referencia.

## 2. Estructura de Directorios y Variantes (Rama 2)

Todo el código fuente y binarios viven en `C:\Firmware Navarrico 4.3\Rama 2 Infraestructura\`:

*   `Infraestructura General\` -> carpeta de la rama **General** (6 variantes con sufijo `R2IG`). **ACTIVA (12/08)**: copiada por el operador desde Propia y diferenciada (K0=Master Node, 1 clave, BT 654321) según `cerebro/09_general_vs_propia.md`.
*   `Infraestructura Propia\` -> carpeta de la rama **Propia** (6 variantes con sufijo `R2IP`). Es la rama de referencia.
*   Cada rama contiene: 6 carpetas de variante (repo git con código fuente) + `UF2\` (binarios `.uf2` para flasheo) + `OTA\` (zips de actualización inalámbrica).
*   Cada carpeta de variante contiene además `Compilados\` (o `compilados\`) con la última compilación `.hex/.uf2/.zip`.

### Variantes activas (las 6, idénticas entre General y Propia salvo sufijo/carpeta):

| # | Variante | Carpeta | Env PlatformIO | Hardware | Potencia máx | Corte batería | LPCOMP |
|---|---|---|---|---|---|---|---|
| 1 | **Promicro fix** | `Promicro NRF52+E22P NavTastic 2.7.26 R2I[G/P]` | `nrf52_promicro_diy_tcxo` | nRF52840 + E22P / TCXO | 12 dBm | 3.5V | `9_16` ~3.81V |
| 2 | **Faketec PROPIA** | `Faketec NavTastic 2.7.26 R2I[G/P]` | `nrf52_promicro_diy_tcxo` | nRF52840 + HT-RA62 (SX1262 std) | 22 dBm | 3.4V | `9_16` ~3.81V |
| 3 | **Seed Studio P1** | `Seed Solar Node P1 NavTastic 2.7.26 R2I[G/P]` | `seeed_solar_node` | Seeed Solar Node (SX1262) | 22 dBm | 3.4V | `3_8` ~3.8V |
| 4 | **Heltec T114** | `Heltec T114 NavTastic 2.7.26 R2I[G/P]` | `heltec-mesh-node-t114` | Heltec T114 (SX1262) | 22 dBm | 3.4V | `2_8` ~4.05V |
| 5 | **Xiao E22P** | `XiaoKitI2c+E22P NavTastic 2.7.26 R2I[G/P]` | `seeed_xiao_nrf52840_kit_i2c` | Seeed Xiao + E22P | 12 dBm | 3.5V | `3_8` ~3.73V |
| 6 | **Xiao Kit i2c** | `XiaoKitI2c NavTastic 2.7.26 R2I[G/P]` | `seeed_xiao_nrf52840_kit_i2c` | Seeed Xiao + OLED I2C (SX1262) | 22 dBm | 3.4V | `3_8` ~3.73V |

> **Las 6 variantes comparten el mismo NavaCLIModule v4.2.1** y los mismos parches núcleo (verificados idénticos contra la Promicro). Solo difieren en: `set_txpower` (0-12 en E22P, 0-22 en SX1262), el manejo del pin de radio (E22P con `RADIO_POWER_ENABLE_PIN`; SX1262 por SPI/driver) y los valores de variante (potencia, corte, LPCOMP, divisor ADC de fábrica — **no tocar los ADCs de serie**).
>
> **⚠️ XiaoKitI2c y XiaoKitI2c+E22P comparten env** (`seeed_xiao_nrf52840_kit_i2c`); la diferencia E22P/SX1262 vive en el `variant.h` de la carpeta `seeed_xiao_nrf52840_kit` de **cada repo**. Compilar siempre dentro de la carpeta de la variante correspondiente; no mezclar binarios entre ambas carpetas.

## 3. Compilación

Cada variante se compila con su env desde su propio directorio (`C:\Firmware Navarrico 4.3\Rama 2 Infraestructura\Infraestructura <General|Propia>\<Carpeta variante>`):

```bash
# Promicro fix / Faketec PROPIA:
pio run -e nrf52_promicro_diy_tcxo
# Seed Studio P1:
pio run -e seeed_solar_node
# Heltec T114:
pio run -e heltec-mesh-node-t114
# Xiao E22P / Xiao Kit i2c:
pio run -e seeed_xiao_nrf52840_kit_i2c
```

*   Binarios `.uf2` / `.hex` / `.zip` (OTA) se generan en `.pio\build\<env>\`.
*   **ADVERTENCIA**: no definir `build_dir` en `[platformio]` del `platformio.ini` (la Faketec PROPIA lo tenía redirigido a `C:/Users/Jesus/.platformio/build`, ya corregido). Eso hacía que los binarios salieran fuera del proyecto.
*   Dependencias (`libdeps_dir`) pueden apuntar fuera sin problema.

### Norma de distribución de binarios (estructura 4.3)

Tras compilar una variante, copiar los binarios a las carpetas de distribución de su rama (`Rama 2 Infraestructura\Infraestructura <General|Propia>\`):

*   `UF2\` -> el `.uf2` (flasheo físico).
*   `OTA\` -> el `.zip` (actualización inalámbrica).

**Nombre de archivo = nombre de la carpeta de la variante** (incluye variante + rama vía sufijo `R2IG`/`R2IP`), p.ej. `Faketec NavTastic 2.7.26 R2IP.uf2` / `.zip`.

> El script `HerramientasPropiasIA\distribuir_binarios.ps1` automatiza el copiado: deduce la rama del sufijo de la carpeta (`R2IG`→General, `R2IP`→Propia), toma el `.uf2` y `.zip` del `.pio\build\<env>` (fallback `Compilados\`) y los coloca en `UF2\` y `OTA\` de la rama correspondiente con el nombre de la carpeta.
> ```powershell
> .\distribuir_binarios.ps1 -Carpeta "Faketec NavTastic 2.7.26 R2IP" -Env nrf52_promicro_diy_tcxo
> ```

## 4. Parches Aplicados (v4.2.1) — Ubicación y Lógica

### A. Gestión de Energía y LPCOMP
*   **Pre-check de batería** (`src/main.cpp` dentro de `setup()`, tras `power->setup()`): mide 8 lecturas espaciadas 200ms (V3, `USERPREFS_LOW_BATTERY_READINGS_COUNT=8`); si están por debajo de `USERPREFS_LOW_BATTERY_SLEEP_THRESHOLD_MV`, entra en `cpuDeepSleep(portMAX_DELAY)` **antes** de inicializar radio/flash (evita brownout de arranque).
*   **Despertador LPCOMP** (`src/platform/nrf52/main-nrf52.cpp` en `cpuDeepSleep()`): histéresis `NRF_LPCOMP_HYST_ENABLED` (50mV), divisor de tensión mantenido (`ADC_CTRL`) durante el sueño, y umbral dinámico `getActiveLpcompThreshold()`. **NUNCA usar `sd_power_system_off()` para storm** (System OFF no despierta por temporizador).
*   **`delay(3000)` en `cpuDeepSleep()`** (tras `RADIO_POWER_ENABLE_PIN=LOW`, antes de armar el LPCOMP) — **propósito confirmado por el operador (2026-08-11)**: dar tiempo al nodo a **leer el voltaje de la batería** y **programar el LPCOMP con éxito**. Tras apagar la radio, la tensión de batería se recupera del pico de consumo; si se armara el comparador durante ese transitorio, un wake con batería baja (reset externo tipo ATtiny, soft reset) podría re-despertar al instante → bucle despertar/dormir continuo que impide quedarse en System OFF esperando el solar. Sin este delay no se pueden usar resets externos (ATtiny): el nodo se encendería pero no se dormiría bien hasta que la placa solar recupere la batería a un voltaje operativo. Origen: Rama 1 (4.2), documentado también en `C:\Firmware Navarrico 4.2\Contexto\Descartables\antigravity_contexto.md` §7.B. **NO eliminar.**
*   **⚠️⚠️ CRÍTICO 2026-08-11 — `getActiveLpcompThreshold()` NO es universal** (fix aplicado en 4 variantes): los niveles 1-5 están calibrados al **divisor físico 0.5 (1M/1M) del Promicro/Faketec**. La Secuencia 2 asumió divisor uniforme y rompió las variantes con divisor distinto (el default `9_16` pedía voltajes inalcanzables → el nodo **no despertaría nunca por solar** tras System OFF). Fix: `#ifdef` por variante que devuelve `BATTERY_LPCOMP_THRESHOLD` (valor de fábrica):
  - `#ifdef SEEED_SOLAR_NODE` → `3_8` (~3.67V; divisor ADC 3.3 ≈0.303)
  - `#ifdef SEEED_XIAO_NRF52840_KIT` → `3_8` (~3.67V; divisor 1M/510k ≈0.3377, cubre Xiao Kit i2c y Xiao E22P)
  - `#ifdef HELTEC_T114` → `2_8` (~4.04V; divisor 100/490 ≈0.204)
  - Promicro/Faketec (divisor 0.5) conservan el `switch` dinámico.
  - **Divisores reales (fuentes primarias)**: Promicro/Faketec 1M+1M=0.5; Xiao 1M/510k (esquemático Seeed R16=1M, R17=510k); Seed ADC_MULTIPLIER 3.3 de fábrica Meshtastic; T114 100/490 (ADC 4.916, fábrica).
  - **Heltec T114**: Meshtastic desactiva `BATTERY_LPCOMP_INPUT` de fábrica por fuga 2.9mA en System OFF (issue #8801); el fork lo activa a propósito. La fuga solo ocurre dormido.
*   **Aislamiento SPI**: `SPI.end()`/`SPI1.end()` antes de dormir. En la variante E22P además se fuerza `RADIO_POWER_ENABLE_PIN` LOW; en la HT-RA62 la radio se apaga por SPI/driver (RXEN en P0.17, sin pin de alimentación).

### B. Base de Nodos y Flash Wear-Leveling
*   **Desalojo Híbrido** (`NodeDB::getOrCreateMeshNode()`): con 80 nodos llenos, desaloja el favorito no-admin más antiguo; si todos son admins/ignorados devuelve `NULL` (evita el crash del índice 81).
*   **Filtro de Guardado Selectivo** (`NodeDB::saveNodeDatabaseToDisk()`): solo persiste nodo propio, favoritos, ignorados y admins criptográficos (protección Flash).
*   **Filtro DeviceState condicional** (`NodeDB::saveDeviceStateToDisk()`): `memcmp` de identidad (owner, my_node_num, device_id); si solo cambian datos volátiles, omite la escritura física.
*   **Auto-favoritos con estrella** (`NodeDB::checkAndRegisterRAMAutoFavorite()`): routers directos (0 saltos) marcados `is_favorite` y añadidos a `activeDirectRouters`. **Gate (12/08)**: `/nava fav auto [on|off]` (default ON) → OFF bloquea nuevos auto-favoritos y registros en RAM; los existentes se conservan.
*   **Anulación de historial** (`TransmitHistory::saveToDisk()`): retorna `true` inmediatamente (Rama 2). *(Nota: el walkthrough recomendaba envolverlo bajo `USERPREFS_NODEDB_RAM_ONLY`; en la implementación desplegada es `return true;` simple, idéntico en las 6 variantes.)*
*   **Límite de huérfanos** (`AdminModule.cpp`): máx. 10 favoritos remotos sobre nodos nunca oídos por RF.

### C. Enrutamiento y Administración
*   **Inmunidad criptográfica de admin** (`NodeDB.h/.cpp`, `AdminModule.cpp`, `NodeInfoModule.cpp`): bitfield `NODEINFO_BITFIELD_IS_CRYPTOGRAPHICALLY_VERIFIED_ADMIN_MASK (0x08)` asignado solo tras validar PKI; `isAdminNode()` lo lee. Evita suplantación por NodeInfo en texto plano.
*   **Rotación de clave admin aceptada en `updateUser`** (`NodeDB.cpp`, fix 2026-08-10, en las 6 variantes): si el NodeInfo entrante trae una clave pública distinta a la de la DB pero **coincide con una `admin_key` configurada** (`config.security.admin_key[0/1/2]`), se acepta el cambio de clave y se re-marca el nodo como favorito (en vez de `return false` con "Public Key mismatch, dropping NodeInfo"). **Por qué**: resuelve el escenario del nodo de rescate/mando que se filtró con una clave no autorizada en la DB del repetidor antes de cargar la clave de admin correcta. El flujo correcto es: NodeInfo con la clave nueva (que coincide con admin_key) → `updateUser` actualiza la DB → el siguiente DM PKI `/nava` ya se descifra con la clave nueva → se valida como admin. Sin este fix, la DB conserva la clave errónea para siempre y el DM PKI nunca descifra (el router descifra con la clave de la DB, no la del paquete).
*   **Bypass de límite de saltos** (`Router::shouldDecrementHopLimit()`): routers favoritos/directos se retransmiten sin restar hop limit.
*   **Always Reply para admins** (`NodeInfoModule.cpp`): los admins verificados se saltan la supresión de 12h y el throttling.
*   **Anti-tormenta NodeInfo** (`NodeInfoModule::NodeInfoModule()`): `currentGeneration = radioGeneration` al boot para no pedir respuestas masivas.
*   **Reinicio universal nRF52** (`Power::reboot()`): `sd_softdevice_disable()` antes de `NVIC_SystemReset()`.
*   **Comandos de administración remota** (`NavaCLIModule`): módulo headless `/nava` por DM PKI y canal Navadmin con whitelist (ver documento 3).

### D. Secuencia de Comandos Remota 2 (Resiliencia Energética y Diagnóstico)
Ficheros que toca y comandos añadidos (ver documento 2 para el código de integración):
*   `src/mesh/RadioLibInterface.h/.cpp` -> `extern float lastRxFrequencyError`.
*   `src/mesh/SX126xInterface.cpp` y `src/mesh/RF95Interface.cpp` -> captura `lora.getFrequencyError()`.
*   `src/mesh/Router.h` -> `RadioInterface* getInterface()`.
*   `src/power.h` / `src/Power.cpp` -> `updateOcvCurve`, `setChemistryProfile`, `OCV` mutable, `uint8_t currentWakeLevel`.
*   `src/modules/Telemetry/EnvironmentTelemetry.h` -> `sendTelemetry()` en `public:`.
*   `src/platform/nrf52/main-nrf52.cpp` -> `rawResetReason`, `getActiveLpcompThreshold()`, sistema de **storm** (`RTC2_IRQHandler` extern "C" + `timedSystemSleepSeconds()` con RTC2 + LOWPWR + `__WFE`).
*   **Comandos**: `set_chem`, `set_vbat`, `set_vwake`, `bat`, `storm [h]` / `storm test1|test2` (60s/120s), `txoff`, `txon`, `ble`, `rxlog`, `afc`, `reset_reason`, `trace !ID`, `route !ID`, `msg "TXT"`, `bell`, `pos`, `nodeinfo`, `sendtel`, `admin_ls`, `power`, `noise`, `fav auto [on|off]`. **Ronda 12/08**: respuestas fragmentadas por palabra/línea (<=190, sin partir comandos) e interrogación universal (`/nava <cmd> ?` / `<cmd> help`); comandos sin argumento responden estado y opciones (p. ej. `set_chem` muestra la tabla de químicas + AVISO rollback `nrf erase` en los persistentes).

### Químicas de batería soportadas (`set_chem`)

| Química | Corte (`vbat`) | `vwake` | Despertar real | Curva OCV |
|---|---|---|---|---|
| `lipo` | 3500 mV | 3 | ~3.71V | `{4190..3100}` |
| `nimh` | 3400 mV | 3 | ~3.71V | `{4300..3400}` |
| `sodium` | 2600 mV | 3 | ~3.71V | `{3950..2500}` |
| `lifepo4` | 2800 mV | **5** | **~3.30V** | `{3650..2800}` |

### Niveles `set_vwake` (divisor 0.5, VDD 3.3V)

| Nivel | Referencia | Voltaje real | Recomendado para |
|---|---|---|---|
| 1 | `5_16` | ~2.06V | Químicas de muy bajo voltaje |
| 2 | `3_8` | ~2.48V | Despertar temprano |
| 3 | `9_16` | ~3.71V | **LiPo / NiMH / Sodio (default)** |
| 4 | `11_16` | ~4.54V | Baterías de voltaje alto |
| 5 | `4_8` | ~3.30V | **LiFePO4** |

> El nivel 5 usa `NRF_LPCOMP_REF_SUPPLY_4_8` (~3.30V), alcanzable por el LiFePO4 (carga máx 3.65V). El antiguo `13_16` (~5.4V) era inalcanzable con baterías típicas y se eliminó.

## 5. Seguridad por Canal (NavaCLIModule v4.2.1)

*   **Claves de administrador unificadas (fix 2026-08-10)**: las 6 variantes de Rama 2 usan exactamente las mismas dos claves que el Promicro — K0/K1 del operador (**valores no publicados**; en el repo unificado se piden por variables de entorno al compilar los envs Propia, nunca se almacenan) — definidas SOLO en `userPrefs.jsonc` (macros `USERPREFS_USE_ADMIN_KEY_0/1`). Antes: Xiao E22P y Seed P1 usaban la clave de rescate `{0xc7...}` como K0, y Heltec T114 no tenía ninguna clave activa. Verificado: sin claves literales hardcodeadas en código fuente (src/variants/ini); los bloques "hardcode" de `NodeDB.cpp:751-771` y `1458-1484` inyectan desde la macro de userPrefs, no literales. También se limpió el harness `.clusterfuzzlite/router_fuzzer.cpp` (tenía la clave antigua `{0xcd...}`; ahora usa las del Promicro). Único resto histórico: `old\` (variantes deprecated) — no copiado a 4.3; vive en `C:\Firmware Navarrico 4.2\Rama 2 Infraestructura\Codigo Rama 2\old\` (no se compila/distribuye).
*   **Canal Navadmin (Canal 1, PSK pública `{0x01}`)**: whitelist de SOLO LECTURA. Los no-admins NO reciben respuesta (silencio total). Permite: `help`, `ping`, `status`, `env`, `channel`, `peers`, `rxlog`, `afc`, `reset_reason`, `noise`, `bat`, `route !ID`, `trace !ID`. Todo lo demás responde `ERR: SOLO DM SEGURO`.
*   **DM cifrado PKI obligatorio**: para comandos destructivos/configuración/energía. Los DMs deben venir con `mp.pki_encrypted`.
*   **Rate-limit de no-admins**: `std::set<NodeNum> unauthorizedReplied` responde una sola vez `NO AUTORIZADO COMO ADMINISTRADOR` por nodo (evita abuso por aire).
*   **ADVERTENCIA — suplantación de `from` en canal 1**: la PSK `{0x01}` es pública; cualquiera puede fabricar paquetes con `from` de un admin verificado. La whitelist de solo-lectura es la mitigación. Comandos destructivos SIEMPRE por DM PKI.
*   El canal Navadmin se identifica por slot (índice 1), NO por nombre: no reordenar canales.

## 6. Adaptaciones por Hardware (al portar entre variantes)

| Parámetro | E22P (Promicro fix / Xiao E22P) | SX1262 (Faketec PROPIA / Xiao Kit / Seed P1 / T114) |
|---|---|---|
| `SX126X_MAX_POWER` / `HARDWARE_TX_POWER_LIMIT` | **12** | **22** |
| Rango `/nava set_txpower` | 0-12 | 0-22 |
| Pin radio | `RADIO_POWER_ENABLE_PIN` (P0.17 o D5) | sin pin; RXEN por GPIO, sleep por SPI/driver |
| Corte batería | 3500 mV | 3400 mV (T114, Seed P1, Xiao Kit, Faketec) |
| LPCOMP efectivo (fix 11/08) | `9_16` (Promicro) / `3_8` (Xiao E22P) | `9_16` (Faketec) / `3_8` (Xiao Kit, Seed) / `2_8` (T114) |

### Divisores de batería REALES por variante (fuentes primarias, no asumir uniforme)

| Variante | Divisor físico | ADC_MULTIPLIER | LPCOMP efectivo | VBAT despertar |
|---|---|---|---|---|
| Promicro fix | 1M+1M = **0.5** (estándar actual del operador; la referencia antigua 2×10MΩ de `Descartables\report_fix.md` está **deprecated**) | 2.0 | `9_16` (dinámico) | ~3.71V |
| Faketec PROPIA | 1M+1M = **0.5** (mismo estándar) | 2.0 | `9_16` (dinámico) | ~3.71V |
| Xiao Kit i2c | 1M/510k = **0.3377** (esquemático Seeed R16=1M, R17=510k) | 3.0 | `3_8` (fijo, `#ifdef SEEED_XIAO_NRF52840_KIT`) | ~3.67V |
| Xiao E22P | 1M/510k = **0.3377** (mismo variant.h/env) | 3.0 | `3_8` (fijo, mismo `#ifdef`) | ~3.67V |
| Seed Solar P1 | **≈0.303** (ADC_MULTIPLIER 3.3 de fábrica Meshtastic) | 3.3 | `3_8` (fijo, `#ifdef SEEED_SOLAR_NODE`) | ~3.67V |
| Heltec T114 | 100/490 = **0.204** (de fábrica, ADC 4.916) | 4.916 | `2_8` (fijo, `#ifdef HELTEC_T114`) | ~4.04V |

> **⚠️ Nunca asumir divisor 0.5**: `getActiveLpcompThreshold()` (Secuencia 2) quedó calibrado al 0.5 del Promicro y rompió Seed/Xiao/T114. El fix 11/08 usa `#ifdef` por variante. Ver sección 4.A.

> **⚠️ IMPORTANTE al portar `main-nrf52.cpp`**: este fichero es COMÚN a las 6 variantes y el manejo del pin de radio se resuelve con `#ifdef RADIO_POWER_ENABLE_PIN`. Las variantes **E22P** (Promicro fix, Xiao E22P) definen el pin en su `variant.h` (P0.17 / D5) → el bloque se activa (HIGH al boot, LOW en deep sleep y en storm). Las variantes **SX1262** (Faketec, Xiao Kit, Seed P1, T114) NO lo definen → el bloque queda inactivo (radio por SPI/driver). **No eliminar estos bloques `#ifdef`**: si se copia el `main-nrf52.cpp` desde una variante SX1262, los E22P perderían el apagado físico de la radio (~40mA).

> Regla del hardcodeo del canal de rescate (`Channels.cpp`): **E22P → `tx_power = 8`** (el usuario luego puede subir a 12 con `set_txpower`), **SX1262 → `tx_power = 22`**.

> Comportamiento común de ROUTER (todas): `node_info_broadcast_secs = 72h`, `position_broadcast_secs = 72h`, `position_broadcast_smart_enabled = false`, `rebroadcast_mode = LOCAL_ONLY` (vía `USERPREFS_CONFIG_DEVICE_REBROADCAST_MODE`), y `USERPREFS_CONFIG_NODEINFO_BROADCAST_INTERVAL`/`POSITION_BROADCAST_INTERVAL = 259200`.

## 7. Reglas de Convivencia para Agentes

1. **No modifiques código C++ sin recompilar** las variantes afectadas de Rama 2. Al tocar archivos comunes, compilar las 6 (`nrf52_promicro_diy_tcxo`, `seeed_solar_node`, `heltec-mesh-node-t114`, `seeed_xiao_nrf52840_kit_i2c`).
2. **Prioriza el bitfield criptográfico**: usa `nodeDB->isAdminNode(*node)`, nunca la clave pública autodeclarada en texto plano.
3. **Mantén los documentos actualizados**: al añadir comandos o parches, actualiza este `transfer_context.md`, los bloques de `guia_integracion_navarrico.md` y el `Manual_NavaTastic.md`. Revisa la sección 6 (Adaptaciones por Hardware) al portar.
4. **Usa `Throttle`** para rate-limiting temporal (no `millis()` bruto, rollover-unsafe).
5. **No especules sobre causas raíz**: si la evidencia no respalda una clasificación, di "unknown" y lista qué lo desambiguaría.

## 8. Historial de Auditoría (Resumen)

*   **Ronda 2026-08-17 (Diseño y Estudio de Viabilidad Frente F21 — Canales Privados y Gestión Remota)**:
  - **Plan de Trabajo F21**: Documentado en [docs/PLAN_CANAL_PRIVADO_Y_REDIRECCION_NAVACLI.md](PLAN_CANAL_PRIVADO_Y_REDIRECCION_NAVACLI.md) con 16 comandos remotos (slots 2-7, `set_cli_chan`, `navadmin_mute`, `ch_mqtt`, `set_ok_to_mqtt`, `set_pos`, `set_pin`, y `stats`/`log` en RAM).
  - **Auditoría de No-Regresión**: Estudio exhaustivo de dependencias en `Channels.cpp`, `CryptoEngine.cpp`, `NavaCLIModule.cpp/.h`, `Router.cpp` y `MQTT.cpp`.
  - **Regla Cero Desgaste de Flash**: `stats` y `log` operan 100% en memoria RAM (cero escrituras a flash).
  - **Migración Atómica a V4**: Diseñado el paso a `NAVS_RESILIENCE_VERSION` `0x4E415634` ("NAV4") para conservar todos los parámetros previos sin roturas.
*   **Ronda 2026-08-17 (Doble Auditoría 100% PASS + Resiliencia Solar Canónica + [Critico] + Guía Maestra)**:
  - **Doble Auditoría End-to-End**: 26 de 26 pruebas superadas (100% PASS) en banco físico real con Faketec Slave (`!43ca4c27`) y Master (`!8289015a`) + Xiaomi Mi 10 (ADB WiFi `RemoteControlReceiver`).
  - **Fix Canónico de Ciclo Solar**: Eliminadas llamadas a `cpuDeepSleep()` directo antes del init de radio (que dejaban la radio SX1262 a 5-10 mA). Implementado pre-check en 2 niveles: `[Vivo]` (Nivel 1: $[\text{corte}-100\text{mV}, \text{corte})$) y `[Critico]` (Nivel 2: $< \text{corte}-100\text{mV}$). Ambos operan 160s (8 lecturas) y duermen limpiamente con `doDeepSleep()` por bus SPI (**0.4 mA** en Faketec / **1.5 mA** en E22P).
  - **Validación en Fuente de Laboratorio**: Capturado `[Critico]` a 3.24 V $\rightarrow$ 139s $\rightarrow$ `[Sueño]` a 0.4 mA $\rightarrow$ Despertar por comparador LPCOMP a **3.77 V** reales (`ADC 3771 mV`, 0.02% error) y emisión de `[Listo]`.
  - **Documentación & Certificación**: Creada `docs/GUIA_MAESTRA_PROCEDIMIENTO_AUDITORIA_NAVATASTIC.md`, `INFORME_DOBLE_AUDITORIA_NAVATASTIC_Y_MESHNAVARRA.md`, los 4 PDFs oficiales y el `README.md` naturalizado sin jerga interna ni menciones a duty cycle.
  - **Publicación y Assets**: 12/12 variantes compiladas con hash **`fw 2.7.26.f12f833`**; 28 assets publicados en GitHub Release `v4.3.2` y rama `main` actualizada.
*   **Ronda 2026-08-16 (retoma post-V3: builds Propia R2IP #1 + F20 aclarado en banco)**:
  - **Builds Propia R2IP #1**: 4 envs compilados con las claves del operador y PIN propio
    vía variables de entorno en memoria (nada en disco): `navarrico_promicro_e22p_r2ip`,
    `navarrico_xiao_e22p_r2ip`, `navarrico_xiao_kit_sx1262_r2ip`, `navarrico_faketec_sx1262_r2ip`
    (4/4 SUCCESS). Verificación por byte-scan de K0/K1/PIN (uint32 LE) en los UF2.
    Destino: `Desktop\Navatastic V3 Eclipse Infraestructura Propia\Rama 2 Routers\UF2|OTA`
    + `PROMPT_BUILD_PROPIA.md` reutilizable (huecos `<K0>/<K1>/<PIN>/<ENVS>`, sin claves).
    Nada commiteado ni subido; repo intacto.
  - **F20 aclarado en banco (preguntas del operador, confirmado por código)**:
    (1) `keys_clear` borra SOLO la copia persistida de `/resilience.bin`; la clave sigue
    autorizada en la config (`/prefs`) hasta que se resetee — para volver a las claves de
    fábrica: `full_reset` (conserva PKI/bonds, recomendado) / `factory_reset` (PKI nuevo)
    / `wipe` (purga total). (2) **No existe el slot 3**: nanopb `admin_key[3]`
    (`src/mesh/generated/meshtastic/config.pb.h:649-650`); una 4ª clave se descarta en la
    decodificación, no autoriza (los checks solo miran slots 0-2: AdminModule.cpp:100-105,
    NavaCLIModule.cpp:1010-1012) y no se persiste (merge F20 `syncAdminKeysFromConfig`
    solo slots 0-2). (3) Re-persistir una clave = escribirla desde la app (el merge la
    persiste automáticamente); no hay comando `/nava` para ello.
*   **Ronda 2026-08-16 (F20 — claves admin persistidas, FASE R2 del BLOQUE R, banco 7/7)**:
  - **F20**: las claves admin PÚBLICAS del usuario se persisten en `/resilience.bin` (struct
    `ResiliencePrefs` 84→180 B, marcador "NAV3" `0x4E415633`, campos `keySlot1/keySlot2/
    keySlot0Own`) y se re-aplican al boot tras un factory/full reset. **Regla final de slot 0
    (ENMIENDA)**: "slot 0 = estado previo del usuario" — si el dueño tenía su clave en slot 0
    (desautorizó la de fábrica), tras el reset vuelve SU clave (sin ventana de secuestro); si
    nunca la desautorizó, slot 0 = clave del proyecto. La auto-recuperación de NodeDB
    (`local_sum==0`, NodeDB.cpp:1460) queda INTACTA y solo cubre configs vacías (wipe/nrf
    erase) → canal de rescate garantizado cuando no hay estado previo.
  - **Sincronización merge** desde `AdminModule::handleSetConfig` (caso security): slot
    entrante no vacío se persiste (slot 0 = proyecto → limpia la override); slot vacío NUNCA
    borra lo persistido. **Quitar una clave en la app NO la purga**: reaparece tras el próximo
    reset. Purga real = `/nava keys_clear` (cero solo los 3 campos, sin tocar config ni
    reiniciar) o `/nava wipe`/`nrf erase`. Comandos nuevos DM-PKI: `keys_ls` (persistidas en
    base64) y `keys_clear` (ACK diferido).
  - **Adopción**: al migrar un fichero legacy (84 B) se adoptan una sola vez las claves de
    usuario ya presentes en la config (dedupe contra K0/K1 del proyecto; General solo define
    K0). Tras wipe no adopta nada (config solo proyecto).
  - **Fix de banco H1**: el full_reset de R1 borraba el fichero entero → mataba las claves.
    Fix: `navaFullResetKeepKeys()` — full_reset reescribe el fichero conservando SOLO las 3
    claves y reseteando el resto a defaults de perfil. **Banco 7/7 PASS (Faketec)**: full_reset
    conserva claves (S0=propia desplaza proyecto, DM OK sin re-acreditar, Master Node NO
    AUTORIZADO), semi-persistentes resetean (rol client→ROUTER, sleepmsg off→ON),
    re-autorización OK, factory_reset conserva claves, wipe purga, keys_clear correcto,
    regresión OK (status NAVA V3, canal 1 SOLO DM SEGURO). **L33**: los comandos que
    reescriben `/resilience.bin` deben decidir QUÉ campos purgan según su propósito.
*   **Ronda 2026-08-16 (cierre "NavaTastic Eclipse V3" — publicación + presentación)**:
  - **Publicado a GitHub**: release v4.3.2 (26 assets: 12 UF2 + 12 OTA V3 + 2 PDFs) desde
    rama huérfana UN commit; verificado **0 tokens** en el árbol público (scan del clon);
    releases v2.6/v2.6.1 conservados. Nomenclatura oficial: 4.3.0 / 4.3.1 "Eclipse" /
    4.3.2 "Eclipse V3" = etiqueta `NAVA V3`.
  - **Presentación**: README con el cartel del operador como primera imagen; PDFs con el
    cartel HD como **página 1** (ajustado al área de texto, centrado) y títulos
    "NavaTastic Eclipse V3". Lecciones L34-L36 (rutas con espacios en `\includegraphics`,
    ediciones sin stagear, `titlepage` con página en blanco) en BITACORA.
  - **Cierre**: snapshot en `_archivo\`, cerebro 35ª parte + handover §5.5, PROMPT DE
    RETOMA de PLAN reescrito para la próxima sesión.
*   **Ronda 2026-08-15 (4/6 placas verificadas + hallazgos de código)**:
  - **Verificado en banco (operador)**: Xiao Kit i2c (SX1262) y Xiao Kit i2c + E22P (15/08) y
    Faketec HT-RA62 (14/08). **L29**: el pico del E22P en TX aplica a TODAS las placas E22P
    (regla de banco ya documentada en L14/L18); SX1262 = 22 dBm siempre (decisión del operador).
  - **Hallazgos anotados (NO tocados)**: contador de baja asimétrico (`>4` Promicro/Faketec vs
    `>10` Seed/T114/Xiao×2 ≈ 100s vs 220s — **CERRADO (V3, 15/08)**: F18 unificó el contador a
    la macro del perfil `USERPREFS_LOW_BATTERY_READINGS_COUNT=8` (~160s) para las 6 placas,
    monitor runtime Y pre-check); `USERPREFS_LORACONFIG_TX_POWER` = config muerta (L30; el default real es
    Channels.cpp 8/22, idéntico recién flasheado y tras factory reset); Xiao `isVbusIn()` por
    tensión >4200 mV (sin VBUS real).
*   **Ronda 2026-08-15 (documentación pública — README + manuales bilingües)**:
  - **README (ES/EN)**: la clave admin de fábrica se explica como **herramienta de rescate
    integrada**: tras un restablecimiento duro (factory reset accidental, `nrf erase`,
    corrupción) el nodo vuelve con la clave pre-hardcodeada y el operador (poseedor de la
    privada) reentra por DM, restaura y deja el nodo **con la clave de su dueño** — por eso la
    auto-recuperación la re-inyecta si el slot 0 queda vacío. Instrucciones para cambiarla a
    mano con VS Code: `profiles/<RAMA>_<Placa>.jsonc` → sustituir los 32 bytes de
    `USERPREFS_USE_ADMIN_KEY_0` por la clave pública propia (base64→hex) → `pio run -e <env>`
    (aviso: se pierde el canal de rescate del proyecto). Versión README → V2.6.1.
  - **Manuales bilingües ES+EN**: traducción EN completa al final de `Manual_NavaTastic.md`
    y `Manual_uso_NavaTastic_4.2.md` (el ES permanece arriba y es la fuente de verdad);
    PDFs regenerados con `generar_pdf.ps1`.
*   **Ronda 2026-08-15 (auditoría externa Claude — pack 14/08, analizada por agente)**:
  - **Resultado**: 2 hallazgos ya resueltos en el código vivo (§4 instrumentación TEMP F15 retirada; §5 gate `version != 0x4E415653` aplicado en las 3 rutas de carga), 1 **RECHAZADO por medición de laboratorio** (§1: proponía Seed 4084 mV — el Seed probado en banco con el MISMO `3_8` despierta a **~3,8 V** → divisor efectivo ~0,326, no 0,303; la teoría desde `ADC_MULTIPLIER 3.3` falla ~300 mV, L27), 1 **APLICADO** (§2: `power` movido de [Q] a [E] en el help del firmware + "SOLO DM SEGURO" — cosmético), F16c **CERRADO** (`substr(7)` correcto en el código actual), F17 anotado con explicación candidata (PKI estándar: clave pública del destino no aprendida antes del 1er NodeInfo, Router.cpp:669), §6 migración 80B por diseño con riesgo 0.
  - **Dato empírico clave**: el Seed Solar Node P1 despierta a **~3,8 V** (fuente de laboratorio, firmware 4.2 con el mismo `3_8` + `HYST_NOHYST`). El repo actual usa el mismo `3_8` con `HYST_ENABLED` (50 mV) → ~3,85 V. El valor 3670 del aviso [Sueño] es teórico/informativo; re-medir en banco para fijar el exacto (~3800). NO aplicar la teoría del divisor 0.303 (daría 4084, falso).
*   **Ronda 2026-08-15 (V2.6 — ciclo sueño/despertar definitivo, verificado en banco)**:
  - **Comportamiento final** (detalle y referencia histórica Eclipse V1 en `cerebro/04_energia_bateria.md`, sección nueva): [Sueño] tras 8 lecturas reales del monitor (~160s desde V3, contador `low_voltage_counter` SOLO con `!force` — el pre-check ya no lo pre-carga) → **`doDeepSleep`** (preflight + `RadioInterface::sleep()` + GPS + pantalla + System OFF) → ~1 mA. [Vivo] en [corte−100, corte) **opera** ~160s antes de dormir. [Listo] con V ≥ corte. [Boot] a los 2 min con causa RESETREAS + etiqueta `NAVA V3`. LED de estado apagado antes de System OFF (main-nrf52.cpp). Avisos = `ADC + CPU` (chip, reintento BUSY; sin INA/I2C).
  - **Fallos del diseño V2.4 intermedio (corregidos)**: [Vivo] con re-sueño inmediato (dormía a los ~8s sin las 5 lecturas), `cpuDeepSleep` directo sin la cadena de observadores (SX1262 dormidas con radio encendida ~5-10 mA), LED enclavado (~10 mA), contador pre-cargado por las lecturas forzadas del pre-check (dormía ~20s tras arranque con batería baja).
  - **L19 — lecturas forzadas vs contador**: `readPowerStatus(force=true)` (diagnóstico/pre-check) NO debe incrementar el contador de baja batería (pre-carga = salto del filtro anti-falsos-positivos).
  - **L20 — dormir SIEMPRE por `doDeepSleep`**: `cpuDeepSleep` directo se salta `notifyDeepSleep` (radio->sleep, GPS, pantalla); solo es correcto en el pre-check de arranque (radio nunca inicializada).
  - **L21 — GPIO enclavados en System OFF**: apagar el LED de estado (y cualquier GPIO con consumo) ANTES de System OFF; escribir justo al final para que los hilos no lo re-enciendan.
*   **Ronda 2026-08-15 (repo unificado — cierre V2.3 + FRENTES A/B verificados en banco)**:
  - **F15 (rol/sleepmsg envenenados)**: `/resilience.bin` corrupto (FILE_O_WRITE de InternalFS NO trunca) dejaba el nodo en CLIENT con `sleepmsg` OFF "para siempre". Fix: `FSCom.remove()` antes de cada escritura + gates `fileSize != sizeof(prefs) || version != 0x4E415653` + saneado de campos fuera de rango + migración de ficheros legacy (84 B con versión; los de 80 B migran a `sleepMsgs=1`, rol sin fijar). Verificado: rol persiste + sobrevive factory reset; `nrf erase` → rol del perfil + fichero 84 B limpio.
  - **FRENTE A (avisos que no llegaban)**: los mensajes [Sueño]/[Vivo]/[Listo] se encolaban con **`to=0`** (no es broadcast: `isBroadcast()` solo acepta NODENUM_BROADCAST) → se emitían pero NADIE los entregaba. Fix: `enqueueResponse(NODENUM_BROADCAST, 1, ...)` (6 sitios). **Verificado en banco**: ciclo completo [Sueño] 3375 mV → dormido → LPCOMP 3710 mV → [Listo] 3772 mV, todo recibido por el observador en el canal Navadmin.
  - **RF en banco**: el E22P genera picos de corriente al TXear; a 8 dBm los frames llegaban corruptos (110 RX/109 bad). TX 1 dBm = enlace estable (SNR ~12). Regla de banco: probar SIEMPRE a 1 dBm; en campo, potencia del env.
  - **FRENTE B/F16a (admin no respondía tras reboot)**: la acreditación (bitfield 0x08 + favorito) solo vivía en RAM hasta el siguiente save. Fix: `nodeDB->saveToDisk(SEGMENT_NODEDATABASE)` tras acreditar/favoritear (solo si cambia algo). Verificado: DM `/nava ping` → PONG antes y después del reboot, sin re-anuncio del admin. (`updateUser`/H3 ya guardaba al recibir nodeinfo del admin — throttled 1 min; el fix cierra el caso AdminMessage-sin-nodeinfo.)
  - **Retirada de instrumentación TEMP F15** (logs F15DBG, breadcrumbs AD/LR/SR/XX, watchdog 1s, HB-60s, bootDiag, `return 1000`) → `runOnce` de vuelta a 60000 ms; contador de Power a LOG_DEBUG. Build limpio banco SUCCESS (UF2 MD5 `f5cb93cd...`).
  - **Trampas nuevas**: (L13) `to=0` ≠ broadcast — usar siempre NODENUM_BROADCAST; los TX no-broadcast no se ecoplean a la API del propio nodo. (L14) picos de corriente E22P en TX. (L15) `--listen` con redirect pierde el buffer al matar el proceso (usar PYTHONUNBUFFERED=1); `--nodes` falla con cp1252 (PYTHONIOENCODING=utf-8). (L16) flash nRF52 = `pio -t upload --upload-port COMx` (nrfutil + touch 1200bps; NO hay unidad UF2). (L17) el reboot de `--set` con requiresReboot tarda 7s; el de `--reboot` CLI 10s. (L18) `nrf erase` regenera claves → limpiar entradas en peers.
*   **Auditoría Claude Code (agosto 2026)**: detectó y se corrigió el crash de DB llena (índice 81), límite de 10 huérfanos remotos, histéresis LPCOMP, adelanto del chequeo anti-brownout y limpieza de TransmitHistory.
*   **Errores de la auditoría (corregidos en hardware real)**: Claude asumió divisor 0.5 y umbral 9_16 -> bucle de reinicios con divisor real; se fijó 9_16 para divisor 0.5 real y 3.4V de corte. *(Este mismo error de asumir divisor 0.5 se repitió en la Secuencia 2 y rompió Seed/Xiao/T114; ver ronda 11/08 abajo.)*
*   **Ronda 2026-08-11 (fix LPCOMP por divisor real)**: la Secuencia 2 calibrada al divisor 0.5 del Promicro hacía que Seed (`9_16`→~5.5V), Xiao Kit/E22P (`9_16`→~5.5V) y Heltec T114 (`9_16`→~9.1V) **no despertaran por solar** tras System OFF. Fix en `main-nrf52.cpp getActiveLpcompThreshold()` con `#ifdef` por variante devolviendo `BATTERY_LPCOMP_THRESHOLD` de fábrica: `SEEED_SOLAR_NODE`→`3_8`, `SEEED_XIAO_NRF52840_KIT`→`3_8`, `HELTEC_T114`→`2_8`. Promicro/Faketec intactos. Compilado SUCCESS y distribuido UF2/OTA de Propia (Seed, Xiao×2, T114). Referencias: `guia_integracion_navarrico.md` sección B, `cerebro/04_energia_bateria.md`.
*   **Ronda v4.2.1 (endurecimiento NavaCLIModule)**: whitelist canal, normalización antes del filtro, guards `substr()`, `help <comando>`, respuestas en español, `factory_reset` diferido, `ign add` seguro, rate-limit de no-admins, `ble` real, `bat` honesto (sin sag falso), storm RTC2 real.
*   **Portabilidad completa (agosto 2026)**: las 6 variantes de Rama 2 fueron portadas/actualizadas a v4.2.1 (Faketec PROPIA, Xiao Kit i2c, Xiao E22P, Seed Studio P1, Heltec T114), todas compilando con sus envs respectivos. Los archivos núcleo se verificaron idénticos contra la Promicro (referencia canónica).
*   **Manuales PDF**: se generó el sistema de documentación PDF (Pandoc + MiKTeX + plantilla LaTeX `plantilla_navatastic.tex` + script `generar_pdf.ps1`). Ver `Manual_uso_NavaTastic_4.2.md` (uso del firmware) y `Manual_NavaTastic.md` (comandos `/nava`).
*   **Ronda de testeo v4.2.1 (fixes operativos)**:
  - **Storm**: ahora duerme la radio de verdad (`notifyDeepSleep.notifyObservers(NULL)` → `RadioInterface::sleep()`), usa `sd_app_evt_wait()` (no `__WFE__`) con SoftDevice, espera 15s antes de dormir (transmite el ACK antes) y apaga pantalla. Consumo de ~12mA a ~7mA (radio dormida; el resto es el SoftDevice/BLE).
  - **BLE off**: usa el switch nativo de Meshtastic (`config.bluetooth.enabled = false` → `startDisabled()`: advertising parado + tx power -40). No apaga el SoftDevice por completo (riesgo de romper el filesystem), pero reduce consumo.
  - **`route !ID`**: si el nodo no está en la NodeDB, lanza un TraceRoute (no requiere estar en la BD) y responde en vez de dar error.
  - **`admin_ls`**: muestra la clave en base64 (antes solo "Registrada/Vacía").
  - **`msg`**: valida texto vacío y responde si no hay memoria (antes podía quedarse mudo).
  - **`ping`**: añade uptime y piso de ruido; rate-limit de 1 respuesta cada 10s por nodo.
  - **CRÍTICO — deep sleep por batería**: se corrigió `Power::OCV[11]` (quedaba sin inicializar tras la Secuencia 2, comparando contra basura en `readPowerStatus()`). Ahora se inicializa con `OCV_ARRAY` y los wrappers `updateOcvCurve`/`setChemistryProfile` sincronizan ambos arrays (el de `Power` y el de `AnalogBatteryLevel`). **Este bug afectaba a las 6 variantes.**
*   **Ronda 2026-08-10 (rotación de clave admin y unificación de claves)**:
  - **Fix `updateUser` en NodeDB.cpp** (las 6 variantes): aceptar cambio de clave pública en NodeInfo si la nueva clave coincide con una `admin_key` configurada, re-marcando favorito. Resuelve el caso del mando de rescate con clave no autorizada en la DB del repetidor: primero el NodeInfo actualiza la DB, luego el DM PKI `/nava` se descifra y valida. Sin él, la DB conserva la clave errónea para siempre y el DM nunca descifra.
  - **Unificación de claves admin**: las 6 variantes ahora tienen K0/K1 idénticas al Promicro (ver sección 5). Antes Xiao E22P/Seed P1 tenían la clave de rescate `{0xc7...}` y Heltec T114 ninguna.
  - **Auditoría de hardcodeo de claves**: confirmado que no hay claves literales en código (solo `userPrefs.jsonc`); limpiado `.clusterfuzzlite/router_fuzzer.cpp` de la clave antigua `{0xcd...}`. Único resto en `old\` (archivado).
  - **Compilación**: las 6 variantes compiladas con su env real (`-e`) con SUCCESS y binarios `.uf2/.hex` regenerados. Nota: `default_envs = tbeam` en el `platformio.ini` de las 6 variantes falla con `CreateProcess: No such file or directory` (toolchain ESP32, problema preexistente ajeno al código); **se compila siempre con `-e <env real>`**.
*   **Ronda 2026-08-12 (canal Navadmin + fix H3 + fav auto + ayuda)**: homogeneizado el canal 1 Navadmin en los 12 `userPrefs.jsonc`; fix H3 (a)+(a2) en NodeDB (bitfield 0x08 al aceptar clave admin); feature `/nava fav auto` (gate del auto-favoriteo, persistido en `/resilience.bin`); fix estético de fragmentación (corte por palabra/línea); ayuda/consultas (sin argumento → estado+opciones, interrogación `?`/`help`, AVISO `nrf erase` en persistentes). **Compilado 13/13 (12 + Felix) y distribuido (17:09-17:15, MD5 OK)**. ⭐ **Este build fue denominado por el operador "NavaTastic 4.3 Eclipse Edition"** y distribuido a sus colegas para pruebas (motivo: eclipse, 12/08). Es la **referencia de regresión** del proyecto: los UF2/OTA actuales de Rama 2 y `felix puerto venecia\` SON Eclipse Edition (Rama 1 es la iteración siguiente, aún no distribuida).
*   **Ronda 2026-08-12 (2ª, Rama 1 Clientes)**: creada `Rama 1 Clientes en Infraestructura\` (R1IG/R1IP, 12 carpetas, copia del operador desde R2). Únicas diferencias vs R2: rol **CLIENT** (jsonc ×12 + fallback `NodeDB.cpp`) y **rol semi-permanente** en `/resilience.bin` (campo `role`, `set_role` lo guarda, `loadResiliencePrefs` lo aplica con `installRoleDefaults` → sobrevive a factory reset). Resto idéntico (filtros de guardado, H3, fav auto ON, canal Navadmin, energía). 12/12 compilado desde cero (limpiar `.pio` heredado) y distribuido a UF2/OTA R1 (MD5 OK). **MAX_PATH (error #13)**: Promicro×2 y E22P×2 usan `libdeps_dir`+`build_dir` cortos en su `platformio.ini` (binarios fuera del proyecto en `C:/Users/Jesus/.platformio/build/r1xxx/<env>/`). Distribución al Desktop: `HerramientasPropiasIA\distribuir_desktop.ps1` (Rama 1 Clientes / Rama 2 Routers × LIPO/NIMH × UF2/OTA; NIMH solo Faketec + XiaoKitI2c). Normas completas en `cerebro/11_rama1_plan.md`.
- **Ronda 2026-08-10 (reestructuración a v4.3 e infraestructura de herramientas)**:
  - **Reestructuración 4.3**: la estructura migró de `C:\Firmware Navarrico 4.2\Rama 2 Infraestructura\Codigo Rama 2\` a `C:\Firmware Navarrico 4.3\Rama 2 Infraestructura\Infraestructura <General|Propia>\` con 6 variantes por rama (sufijos `R2IG`/`R2IP`). `Infraestructura General` es actualmente copia exacta de `Infraestructura Propia` (verificado: 0 diffs en código, excluyendo `.pio/.git/Compilados`). A futuro General tendrá claves/PSK distintas a Propia.
  - **`HerramientasPropiasIA\distribuir_binarios.ps1` reescrito**: deduce la rama del sufijo de la carpeta (`R2IG`→General, `R2IP`→Propia), lee `.uf2`/`.zip` del `.pio\build\<env>` (fallback `Compilados\`) y copia a `UF2\`/`OTA\` de la rama correspondiente con el nombre de la carpeta de la variante.
  - **`HerramientasPropiasIA\generar_pdf.ps1`** actualizado: apunta por defecto a `Contexto y Manuales` y usa `plantilla_navatastic.tex` local (con fallback sin plantilla). Plantilla versionada a 4.3.
  - **Distribución inicial**: 12 binarios distribuidos (6 por rama) en `UF2\`/`OTA\` con los nombres de carpeta `R2IG`/`R2IP`.
*   El detalle completo de la ronda de auditoría (fallos, fixes y código) se conserva en `OLD_CONTEXT/`.
