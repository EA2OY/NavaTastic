# PORTING_NUEVO_FORK — Guía Maestra de Portabilidad NavaTastic

Cómo integrar TODAS las mejoras del fork Navarrico/NavaTastic en un fork NUEVO de
Meshtastic donde las líneas hayan cambiado. No depende de números de línea: usa
**anclas de búsqueda** (comentarios `NAVARICO:`, macros `USERPREFS_*`, `FIX_NATIVE_CORE_RESET`,
nombres de funciones) y el **catálogo de bloques con dependencias**.

- **Fork de origen (funcional, paridad 12/12)**: `C:\NavaTastic Codigo completo`
- **Referencia prístina (sin tocar)**: `C:\Users\Jesus\Desktop\firmware` (Meshtastic 2.7.26, base `54e0d8d`)
- **Material base**: `BITACORA_TECNICA.md` (fallos/fixes F1-F12), `Guia_para_agente_sobre_NavaTastic.md`,
  `docs\cerebro\` (cerebro + subnotas 01-12), `docs\transfer_context.md`, `docs\guia_integracion_navarrico.md`.
- **Datos de verificación**: `git diff 54e0d8d0a..HEAD` del repo unificado = **69 ficheros, +6036/−114**
  (todo lo Navarrico frente al prístino).

---

## 0. ÍNDICE

1. [INVENTARIO COMPLETO de añadidos vs prístino (fichero por fichero)](#1-inventario-completo-de-añadidos-vs-prístino)
2. [CATÁLOGO de mejoras por bloque (comportamiento + ficheros + dependencias)](#2-catálogo-de-mejoras-por-bloque)
3. [PROCEDIMIENTO de portado paso a paso](#3-procedimiento-de-portado-paso-a-paso)
4. [CHECKLIST de trampas (todo lo que rompe la paridad o el despertar)](#4-checklist-de-trampas)
5. [Referencias y fuentes de verdad](#5-referencias-y-fuentes-de-verdad)

---

## 1. INVENTARIO COMPLETO de añadidos vs prístino

Leyenda de anclas: **ANCLA** = texto/identificador que localiza el bloque con grep
(`rg "ANCLA"` o Ctrl+F) en el fork nuevo aunque las líneas difieran. Los números de
línea citados son del repo unificado (referencia, no son sagrados).

### 1.1 Mecánica de build del repo único (nueva)

| Fichero | Qué añade | Anclas |
|---|---|---|
| `variants/nrf52840/navarrico.ini` (nuevo, +140) | Los **12 envs** `navarrico_<placa>_<radio>_<rama>`: 6 placas/radios × R2IG/R1IG. Cada env `extends` al env upstream de su placa + 3 opciones custom + macros de radio/rama | `[env:navarrico_promicro_e22p_r2ig]`, `custom_meshtastic_app_env`, `custom_meshtastic_prefs`, `custom_meshtastic_libdeps_map`, `-DNAVARICO_RADIO_E22P`, `-DNAVARICO_RADIO_SX1262`, `-DNAVARICO_RAMA_1` |
| `platformio.ini` (raíz, +17) | `default_envs` = los 12 envs `navarrico_*` (sustituye al `tbeam` original que fallaba por toolchain ESP32) | `default_envs` |
| `bin/platformio-custom.py` (+68) | Overrides **inertes por defecto** (solo actúan con variables de entorno/opciones): `NAVARICO_APP_VERSION` (git SHA), `NAVARICO_BUILD_EPOCH` (día), `NAVARICO_BUILD_TIME/DATE` (`__TIME__`/`__DATE__`), `custom_meshtastic_prefs` (perfil → macros `-D USERPREFS_*`), `custom_meshtastic_app_env` (APP_ENV canónico), `custom_meshtastic_libdeps_map` (→ `-ffile-prefix-map` inyectado a los LIB BUILDERS, ver F4) | `NAVARICO_APP_VERSION`, `NAVARICO_BUILD_EPOCH`, `NAVARICO_BUILD_TIME`, `custom_meshtastic_prefs`, `custom_meshtastic_app_env`, `custom_meshtastic_libdeps_map`, `GetLibBuilders` |
| `profiles/*.jsonc` ×12 (nuevos) + `userPrefs.jsonc` (raíz, +31) | Perfiles 1:1 de los jsonc originales: claves admin, canal Navadmin (slot 1), rol, BT, energía, tiempos. El raíz = perfil por defecto R2IG Promicro | `USERPREFS_USE_ADMIN_KEY_0/1/2`, `USERPREFS_CHANNEL_1_*`, `USERPREFS_CHANNELS_TO_WRITE`, `USERPREFS_CONFIG_DEVICE_ROLE`, `USERPREFS_FIXED_BLUETOOTH`, `USERPREFS_NODEDB_RAM_ONLY`, `USERPREFS_LOW_BATTERY_LOWPOWER_ENABLED`, `USERPREFS_LOW_BATTERY_SLEEP_THRESHOLD_MV`, `USERPREFS_LOW_BATTERY_READINGS_COUNT`, `USERPREFS_CONFIG_DEVICE_REBROADCAST_MODE`, `USERPREFS_CONFIG_POSITION_BROADCAST_INTERVAL`, `USERPREFS_CONFIG_NODEINFO_BROADCAST_INTERVAL`, `USERPREFS_LORACONFIG_CHANNEL_NUM/MODEM_PRESET/TX_POWER`, `USERPREFS_CHANNEL_0_PSK/NAME` |
| `build.ps1`, `distribuir.ps1`, `verificar_paridad.ps1` (nuevos) | Build por env (+`-Paridad`), distribución a `distribucion\Rama 1 Clientes|Rama 2 Routers × LIPO|NIMH × UF2|OTA`, verificación MD5 12/12 | `-Paridad`, `NAVARICO_BUILD_EPOCH`, `NAVARICO_APP_VERSION` |
| `.clusterfuzzlite/router_fuzzer.cpp` (+4) | Claves admin del perfil General (K0 = Master Node, `count=1`) | `admin_key`, `admin_key_count` |
| `variants/nrf52840/diy/nrf52_promicro_diy_tcxo/platformio.ini` (+11) | `lib_ldf_mode = deep+`, `lib_ignore` de libs no usadas, `-DMESHTASTIC_EXCLUDE_MAGNETOMETER=1 -DMESHTASTIC_EXCLUDE_ACCELEROMETER=1` | `lib_ldf_mode = deep+`, `MESHTASTIC_EXCLUDE_MAGNETOMETER` |

### 1.2 Variantes por placa (variant.h fusionados)

| Fichero | Qué añade | Anclas |
|---|---|---|
| `variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h` (+55) | Fusión Promicro (E22P) + Faketec (HT-RA62) en un solo variant.h elegido por `NAVARICO_RADIO_*`: pin de alimentación, RXEN, potencia máx 12/22, curva OCV clamp 3500/3400, LPCOMP, `FIX_NATIVE_CORE_RESET`, salvaguarda `#error` si falta la macro de radio | `NAVARICO_RADIO_E22P`, `RADIO_POWER_ENABLE_PIN`, `SX126X_MAX_POWER`, `HARDWARE_TX_POWER_LIMIT`, `OCV_ARRAY`, `FIX_NATIVE_CORE_RESET`, `BATTERY_LPCOMP_INPUT`, `#error "NAVARICO:` |
| `variants/nrf52840/seeed_xiao_nrf52840_kit/variant.h` (+35) | Fusión Xiao Kit (SX1262) + Xiao E22P (D5): pin D5, potencia 22/12, OCV 3400/3500, LPCOMP común `3_8` (divisor 1M/510k), `#error` salvaguarda | `NAVARICO_RADIO_E22P`, `RADIO_POWER_ENABLE_PIN` (D5), `BATTERY_LPCOMP_THRESHOLD`, `ADC_MULTIPLIER (3)`, `#error "NAVARICO:` |
| `variants/nrf52840/seeed_solar_node/variant.h` (+12) | Bloque Seed: `FIX_NATIVE_CORE_RESET`, LPCOMP `3_8` (divisor ~0.303), `ADC_CTRL`, 22 dBm, OCV clamp 3400 | `FIX_NATIVE_CORE_RESET`, `BATTERY_LPCOMP_THRESHOLD`, `ADC_CTRL`, `OCV_ARRAY` |
| `variants/nrf52840/heltec_mesh_node_t114/variant.h` (+11) | LPCOMP ACTIVADO (Meshtastic lo desactiva por fuga 2.9 mA en System OFF, issue #8801), `2_8` (~4.04V, divisor 100/490), `FIX_NATIVE_CORE_RESET`, 22 dBm, OCV 3400 | `BATTERY_LPCOMP_INPUT`, `BATTERY_LPCOMP_THRESHOLD`, `FIX_NATIVE_CORE_RESET`, `OCV_ARRAY` |
| `variants/nrf52840/seeed_solar_node/platformio.ini` (+10) | `lib_ldf_mode deep+`, `lib_ignore` sensores no montados, EXCLUDE magnet/accel | `lib_ldf_mode = deep+`, `MESHTASTIC_EXCLUDE_` |
| `variants/nrf52840/heltec_mesh_node_t114/platformio.ini` (+3) | EXCLUDE magnet/accel | `MESHTASTIC_EXCLUDE_` |

### 1.3 Código núcleo (lo sustancial)

| Fichero | Qué añade | Anclas |
|---|---|---|
| `src/main.cpp` (+33) | **Pre-check de batería** en `setup()` tras `power->setup()`: 5 lecturas espaciadas 200 ms; si ≥5 por debajo de `USERPREFS_LOW_BATTERY_SLEEP_THRESHOLD_MV` → `cpuDeepSleep(portMAX_DELAY)` ANTES de inicializar radio/flash (anti-brownout de arranque). Además `waitUntilPowerLevelSafe()` (285) espera VDD ≥ umbral+histéresis antes de init | `USERPREFS_LOW_BATTERY_LOWPOWER_ENABLED`, `waitUntilPowerLevelSafe`, `isLowNow`, `cpuDeepSleep(portMAX_DELAY)` |
| `src/platform/nrf52/main-nrf52.cpp` (+235) | El más tocado. (1) `rawResetReason` + `setBleForceDisabled()` (resilience.bin `ble_disabled`). (2) `powerHAL_isPowerLevelSafe()` (100) + **POFCON 2.2 V** (146, último recurso). (3) LPCOMP disarm limpio al boot + `RADIO_POWER_ENABLE_PIN` HIGH en `nrf52Setup()`. (4) **Asserts con `#line` fijo por radio** (451/454 SX1262 vs 456/459 E22P, comentario NAVARICO: ~456). (5) `cpuDeepSleep()`: pin radio LOW, **`delay(3000)`** (leer voltaje + programar LPCOMP; evita bucle wake/sleep), divisor `ADC_CTRL` activo, re-arm LPCOMP completo con histéresis 50 mV. (6) `getActiveLpcompThreshold()` (610): **`#ifdef` por placa** → `BATTERY_LPCOMP_THRESHOLD` en Seed/Xiao×2/T114 (divisor ≠ 0.5); switch dinámico 1-5 solo Promicro/Faketec. (7) **Storm**: `RTC2_IRQHandler` extern "C" (663) + `timedSystemSleepSeconds()` (671): dormir radio de verdad (`notifyDeepSleep`), Wire/SPI end, `setBluetoothEnable(false)`, pantalla deep sleep, pin radio LOW, RTC2 en bloques de 500 s con `sd_app_evt_wait()` y `NVIC_SystemReset()` al final | `rawResetReason`, `setBleForceDisabled`, `powerHAL_isPowerLevelSafe`, `POFCON`, `NAVARICO_LINE_ASSERT1`, `#line`, `delay(3000)`, `getActiveLpcompThreshold`, `SEEED_SOLAR_NODE`, `SEEED_XIAO_NRF52840_KIT`, `HELTEC_T114`, `RTC2_IRQHandler`, `timedSystemSleepSeconds`, `NRF_LPCOMP_HYST_ENABLED`, `ADC_CTRL` |
| `src/Power.cpp` (+92) | (1) `HasBatteryLevel` gana virtuals `updateOcvCurve()/setChemistryProfile()` (179-180); `AnalogBatteryLevel` las implementa (532-575). (2) **`OCV[11]` con inicializador** `{OCV_ARRAY}` (568) — sin él, deep sleep por batería NO funciona (comparaba contra basura). (3) `reboot()` con `#ifdef FIX_NATIVE_CORE_RESET` → `sd_softdevice_disable()` + `NVIC_SystemReset()` (804). (4) `uint8_t currentWakeLevel = 3;` (1981) + wrappers `Power::updateOcvCurve/setChemistryProfile` que sincronizan TAMBIÉN el array de Power (1983-2012) | `updateOcvCurve`, `setChemistryProfile`, `OCV[NUM_OCV_POINTS]`, `FIX_NATIVE_CORE_RESET`, `currentWakeLevel`, `sd_softdevice_disable` |
| `src/power.h` (+6) | `OCV` mutable, wrappers públicos, `extern uint8_t currentWakeLevel;` | `uint16_t OCV[11]`, `updateOcvCurve`, `extern uint8_t currentWakeLevel` |
| `src/mesh/Channels.cpp` (+23) | Bajo `#ifdef FIX_NATIVE_CORE_RESET`: `initDefaultLoraConfig()` fuerza **SFNarrow** (EU_868, `use_preset=false`, BW 62, SF7, CR5, canal 4, `override_frequency=869.618f`, **TX base 8 E22P / 22 SX1262** por `NAVARICO_RADIO_*`); `initDefaultChannel()` fuerza canal 0 = "SFNarrow" PSK `{0x01}` | `FIX_NATIVE_CORE_RESET`, `override_frequency`, `NAVARICO_RADIO_E22P`, `strcpy(channelSettings.name, "SFNarrow")` |
| `src/mesh/NodeDB.cpp` (+332) | El otro grande. (1) Claves desde macros: `userprefs_admin_key_0/1/2[]` (80-87). (2) Callback `meshtastic_NodeDatabase_callback` FILTRADO: solo serializa propio + favorito + ignorado + `KEY_MANUALLY_VERIFIED` (171-190). (3) **`checkAndRegisterRAMAutoFavorite()`** (196-220): router directo 0-hop → estrella + `router->activeDirectRouters`; gate global `navaAutoFavoriteEnabled` (por `/nava fav auto`). (4) `isAdminNode()` (222, bitfield), `countOrphanFavorites()` (228). (5) `resetRadioConfig`/`installDefaultConfig` con `FIX_NATIVE_CORE_RESET`: rol desde `USERPREFS_CONFIG_DEVICE_ROLE` (698-706), claves "hardcode" desde macros (750-770). (6) **Auto-recuperación de claves**: en `loadFromDisk()` si `local_sum==0` re-inyecta K0/K1 de fábrica (1460-1488) — el nodo nunca queda sin admin. (7) `saveDeviceStateToDisk()` con `memcmp` de identidad (owner/node_num/device_id) — omite escritura si solo cambian datos volátiles (1658-1710) + guardas de nivel de energía seguro en los save*. (8) **`saveNodeDatabaseToDisk()` FILTRADO**: solo propio, favoritos, admins, routers directos de `activeDirectRouters`, ignorados (1717-1770). (9) `saveToDisk()` con guarda de potencia insegura (1775-1860). (10) **Fix H3 (a)+(a2)** en `updateUser()` (~2096-2130): si la nueva clave == admin_key → acepta + re-favorito + **bitfield 0x08 directo** (rompe el círculo clave-stale → PKI no descifra). (11) `checkAndRegisterRAMAutoFavorite()` llamada en `updateUser()` y `updateFrom()`. (12) **Desalojo híbrido** en `getOrCreateMeshNode()` (2380-2400): con 80 llenos, desaloja favorito no-admin más antiguo; si todo protegido → NULL (evita crash índice 81). (13) Fallback de rol a 0 líneas netas (702-703, ver F7). | `userprefs_admin_key_0`, `checkAndRegisterRAMAutoFavorite`, `navaAutoFavoriteEnabled`, `isAdminNode`, `countOrphanFavorites`, `NODEINFO_BITFIELD_IS_KEY_MANUALLY_VERIFIED_MASK`, `local_sum`, `loadDefaultAdminKeys`, `saveNodeDatabaseToDisk`, `saveDeviceStateToDisk`, `newKeyIsAdmin`, `NODEINFO_BITFIELD_IS_CRYPTOGRAPHICALLY_VERIFIED_ADMIN_MASK`, `USERPREFS_CONFIG_DEVICE_ROLE`, `FIX_NATIVE_CORE_RESET` |
| `src/mesh/NodeDB.h` (+9) | `getOrCreateMeshNode`, `isAdminNode`, `countOrphanFavorites`, `checkAndRegisterRAMAutoFavorite` públicos; **`NODEINFO_BITFIELD_IS_CRYPTOGRAPHICALLY_VERIFIED_ADMIN_MASK (1<<3)`** (383-387); `extern bool navaAutoFavoriteEnabled;` | `NODEINFO_BITFIELD_IS_CRYPTOGRAPHICALLY_VERIFIED_ADMIN_SHIFT`, `countOrphanFavorites`, `navaAutoFavoriteEnabled` |
| `src/mesh/Router.cpp` (+9) | **Bypass de hop limit**: en `shouldDecrementHopLimit()` Check 1 pasa a `is_favorite` OR `activeDirectRouters` (105-119) + `#include <algorithm>`. Nota: con rol CLIENT (Rama 1) devuelve `true` siempre → bypass inactivo sin tocar código | `shouldDecrementHopLimit`, `activeDirectRouters` |
| `src/mesh/Router.h` (+3) | `RadioInterface* getInterface()` (37, para `/nava noise`/`afc`) y `std::vector<NodeNum> activeDirectRouters` (39) | `getInterface`, `activeDirectRouters` |
| `src/mesh/TransmitHistory.cpp` (+1) | `saveToDisk()` → `return true;` inmediato (bypass de escritura de historial en Flash) | `return true; // Bypass saving duplicate packet history` |
| `src/mesh/RadioLibInterface.h/.cpp` | `extern float lastRxFrequencyError;` + definición `= 0.0f` (para `/nava afc`) | `lastRxFrequencyError` |
| `src/mesh/SX126xInterface.cpp` / `RF95Interface.cpp` | En `addReceiveMetadata()`: `lastRxFrequencyError = lora.getFrequencyError();` | `lastRxFrequencyError =` |
| `src/mesh/RadioInterface.cpp` (+9) | `limitPower()`: clamp de `config.lora.tx_power` y `loraMaxPower` a `HARDWARE_TX_POWER_LIMIT` (909-918) | `HARDWARE_TX_POWER_LIMIT` |
| `src/modules/AdminModule.cpp` (+32) | (1) Tras validar PKI de un AdminMessage → **bitfield 0x08** + auto-favorito del nodo (105-117) + **F16a: `saveToDisk(SEGMENT_NODEDATABASE)` si cambió algo** (acreditación sobrevive a reboot). (2) **Fix #10873**: `disableBluetooth()` movido DESPUÉS de `factoryReset()`/`resetNodes()` en los 3 casos (config/device/nodedb reset, 298-323) — sin él, factory reset se colgaba (issue #10851). (3) `set_favorite_node_tag`: si no existe y `countOrphanFavorites() >= 10` → rechaza; si no, `getOrCreateMeshNode()` (367-377) | `NODEINFO_BITFIELD_IS_CRYPTOGRAPHICALLY_VERIFIED_ADMIN_MASK`, `disableBluetooth()`, `countOrphanFavorites`, `getOrCreateMeshNode`, `SEGMENT_NODEDATABASE` |
| `src/modules/NodeInfoModule.cpp` (+76) | (1) Inmunidad de admins: `isAdminNode()` se salta supresión 12h y throttling (40, 147). (2) `allocReply()` con `Throttle::isWithinTimespanMs` (<12h / 60 s shorter) (141-155). (3) **Anti-tormenta**: `currentGeneration = radioGeneration;` en el constructor (228-231) — no pide respuestas masivas al boot | `currentGeneration = radioGeneration`, `isAdminNode`, `Throttle::isWithinTimespanMs` |
| `src/modules/Telemetry/EnvironmentTelemetry.h/.cpp` | `sendTelemetry()` y `getEnvironmentTelemetry()` pasan a `public:` + global `environmentTelemetryModule` (para `/nava sendtel`/`env`) | `environmentTelemetryModule`, `sendTelemetry(NodeNum dest` |
| `src/modules/NavaCLIModule.h` (nuevo, +99) | Clase `NavaCLIModule` (SinglePortModule + OSThread). `ResiliencePrefs` (26-35): `chemistry`, `vbat_cutoff`, `vwake_level`, `tx_off`, `ble_disabled`, `auto_fav` y (Rama 1) `role` (0xFF=sin fijar). `unauthorizedReplied` (rate-limit no-admins), `helpForCommand()`, `usageAndState()`. Persistencia `/resilience.bin` | `ResiliencePrefs`, `unauthorizedReplied`, `usageAndState`, `loadResiliencePrefs`, `saveResiliencePrefs` |
| `src/modules/NavaCLIModule.cpp` (nuevo, +1373) | El módulo `/nava` completo: `wantPacket()` (DM o canal 1 + olfateo telemetría), `handleReceived()` (auth: DM exige `pki_encrypted`; no-admin → 1 sola vez `NO AUTORIZADO...`; **canal 1 whitelist + rate-limit 30 s por nodo** con `lastChannel1Cmd`, 224-229), `executeCommand()` (~45 comandos), `helpForCommand`, `usageAndState`, `enqueueResponse` (fragmentos ≤190 chars cortando por palabra/línea), `runOnce()` (drena cola + diferidos: txoff/reboot/factory_reset/storm 15 s). Comandos: `help`, `ping`, `status`, `env`, `channel`, `peers`, `rxlog`, `afc`, `reset_reason`, `noise`, `bat`, `route !ID`, `trace !ID` (canal 1); `set_chem` (**lifepo4 rechazado con `#ifdef` en Seed/Xiao×2/T114**), `set_vbat`, `set_vwake`, `set_txpower` (0-12/0-22 por `NAVARICO_RADIO_*`), `set_hops`, `set_role` (**semi-permanente en AMBAS ramas**: guarda en resilience.bin + `installRoleDefaults` al boot), `set_mqtt`, `set_tz`, `set_name`, `fav add/rm/ls` + **`fav auto [on|off]`**, `ign add/rm/ls`, `db_purge`, `db_clear`, `reboot`, `factory_reset`, `storm [1-720]`/`test1`/`test2`, `txoff`/`txon`, `ble`, `msg`, `bell`, `pos`, `nodeinfo`, `sendtel`, `admin_ls`, `power`, `sleepmsg [on|off]`, `noise`; interrogación universal (`<cmd> ?`/`help`) y sin-argumento → estado+opciones; AVISO `nrf erase` en los persistentes. **CRÍTICO**: los avisos [Sueño]/[Vivo]/[Listo] se encolan con **`NODENUM_BROADCAST`** (nunca `to=0`: no es broadcast y nadie los entrega); `sleepmsg` parsea con `substr(9)`; `/resilience.bin` se recrea antes de cada escritura (FILE_O_WRITE no trunca) + gates versión/tamaño | `navaAutoFavoriteEnabled`, `lastChannel1Cmd`, `LIFEPO4 NO COMPATIBLE`, `usageAndState`, `helpForCommand`, `enqueueResponse`, `set_role`, `fav auto`, `sleepmsg`, `NODENUM_BROADCAST`, `ResiliencePrefs`, `NAVARICO_RAMA_1`, `NAVARICO_RADIO_E22P` |
| `src/modules/Modules.cpp` (+2) | Registro del módulo: `#include "modules/NavaCLIModule.h"` + `navaCLIModule = new NavaCLIModule();` en `setupModules()` | `NavaCLIModule`, `navaCLIModule = new` |

---

## 2. CATÁLOGO de mejoras por bloque

### BLOQUE E1 — Energía y resiliencia física (Rama 1)
| Mejora | Comportamiento | Ficheros | Depende de |
|---|---|---|---|
| Anti-brownout de arranque | Espera VDD ≥ umbral+histéresis antes de init (evita boot en rango inestable del "brownout de ascenso solar") | `main.cpp` (`waitUntilPowerLevelSafe`), `main-nrf52.cpp` (`powerHAL_isPowerLevelSafe`, POFCON 2.2 V) | — |
| Pre-check de batería | 5 lecturas; si baja → `cpuDeepSleep` antes de encender radio (evita brownout por pico de arranque) | `main.cpp` | Perfil: `USERPREFS_LOW_BATTERY_*` |
| LPCOMP con histéresis + `delay(3000)` | Despertar por tensión solar con histéresis 50 mV; el delay da tiempo a leer V y programar el comparador (evita bucle wake/sleep) | `main-nrf52.cpp` `cpuDeepSleep` | `variant.h`: `BATTERY_LPCOMP_INPUT/THRESHOLD`, `ADC_CTRL`; NO eliminar `delay(3000)` |
| Umbral LPCOMP por divisor real | `getActiveLpcompThreshold()`: niveles 1-5 SOLO en divisor 0.5 (Promicro/Faketec); `#ifdef` por placa → umbral de fábrica en Seed/Xiao×2/T114 | `main-nrf52.cpp` | Macros de placa del env; `variant.h`; **CRÍTICO: sin esto el nodo no despierta por solar** |
| Storm (hibernación RTC2) | Radio dormida de verdad, BLE off, RTC2 en bloques, reset al final | `main-nrf52.cpp` (`RTC2_IRQHandler`, `timedSystemSleepSeconds`) | `sleep.h`, `notifyDeepSleep`, `setBluetoothEnable`, `RADIO_POWER_ENABLE_PIN`; el ACK sale 15 s antes (NavaCLI `stormPending`) |
| Químicas de batería | `set_chem lipo/nimh/sodium/lifepo4` ajusta corte+OCV+vwake; curvas OCV en RAM | `Power.cpp` + `power.h` | `OCV_ARRAY` del variant.h; `currentWakeLevel`; **OCV[11] debe inicializarse** |
| Reboot limpio | `sd_softdevice_disable()` antes de `NVIC_SystemReset()` | `Power.cpp` | `FIX_NATIVE_CORE_RESET` en variant.h |
| Apagado físico de radio E22P | `RADIO_POWER_ENABLE_PIN` HIGH al boot, LOW en deep sleep/storm (~40 mA) | `main-nrf52.cpp` (`#ifdef RADIO_POWER_ENABLE_PIN` ×3) | `variant.h` E22P; **conservar los `#ifdef` al portar** |

### BLOQUE E2 — Protección de Flash e infraestructura (Rama 2)
| Mejora | Comportamiento | Ficheros | Depende de |
|---|---|---|---|
| NodeDB RAM-only | `USERPREFS_NODEDB_RAM_ONLY=true` + guardados mínimos | Perfil + `NodeDB.cpp` | Perfil |
| Filtro de guardado selectivo | Solo persiste propio/favoritos/ignorados/admins/backbone | `NodeDB.cpp` `saveNodeDatabaseToDisk` + callback filtrado | `Router::activeDirectRouters` (criterio 4) |
| DeviceState condicional | `memcmp` de identidad → omite escritura si solo cambian datos volátiles | `NodeDB.cpp` | — |
| Desalojo híbrido | 80 llenos → desaloja favorito no-admin más antiguo; todo protegido → NULL (no crash 81) | `NodeDB.cpp` `getOrCreateMeshNode` | — |
| Auto-favoritos con estrella | Router directo 0-hop → `is_favorite` + `activeDirectRouters`; **gate `/nava fav auto`** (persiste en resilience.bin) | `NodeDB.cpp`, `NodeDB.h` (`navaAutoFavoriteEnabled`), `Router.h` | BLOQUE N (gate) + `Router::activeDirectRouters` |
| Bypass de hop limit | Routers favoritos/directos se retransmiten sin restar hop | `Router.cpp` | E2 auto-fav; queda inactivo con rol CLIENT (R1) |
| Historial sin Flash | `TransmitHistory::saveToDisk()` → `return true;` | `TransmitHistory.cpp` | — |
| Límite de huérfanos | Máx 10 favoritos remotos sobre nodos nunca oídos | `AdminModule.cpp` | — |

### BLOQUE S — Seguridad y administración remota
| Mejora | Comportamiento | Ficheros | Depende de |
|---|---|---|---|
| Claves desde perfil | `USERPREFS_USE_ADMIN_KEY_X` → macro → `NodeDB.cpp`; sin literales en código | Perfil + `platformio-custom.py` + `NodeDB.cpp` | Mecánica de build |
| Auto-recuperación de claves | Si slot 0 suma 0 en cada boot → re-inyecta K0/K1 de fábrica | `NodeDB.cpp` `loadFromDisk` | `FIX_NATIVE_CORE_RESET` |
| Bitfield criptográfico | `isAdminNode()` lee `0x08`; solo PKI validada marca admin (nunca NodeInfo en claro) | `NodeDB.h/.cpp`, `AdminModule.cpp`, `NodeInfoModule.cpp` | — |
| Canal Navadmin | Canal 1 (slot, no nombre) PSK `{0x01}`; whitelist SOLO LECTURA; silencio para no-admins; **rate-limit 30 s** por nodo | Perfil (`USERPREFS_CHANNEL_1_*`, `CHANNELS_TO_WRITE=2`) + `NavaCLIModule.cpp` | BLOQUE N; canal identificado por slot: no reordenar |
| Fix `updateUser` (10/08) + **H3 (a)+(a2)** (12/08) | NodeInfo con clave == admin_key → acepta + re-favorito + bitfield directo; el mando queda acreditado en canal y DM con UN NodeInfo broadcast | `NodeDB.cpp` `updateUser` | Bitfield (S); **H3 construye sobre el fix de 10/08: van juntos** |
| Fix #10873 | `disableBluetooth()` DESPUÉS de factoryReset/resetNodes (3 casos) → factory reset no se cuelga | `AdminModule.cpp` | — |
| P0 lifepo4 rechazado | `set_chem lifepo4` → `ERR` en Seed/Xiao×2/T114 (LPCOMP fijo > 3.65 V de LiFePO4 → ladrillo solar) | `NavaCLIModule.cpp` | E1 umbral por placa |

### BLOQUE N — NavaCLI `/nava` (v4.2.1)
| Mejora | Comportamiento | Ficheros | Depende de |
|---|---|---|---|
| Módulo completo | ~45 comandos por DM PKI / canal 1; fragmentación ≤190 chars por palabra; interrogación universal; ayuda por comando; diferidos (txoff/reboot/factory_reset/storm) | `NavaCLIModule.h/.cpp` + `Modules.cpp` | **Externs imprescindibles**: `lastRxFrequencyError` (RadioLibInterface), `rawResetReason`/`timedSystemSleepSeconds`/`setBleForceDisabled` (main-nrf52), `router->getInterface()` (Router.h), `environmentTelemetryModule->sendTelemetry()` (EnvironmentTelemetry pública), `power->updateOcvCurve/setChemistryProfile` (Power), `nodeDB->installRoleDefaults` (NodeDB) |
| Fav auto gate | `/nava fav auto [on|off]` controla E2 auto-fav; persistido | `NavaCLIModule` + `NodeDB` | E2 auto-fav |
| Rol semi-permanente (R1) | `set_role` guarda en resilience.bin (`role`); se aplica al boot con `installRoleDefaults` → sobrevive a factory reset | `NavaCLIModule.h/.cpp` (`NAVARICO_RAMA_1`) | `NodeDB::installRoleDefaults` |

### BLOQUE P — Paridad byte-a-byte (reproducibilidad)
| Mejora | Comportamiento | Ficheros | Depende de |
|---|---|---|---|
| Metadatos fijos | APP_VERSION (git SHA), APP_ENV canónico, BUILD_EPOCH, `__TIME__/__DATE__` | `bin/platformio-custom.py` (overrides inertes) | Variables de entorno `NAVARICO_*` |
| Rutas libdeps embebidas | `-ffile-prefix-map` por env para que los `__FILE__` de libs coincidan con la referencia | `platformio-custom.py` + `custom_meshtastic_libdeps_map` por env | **Inyección a LIB BUILDERS** (F4) |
| Líneas "mágicas" | `#line` en asserts de main-nrf52 por radio; NodeDB a 0 líneas netas | `main-nrf52.cpp`, `NodeDB.cpp` | Ver CHECKLIST 3-4 |

**Dependencias críticas (qué va SIEMPRE junto):**
- **H1 (fix updateUser 10/08) + H3 (a)+(a2)**: H3 inserta el bitfield dentro del bloque `newKeyIsAdmin` de H1. Aplicar H3 sin H1 rompe la acreditación. Ambos + canal Navadmin = mando acreditado con un solo NodeInfo.
- **Canal Navadmin (perfil) + fix H3**: tras factory reset el canal 1 se materializa y el NodeInfo del mando acredita canal+DM.
- **`getActiveLpcompThreshold`**: los 3 `#elif` por placa son obligatorios si el fork va a más de una placa; el `switch` dinámico SOLO es válido con divisor 0.5.
- **`main-nrf52.cpp` común**: los bloques `#ifdef RADIO_POWER_ENABLE_PIN` ×3 y `#ifdef BATTERY_LPCOMP_INPUT` deben conservarse (E22P perdería 40 mA de apagado físico).
- **`OCV[11]` con inicializador + wrappers que sincronizan Power**: sin esto, `readPowerStatus()` compara contra basura y el deep sleep por batería baja no funciona.
- **BLOQUE N sin sus externs no compila**: aplicar el módulo + Modules.cpp DESPUÉS de todos los demás bloques (los externs viven en E1/RadioLib/EnvironmentTelemetry).
- **Auto-fav (E2) sin `activeDirectRouters` (Router.h) y `navaAutoFavoriteEnabled` (NodeDB.h extern)**: no compila.

---

## 3. PROCEDIMIENTO de portado paso a paso

### FASE 0 — Preparación (30 min)
1. Clona el fork nuevo y el prístino de referencia (`C:\Users\Jesus\Desktop\firmware`, 54e0d8d).
2. Verifica la base: `version.properties` (2.7.26), `git log -1`, y que `APP_REPO` del binario de referencia = `meshtastic/firmware` (viene del remote del `.git`).
3. Copia al lado la referencia de paridad: los 12 UF2 originales (`Desktop\NavaTastic 4.3 120826\` o `distribucion\` del repo) + su `userPrefs.jsonc`/perfiles.
4. Haz un commit baseline del fork nuevo (para poder hacer `git diff` del trabajo).

### FASE 1 — Análisis del fork nuevo
1. `git diff --stat <baseline>` contra el prístino: identifica qué ha cambiado el fork nuevo y qué archivos del inventario existen o cambiaron de nombre.
2. Localiza las anclas del inventario con `rg` (una por una): `NAVARICO:` (no existirán en un fork virgen — se usan al aplicar), `FIX_NATIVE_CORE_RESET`, `USERPREFS_*`, `waitUntilPowerLevelSafe`, `shouldDecrementHopLimit`, `updateUser`, `addReceiveMetadata`, `saveNodeDatabaseToDisk`, `TransmitHistory::saveToDisk`, `limitPower`, `installRoleDefaults`, `setupModules`, `disableBluetooth`.
3. Detecta divergencias de API (p. ej. firma de `updateUser`, clases renombradas) y anótalas: son los puntos donde el bloque se adapta.
4. Decide el alcance: ¿qué placas? ¿Rama 1 y 2? ¿paridad con qué referencia? (Recomendado: portar General R2IG primero, validar, y después R1IG.)

### FASE 2 — Aplicar por pases (en este orden)
1. **Infraestructura**: perfiles jsonc + `platformio-custom.py` + envs en `variants/.../navarrico.ini` + `platformio.ini` (default_envs) + scripts (`build.ps1`/`distribuir.ps1`/`verificar_paridad.ps1`) + `.gitignore`.
2. **Variantes**: `variant.h` por placa (bloques + `#ifdef` de radio) + `platformio.ini` de placa (EXCLUDE/lib_ignore).
3. **Energía (E1)**: `main.cpp` → `main-nrf52.cpp` → `Power.cpp`/`power.h`.
4. **Flash + seguridad (E2 + S)**: `Channels.cpp` → `NodeDB.h` → `NodeDB.cpp` → `Router.h` → `Router.cpp` → `TransmitHistory.cpp` → radios (afc: `RadioLibInterface.h/.cpp`, `SX126xInterface.cpp`, `RF95Interface.cpp`; clamp: `RadioInterface.cpp`) → `AdminModule.cpp` → `NodeInfoModule.cpp` → `EnvironmentTelemetry.h/.cpp`.
5. **NavaCLI (N)**: `NavaCLIModule.h` → `NavaCLIModule.cpp` → registro en `Modules.cpp`. (Después de todos los externs.)
6. **Fuzzer**: `.clusterfuzzlite/router_fuzzer.cpp` con las claves del perfil.
7. Marca cada bloque aplicado con comentario `NAVARICO:` inline (qué hace, qué cambiar por versión) — **0 líneas netas en NodeDB.cpp** si buscas paridad (ver CHECKLIST 3).

### FASE 3 — Compilar por env
1. `pio run -e navarrico_<placa>_<radio>_<rama>` desde la raíz, UN env a la vez (ver CHECKLIST 8).
2. Si el `.pio` viene heredado de otra copia: limpiarlo antes (resultados engañosos).
3. Si la raíz del proyecto es larga (>260 chars con libdeps): usar `libdeps_dir`/`build_dir` cortos o mover el repo (ver CHECKLIST 10).

### FASE 4 — Verificar
1. **Paridad** (si la referencia existe): `.\verificar_paridad.ps1` (fija `NAVARICO_APP_VERSION=2.7.26.54e0d8d`, `NAVARICO_BUILD_EPOCH`, `__TIME__/__DATE__` y el mapa de libdeps) y compara MD5 de los 12 UF2 contra la referencia. Regla de oro: **verificar con el BINARIO, no con heurísticas de carpetas** (F8).
2. **Comportamiento**: test en banco mínimo — arranque, factory reset (USB y BLE: fix #10873), `/nava ping`/`status` por DM PKI y canal 1, `set_chem` (lifepo4 rechazado en las 4 placas fijas), `fav auto`, `set_role` (R1), deep sleep con fuente regulable (V de despertar por placa: Promicro ~3.71V, Xiao/Seed ~3.67V, T114 ~4.04V).
3. Regresión: comparar contra la referencia Eclipse Edition ("esto en Eclipse iba bien") antes de tocar nada más.

---

## 4. CHECKLIST de trampas

> Todas documentadas como F1-F12 en `BITACORA_TECNICA.md`. Pásalo completo ANTES de dar por bueno un build.

1. **PIO elimina los backslashes de `build_flags`** → `-ffile-prefix-map` escrito en el ini llega mutilado (`.piolibdeps...`). **Inyectar por Python a los LIB BUILDERS** (`for lb in env.GetLibBuilders(): lb.env.Append(CCFLAGS=[flag])`), no a `projenv` (solo afecta al SRC) ni a `env` principal (no llega a las libs). (F4)
2. **`__TIME__`/`__DATE__` entran en el binario**: `Crypto/RNG.cpp` y `RadioLib/Module.cpp` (RNG seed + banner). Recompilar otro día = binario distinto. Fijar con `-D__TIME__=\"HH:MM:SS\" -D__DATE__=\"Mon DD YYYY\"` crudos como CCFLAGS (las comillas escapadas con backslash; CommandLineToArgvW borra las comillas simples → con CPPDEFINES rompe la compilación). (F6)
3. **Líneas "mágicas" en `NodeDB.cpp` (`saveToDisk`)**: cualquier línea añadida/eliminada cambia el binario 1 byte (el `saveToDisk` embebe `__LINE__` de un assert interno). Regla: **0 líneas netas** en NodeDB.cpp (comentarios inline, rol por perfil no por código). (F7)
4. **Asserts en `main-nrf52.cpp` embeben `__LINE__`** y las 6 placas originales divergían (bloques por placa presentes/ausentes). Fijar con **`#line` por radio**: `NAVARICO_LINE_ASSERT1/2` = 451/454 (SX1262) vs 456/459 (E22P). (F11)
5. **UF2 nRF52 mixtos**: PIO genera ~312 bloques UF2 codificados + el RESTO CRUDO. La marca temporal puede caer en la zona cruda: buscar `Get-Stamp` en el fichero crudo, no reconstruir la imagen. (F12)
6. **Rutas de libdeps embebidas por env** (`arduino-fsm` usa `__FILE__`): cada referencia original embebió rutas distintas (R2: relativas `.pio\libdeps\<env>`; R1: absolutas `C:/Users/Jesus/.platformio/libdeps/r1promic` y `r1xiaoki` — el workaround MAX_PATH de R1 cambió las rutas de 4 boards). Por eso existe `custom_meshtastic_libdeps_map` POR ENV. (F4/F5)
7. **APP_VERSION** (git SHA): `bin/readprops.py` hace `git rev-parse --short=7 HEAD` → el fork nuevo embebe su propio SHA. Override con `NAVARICO_APP_VERSION`. (F1)
8. **APP_ENV**: el nombre del env (`navarrico_*`) se filtra al binario. Override con `custom_meshtastic_app_env` = nombre canónico de placa. (F3)
9. **BUILD_EPOCH**: `datetime.now()` del build. Override con `NAVARICO_BUILD_EPOCH`. (F2)
10. **MAX_PATH (260)**: si la raíz del proyecto es larga, `SparkFun_MMC5983MA_Arduino_Library_Constants.h` (vía main.h→MagnetometerThread) rompe con `fatal error: No such file or directory` falso. El repo unificado lo resolvió de raíz (raíz corta); si no puedes, `libdeps_dir`+`build_dir` cortos (y entonces LAS RUTAS EMBEBIDAS CAMBIAN → hay que reflejarlo en el mapa de libdeps).
11. **No paralelizar dos builds del MISMO env**: pio comparte la caché de downloads → carreras que corrompen libs (señal: libs parciales o zips de 1 byte en `~/.platformio/.cache/downloads`). Distintos envs pueden ir en paralelo.
12. **`.pio` heredado enmascara errores**: al copiar carpetas, el `.pio` puede venir con paths viejos (builds incrementales "sanos" falsos). Limpiar antes de la primera compilación.
13. **`default_envs = tbeam` (upstream) falla** por toolchain ESP32; en el repo unificado se sustituyó por los 12 envs `navarrico_*`. No dejar `default_envs` a un env equivocado (build silencioso equivocado).
14. **Sondas de referencia equivocadas**: verificar SIEMPRE la sonda contra el binario correcto (R2IG vs R1IG) — una sonda confundida provocó más diff que el bug real (F5).
15. **Falso positivo de orden de libs**: no comparar carpetas/libNXX con orden alfabético; verificar con diff byte a byte del binario (F8).
16. **`to=0` NO es broadcast** (`isBroadcast()` solo NODENUM_BROADCAST/NODENUM_BROADCAST_NO_LORA): un paquete con `to=0` se emite al aire y NADIE lo entrega (ni nodos ni la API local — los TX no-broadcast no se ecoplean). Los avisos de canal 1 SIEMPRE con `enqueueResponse(NODENUM_BROADCAST, 1, ...)`. (Frente A 15/08)
17. **Adafruit InternalFS `FILE_O_WRITE` NO trunca** (`LFS_O_RDWR|LFS_O_CREAT`): escribir menos bytes deja el tamaño histórico máximo → gatear ficheros binarios de estado por `fileSize != sizeof(struct) || marcador de versión` y recrear con `FSCom.remove()` antes de escribir. (F15 15/08)
18. **E22P en banco**: picos de corriente en TX → frames corruptos a ≥8 dBm en fuente justa; probar a 1 dBm. En campo, potencia del env. (15/08)

---

## 5. Referencias y fuentes de verdad

| Qué | Dónde |
|---|---|
| Código funcional (canónico) | `C:\NavaTastic Codigo completo\src\` + `variants\` + `profiles\` (paridad 12/12 verificada 14/08) |
| Receta de paridad y fixes F1-F12 | `C:\NavaTastic Codigo completo\BITACORA_TECNICA.md` |
| Cómo funciona la selección por env/perfil | `C:\NavaTastic Codigo completo\Guia_para_agente_sobre_NavaTastic.md` |
| Estado y próximos pasos | `C:\NavaTastic Codigo completo\PLAN_DE_TRABAJO.md` |
| Cerebro + subnotas (valores por placa, claves, seguridad, energía) | `C:\NavaTastic Codigo completo\docs\cerebro\` (cerebro.md + 01..12) |
| Memoria técnica de comportamiento | `docs\transfer_context.md` y `docs\guia_integracion_navarrico.md` (bloques copy-paste) |
| Manual de comandos `/nava` | `docs\Manual_NavaTastic.md` |
| Prístino sin tocar | `C:\Users\Jesus\Desktop\firmware` (54e0d8d) |
| Binarios de referencia de paridad | `distribucion\` del repo (o `Desktop\NavaTastic 4.3 120826\`) |

> **Regla de oro final**: paridad byte-a-byte ≠ objetivo funcional. Si no se busca paridad,
> los overrides de metadatos y el mapa de libdeps son opcionales — pero las **trampas 1-5
> afectan también a builds "normales"** (marcas temporales no deterministas, libs rotas,
> líneas mágicas). El orden de bloques de la FASE 2 y las dependencias de la sección 2
> se aplican siempre.
