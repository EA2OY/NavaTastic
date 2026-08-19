# Versión Desplegada — Nodo Tierra Estella (Promicro, 8/8/26 09:42)

> **ESTADO 14/08/2026 — REPO UNIFICADO**: snapshot histórico **VIGENTE como referencia**
> (la versión desplegada en Estella). El build equivalente hoy = env
> `navarrico_promicro_e22p_*` del repo único. El contenido del snapshot (comandos que
> incluía y qué le faltaba) no ha cambiado.

> Importada de `C:\Firmware Navarrico 4.2\Contexto\cerebro\07_version_desplegada_estella.md` (2026-08-10).

Punto de referencia de lo que está DESPLEGADO en el nodo de Tierra Estella. Capturado a partir del snapshot congelado `NavaCLIModule.cpp.bak` (8/8/26 09:42:12) + `NavaCLIModule.h.bak` (8/8/26 08:55:11) + `userPrefs.jsonc` (8/8/26 09:24:26) de `C:\Firmware Navarrico 4.2\Rama 2 Infraestructura\Codigo Rama 2\Promicro fix 2.7.26 Rama 2\`.

> **IMPORTANTE**: esta versión es ANTERIOR a todo el trabajo posterior. No incluye la Secuencia de Comandos Remota 2 ni la ronda v4.2.1 de fixes operativos ni el fix `updateUser` de claves.

---

## 1. Identidad del firmware

- Variante: **Promicro fix** — env `nrf52_promicro_diy_tcxo` (nRF52840 + E22P/TCXO).
- Módulo de admin remota: `NavaCLIModule` (SinglePortModule + OSThread), con cola de respuestas fragmentadas (`NavaResponse`).
- El `.cpp` desplegado tenía **639 líneas** (el actual, 10/8/26, ya tiene 1189 — el doble, por toda la Secuencia 2 añadida después).

## 2. Configuración de placa (`userPrefs.jsonc` 8/8/26 09:24)

| Parámetro | Valor desplegado |
|---|---|
| Región | `EU_868` |
| Modem preset | `MEDIUM_FAST` |
| TX power | `8` (base) |
| Rol | `ROUTER` |
| Rebrodecast | `LOCAL_ONLY` |
| NodeInfo broadcast | `259200` (72h) |
| NodeDB | `RAM_ONLY = true` (protección Flash) |
| **Admin K0** | `K0 del operador (valores no publicados — se piden al compilar los envs Propia)` |
| **Admin K1** | `K1 del operador (ídem)` |

Las claves admin ya eran las actuales (no cambiaron desde entonces).

## 3. Funciones de gestión remota que INCLUYE

### Disparo del módulo (`wantPacket`)
- Comando `/nava` por **DM** o por **Canal 1 (Navadmin)**.
- Olfateo pasivo de telemetría local (cache temp/hum en RAM para `status`).

### Comandos implementados (lista completa de la versión 09:42)
| Comando | Tipo |
|---|---|
| `help` | Información |
| `ping` | Información |
| `peers` | Información |
| `status` | Información (Bat mV, Heap, Chip temp, estado) |
| `env` | Información (temperatura/humedad de caché I2C) |
| `channel` | Información |
| `fav add` / `fav rm` / `fav ls` | Gestión de favoritos |
| `ign add` / `ign rm` / `ign ls` | Gestión de bloqueos |
| `set_name` | Config |
| `set_role` | Config |
| `set_mqtt` | Config |
| `set_tz` | Config |
| `set_hops` | Config |
| `set_txpower` | Config (0-12 E22P) |
| `db_purge` | Base de datos |
| `db_clear` | Base de datos |
| `reboot` | Mantenimiento |
| `factory_reset` | Mantenimiento (diferido) |

### Seguridad de esa versión
- **DM PKI obligatorio** para: `fav*`, `ign*`, `set_*`, `db_*`, `reboot`, `factory_reset`. `mp.pki_encrypted` requerido si `replyChannel == 0`.
- **Canal 1 (Navadmin)**: solo lectura (info/whitelist). Los no-admins reciben silencio.
- **Validación de admin**: `nodeDB->isAdminNode()` (bitfield criptográfico) + comparación de clave pública contra `admin_key[0..2]`.
- Fragmentación de respuestas a **190 caracteres** (MTU LoRa SFNarrow).

## 4. Lo que esta versión NO incluye (añadido DESPUÉS del despliegue)

Secuencia de Comandos Remota 2 (la ronda posterior al 8/8) añadió, entre otros:
- `trace !ID` / `route !ID`
- `storm [h]` / `storm test1|test2`
- `set_chem`, `set_vbat`, `set_vwake`, `bat` (honesto)
- `ble`, `rxlog`, `afc`, `reset_reason`, `msg`, `bell`, `pos`, `nodeinfo`, `sendtel`, `admin_ls`, `power`, `noise`
- Fixes operativos v4.2.1 (storm real con radio dormida, route sin NodeDB, admin_ls con base64, msg con validación, ping con rate-limit)
- Fix `updateUser` de rotación de clave admin (10/8/26)
- Cambios en `Power.cpp` (fix `OCV[11]`), `main-nrf52.cpp` (storm RTC2 real), `EnvironmentTelemetry` (sendTelemetry público), radio (`afc`), etc.

## 5. Conclusión para el operador

El nodo de Tierra Estella, **a fecha 8/8/26 09:42**, gestiona remotamente: diagnóstico básico (`status`, `env`, `channel`, `peers`, `ping`), favoritos y bloqueos (`fav*`/`ign*`), configuración en caliente (`set_name/role/mqtt/tz/hops/txpower`), y mantenimiento (`db_purge`, `db_clear`, `reboot`, `factory_reset`). NO tiene gestión de energía avanzada (`storm`, `set_chem`, `set_vbat`, `set_vwake`, `bat`) ni trazado de rutas (`trace`/`route`) ni los fixes posteriores. Si se quiere esa funcionalidad, el nodo debe actualizarse a la versión actual.
