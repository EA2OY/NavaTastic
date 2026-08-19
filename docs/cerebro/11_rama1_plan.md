# 11 — Rama 1 Clientes en 4.3: NORMAS APLICADAS (vs Rama 2)

> **ESTADO 14/08/2026 — REPO UNIFICADO**: normas **VIGENTES**, mecánica actualizada:
> la Rama 1 Clientes es el env `navarrico_<placa>_<radio>_r1ig` (macro `NAVARICO_RAMA_1`
> + perfil `R1IG_*.jsonc` con `USERPREFS_CONFIG_DEVICE_ROLE=CLIENT`). El rol
> semi-permanente vive en `ResiliencePrefs.role` (`src/modules/NavaCLIModule.h/.cpp`).
> La distribución NO va al Desktop: `distribuir.ps1` → `distribucion\Rama 1 Clientes\`
> (LIPO/NIMH × UF2/OTA, norma NIMH = solo Faketec y XiaoKitI2c SIN +E22P intacta).
> El workaround MAX_PATH (r1promic/r1xiaoki) quedó en `custom_meshtastic_libdeps_map`
> para la paridad (BITACORA F4/F5).

Estado: 2026-08-12 · **EJECUTADO** (12 carpetas, 12/12 compiladas y distribuidas). Este documento es la neurona de referencia de las normas R1 vs R2.

---

## 1. Qué es Rama 1 (Clientes)

Nodos de infraestructura que NO son routers: aparecen como **CLIENT** en la malla. Mismo núcleo y misma protección de Flash que R2.

- Carpeta: `C:\Firmware Navarrico 4.3\Rama 1 Clientes en Infraestructura\` → `Infraestructura General\` (sufijo `R1IG`) + `Infraestructura Propia\` (sufijo `R1IP`), 6 variantes por rama + `UF2\`/`OTA\` de salida.
- Copia inicial hecha por el operador desde Rama 2 (12/08). **El agente NO copia carpetas.**
- ⚠️ `distribuir_binarios.ps1` deduce `R2IG/R2IP` — para R1 se usó copia manual con nombres R1 (o adaptar el script a futuro).

## 2. NORMAS R1 vs R2 (las ÚNICAS diferencias aplicadas)

| # | Norma | Dónde | Detalle |
|---|---|---|---|
| 1 | **Rol CLIENT** (R2 tenía ROUTER) | `userPrefs.jsonc` ×12 (`USERPREFS_CONFIG_DEVICE_ROLE=meshtastic_Config_DeviceConfig_Role_CLIENT`) + fallback `NodeDB.cpp` (`Role_CLIENT; // Default to client (Rama 1)`) | `rebroadcast_mode LOCAL_ONLY` se mantiene. Con rol CLIENT, `Router::shouldDecrementHopLimit()` devuelve `true` siempre → el bypass de hops de R2 queda inactivo SIN tocar Router.cpp. |
| 2 | **Rol semi-permanente** en `/resilience.bin` | `ResiliencePrefs.role` (uint8_t, `0xFF`=sin fijar) + `NavaCLIModule.cpp` (`set_role` lo guarda; `loadResiliencePrefs` lo aplica con `nodeDB->installRoleDefaults()`) | `set_role router|client|mute` sobrevive a **factory reset** (solo `/prefs` se borra). Bidireccional. Valores válidos 0-2 (CLIENT/CLIENT_MUTE/ROUTER; enum del protobuf). Compat ficheros viejos: el guard `fileSize < sizeof(prefs)` fuerza `role=0xFF`. |
| 3 | **Nada más cambia**: filtros de guardado (persisten admins/backbone con claves → DM PKI sobrevive a reboot), desalojo híbrido, límite huérfanos, `TransmitHistory` bypass, H3 (a)+(a2), fav auto (ON), canal Navadmin, claves, BT, energía/LPCOMP | — | Decisión del operador: NO restaurar el guardado completo de la DB. Pares no-admin se pierden en reboot y se re-aprenden por aire (cosmético). |

## 3. Decisiones aún pendientes (operador)

1. Position broadcast: hoy apagado (72h + smart OFF) — ¿activar para clientes visibles en la app? (p. ej. 600s + smart)
2. NodeInfo broadcast 72h: mantenido (admins/backbone se recuperan de Flash, así que no cegue tras reboot)
3. TX power default 8/22: mantenido por hardware
4. Alcance móvil: ¿variantes distintas para nodos móviles de verdad (p. ej. menor NodeInfo)?
5. `distribuir_binarios.ps1` para R1 (hoy copia manual)

## 4. Compilación R1 (normas nuevas)

- **MAX_PATH (error #13)**: las carpetas Promicro×2 y XiaoKitI2c+E22P×2 requieren `libdeps_dir` + `build_dir` cortos (`C:/Users/Jesus/.platformio/{libdeps,build}/r1xxx`) en su `platformio.ini` (rutas >260 chars rompen el include de `SparkFun_MMC5983MA_Arduino_Library_Constants.h`). Sus binarios salen fuera del proyecto → copiar de `C:/Users/Jesus/.platformio/build/r1xxx/<env>/` al distribuir.
- No paralelizar dos builds del MISMO env (corrompe la caché pio).
- Limpiar `.pio` heredado antes de compilar R1 (la copia de carpetas puede traerlo → resultados engañosos).

## 5. Distribución al Desktop (norma)

Script `HerramientasPropiasIA\distribuir_desktop.ps1` → `Desktop\NavaTastic 4.3 120826\`:
- `Rama 1 Clientes\` ← binarios R1; `Rama 2 Routers\` ← binarios R2.
- Cada rama: `LIPO\` (todas las variantes) y `NIMH\` (**solo Faketec y XiaoKitI2c SIN +E22P** — compatibilidad NiMH declarada por el operador 12/08; el resto NO se copia a NIMH).
- `UF2\` y `OTA\` con nombre de fichero = nombre de carpeta variante (p. ej. `Faketec NavTastic 2.7.26 R1IP.uf2`).

## 6. Verificación (hecha en 12/08)

- Paridad MD5: NodeDB.cpp ×12=`CD708322`, NavaCLIModule.h ×12=`F1668D8E`, .cpp 8×`CD6F41BB`(22dBm)/4×`4DB6CFB4`(12dBm).
- Grep 12/12: fallback CLIENT, campo role, load apply, set_role save, 0xFF×2.
- 12/12 compilación desde cero SUCCESS + MD5 UF2 vs build 12/12 OK.
- Pendiente: test en banco (rol visible, conversión semi-permanente, NIMH Faketec/XiaoKitI2c, compat resilience.bin viejo).

## 7. Referencias

- `01_ramas_variantes.md` (estructura), `04_energia_bateria.md`, `05_nodedb_flash.md` (filtros conservados), `10_hardcodeos_nodo.md` (mapa), `cerebro.md` log 11ª parte + error #13.
