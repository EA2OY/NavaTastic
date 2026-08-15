# 09 — Rama General vs Propia: Normas de Diferenciación

> **ESTADO 15/08/2026 — REPO UNIFICADO**: la norma **sigue vigente** y **General ya está
> implementada** en el repo único (12 perfiles con K0=Master Node + BT 654321: `R2IG_*` y
> `R1IG_*`). La copia de carpetas queda obsoleta: en el repo se añaden perfiles y envs,
> sin copiar nada. **Propia implementada con claves NO almacenadas (15/08)**: 12 envs
> `R2IP_*/R1IP_*` que extienden los de General; al compilarlos, `bin/platformio-custom.py`
> exige las variables de entorno `NAVARICO_PROPIA_KEY_0` / `NAVARICO_PROPIA_KEY_1` /
> `NAVARICO_PROPIA_BT` (las pide `build_propia.ps1` sin almacenarlas en disco) y las inyecta
> como macros. **Las claves del operador NO viven en ningún fichero del repo.** El fuzzer
> (`.clusterfuzzlite/router_fuzzer.cpp`) usa la clave General (1 clave).

Estado 2026-08-12 (histórico): **`Infraestructura General\` ACTIVA (primera ronda 12/08)**: el operador copió los 6 folders desde Propia y aplicamos EN LOTE esta norma: K0 = Master Node (1 sola clave), K1/K2 sin usar, `USERPREFS_FIXED_BLUETOOTH=654321` (regla nueva del operador), fuzzer con 1 clave. Compiladas SUCCESS 6/6 y distribuidas a UF2/OTA de General. **Rama 1 Clientes hereda el MISMO split (12/08)**: los 6 `R1IG` llevan K0=Master Node + BT 654321 y los 6 `R1IP` las claves propias del operador + BT propio (ahora se piden al compilar, no se almacenan).

---

## 1. DIFERENCIA ÚNICA: claves de administración (2 → 1)

**General tendrá UNA sola clave admin = clave pública del Master Node.** Propia tiene 2 (K0/K1 del Promicro).

| Item | Propia (actual) | General (futura) |
|---|---|---|
| Claves admin | **2**: K0/K1 del operador (**valores no publicados** — se piden al compilar: `NAVARICO_PROPIA_KEY_0/1`) | **1**: K0 = Master Node `{0xc7,0xdc,...0x00,0x55}`; K1 sin usar (comentado/`{}`) |
| Canal Navadmin (slot 1) | PSK `{0x01}` | igual (sin cambios) |
| Canal 0 SFNarrow | PSK `{0x01}` | igual (sin cambios) |
| Núcleo / variantes | — | idéntico a Propia (solo cambia `userPrefs.jsonc`) |

## 2. Clave del Master Node (fuente y formatos)

- Fichero fuente (solo lectura): `C:\Firmware Navarrico 4.3\Nodo rescate\MasterNode_keys_1778158619918.json`
- **Public key (base64)**: `x9wN6W0TuoY/gtVKM/+lysx8Rewb5CAdZ9YfzIVRAFU=`
- **Public key (hex, 32 bytes)**: `c7dc0de96d13ba863f82d54a33ffa5cacc7c45ec1be4201d67d61fcc85510055`
- **Formato `userPrefs.jsonc` (macros de clave admin)**:
  ```
  "USERPREFS_USE_ADMIN_KEY_0": "{ 0xc7, 0xdc, 0x0d, 0xe9, 0x6d, 0x13, 0xba, 0x86, 0x3f, 0x82, 0xd5, 0x4a, 0x33, 0xff, 0xa5, 0xca, 0xcc, 0x7c, 0x45, 0xec, 0x1b, 0xe4, 0x20, 0x1d, 0x67, 0xd6, 0x1f, 0xcc, 0x85, 0x51, 0x00, 0x55 }",
  // "USERPREFS_USE_ADMIN_KEY_1": "{}",   (sin usar en General)
  // "USERPREFS_USE_ADMIN_KEY_2": "{}",   (sin usar en General)
  ```
- **Nota histórica**: esta clave comienza por `0xc7...` — es la misma "clave de rescate" que Xiao E22P y Seed P1 usaban como K0 antes de la unificación de claves (ver `02_claves_admin.md`). Es el Master Node / mando de rescate de la malla.
- **Regla del hardcodeo aplica igual**: SOLO en `userPrefs.jsonc` (macros `USERPREFS_USE_ADMIN_KEY_0`), nunca literales en código. También actualizar `.clusterfuzzlite/router_fuzzer.cpp` de General (usa `admin_key[0]`/`admin_key_count`; en General: `count=1` y solo la clave Master Node).

## 3. Plan de ejecución en lote (cuando el operador lo pida)

1. **Recopiar** `Infraestructura General\` desde `Infraestructura Propia\` (6 variantes, sin `.pio/.git/Compilados`). **⚠️ La copia la hace el OPERADOR; el agente NO copia nada en General.**
2. **Aplicar la norma**: en los 6 `userPrefs.jsonc` de General → `USERPREFS_USE_ADMIN_KEY_0` = clave Master Node, `KEY_1`/`KEY_2` sin usar (comentado/`{}`). Actualizar `.clusterfuzzlite/router_fuzzer.cpp` (1 clave, Master Node).
3. **Compilar** las 6 variantes de General con su env real (`-e`), cada una en su carpeta.
4. **Distribuir** con `distribuir_binarios.ps1` (deduce `R2IG` → General → `UF2\`/`OTA\` de General).
5. **Verificar** paridad núcleo: los `.cpp` de General deben ser idénticos a Propia (solo difiere `userPrefs.jsonc` y el fuzzer).

## 4. Precauciones

- No mezclar binarios General/Propia (claves admin distintas → `/nava` no intercambiable entre ramas).
- `distribuir_binarios.ps1` deduce la rama por el sufijo `R2IG`/`R2IP` — no cambiar los sufijos.
- El zip de auditoría (`Navarrico4.3_Contexto_Auditoria_2026-08-11.zip`) NO incluye claves privadas; el Master Node keys JSON NO debe distribuirse.
