# 10 — Mapa de Hardcodeos del Nodo (dónde vive cada valor)

> **ESTADO 14/08/2026 — REPO UNIFICADO**: mapa **VIGENTE**, con la ubicación física
> actualizada: `userPrefs.jsonc` raíz + `profiles/*.jsonc` (macros `USERPREFS_*`),
> `variant.h` de cada placa en `variants/nrf52840/...`, `src/mesh/Channels.cpp`,
> `src/mesh/NodeDB.cpp`, `src/platform/nrf52/main-nrf52.cpp`, `src/Power.cpp`. Para
> cambiar un valor: editar perfil o variant.h del repo y recompilar el env afectado
> (`pio run -e navarrico_*`). Las líneas citadas abajo son orientativas.

Inventario verificado 2026-08-11 contra el código real (Promicro R2IP como referencia canónica; los 6 repos comparten núcleo). **Objetivo**: si se quiere cambiar un valor, saber EXACTAMENTE en qué archivo vive y a qué afecta — para no perderlo ni ponerlo en un archivo equivocado.

> Fuentes de verdad: este mapa + `userPrefs.jsonc` + `variant.h` + `platformio.ini` de CADA variante. Nada de valores se "inventa" en otro archivo: si no está en este mapa, no está hardcodeado.

---

## 1. Valores de radio y canal (SFNarrow)

| Valor | Dónde se define (fuente) | Archivo(s) |
|---|---|---|
| Región EU_868 | `USERPREFS_CONFIG_LORA_REGION` (macro) | `userPrefs.jsonc` → `platformio-custom.py` → `-D` |
| Canal 0 nombre "SFNarrow" | `USERPREFS_CHANNEL_0_NAME` (macro) + fallback `Channels.cpp:159` (`#ifdef FIX_NATIVE_CORE_RESET`) | `userPrefs.jsonc` + `src/mesh/Channels.cpp` |
| Canal 0 PSK `{0x01}` | `USERPREFS_CHANNEL_0_PSK` (macro) | `userPrefs.jsonc` |
| Canal 1 "Navadmin" PSK `{0x01}` | `USERPREFS_CHANNEL_1_*` + `CHANNELS_TO_WRITE=2` (macros) | `userPrefs.jsonc` (6/6 desde 12/08; antes solo Promicro) |
| `CHANNELS_TO_WRITE = 2` | macro | `userPrefs.jsonc` |
| `channel_num = 4` (869.618 MHz) | `USERPREFS_LORACONFIG_CHANNEL_NUM` + fallback `Channels.cpp:103-104` (`channel_num=4`, `override_frequency=869.618f`) | `userPrefs.jsonc` + `src/mesh/Channels.cpp` |
| Modem preset MEDIUM_FAST | `USERPREFS_LORACONFIG_MODEM_PRESET` (macro) | `userPrefs.jsonc` |
| TX power base = 8 | `USERPREFS_LORACONFIG_TX_POWER` + fallback `Channels.cpp:105` | `userPrefs.jsonc` + `src/mesh/Channels.cpp` |
| Potencia máx E22P = 12 dBm | `SX126X_MAX_POWER 12` / `HARDWARE_TX_POWER_LIMIT 12` | `variant.h` (Promicro, Xiao E22P) |
| Potencia máx SX1262 = 22 dBm | `SX126X_MAX_POWER 22` / `HARDWARE_TX_POWER_LIMIT 22` | `variant.h` (Faketec, Xiao Kit, Seed, T114) |

## 2. Rol y comportamiento de nodo (router de infraestructura)

| Valor | Dónde | Archivo |
|---|---|---|
| Rol ROUTER por defecto | `USERPREFS_CONFIG_DEVICE_ROLE` + fallback `NodeDB.cpp:701` (`Role_ROUTER`) | `userPrefs.jsonc` + `src/mesh/NodeDB.cpp` |
| Rol CLIENT por defecto (**Rama 1 Clientes** — rol semi-permanente sobreviviente a factory reset) | `USERPREFS_CONFIG_DEVICE_ROLE=CLIENT` (jsonc) + fallback `NodeDB.cpp` (`Role_CLIENT`) + `ResiliencePrefs.role` (0xFF=sin fijar) aplicado en `loadResiliencePrefs()` con `installRoleDefaults()`; `set_role` lo guarda | `userPrefs.jsonc` + `NodeDB.cpp` + `NavaCLIModule.h/.cpp` |
| `ALLOW_ROUTER_DEFAULT_ROLE` | macro (huérfana, no gatea nada — ver auditoría C8) | `userPrefs.jsonc` |
| `rebroadcast_mode = LOCAL_ONLY` | `USERPREFS_CONFIG_DEVICE_REBROADCAST_MODE` + `NodeDB.cpp:1062/1115` | `userPrefs.jsonc` + `src/mesh/NodeDB.cpp` |
| NodeInfo broadcast 72 h | `USERPREFS_CONFIG_NODEINFO_BROADCAST_INTERVAL=259200` + `NodeDB.cpp:1067` | `userPrefs.jsonc` + `src/mesh/NodeDB.cpp` |
| Position broadcast 72 h / smart OFF | `USERPREFS_CONFIG_*` + `NodeDB.cpp:1068-1069` | `userPrefs.jsonc` + `src/mesh/NodeDB.cpp` |

## 3. Claves admin

| Valor | Dónde | Archivo |
|---|---|---|
| K0 = `{0x12,0x48,...0xaa,0x68}` | `USERPREFS_USE_ADMIN_KEY_0` | `userPrefs.jsonc` (macro → `NodeDB.cpp:77-78,723-726,753-754,1465-1466,1489-1497`) |
| K1 = `{0x3f,0x38,...0x73,0x38}` | `USERPREFS_USE_ADMIN_KEY_1` | `userPrefs.jsonc` (idem, slots 1) |
| K2 vacía | `USERPREFS_USE_ADMIN_KEY_2` comentada | `userPrefs.jsonc` |

**Regla**: la clave SOLO se inyecta en `installDefaultConfig()` (factory reset / config corrupto) y en `loadFromDisk()` si la suma es 0. **NUNCA** sobrescribir en reinicios normales (los cambios del usuario persisten). Si se quiere cambiar una clave: editar `userPrefs.jsonc` y recompilar.

## 4. Energía y batería (hardware)

| Valor | Dónde | Archivo |
|---|---|---|
| ADC multiplier 2.0 (divisor 2×1M) | `VBAT_DIVIDER_COMP 2.0` / `ADC_MULTIPLIER` | `variant.h` (Promicro, Faketec) |
| ADC multiplier 3.0 (1M/510k) | `ADC_MULTIPLIER (3)` | `variant.h` (Xiao Kit, Xiao E22P) |
| ADC multiplier 3.3 (Seed) / 4.916 (T114) | `ADC_MULTIPLIER` | `variant.h` |
| LPCOMP input/threshold | `BATTERY_LPCOMP_INPUT` / `BATTERY_LPCOMP_THRESHOLD` | `variant.h` (cada variante) |
| Umbral LPCOMP dinámico (Promicro/Faketec) o fijo (`#ifdef` por placa) | `getActiveLpcompThreshold()` | `src/platform/nrf52/main-nrf52.cpp` |
| Corte batería (OCV clamp) | `OCV_ARRAY` | `variant.h` (cola: 3500 Promicro/E22P; 3400 resto) |
| Pre-check sueño 3500 mV / 8 lecturas (V3/F18) | `USERPREFS_LOW_BATTERY_SLEEP_THRESHOLD_MV` / `_READINGS_COUNT` | `userPrefs.jsonc` + perfiles |
| Filtro anti-rebote low_voltage_counter (8 unificado V3/F18) | `USERPREFS_LOW_BATTERY_READINGS_COUNT` (fallback 8; antes `>4` Promicro vs `>10` resto) | `src/Power.cpp` |
| `delay(3000)` antes de LPCOMP (leer voltaje + programar LPCOMP con éxito; evita bucle wake/sleep) | `cpuDeepSleep()` | `src/platform/nrf52/main-nrf52.cpp:522` |
| `RADIO_POWER_ENABLE_PIN P0.17` (E22P) | `variant.h` | Promicro, Xiao E22P |
| `waitUntilPowerLevelSafe` 2.7 V + 0.2 V | default `SAFE_VDD_VOLTAGE_THRESHOLD` | `src/platform/nrf52/main-nrf52.cpp` |

## 5. Resets y seguridad de núcleo

| Valor | Dónde | Archivo |
|---|---|---|
| `FIX_NATIVE_CORE_RESET` (reset limpio + canal + claves) | `variant.h` (6/6) | `variant.h` → `Power.cpp`, `Channels.cpp`, `NodeDB.cpp` |
| Reset limpio `NVIC_SystemReset()` (no `sd_nvic_SystemReset()`) | `Power::reboot()` | `src/Power.cpp` |
| `USERPREFS_FIXED_BLUETOOTH` (General 654321; **Propia: PIN propio, se pide al compilar**) | macro | `userPrefs.jsonc` / variables de entorno Propia |

## 6. Cómo cambiar un valor correctamente (reglas)

1. **Identifica el valor en este mapa** → edita SOLO ese archivo.
2. **Macros de `userPrefs.jsonc`**: el flujo es `userPrefs.jsonc` → `bin/platformio-custom.py` → `-D macro` → código. Editar el `.jsonc` y recompilar con `-e <env>` de ESA variante.
3. **`variant.h`**: valores físicos por placa. Editarlo afecta SOLO a esa variante (el `variant.h` se selecciona por env).
4. **Núcleo común** (`NodeDB.cpp`, `Channels.cpp`, `Power.cpp`, `main-nrf52.cpp`): afecta a las 6 → recompilar las 6 y verificar paridad.
5. **NUNCA** poner un valor en un archivo distinto al del mapa (p. ej. clave en `Channels.cpp` o ADC en `Power.cpp`) — se pierde o se duplica.
6. **Backup** antes de tocar: `.bak-AAAAMMDD-HHMM` y/o `snap-AAAAMMDD-HHMMSS.zip`.
7. **Verificación**: `pio run -e <env>` en la carpeta de la variante + test en banco si cambia energía/claves.

## 7. Nota histórica

La guía `C:\Firmware Navarrico 4.2\Guia para agente para Hardcodear el nodo promicro.odt` (9/05/2026) documenta el origen del hardcodeo (clave `{0xc7...}`, EU_868, TX=8). Propuso `adc_multiplier_override=2.5` — **no implementado en 4.3** (ADC real = 2.0, 2×1M). La regla "clave solo en `installDefaultConfig`, nunca en `loadLocalConfig`" es la que sigue el código actual.
