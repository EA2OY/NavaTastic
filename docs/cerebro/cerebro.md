# CEREBRO — NavaTastic (Índice Global y Registro de Estado)

> **ESTADO 14/08/2026 — REPO UNIFICADO (LEER PRIMERO)**: este cerebro es ahora el del
> fork unificado `C:\NavaTastic Codigo completo` (un solo código → **12 firmwares** por
> env/perfil). **Todo el contenido histórico de 4.3 (secciones 1-4 y subnotas 01-12) se
> conserva íntegro como conocimiento técnico**, y lo nuevo (log de la sesión 14/08,
> errores F1-F12, VIGENTE vs OBSOLETO, handover) está en la **sección 5**. El original
> `C:\Firmware Navarrico 4.3` es ARCHIVO HISTÓRICO SOLO LECTURA. Guías vivas del repo:
> `docs/Guia_para_agente_sobre_NavaTastic.md` (cómo funciona), `docs/BITACORA_TECNICA.md` (receta
> de paridad y fixes F1-F12), `docs/PLAN_DE_TRABAJO.md` (estado), `docs/PORTING_NUEVO_FORK.md`
> (guía maestra para portar a un fork nuevo).

Capa de conocimiento portable del proyecto. Inicializa contexto en cualquier agente sin reenviar documentación original.

---

## 1. VISIÓN GENERAL (resumen ejecutivo)

Fork de Meshtastic v2.7.26 optimizado para repetidores solares de infraestructura (malla SFNarrow, Madrid). **Rama 1** = resiliencia física/energía (batería, LPCOMP, sleep, sensores). **Rama 2** = protección de Flash + infraestructura (NodeDB en RAM, auto-favoritos, bypass de hops, admin PKI), subdividida en **`Infraestructura General\`** (sufijo `R2IG`) e **`Infraestructura Propia\`** (sufijo `R2IP`), cada una con **6 variantes** de código y carpetas `UF2\`/`OTA\`. **General ACTIVA (12/08): primera ronda con K0=Master Node (1 sola clave) y BT 654321; canal Navadmin homogeneizado en las 12 (ver subnota 09).** Propia es la rama de referencia con los fixes LPCOMP aplicados. Todas con el mismo núcleo v4.2.1 + NavaCLIModule `/nava`: Promicro fix, Faketec PROPIA, Xiao Kit i2c, Xiao E22P, Seed P1, Heltec T114. Compilación SIEMPRE con `-e <env real>` (el `default_envs = tbeam` falla por toolchain ESP32). Claves admin unificadas (K0/K1 = Promicro) solo en `userPrefs.jsonc`, sin hardcodeo en código.

---

## 2. ÍNDICE DE CONTENIDOS

| Nota | Contenido |
|---|---|
| [01_ramas_variantes.md](./01_ramas_variantes.md) | Separación Rama 1/2, 6 variantes, envs PlatformIO, hardware |
| [02_claves_admin.md](./02_claves_admin.md) | Claves admin (K0/K1 Promicro), regla del hardcodeo, fix `updateUser` 2026-08-10 |
| [03_seguridad_nava.md](./03_seguridad_nava.md) | NavaCLIModule `/nava`, canal Navadmin, DM PKI, whitelist |
| [04_energia_bateria.md](./04_energia_bateria.md) | LPCOMP, química baterías, storm, deep sleep, ADC |
| [05_nodedb_flash.md](./05_nodedb_flash.md) | Protección Flash, RAM-only, favoritos, desalojo, filtros de guardado |
| [06_compilar_distribuir.md](./06_compilar_distribuir.md) | Comandos de build por variante, distribución binarios, errores conocidos |
| [07_version_desplegada_estella.md](./07_version_desplegada_estella.md) | Snapshot de la versión DESPLEGADA en Tierra Estella (Promicro 8/8/26 09:42): comandos `/nava` que incluía (básicos) y qué falta (Secuencia 2, v4.2.1, fix `updateUser`) |
| [08_diagnostico_lab.md](./08_diagnostico_lab.md) | Instrumento de diagnóstico LAB (fork Promicro en `C:\Firmware Navarrico 4.3\LAB\`): buffer RAM + `/navalog.txt`, hooks de sleep/reset/duty, `/nava log`. **+ Pruebas por USB con el CLI** (comandos validados, comportamiento config vs full reset). NO desplegar en campo. **⛔ DESCARTADO (12/08, decisión del operador): NO usar — instrumento antiguo para un problema puntual.** |
| [09_general_vs_propia.md](./09_general_vs_propia.md) | Normas de diferenciación de la rama **General** vs **Propia**: ÚNICA diferencia = claves admin (General: 1 sola = Master Node; Propia: 2). Canales/PSK sin cambios. Plan de ejecución en lote |
| [10_hardcodeos_nodo.md](./10_hardcodeos_nodo.md) | **Mapa de hardcodeos del nodo**: dónde vive cada valor (radio/canal, rol, claves admin, energía/batería, resets). Cambiar un valor = editar SOLO ese archivo y recompilar las variantes afectadas |
| [11_rama1_plan.md](./11_rama1_plan.md) | **Rama 1 Clientes EJECUTADA (12/08)**: normas R1 vs R2 (rol CLIENT + rol semi-permanente en resilience.bin; resto idéntico a R2), decisiones pendientes, compilación MAX_PATH y distribución al Desktop |
| [12_auditoria_navatastic.md](./12_auditoria_navatastic.md) | **Plan Maestro y Auditoría Automatizada (17/08)**: Doble auditoría en banco de radio real (NavaTastic V3 + MeshNavarra Utility), 26/26 pruebas superadas (100% PASS). Guía Maestra de Auditoría y [Informe Consolidado](../INFORME_DOBLE_AUDITORIA_NAVATASTIC_Y_MESHNAVARRA.md) |

---

## 3. REGISTRO DE ESTADO (LOG)

### Decisiones de diseño clave
- **Diseño del Frente F21 y Preparación para Ejecución (2026-08-17)**: Diseñado el plan de trabajo [docs/PLAN_CANAL_PRIVADO_Y_REDIRECCION_NAVACLI.md](../PLAN_CANAL_PRIVADO_Y_REDIRECCION_NAVACLI.md) con 16 comandos remotos (canales secundarios en slots 2-7, redirección de NavaCLI, silenciamiento de Navadmin, control MQTT Up/Down, `set_ok_to_mqtt`, `set_pos`, `set_pin`, y `stats`/`log` RAM-Only). Mandato estricto de cero regresiones y migración a `NAVS_RESILIENCE_VERSION` V4 (`NAV4` / `0x4E415634`).
- **Resiliencia Solar Canónica y Estado [Crítico] (2026-08-17, fw 2.7.26.f12f833)**: Eliminadas todas las llamadas directas prematuras a `cpuDeepSleep()` en `main.cpp` bajo 3.30V que dejaban la radio SX1262 encendida (5-10 mA). Implementado el pre-check en 2 niveles (`[Vivo]` en Nivel 1 y `[Critico]` en Nivel 2) con ventana obligatoria de 160s (8 lecturas) y apagado canónico por `doDeepSleep()` (radio dormida por SPI $\rightarrow$ **0.4 mA**). Disparo LPCOMP validado en fuente a **3.77 V** (ADC reportó 3771 mV, 0.02% error).
- **PORQUÉ de la resiliencia energética (2026-08-11, operador)**: brownout de ascenso solar documentado (Nordic nRF52, también ESP32): batería sobredescargada → nodo apagado → el sol hace ascender la tensión → estado inestable del MCU que lo **bloquea** (ni reset; solo corte limpio). NavTastic mitiga con `waitUntilPowerLevelSafe()` (espera VDD ≥ umbral+histéresis antes de init), POFCON, pre-check y LPCOMP con histéresis. Ver `04_energia_bateria.md`.
- **RAM vs Flash**: Rama 2 usa `USERPREFS_NODEDB_RAM_ONLY` + guardado selectivo (solo favoritos/ignorados/admins) para no desgastar Flash.
- **Admin por bitfield**: `NODEINFO_BITFIELD_IS_CRYPTOGRAPHICALLY_VERIFIED_ADMIN_MASK (0x08)` — solo PKI validada marca admin, nunca NodeInfo en claro.
- **Claves desde userPrefs, no código**: macros `-DUSERPREFS_USE_ADMIN_KEY_X` inyectadas por `platformio-custom.py` desde `userPrefs.jsonc`; NodeDB.cpp las lee (`userprefs_admin_key_X[]`). Sin literales en src.
- **`default_envs = tbeam` NO tocar** (decisión 2026-08-10): si falla, el error es visible y se corrige con `-e`; cambiar a otro env silenciaría un build equivocado.
- **Backups `.bak4`**: creados de `NodeDB.cpp` y `userPrefs.jsonc` en las 6 variantes antes del fix.

### Historial archivado en 4.2 (referencias no migradas a 4.3)
- **`Rama 1 General\`** — NO migrada a 4.3. Vive en `C:\Firmware Navarrico 4.2\Rama 1 General`. Se creará de nuevo en 4.3 a futuro (todavía no existe).
- **`old\`** (variantes deprecated) — estaba en `Rama 2 Infraestructura\Codigo Rama 2\old\` de 4.2. No copiado a 4.3; el historial queda en `C:\Firmware Navarrico 4.2\Rama 2 Infraestructura\Codigo Rama 2\old\`.
- **`OLD_CONTEXT\`** — vive en `C:\Firmware Navarrico 4.2\OLD_CONTEXT` (raíz de 4.2). No copiado a 4.3.
- **`Manual_uso_NavaTastic_4.2.md`** — el `.md` vive en `Contexto y Manuales\` de 4.3 (`Manual_uso_NavaTastic_4.2.md`, corregido 11/08: químicas por placa); original en `C:\Firmware Navarrico 4.2\Manual_uso_NavaTastic_4.2.md`. PDF del 11/08: `Contexto y Manuales\Manual_uso_NavaTastic_4.2.pdf`.

### Estado git
- **HEAD detached en los 6 repos** (base `54e0d8d`, parches sin commitear). No commitear sin orden expresa.

### Registro de Estado (log)
- **2026-08-10 — documentada versión desplegada Tierra Estella** (Promicro 8/8/26 09:42): incluye gestión básica `/nava` (status/env/channel/peers/ping, fav, ign*, set_name/role/mqtt/tz/hops/txpower, db_purge, db_clear, reboot, factory_reset); **SIN** Secuencia 2 ni fixes posteriores. Importada desde 4.2 a `07_version_desplegada_estella.md`.
- **2026-08-11 — Fix Seed Solar Node P1** (solo carpeta R2IP): `getActiveLpcompThreshold()` bajo `#ifdef SEEED_SOLAR_NODE` devuelve `BATTERY_LPCOMP_THRESHOLD` (`3_8`) porque los niveles `set_vwake` 1-5 están calibrados al divisor 0.5 del Promicro y en el Seed pedían voltajes inalcanzables (~5.5V). Distribuido UF2/OTA de Propia. Detalle en `04_energia_bateria.md`.
- **2026-08-11 — Fix Xiao Kit i2c + Xiao E22P** (carpetas R2IP): mismo bug; divisor de fábrica 1M/510k (0.3377) no 0.5. `getActiveLpcompThreshold()` bajo `#ifdef SEEED_XIAO_NRF52840_KIT` devuelve `BATTERY_LPCOMP_THRESHOLD` (`3_8`, de fábrica Meshtastic verificado en `Desktop\firmware` 2.7.26 sin tocar). Ambos Xiao comparten env/variant.h → el fix cubre los dos. Distribuido UF2/OTA de Propia. Detalle en `04_energia_bateria.md`.
- **2026-08-11 — Fix Heltec T114** (carpeta R2IP): divisor de fábrica 100/490 (~0.204), no 0.5 → `9_16` pedía ~9.1V. `getActiveLpcompThreshold()` bajo `#ifdef HELTEC_T114` devuelve `BATTERY_LPCOMP_THRESHOLD` (`2_8`, ~4.04V). Nota: Meshtastic desactiva el LPCOMP en T114 por fuga 2.9mA dormido (issue #8801); el fork lo activa a propósito. Distribuido UF2/OTA de Propia. Detalle en `04_energia_bateria.md`.
- **2026-08-11 — Instrumento LAB de diagnóstico** creado en `C:\Firmware Navarrico 4.3\LAB\Promicro NRF52+E22P NavTastic 2.7.26 LAB` (fork Promicro, fuera de ramas): `src/NavarricoLog.h/.cpp` (ring buffer RAM 96 líneas + volcado a `/navalog.txt` 8KB + `[NAVA]` USB en vivo), hooks en pre-check, `cpuDeepSleep`, low battery, duty cycle y reset reason; comando `/nava log`. Binario en `LAB\UF2LAB\Promicro NavTastic 2.7.26 LAB.uf2`. NO se despliega en campo.
- **2026-08-11 (2ª parte) — Estado de ramas**: `Infraestructura General\` **VACÍA** (decisión del operador: se borra y se recopiará desde Propia ya corregida). Propia = rama de referencia con fixes LPCOMP. **Norma de diferenciación de General: ÚNICA diferencia = claves admin — General usará 1 sola clave (K0 = clave pública del Master Node `{0xc7,...0x55}`) vs 2 de Propia (K0/K1 Promicro); canales (Navadmin slot 1, SFNarrow) y PSK sin cambios.** Detalle en `09_general_vs_propia.md`.
- **2026-08-11 (2ª parte) — Paquete de auditoría para Claude**: creado `Contexto y Manuales\Navarrico4.3_Contexto_Auditoria_2026-08-11.zip` (13 ficheros: 3 docs + instrucción `INSTRUCCION_AUDITORIA_CLAUDE.md` + cerebro+subnotas) para auditoría quirúrgica externa. Los `.md` en disco siguen siendo el fuente editable.
- **2026-08-11 (2ª parte) — Doble auditoría (Claude + DeepSeek v4 Pro)**: núcleo sano (0 bugs en NodeDB/Router/AdminModule), 3 hallazgos de acción en `NavaCLIModule` (P0: `set_chem lifepo4` debe rechazarse en Seed/Xiao×2/T114 por LPCOMP fijo; P1: rate-limit 30s en canal Navadmin; P2: corregir Manual sobre `db_clear`→reenviar NodeInfo) + 6 de documentación (C6 main-nrf52 no bit-idéntico, C7 rol ROUTER, H2 delay3000, H6 comentario OCV, H7 low_voltage_counter 4/10, C5 vwake 1/2) + 2 código opcional (C4 factoryReset #ifdef muerto, C8 macro huérfana). Respuesta completa en `Desktop\respuesta_doble_auditoria.md`. **Pendiente de aplicar (espera orden del operador).**
- **2026-08-11 (2ª parte) — `delay(3000)` en cpuDeepSleep**: propósito confirmado por el operador (leer voltaje de batería + programar LPCOMP con éxito; evita bucle despertar/dormir con batería baja, p. ej. reset ATtiny). NO es código stock (verificado ausente en `Desktop\firmware`); origen Rama 1 (4.2). **Divisor Promicro/Faketec actual = 2×1M (0.5)**; los 2×10MΩ de `Descartables\report_fix.md` están deprecated. Documentado en `transfer_context.md` §4.A y `04_energia_bateria.md`.
- **2026-08-11 (2ª parte) — Mapa de hardcodeos creado** (`10_hardcodeos_nodo.md`): inventario verificado de dónde vive cada valor del nodo (radio/canal SFNarrow, rol router, claves admin K0/K1, ADC/LPCOMP/OCV por variante, resets `FIX_NATIVE_CORE_RESET`, `delay(3000)`, pre-check 3500 mV) + reglas para cambiarlos sin perderlos ni ponerlos en archivo equivocado. Referencia histórica: guía ODT 9/05/2026 (origen clave `{0xc7...}`, TX=8; `adc_multiplier_override=2.5` NO implementado en 4.3).
- **2026-08-11 (3ª parte) — FIXES DE AUDITORÍA APLICADOS (P0+P1+P2)** con snapshot previo `snap-20260811-195929.zip`:
  - **P0**: `set_chem lifepo4` rechazado con `ERR: LIFEPO4 NO COMPATIBLE, UMBRAL LPCOMP FIJO` en Seed/Xiao×2/T114 (LPCOMP fijo) — `#if defined(SEEED_SOLAR_NODE) || defined(SEEED_XIAO_NRF52840_KIT) || defined(HELTEC_T114)` dentro del bloque. Promicro/Faketec intactos (LPCOMP dinámico). Forma de DeepSeek (dentro del `else if`, no envolviéndolo).
  - **P1**: rate-limit genérico de **30 s** por `fromNode` en canal Navadmin (`static std::map<NodeNum,uint32_t> lastChannel1Cmd`, silencio sin revelar). Cubre los 13 comandos del canal 1, no solo `ping`.
  - **P2**: `Manual_NavaTastic.md` §5 corregido (`db_clear` → forzar reenvío de NodeInfo broadcast del mando, no basta reenviar un DM).
  - **Verificación**: las 6 variantes compiladas SUCCESS con `-e` real y distribuidas a UF2/OTA de Propia (11/08, 20:03-20:10). Queda pendiente test en banco (lifepo4 rechazado, rate-limit canal 1, ping sin cambios).
- **2026-08-11 (3ª parte) — P3-código aplicado (C8 + C4)**, con las 6 variantes recompiladas SUCCESS y redistribuidas a UF2/OTA de Propia:
  - **C8**: eliminada `USERPREFS_ALLOW_ROUTER_DEFAULT_ROLE` de los 6 `userPrefs.jsonc` (macro huérfana, nunca referenciada ni en stock ni en fork).
  - **C4**: restaurado el `NVIC_SystemReset()` inmediato al final de `factoryReset()` (rama activa, `FIX_NATIVE_CORE_RESET`) — diseño original de Antigravity (`implementation_plan.md`); eliminado el `#ifdef` muerto de la rama `#else`. Evita que hilos accedan a estructuras reconfiguradas en la ventana de ~25 ms antes del reboot programado. Documento de detalle: `Desktop\fix_implementado.md`.
- **2026-08-11 (3ª parte) — ⚠️ C4 REVERTIDO (rompía el reset de fábrica) + fix #10873 aplicado** (verificado en hardware Faketec):
  - **C4 REVERTIDO** en las 6: el `NVIC_SystemReset()` inmediato dentro de `factoryReset()` **rompía el reset de fábrica** (el flujo diferido del módulo ya gestiona el reboot: ACK → 3 s → `factoryReset()` → 25 ms → `Power::reboot()` con `NVIC_SystemReset()` limpio). Se restauró `return true;` simple y el `#ifdef` muerto de la rama `#else`. El diseño de Antigravity (reset inmediato) era redundante y perjudicial en la práctica.
  - **Fix #10873 (Meshtastic oficial, issue #10851 del operador)**: reordenar `disableBluetooth()` **después** de `factoryReset()`/`resetNodes()` en `AdminModule.cpp` (3 casos: factory_reset_config, factory_reset_device, nodedb_reset). El bug real no era `sd_nvic_SystemReset()` (2.7.26 ya usaba `NVIC_SystemReset()`) sino **parar el advertising BLE antes de la limpieza flash del reset** → el nodo se colgaba. PR oficial: https://github.com/meshtastic/firmware/pull/10873.
  - **Verificado en hardware (Faketec)**: factory reset por USB **y** por BLE → el nodo se resetea bien, vuelve a fábrica (nombre default, SFNarrow, EU_868, claves K0/K1 de fábrica), BLE vuelve a vincular. Persistencia de cambios del usuario (nombre, preset, canal, tiempos) tras soft reset: ✅. Auto-recuperación de claves en cada boot: ✅ (ver `02_claves_admin.md`).
- **2026-08-11 (3ª parte) — C4 revertido + fix #10873 aplicado en las 6 variantes**, compiladas SUCCESS y distribuidas a UF2/OTA de Propia (22:04-22:09):
  - `NodeDB.cpp` (6/6): sin reset inmediato en `factoryReset()` (ni en rama activa ni bloque muerto — unificado, `return true;` simple). El flujo diferido del módulo cubre el reboot (ACK → 3 s → factoryReset → 25 ms → `Power::reboot()`).
  - `AdminModule.cpp` (6/6): `disableBluetooth()` movido DESPUÉS de `factoryReset()`/`resetNodes()` en los 3 casos (config reset, device reset, nodedb reset) — fix oficial #10873.
  - Verificación hardware completa en Faketec: reset por USB ✅, reset por BLE ✅ (vuelve a vincular), persistencia tras soft reset ✅, auto-recuperación de claves ✅.
- **2026-08-11 (2ª parte) — Pautas operativas nuevas** (registradas por el operador): backup/rollback por marca de tiempo (`.bak-AAAAMMDD-HHMM` por archivo, `snap-AAAAMMDD-HHMMSS.zip` snapshot); autorización de proyectos (solo 4.3 editable salvo orden explícita puntual); método de verificación obligatorio en FASE 1; actualización continua del cerebro.
- **2026-08-12 — RAMA GENERAL COMPILADA Y DISTRIBUIDA (primera ronda)**: el operador copió los 6 folders de Propia a `Infraestructura General\` (renombrados `R2IG`) + creó `UF2\`/`OTA\` vacíos. Aplicado EN LOTE (norma `09` + 1 regla nueva del operador):
  - **K0 = Master Node** `{0xc7,0xdc,...0x00,0x55}` en los 6 `userPrefs.jsonc` (K1/K2 comentadas, 1 sola clave admin).
  - **`USERPREFS_FIXED_BLUETOOTH = 654321`** (regla nueva del operador; Propia: PIN propio, se pide al compilar — no se almacena).
  - `.clusterfuzzlite/router_fuzzer.cpp` ×6: `admin_key[0]`=Master Node, `admin_key_count=1`.
  - Compiladas las 6 con env real: Promicro/Faketec `nrf52_promicro_diy_tcxo`, Seed `seeed_solar_node`, T114 `heltec-mesh-node-t114`, Xiao×2 `seeed_xiao_nrf52840_kit_i2c` → **SUCCESS 6/6** (08:40-14:20 aprox). Distribuidas a `UF2\`/`OTA\` de General (nombres `R2IG`).
  - Backup previo: `userPrefs.jsonc.bak-20260812-0726` ×6 + `snap-20260812-072636.zip` (raíz 4.3).
  - Nota: la copia de folders la hizo el operador; el agente solo aplicó normas + compiló + distribuyó.
- **2026-08-12 — Fix H3 (a)+(a2) aplicado en `NodeDB.cpp` ×12 (sin compilar)** — plan `Desktop\plan_h3_agente_fix.md`, respuestas del auditor validadas:
  - **(a)** en `updateUser()` (bloque `newKeyIsAdmin`, ~línea 2105): al aceptar clave admin por NodeInfo → `info->bitfield |= NODEINFO_BITFIELD_IS_CRYPTOGRAPHICALLY_VERIFIED_ADMIN_MASK` (además de `is_favorite=true`). Rompe el círculo clave-stale → PKI no descifra → sin bitfield: **un solo NodeInfo broadcast acredita completamente** (canal 1 + DM).
  - **(a2)** en la rama de primera clave (`LOG_INFO("Update Node Pubkey!")`, ~línea 2113): si la primera clave coincide con `admin_key[]` → bitfield directo (elimina el vector "primera clave basura" para mandos legítimos).
  - **Backup**: `NodeDB.cpp.bak-20260812-1536` ×12 + `snap-20260812-153628.zip`. Paridad MD5 12/12 pre-edición (`6AB845EB…`) y post-edición (`225953D3…`). Grep verificado ×12 (FIX=1, bitfield=2, a2=2).
  - **NO compilado** (el operador añadirá una feature y compilará todo junto). Sinergia con canal Navadmin homogeneizado (12/08): tras desplegar ambos + factory reset, el mando solo necesita emitir un NodeInfo para quedar acreditado en canal y DM.
  - **Nota seguridad**: bitfield sin prueba de posesión (NodeInfo no firmado) → el atacante que forje un NodeInfo con clave admin solo gana respuestas read-only de whitelist (ya broadcast) e inmunidad menor de NodeInfo; no puede firmar DMs ni ejecutar destructivos. Aceptado por diseño (análisis completo en `Desktop\plan_h3_agente_fix.md` §4).
  - **8 jsonc sin canal 1** (Faketec, XiaoKitI2c, XiaoKitI2c+E22P, Seed × Propia R2IP + General R2IG): añadido `CHANNELS_TO_WRITE=2` + bloque canal 1 (Navadmin, PSK `{0x01}`, precision 0, up/down off) tras `CHANNEL_0_PSK` (plantilla Promicro).
  - **T114 ×2** (Propia + General): además REGION **EU_868** (descomentada, era US), CHANNEL_0 SFNarrow/PSK, CHANNEL_NUM 4, MODEM_PRESET MEDIUM_FAST, TX_POWER 22; corregida coma suelta del TZ y reindentadas las claves admin (valores intactos: Propia K0/K1 Promicro; General K0=Master Node).
  - **Backup**: `.bak-20260812-1503` ×10 + `snap-20260812-150316.zip` (raíz 4.3). **Verificación**: JSON parse OK ×12 (método del build), grep canal 1 OK, diffs vs .bak = solo los cambios previstos.
  - **Compilación 12/12 SUCCESS** (6 Propia + 6 General, cada una con su env real en su carpeta; 15:05-15:22) y **distribuidas** a `UF2\`/`OTA\` de ambas ramas vía `distribuir_binarios.ps1` (Promicro R2IG = build de las 07:37 vigente, jsonc sin cambios).
  - **⚠️ Despliegue**: las macros solo actúan en primer arranque/factory reset — flashear el build nuevo sobre nodos ya configurados NO añade el canal 1 (persiste `channels.proto`); los 3 nodos silenciosos necesitarán factory reset o escritura del canal por la app/CLI.
- **2026-08-11 (3ª parte) — Correcciones de doc (lagunas PASO 0)**: (1) `Manual_uso_NavaTastic_4.2.md` existe en 4.3 (`Contexto y Manuales\`, corregido 11/08 — químicas por placa); actualizados `cerebro.md` y `transfer_context.md` (el `.md` vive en 4.3, original en 4.2). (2) **Decisión del operador: los PDFs de transfer_context/guia_integracion NO se regeneran** (solo el PDF del manual de uso se entrega al usuario) — tarea P3-doc queda como pendiente opcional, no tocar. (3) `PROMPT_INICIALIZACION.md` alineado: "General hoy es copia exacta de Propia" → "General VACÍA (11/08); se recopilará desde Propia".
- **2026-08-12 (2ª parte) — LAB DESCARTADO + docs stale actualizadas**: (1) decisión del operador: el instrumento LAB (subnota 08) queda **DESCARTADO** — era un intento antiguo para arreglar el bucle sleep del Promicro; NO usar para test ni reproducción (además no incluye el rate-limit 30 s del canal Navadmin ni el guard de lifepo4 del build de producción). Test de banco H3/P0-P1 se hará con variantes reales. (2) Actualizadas docs stale (General ACTIVA 12/08): `01_ramas_variantes.md`, `transfer_context.md` Â§1/Â§2, `PROMPT_INICIALIZACION.md`, `GUIA_AGENTE_NAVTASTIC.md`, `06_compilar_distribuir.md`. Snapshot previo: `snap-20260812-155209.zip`.
- **2026-08-12 (3ª parte) — FEATURE fav auto + fix estético de fragmentación (x12, SIN COMPILAR)**: feature decidida por el operador: `/nava fav auto [on|off]` (default ON) controla el auto-favoriteo de routers directos 0-hop (`checkAndRegisterRAMAutoFavorite`: estrella + `activeDirectRouters`/bypass de hops). Flag persistido en `/resilience.bin` (`auto_fav` en `ResiliencePrefs`, compat ficheros viejos —> ON). Estado en `/nava status` y `/nava fav auto` sin arg. Fix estético: `enqueueResponse` corta a <=190 en límite de palabra/línea (`find_last_of(" \\n")`) sin partir comandos. Help actualizado (banner + `help fav`). Backup: `.bak-20260812-1614` x12 (NavaCLIModule.h/.cpp + NodeDB.cpp) + `snap-20260812-1614.zip` + rollback consolidado `rollback-20260812-1614.zip` (pre_favauto x36 + LEEME). MD5: NodeDB.cpp x12 = `5234C510`; NavaCLIModule.h x12 = `11AC29BB`; NavaCLIModule.cpp 2 grupos (8x `908B7A57` / 4x `9BF0FC5C`), diff solo set_txpower 0-22/0-12. **NO compilado** (el operador añadirá más cosas).
- **2026-08-12 (4ª parte) — Verificación campo LPCOMP Xiao (datos del operador)**: Xiao Kit i2c despierta a **~3.8V real** (11/08) y Xiao+E22P igual de bien — coherente con `3_8` teórico (~3.67V) + tolerancias/histéresis. **NO tocar los Xiao.** T114 (`2_8` ~4.04V) y Seed (divisor stock 3.3 vs 3.0) pendientes de verificar en banco (test de fuente regulable). Anotado en `04_energia_bateria.md`.
- **2026-08-12 (5ª parte) — AYUDA/CONSULTAS NavaCLI (x12, SIN COMPILAR)**: (1) interrogación genérica `/nava <cmd> ?` o `/nava <cmd> help` (whitelist: set_*, storm, ble, fav, ign, route, trace; `msg` EXCLUIDO); (2) comandos con parámetros SIN argumento `->` consulta rica (valor actual + opciones): set_chem (química actual + tabla cortes/despertar + [lifepo4 NO DISP en Seed/Xiao/T114, mismo #ifdef] + AVISO ROLLBACK SOLO: nrf erase), set_vbat/set_vwake (valor actual, vwake con aviso umbral fijo por placa), set_txpower/set_hops/set_role/set_mqtt/set_tz/set_name/ble/storm/fav/ign/route/trace; (3) AVISO `ROLLBACK SOLO: nrf erase` en los OK de los que persisten (set_chem, set_vbat, set_vwake, txoff, ble); (4) `fav auto` sin arg `->` AUTO-FAV: ON/OFF + auto-favs: N; (5) `help <comando>` usa usageAndState. Textos estilo SMS (abreviados). Nuevo: `NavaCLIModule::usageAndState()`. Backup: `.bak-20260812-1633` x12 + `snap-20260812-1633.zip` + rollback consolidado `rollback-20260812-1633.zip`. MD5: NavaCLIModule.h x12 = `B3565F59`; .cpp 2 grupos (8x `1C6BFD1B` / 4x `19AE69B5`), diff solo set_txpower 0-12/0-22. **NO compilado** (ronda pendiente).
- **2026-08-12 (6ª parte) — Hook de interrogación universalizado (x12, SIN COMPILAR)**: la interrogación `?/help` ahora funciona con CUALQUIER comando (regla: `ultimo token ?|help` → ayuda del comando raíz, p. ej. `ping ?`, `fav add ?` → uso de fav). Excluido `msg`. Corregido footgun: `fav add ?` creaba favorito !00000000 (parseaba ? como ID). MD5: .cpp 2 grupos (8x `E656C395` / 4x `8A9141DD`). Backup: `.bak-20260812-1639` x12 + `snap-20260812-1639.zip` + `rollback-20260812-1639.zip`.
- **2026-08-12 (9ª parte) — COMPILADAS 13/13 Y DISTRIBUIDAS (17:09-17:15)**: ronda completa con TODO lo pendiente (canal Navadmin jsonc + fix H3 + fav auto + fragmentación por palabra + ayuda/consultas + hook universal): 6 Propia + 6 General (4 rondas paralelas por env, SUCCESS 13/13 incl. Felix) y Felix Xiao Kit i2c (env `seeed_xiao_nrf52840_kit_i2c`) salida a `felix puerto venecia\` (17:15, MD5 idéntico al build). Distribuidas las 12 váa `distribuir_binarios.ps1` (UF2/OTA ambas ramas, MD5 vs .pio OK x12). Durante la compilación se corrigió un error introducido por el agente: faltaba la llave de cierre de `helpForCommand()` al insertar `usageAndState()` (corregido, repropagado, MD5 grupos: 9x `6C2EE6AB` / 4x `954C13F6`). Binarios previos archivados: `snap-binarios-previos-20260812-1706.zip` (12 MB, UF2/OTA de 15:11-15:22). **Despliegue**: flashear + factory reset en nodos ya configurados para materializar el canal 1 (los /prefs persisten al flashear).
- **2026-08-12 (10ª parte) — HANDOVER PREPARADO**: creada la subnota `11_rama1_plan.md` (plan de recreación de Rama 1 en 4.3: base = Propia, quitar lo Rama 2 puro, mantener energía/NavaCLI/fixes; 5 decisiones pendientes del operador). Actualizados: handover (sección 4), `GUIA_AGENTE_NAVTASTIC.md` (subnotas 10 y 11 añadidas), `PROMPT_INICIALIZACION.md` (11 subnotas), subnota `01_ramas_variantes.md` y `transfer_context.md` §1. Snapshot: `snap-20260812-171903.zip`.
- **2026-08-12 (11ª parte) — RAMA 1 CLIENTES CREADA Y COMPILADA (12/12 SUCCESS limpio)**: el operador copió R2→R1 (`Rama 1 Clientes en Infraestructura\`, sufijos R1IG/R1IP, 12 carpetas + UF2/OTA) y dictó las normas (detalle en `11_rama1_plan.md`):
  - **Rol CLIENT** (era ROUTER): `USERPREFS_CONFIG_DEVICE_ROLE=CLIENT` en los 12 jsonc + fallback `NodeDB.cpp` (`Role_CLIENT; // Default to client (Rama 1)`). `rebroadcast_mode LOCAL_ONLY` se mantiene. Efecto: el bypass de hops de Rama 2 queda inactivo por rol (no se tocó Router.cpp).
  - **Rol semi-permanente en `/resilience.bin`**: campo `role` (uint8_t, 0xFF=sin fijar) en `ResiliencePrefs`; `set_role` lo guarda y `loadResiliencePrefs()` lo aplica al boot con `installRoleDefaults()` → un cliente puede convertirse en router por DM y **sobrevive a factory reset** (bidireccional, client↔router). Compat ficheros viejos: guard `fileSize < sizeof(prefs)` extiende auto_fav=1 + role=0xFF.
  - **Todo lo demás se mantiene igual que R2** (filtros de guardado, eviction, TransmitHistory, H3, fav auto, canal Navadmin, energía). Decisión: NO se restaura el guardado completo de la DB (los filtros R2 ya persisten admins/backbone con sus claves → DM PKI sobrevive a reboot; pares no-admin se re-aprenden por aire).
  - **Backup**: `.bak-20260812-1806` ×48 (4 ficheros × 12) + `snap-r1-20260812-1806.zip`. MD5 post-edit: NodeDB.cpp ×12=`CD708322`, NavaCLIModule.h ×12=`F1668D8E`, .cpp 2 grupos (8×`CD6F41BB` 22dBm / 4×`4DB6CFB4` 12dBm). Verificado por grep 12/12 (fallback CLIENT, campo role, load apply, set_role save, 0xFF×2).
  - **Compilación**: 12/12 SUCCESS desde cero (se limpió el `.pio` heredado de la copia R2 — resultados no fiables). Distribuidos a UF2/OTA de ambas ramas R1 (MD5 12/12 OK). Nota: E22P×2 y Promicro×2 requieren fix MAX_PATH (ver error #13).
- **2026-08-12 (12ª parte) — ⭐ SNAPSHOT DE REFERENCIA: "NavaTastic 4.3 Eclipse Edition"**: el operador denominó así al firmware v4.3 que **distribuyó a sus colegas para pruebas** (motivo: eclipse, 12/08). **Eclipse Edition = build del 12/08 17:09-17:15 (13/13: 6 R2IP + 6 R2IG + Felix Xiao Kit i2c)** con TODO lo pendiente de esa fecha: canal Navadmin homogeneizado (jsonc ×12) + fix H3 (a)+(a2) + fav auto + fragmentación por palabra + ayuda/consultas + hook universal. **Es el ÚLTIMO build de Rama 2 distribuido: los UF2/OTA actuales de R2 y el binario de `felix puerto venecia\` SON Eclipse Edition** (Rama 1 NO está en Eclipse — es la iteración siguiente, aún no distribuida). **Uso**: punto de referencia de regresión — ante cualquier comportamiento anómalo, comparar con Eclipse Edition ("esto en Eclipse iba bien"). Binarios: UF2/OTA de ambas ramas R2 (12/08 17:15, MD5 vs .pio OK) + `felix puerto venecia\`; no hay zip propio (no se necesita: son los binarios vigentes de R2). Archivado de los binarios ANTERIORES a Eclipse (15:11-15:22): `snap-binarios-previos-20260812-1706.zip`.
- **2026-08-12 (8ª parte) — Build Felix Xiao Kit i2c (Puerto Venecia) al día con los pendientes (SIN COMPILAR)**: canal Navadmin en su jsonc (claves K0=LKC8/K1=EkjE intactas, JSON válido), NodeDB.cpp = General XiaoKitI2c R2IG (MD5 `5234C510`: H3 + gate fav auto), NavaCLIModule.h/.cpp = grupo 22 dBm (`B3565F59`/`E656C395`: fav auto + fragmentación + ayuda/consultas + hook universal), fuzzer con sus 2 claves (count=2). Backups: `.bak-20260812-1703` x5 + `snap-20260812-1703.zip`. PENDIENTE: `pio run -e seeed_xiao_nrf52840_kit_i2c` en su carpeta → salida a `felix puerto venecia\` (orden separada del operador).
- **2026-08-12 (7ª parte) — Documentación NavaTastic actualizada + PDFs de los manuales regenerados**: `Manual_NavaTastic.md` (help con interrogación universal, no-arg con estado/opciones, AVISO `nrf erase` en set_chem/set_vbat/set_vwake/txoff/ble), `transfer_context.md` (gate fav auto en 4.B, comandos 4.D, historial ronda 12/08), subnotas 03 y 05 (fav auto + interrogación), `Manual_uso_NavaTastic_4.2.md` (changelog fila 4.4 + referencia a interrogación). **PDFs regenerados (16:48)**: `Manual_NavaTastic.pdf` y `Manual_uso_NavaTastic_4.2.pdf` váa `generar_pdf.ps1`. **Aclaración decisión PDFs (11/08)**: la no-regeneración afectaba SOLO a transfer_context/guia_integracion (`tercer PDF que no sirve`); los manuales (firmware y comandos) SÍ se generan. Snapshots: `snap-20260812-164505.zip` (4 docs) + backup `Manual_uso_NavaTastic_4.2.md.bak-20260812-1647`.

### Errores encontrados → solución (consolidado, 2026-08-11)

| # | Error encontrado | Solución aplicada | Estado |
|---|---|---|---|
| 1 | `set_chem lifepo4` en Seed/Xiao×2/T114 (LPCOMP fijo ~3.67-4.04V) → nodo nunca despertaría por solar | P0: bloqueo con `#ifdef` en `set_chem` (ERR LIFEPO4 NO COMPATIBLE) | ✅ aplicado |
| 2 | Canal Navadmin (PSK pública) sin rate-limit salvo ping → abuso batería/airtime | P1: rate-limit 30 s por nodo en canal 1 | ✅ aplicado |
| 3 | `db_clear` deja al admin mudo (DM PKI usa clave de la DB) | P2: Manual corregido (reenviar NodeInfo broadcast, no DM) | ✅ aplicado |
| 4 | `USERPREFS_ALLOW_ROUTER_DEFAULT_ROLE` huérfana (falsa sensación de control) | C8: eliminada de los 6 `userPrefs.jsonc` | ✅ aplicado |
| 5 | `factoryReset()` con reset inmediato (C4) **rompía el reset de fábrica** | C4 REVERTIDO: `return true;` simple; el módulo ya programa reboot diferido (ACK→3s→factoryReset→25ms→Power::reboot) | ✅ revertido |
| 6 | Factory reset por USB/BLE dejaba el nodo colgado (issue #10851, tuyo) | Fix oficial **#10873**: `disableBluetooth()` DESPUÉS de `factoryReset()`/`resetNodes()` en AdminModule (3 casos) | ✅ aplicado + verificado HW |
| 7 | LPCOMP de Seed/Xiao/T114 con divisor ≠ 0.5 (umbral inalcanzable) | Fix LPCOMP: `#ifdef` por variante → `BATTERY_LPCOMP_THRESHOLD` (3_8/3_8/2_8) | ✅ aplicado 11/08 |
| 8 | `getActiveLpcompThreshold()` documentado como "COMÚN a las 6" (falso) | C6: doc corregida (difiere en `#ifdef` por placa) | ✅ doc |
| 9 | Clave admin borrada a mano → nodo sin admin | Auto-recuperación: `local_sum==0` en cada boot → re-inyecta K0/K1 de fábrica | ✅ verificado HW (ver `02`) |
| 10 | `generar_pdf.ps1` fallaba con ruta relativa | Script arreglado: resuelve rutas contra `Contexto y Manuales` | ✅ |
| 11 | Reset de fábrica por BLE "parecía no funcionar" (conservaba bonds) | No era bug: `--factory-reset` (config) conserva bonds; `--factory-reset-device` (full) los borra | ✅ documentado (ver `08`) |
| 12 | Cerebro al día pero docs enlazadas STALE (01/06, transfer_context, GUIA, PROMPT seguían diciendo General VACÍA tras activarla 12/08; PROMPT decía 6 subnotas cuando son 10) | Propagación de cambios a las docs enlazadas en la MISMA pasada + checklist PASO 0 (grep de estados en 01/06/transfer_context/GUIA/PROMPT) | ✅ aplicado 12/08 |
| 13 | **MAX_PATH (260) en Rama 1**: la ruta `Rama 1 Clientes en Infraestructura\...\XiaoKitI2c+E22P\` + libs hacía que `SparkFun_MMC5983MA_Arduino_Library_Constants.h` (incluido vía main.h→MagnetometerThread) quedara a 261-262 chars → `fatal error: No such file or directory` pese a existir el archivo (gcc no distingue de ENOENT real). Afectaba a Promicro×2 y E22P×2 (const=261/262; el resto ≤257). R2 no lo tenía (base 12 chars más corta). El `.pio` heredado de la copia R2 enmascaraba el error en builds incrementales. | **Fix**: `libdeps_dir` + `build_dir` cortos en `platformio.ini` (`C:/Users/Jesus/.platformio/{libdeps,build}/r1xxx`) en las 4 carpetas afectadas (backup `.bak-20260812-1806`). Los binarios de esas 4 salen fuera del proyecto (distribución manual con esa ruta). Regla: al compilar los 4 proyectos "largos" de R1 SIEMPRE con este ini; no paralelizar dos builds del MISMO env (corrompe la caché de downloads → libs rotas tipo lib sin Constants; los zips de caché de 1 byte son señal). Compilar el mismo env secuencialmente | ✅ aplicado + 12/12 limpio |

### Dependencias aprobadas
- Núcleo común idéntico en las 6 variantes (verificado contra Promicro).
- Diferencias permitidas entre variantes: `set_txpower` (0-12 E22P / 0-22 SX1262), `RADIO_POWER_ENABLE_PIN`, valores de variante (potencia/corte/LPCOMP/ADC). **No tocar ADCs de fábrica.**

### Tareas pendientes
- [x] (Sesión 2026-08-10): fix `updateUser` aplicado + compilado (SUCCESS x6) + claves unificadas + fuzzer limpio. **Verificar en campo** el flujo NodeInfo→DM PKI con mando de rescate.
- [x] Compilación completa de las 6 variantes de **Infraestructura Propia** (SUCCESS, 10/08/2026) y distribución de `.uf2`/`.zip` a `UF2\` y `OTA\` de Propia vía `distribuir_binarios.ps1`.
- [x] (Sesión 2026-08-10, tarde): reestructuración 4.3 verificada, herramientas IA actualizadas, doc de contexto + cerebro alineados, guía raíz `GUIA_AGENTE_NAVTASTIC.md` creada.
- [x] (2026-08-11): fixes LPCOMP por divisor real en 4 variantes (Seed, Xiao×2, T114) + paquete de auditoría zip.
- [x] (2026-08-11): fixes de auditoría P0 (lifepo4) + P1 (rate-limit canal 1) + P2 (Manual db_clear) aplicados, compilados (SUCCESS x6) y distribuidos a Propia.
- [x] (2026-08-11): P3-código C8 (macro huérfana eliminada) + C4 (reset inmediato factoryReset restaurado) aplicados, compilados (SUCCESS x6) y redistribuidos a Propia. Detalle: `Desktop\fix_implementado.md`.
- [x] (2026-08-11, noche): **C4 REVERTIDO** (rompía factory_reset) + **fix #10873** (disableBluetooth después del reset en AdminModule) aplicados en las 6, compilados SUCCESS y distribuidos. Verificado en hardware (Faketec): factory reset USB/BLE ✅, persistencia tras soft reset ✅, auto-recuperación de claves ✅.
- [x] (2026-08-12): **Rama General primera ronda**: copiada por el operador + norma 09 (K0=Master Node, 1 clave) + regla nueva `USERPREFS_FIXED_BLUETOOTH=654321` aplicadas ×6, compiladas SUCCESS 6/6 y distribuidas a UF2/OTA de General (R2IG). Detalle en log 2026-08-12.
- [x] (2026-08-12): **Homogeneización canal Navadmin**: 10 jsonc editados (8 con bloque canal 1 + T114 ×2 con bloque completo), **12/12 compiladas SUCCESS (6 Propia + 6 General) y distribuidas** a UF2/OTA de ambas ramas. Detalle en log 2026-08-12. **Pendiente despliegue**: nodos ya configurados necesitarán factory reset (o escritura del canal) para que Navadmin actúe.
- [x] (2026-08-12): **Fix H3 (a)+(a2) aplicado en NodeDB.cpp ×12** (bitfield 0x08 al aceptar clave admin por NodeInfo + primera clave). **NO compilado** (operador añadirá feature y compilará todo junto). Detalle en log 2026-08-12. **Pendiente**: compilar 12 + test banco §7 del plan (`Desktop\plan_h3_agente_fix.md`). **Rollback**: `rollback-20260812-*.zip` (raíz 4.3) + `NodeDB.cpp.bak-20260812-1536` ×12 + `snap-20260812-153628.zip`.
- [x] **(12/08, 17:15) Compiladas y distribuidas las 12 variantes + Felix (13/13 SUCCESS)**: canal Navadmin + H3 + fav auto + fragmentación + ayuda/consultas. hay cambios SIN COMPILAR = homogeneización canal Navadmin (jsonc ×10, 12/08) + fix H3 (a)+(a2) (NodeDB.cpp ×12, 12/08). El operador ha decidido la feature (fav auto + fix fragmentación) y sigue añadiendo cosas; cuando dé la orden, compilar TODO junto (4 rondas, env real por carpeta) y distribuir a UF2/OTA de ambas ramas. Los UF2/OTA de 12/08 (15:11-15:22) NO incluyen nada de esto.
- [ ] **(Excepción puntual) Build "Xiao Kit i2c R2IG Felix Puerto Venecia"** (`C:\Firmware Navarrico 4.3\Xiao Kit i2c R2IG Felix Puerto Venecia\`): copia de General XiaoKitI2c con claves propias de Felix (K0=LKC8…, K1=EkjE…, BT 654321). **Pendiente de aplicar**: canal Navadmin (jsonc, 6 líneas tras CHANNEL_0_PSK) + fix H3 (copiar NodeDB.cpp desde General XiaoKitI2c R2IG, hash 225953D3…) + opcional fuzzer con claves de Felix. Env `seeed_xiao_nrf52840_kit_i2c`; salida a `felix puerto venecia\`. Anotado en `PENDIENTES_APLICAR.md` dentro del folder. **No se toca en las rondas normales.**
- [ ] **Test en banco** de P0/P1: `set_chem lifepo4` rechazado en Seed/Xiao×2/T114, rate-limit 30 s en canal 1, `ping` sin cambios.
- [ ] **Test en banco del fix H3** (§7 de `Desktop\plan_h3_agente_fix.md`): rotar clave del mando → DM muere → forzar NodeInfo → DM + canal funcionan sin PKI previo; persistencia tras reboot; negativos (clave no-admin sigue silenciosa).
- [ ] **Test banco Rama 1**: (1) rol CLIENT visible como cliente en la malla + sin bypass de hops (favoritos no restan hop limit); (2) `set_role router` → conversión en caliente + tras factory reset se mantiene (semi-permanente); `set_role client` → vuelta; (3) NIMH en banco con Faketec y XiaoKitI2c (compatibilidad declarada por el operador); (4) compat de ficheros `resilience.bin` viejos (R2→R1 no debe romper química/vbat/auto_fav).
- [ ] **Regresión contra Eclipse Edition** (baseline 12/08 17:09-17:15, distribuido a colegas): R2 no ha cambiado desde entonces — si algo falla en el despliegue/uso de R2, es estado/config de campo, no firmware (comparar con Eclipse antes de tocar código).
- [ ] **Test banco fav auto**: `/nava fav auto off` → NodeInfo de router 0-hop NO genera estrella ni bypass; `on` → recupera; `fav auto` sin arg muestra estado; persistencia tras reboot (`/resilience.bin`); `help`/`fav ls` sin cortes a mitad de palabra.
- [ ] Verificar en campo la rama **General** (claves Master Node) cuando el operador la despliegue.
- [ ] Revisar `old\` (variantes deprecated) si se quiere purgar claves antiguas — no afecta activos. **No copiado a 4.3**: vive en `C:\Firmware Navarrico 4.2\Rama 2 Infraestructura\Codigo Rama 2\old\`.
- [ ] P3-doc **opcional** (decisión del operador 11/08)
- [ ] **Rama 1 en 4.3** (plan en subnota `11_rama1_plan.md`): decidir alcance/estructura con el operador y ejecutar cuando ordene.: NO regenerar PDFs de transfer_context/guia_integracion; solo el PDF del manual de uso se entrega al usuario.

---

## 4. HANDOVER (resumen de traspaso)

- **Objetivo**: mantener el proyecto en estructura 4.3 (dos ramas de infraestructura General/Propia) con la esencia y normas técnicas intactas (parches Rama 2, claves admin, seguridad `/nava`, energía, protección Flash). Flujo de trabajo: nueva sesión → leer `GUIA_AGENTE_NAVTASTIC.md` (raíz) → cargar este cerebro + subnotas → cargar doc de contexto.
- **Decisiones tomadas**: reestructuración 4.3 (`Infraestructura General|Propia\<variante> R2IG|R2IP`); `distribuir_binarios.ps1` y `generar_pdf.ps1` en `HerramientasPropiasIA\`; plantilla LaTeX versionada a 4.3; documentación de contexto (3 .md) y cerebro actualizados a 4.3; guía raíz `GUIA_AGENTE_NAVTASTIC.md` como punto único de entrada; fixes LPCOMP por divisor real (Seed/Xiao×2/T114, 11/08); **General = 1 sola clave admin (K0 Master Node) vs 2 de Propia; canales/PSK sin cambios**; **General operativa 12/08** (K0=Master Node, BT 654321); **canal Navadmin homogeneizado en las 12 (12/08)**; **fix H3 (a)+(a2) aplicado en NodeDB.cpp ×12 (12/08, SIN COMPILAR)**; diagnóstico de administración remota documentado (`Desktop\problema_administracion.md`), propuesta de ejecución (`Desktop\fase_prefia_fix.md`) y plan de fix (`Desktop\plan_h3_agente_fix.md`); paquete de auditoría zip creado.
- **Estado actual (12/08, 17:15)**: **13/13 COMPILADAS Y DISTRIBUIDAS** (6 Propia + 6 General + Felix Xiao Kit i2c) con TODO lo pendiente: canal Navadmin (jsonc ×12) + fix H3 + fav auto + fragmentación por palabra + ayuda/consultas + hook universal. UF2/OTA de ambas ramas actualizados (MD5 vs .pio OK) + `felix puerto venecia\` (Felix). Binarios previos archivados: `snap-binarios-previos-20260812-1706.zip`. LAB **DESCARTADO (no usar)**. Backups/rollbacks del día: 1503/1536/1537/1614/1633/1639/1703/1706/1719 (detalle en log). Docs y PDFs de manuales al día. Rama 1: plan en subnota `11` (no ejecutado).
- **Estado actual (12/08, ~19:00, añadido)**: **RAMA 1 CLIENTES CREADA** (`Rama 1 Clientes en Infraestructura\`, R1IG/R1IP, 12 carpetas): rol CLIENT + rol semi-permanente en `/resilience.bin`, 12/12 compiladas desde cero SUCCESS y distribuidas a UF2/OTA R1 (MD5 OK). Fix MAX_PATH en 4 carpetas (error #13). **Distribución Desktop**: creado `HerramientasPropiasIA\distribuir_desktop.ps1` → `Desktop\NavaTastic 4.3 120826\` con `Rama 1 Clientes\` y `Rama 2 Routers\` × `LIPO/NIMH` × `UF2/OTA` (NIMH = solo Faketec + XiaoKitI2c, norma del operador; 32 ficheros por rama, verificado).
- **Siguiente paso**: **(1) TEST EN BANCO** (variantes reales, LAB descartado): P0/P1 (lifepo4 rechazado, rate-limit 30s canal, ping), H3 (§7 plan_h3), fav auto (off sin estrella/bypass, on recupera, persistencia), ayuda/consultas (no-arg, `?`, AVISO nrf erase), fragmentación sin cortes, **Rama 1** (rol CLIENT visible, conversión semi-permanente `set_role`, NIMH Faketec/XiaoKitI2c, compat `resilience.bin` viejo); (2) **DESPLIEGUE**: flashear + **factory reset** en nodos ya configurados (materializar canal 1) → NodeInfo del mando → verificar canal Navadmin + DM (los 3 silenciosos: Faketec ×2 y Xiao Kit i2c); (3) verificación campo: Estella (Promicro) y General (mando de rescate); Xiao ~3.8V ya verificado; (4) **RAMA 1** (subnota 11): decisiones restantes (position on/off, TX power, alcance variantes móviles); (5) P3-doc opcional: PDFs de transfer_context/guia NO regenerar (decisión 11/08).

---

## 5. REPO UNIFICADO (14/08/2026) — ESTADO VIGENTE

> Esta sección es la que manda. Las secciones 1-4 y subnotas 01-12 documentan el
> proyecto histórico 4.3 (24 repos, 12/08); su contenido técnico sigue siendo la
> verdad de comportamiento, pero la **estructura física es la del repo único**.

### 5.1 Qué es el repo unificado

- **Ubicación**: `C:\NavaTastic Codigo completo` (fork Meshtastic 2.7.26, base `54e0d8d`).
- **Un solo código → 12 firmwares** elegidos por env (`variants/nrf52840/navarrico.ini`):
  `navarrico_<placa>_<radio>_<rama>` — 6 placas/radios × R2IG (Routers) / R1IG (Clientes).
- **Perfiles** `profiles/<RAMA>_<Placa>.jsonc`: claves admin, canal Navadmin, rol, BT
  (12 ficheros = copias 1:1 de los jsonc originales; el `userPrefs.jsonc` raíz es el
  perfil por defecto R2IG Promicro).
- **Macros ortogonales**: `NAVARICO_RADIO_E22P`/`NAVARICO_RADIO_SX1262` (potencia/pin/OCV/
  set_txpower) y `NAVARICO_RAMA_1` (rol CLIENT + rol semi-permanente en `/resilience.bin`).
- **Scripts**: `build.ps1`, `distribuir.ps1` (→ `distribucion\Rama 1 Clientes|Rama 2 Routers
  × LIPO|NIMH × UF2|OTA`, 32 ficheros), `verificar_paridad.ps1` (modo paridad + MD5).
- **⭐ PARIDAD 12/12 byte-idéntica (14/08)**: los 12 UF2 del repo = binarios originales
  (Eclipse Edition R2IG + R1IG), con BUILD_EPOCH 12/08, APP_VERSION `2.7.26.54e0d8d`,
  marcas temporales y rutas de libdeps fijadas por env (detalle en `BITACORA_TECNICA.md`).
- **Solo General (R2IG/R1IG) migrada**. Propia (R2IP/R1IP) = perfiles + 12 envs futuros,
  sin tocar código (perfiles gitignored). Felix fuera. GitHub pendiente (solo General).
- Guías vivas: `Guia_para_agente_sobre_NavaTastic.md` (qué es cada cosa y dónde se toca),
  `BITACORA_TECNICA.md` (fallos/fixes del proceso), `PLAN_DE_TRABAJO.md` (avance),
  `PORTING_NUEVO_FORK.md` (guía maestra de portabilidad a forks nuevos).

### 5.2 Registro de estado (log) — sesión 14/08 (portabilidad)

- **2026-08-15 (14ª parte) — FRENTE A CAUSA RAÍZ + fix FRENTE B (sesión de banco)**: diagnóstico en
  banco (test node COM15 + observador COM9, 869.545): (1) **RF**: con TX 8 dBm del E22P los TX del
  test node llegaban corruptos al observador (numPacketsRx=110, RxBad=109; consejo del operador:
  picos de corriente del E22P al TXear) → bajado TX a 1 dBm (`--set lora.tx_power 1`): TODO decodifica
  (SNR ~12), nodeinfo reaprendido (clave nueva s1HpF…). (2) **CAUSA RAÍZ del FRENTE A (código)**: los
  mensajes [Sueño]/[Vivo]/[Listo] y los diags TEMP por canal 1 se encolaban con **destino 0**
  (`enqueueResponse(0, 1, …)`) en vez de `NODENUM_BROADCAST`: `isBroadcast(0)=false` → el paquete
  TXea pero nadie lo entrega (muere en el aire, invisible en API). El PONG sí llegaba porque usa
  NODENUM_BROADCAST. Bug presente desde FASE 2 V2 (verificado en .bak-20260814-1819). **Fix**: 6
  ocurrencias en NavaCLIModule.cpp (351/369/1465/1477/1488/1498) → NODENUM_BROADCAST. (3) **FRENTE B
  (F16a)**: aplicado `nodeDB->saveToDisk(SEGMENT_NODEDATABASE)` tras acreditar/favoritear admin en
  AdminModule.cpp (solo si cambia algo — protección Flash; el save filtrado ya persiste
  favoritos/admins/direct routers/ignored). Compilado SOLO env banco
  (`navarrico_promicro_e22p_r2ig`, SUCCESS 62.99s, UF2 MD5 0ddd16a5…) — pendiente flash + verificación
  en banco. Backups código `.bak-20260815-0100` (NavaCLIModule/AdminModule). Nota: USERPREFS_NODEDB_
  RAM_ONLY no se referencia en src (macro inerte); loadFromDisk carga nodes.proto filtrado → el
  bitfield admin persiste. Abierto: PKI_SEND_FAIL_PUBLIC_KEY esporádico en el test node (~uptime 243s,
  origen sin identificar — candidato F17).
- **2026-08-15 (15ª parte) — VERIFICADO EN BANCO: fixes A+B flasheados (pio -t upload, nrfutil,
  113.93s)**: (1) FRENTE A ✓✓: el observador recibe por canal 1 el bootDiag
  ("F15DBG precheck: wasInSleep=0 gate=3500 ... low=0/5") al primer tick y el HB-60s
  ("F15DBG HB bat=4053 usb=1 sleepMsgs=1 rol=2...") cada minuto → el camino de [Sueño]/[Vivo]/[Listo]
  (mismo enqueue NODENUM_BROADCAST canal 1) queda probado punta a punta. rol=2 ROUTER persistente
  tras flash (F15 sigue OK). (2) FRENTE B ✓ (nivel síntoma): DM `/nava ping` del observador → PONG
  ANTES y DESPUÉS de `--reboot` del test node (el observador NO re-anunció nodeinfo → el bitfield
  admin vino del disco). Matiz técnico: el bitfield pudo llegar al disco también por el save de
  updateUser/H3 (NodeDB.cpp:2153-2165, throttled 1/min) tras el nodeinfo del observador; el fix F16a
  cubre el caso exacto del operador (acreditación SOLO por AdminMessage sin nodeinfo posterior →
  ahora guarda al instante). Reproducción dirigida del caso exacto queda pendiente (requiere
  limpiar la entrada del observador y rompe el PKI del DM). (3) Pendiente 4.5: test real de sueño
  con la fuente SIN USB en el test node (getHasUSB() bloquea la detección de batería baja).
- **2026-08-15 (16ª parte) — 4.5 VERIFICADO: CICLO SUEÑO/DESPERTAR COMPLETO EN BANCO**: con el test
  node SOLO en fuente (usb=0) y el observador escuchando: bajada a ~3.4V → monitor cuenta 5
  lecturas bajas → `F15DBG lowbat: ... bat=3375 usb=0` → **[Sueno] ... ADC 3375 mV | INA 3.40 V
  -51 mA DESCARGANDO | despertara >= 3710 mV** recibido → HBs en silencio (radio apagada, dormido).
  Subida a 4V → LPCOMP despierta (~3710 mV) → `F15DBG precheck: wasInSleep=1 gate=3710 corte=3500
  low=0/5` → **[Listo] ... ADC 3772 mV | despierto, cargando** → HBs de vuelta. Los 3 mensajes
  [Sueño]/[Vivo]/[Listo] llegan por canal Navadmin: **FRENTE A CERRADO** (el [Vivo] no salió en este
  ciclo porque V saltó directamente de 3.4 a 4V; la rama [Vivo] es para V en [corte, LPCOMP) —
  opcional test intermedio a ~3.55V si se quiere). Siguiente: CIERRE 4.7 (retirar instrumentación
  TEMP F15 → build banco → 12 envs → distribuir -Todo -V2 → manual/PDF → docs → commit).
- **2026-08-15 (17ª parte) — CIERRE F15: instrumentación retirada + docs actualizadas (pendiente
  flash limpio + 12 envs + distribución)**: retirada TODA la instrumentación TEMP F15 del código
  (verificado: 0 restos en src): logs F15DBG, breadcrumbs AD/LR/SR/XX, watchdog 1s, HB-60s canal
  1, bootDiag (mecanismo navaSetBootDiag/GetBootDiag eliminado de .h/.cpp), logs de escritura,
  `return 1000`→60000, contador Power a LOG_DEBUG. Los fixes F15/A/B reales quedan intactos
  (remove-antes-de-escribir, gates versión/tamaño, saneado, substr(9), NODENUM_BROADCAST,
  saveToDisk acreditación). Build banco LIMPIO SUCCESS 66.11s (UF2 MD5 `f5cb93cd6f...`), sin
  flashear aún. Docs actualizadas (norma 0.11: backups `.bak-20260815-0137`): manual de comandos
  (adenda 15/08, set_role ambas ramas, sección 8 verificada, notas de despliegue, código fuente
  de referencia al repo), transfer_context (ronda 15/08 con L13-L18), guia_integracion (bloque
  O.6), GUIA_AGENTE_NAVTASTIC (adenda 15/08), PORTING_NUEVO_FORK (inventario + trampas 16-18).
  Pendiente: PDFs (generar_pdf.ps1) + flash limpio + smoke + 12 envs + distribuir -Todo -V2 +
  commit local.
- **2026-08-15 (18ª parte) — SANEADO DE CLAVES + MECANISMO PROPIA SIN ALMACENAR (pre-GitHub)**:
  auditoría de fuga de claves ordenada por el operador. **Eliminado del repo**: (1) la K1 Propia
  `{0x3f,0x38,...}` que estaba comentada con hex completo en `userPrefs.jsonc` + 12 perfiles;
  (2) los prefijos truncados de K0/K1 Propia en 02_claves_admin/07_estella/09_general_vs_propia/
  transfer_context (sustituidos por "valores no publicados"); (3) todas las referencias a BT
  123457 (Propia: PIN propio, se pide al compilar). **Conservado** (decisión del operador): clave
  privada+pública del Master Node en el manual y pública en perfiles/fuzzer (General pública).
  `.gitignore` += `*.bak-*` (los backups pueden contener claves). **Mecanismo Propia nuevo**
  (probado 3/3: IG sin regresión, IP sin vars → error claro, IP con vars → SUCCESS): opción de
  env `custom_meshtastic_propia_keys=true` en `bin/platformio-custom.py` (inyecta
  `USERPREFS_USE_ADMIN_KEY_0/1` + `USERPREFS_FIXED_BLUETOOTH` desde `NAVARICO_PROPIA_KEY_0/1` y
  `NAVARICO_PROPIA_BT`); 12 envs `R2IP_*/R1IP_*` en navarrico.ini (extienden los General);
  `build_propia.ps1` pide las claves de forma interactiva y NO las guarda. **Las claves Propia
  no existen en ningún fichero del repo** (verificado por grep, 0 restos). Para GitHub queda:
  repo nuevo con un solo commit del árbol saneado (el historial actual contiene claves de
  commits del 14/08). Backups `.bak-20260815-0200` (25 ficheros).
- **2026-08-15 (19ª parte) — V2.4: rediseño [Vivo] + [Boot] diferido + temp de chip (verificado en banco)**:
  (1) **Bandas de despertar rediseñadas** (el gate ya no es LPCOMP sino el corte OCV): al venir de
  sueño: V < corte−100 → silencio+re-sueño; [corte−100, corte) → **[Vivo]** + re-sueño (E22P
  3400-3500 / SX1262 3300-3400); V ≥ corte → boot normal → **[Listo]** y el nodo OPERA. (2)
  **Nuevo aviso [Boot] diferido 2 min** (idea del operador): todo arranque que NO venga del ciclo
  de sueño manda un aviso con **causa del reset** (RESETREAS: WDT/RESETPIN/SOFT/LOCKUP/LPCOMP/VBUS)
  — el retraso es el anti-bucle (un nodo en ciclo de resets nunca llega a 2 min). **Verificado en
  banco**: el operador recibió el [Boot] en su nodo personal por Navadmin (869.618). (3) Los 3
  mensajes + [Boot] incluyen **temperatura del chip** (sd_temp_get del nRF52; los sensores I2C no
  están disponibles en esos momentos). (4) TX en banco con USB = 1 dBm (regla del operador, los
  picos del E22P corrompen frames). (5) **Corrección L16**: el eco de TX propios en la API local
  NO aplica a los envíos del NavaCLI (sendToMesh sin ccToPhone) — para verificar avisos hay que
  escuchar con un nodo observador, no con el emisor. Pendiente: test de la banda [Vivo]
  (fuente 3.45V + reset) → 12 envs → distribuir → commit.
- **2026-08-15 (20ª parte) — V2.6: ciclo sueño/despertar definitivo (verificado en banco, EUREKA del
  operador)**: correcciones sobre el V2.4: (1) **[Vivo] ya NO re-duerme**: anuncia y el nodo OPERA;
  el monitor runtime (5 lecturas reales a 20s ≈ **100s**) decide dormir — restaurado el filtro
  anti-falsos positivos de Eclipse (RF/temperatura/transitorios pueden dar lecturas ADC puntuales
  erróneas). (2) **Fix contador pre-cargado**: el pre-check llamaba `readPowerStatus(true)` ×5 y
  pre-cargaba `low_voltage_counter` a 5 → el primer tick (~20s) dormía el nodo recién arrancado.
  Fix: el contador solo cuenta con `!force` (Power.cpp). (3) **Dormir TODO como Eclipse**: el sueño
  diferido ejecuta `doDeepSleep(portMAX_DELAY, false, true)` (preflight + `RadioInterface::sleep()`
  + GPS + pantalla) en vez de `cpuDeepSleep` directo — las SX1262 sin corte de radio se quedaban a
  ~5-10 mA dormidas. (4) **LED apagado antes de System OFF** (main-nrf52.cpp, tras la estabilización):
  un LED enclavado consumía ~10 mA. (5) **Avisos solo con ADC + CPU** (chip; reintento si BUSY):
  fuera la INA del contenido (el operador: el I2C no está disponible en esos momentos). Verificado
  en banco: [Vivo] → opera 100s → [Sueño] → dormido **~1 mA** → LPCOMP ~3.7-3.8V → [Listo]; [Boot]
  a los 2 min con causa. **Comportamiento = Eclipse V1 + avisos encima** (referencia histórica en
  subnota 04, sección nueva). Backups `.bak-20260815-0301/0315/0352/0418/0517`.
- **2026-08-15 (21ª parte) — SNAPSHOT V2 + COMMIT DEL HITO**: commit local `80e9f7e14` (V2.6,
  12 ficheros, +249/−63: código main/NavaCLI/Power/main-nrf52 + docs completas). **Snapshot
  `_archivo\NavaTastic Eclipse Edition V2 - Unificado 20260815 (HEAD 80e9f7e14).zip`** (5.26 MB,
  2204 entradas, árbol limpio del commit vía `git archive`: fuentes + config + perfiles +
  scripts + docs/cerebro, SIN .pio/.git/binarios/distribucion/PDFs/baks — mismo criterio que
  el baseline del 14/08). Rollback de este punto: `git checkout 80e9f7e14` o descomprimir el
  zip sobre la raíz. Pendiente: 12 envs + `distribuir.ps1 -Todo -V2` + commit de cierre.
- **2026-08-15 (22ª parte) — GITHUB PUBLICADO (EA2OY/NavaTastic) + README completo + RELEASE v2.6**:
  - **Publicación**: rama `main` de https://github.com/EA2OY/NavaTastic = UN solo commit raíz
    (rama huérfana `github-public` regenerada en cada publicación). El historial local (con
    claves Propia de commits del 14/08) NO sube nunca. Ficheros gitignored (`.bak-*`,
    `_archivo/`, `docs/pdf`, `distribucion/`) se suben solo los elegidos con `git add -f`
    (distribucion + PDFs) y `.github/workflows` (CI upstream) se EXCLUYE de la rama pública +
    **Actions desactivadas** vía API (`PUT .../actions/permissions` enabled:false) — los
    workflows de releases upstream disparaban correos de error al crear el release.
  - **Release v2.6** (tag v2.6, id 370958405): 26 assets (12 UF2 + 12 OTA + 2 PDFs) subidos
    por API (`uploads.github.com`). Las descargas del README apuntan a Releases (panel derecho),
    no a las carpetas.
  - **README bilingüe (ES/EN)**: qué aporta, tabla completa de comandos `/nava` con accesos,
    sección NavaTastic + **MeshNavarra-Utility** (proyecto hermano del operador:
    https://github.com/EA2OY/MeshNavarra-Utility — la app envía los comandos como mensajes
    predefinidos, sin escribir), divisor ADC 1M+1M + tip/LPCOMP, químicas de batería, PIN BT
    654321, backup de claves, gestión de claves admin (2 propias + desautorizar la de fábrica
    en slot 0 — el slot 0 vacío se re-inyecta), estado de pruebas por placa, licencia GPL v3
    + cumplimiento (fuente de los binarios en el mismo commit) + disclaimer.
- **Trampas de la publicaci�n (L24-L26 en BITACORA)**: el checkout entre ramas BORRA del
    disco los ficheros force-add que la otra rama no trackea (distribucion/workflows) ?
    repoblar `distribuir.ps1 -Todo` tras cada publicaci�n y regenerar la hu�rfana borr�ndola
    antes; el token GitHub del operador se comparti� por chat ? REVOCAR y usar uno puntual
    por sesi�n (o gh CLI).
- **2026-08-15 (23� parte) � HANDOVER FINAL (cierre de sesi�n)**: sesi�n cerrada y
  commitada (master `9d45c2bbf`). **SNAPSHOT FINAL**: `_archivo\NavaTastic Eclipse Edition
  V2 - FINAL 20260815 (HEAD 9d45c2bbf).zip` (5.85 MB, 2205 entradas) � versi�n final y
  completa (V2.6 + README biling�e + docs GitHub + escudo), reemplaza al intermedio del
  80e9f7e14. **PROMPT DE RETOMA reescrito** en PLAN_DE_TRABAJO.md (estado V2.6 + GitHub +
  snapshot FINAL + c�mo trabajar sobre el c�digo + Propia + trampas).
  `docs\INSTRUCCION_AUDITORIA_CLAUDE.md` actualizado al repo unificado con el material
  personal excluido (orden del operador). Queda en manos del operador: auditor�a externa
  del c�digo (Claude, amigo del operador) y prompt de auditor�a de MeshNavarra-Utility.
- **2026-08-15 (24ª parte) — RONDA AUDITORÍA EXTERNA (Claude, pack 14/08) analizada y resuelta**:
  la auditoría encontró 1 bug "MEDIO" + 1 "BAJO" + 2 observaciones + 1 confirmación. Contrastada
  contra el código VIVO: (1) §1 Seed 3670 mV en `navaGetLpcompWakeMv` — **RECHAZADO el fix
  propuesto (4084)**: el operador verificó en banco (firmware 4.2, mismo `3_8`) que el Seed
  despierta a **~3,8 V** → divisor efectivo ~0,326, NO 0,303 (la teoría desde ADC_MULTIPLIER 3.3
  falla ~300 mV; L27 en BITACORA). Hardware idéntico 4.2→actual (variant.h LPCOMP byte-idéntico;
  única diferencia HYST_NOHYST→ENABLED 50 mV, diseño 4.3/V2). El número del aviso es solo
  informativo; pendiente opcional re-medición fina en banco (~3800). (2) §2 help listaba `power`
  en [Q] sin whitelist — **FIX APLICADO** (movido a [E] + "SOLO DM SEGURO"; cosmético). (3) §3
  F16c confirmado correcto (`substr(7)`) → CERRADO. (4) §4 TEMP F15 y §5 gate version: ya
  resueltos el 15/08 (verificado). (5) §6 migración 80B: por diseño, riesgo 0. (6) F17: anotada
  explicación candidata (PKI estándar, clave no aprendida antes del 1er NodeInfo). Backups
  `.bak-20260815-1558`. Detalle: BITACORA "RONDA AUDITORÍA EXTERNA 15/08".
- **2026-08-15 (25ª parte) — PUBLICACIÓN GITHUB v2.6.1**: rama huérfana regenerada (UN commit,
  árbol saneado sin workflows) → `main` actualizada. **Release v2.6.1** (id 371073616): 26 assets
  (12 UF2 + 12 OTA + 2 PDFs) subidos por API; v2.6 anterior conservado. Credencial: la del
  Administrador de credenciales de Windows (recomendado rotarla, L26). Higiene: `.bak` histórico
  untracked (L28) + L24 aplicada (distribucion\ repoblada, PDFs regenerados). Detalle: BITACORA.
- **2026-08-15 (26ª parte) — README: CLAVE DE FÁBRICA = HERRAMIENTA DE RESCATE + PDFs BILINGÜES**:
  README (ES/EN): la advertencia de "cambia la clave por defecto" ahora explica que la clave
  admin pre-hardcodeada es la **llave de rescate integrada del proyecto** (tras un
  restablecimiento duro —factory reset accidental, `nrf erase`, corrupción— el nodo vuelve con
  esa clave y el operador puede reentrar por DM, restaurarlo y dejarlo con la clave de su dueño;
  por eso se re-inyecta si el slot 0 queda vacío). Añadidas **instrucciones sencillas para
  cambiar la clave pre-hardcodeada a mano con VS Code** (editar `profiles/<RAMA>_<Placa>.jsonc`
  → `USERPREFS_USE_ADMIN_KEY_0` con tu clave pública en hex, base64→hex; recompilar `pio run -e
  <env>`; aviso: se pierde el canal de rescate del proyecto). Versión del README → V2.6.1.
  **Manuales bilingües**: traducción EN completa añadida al FINAL de
  `Manual_NavaTastic.md` y `Manual_uso_NavaTastic_4.2.md` (el ES sigue arriba y es la fuente de
  verdad); PDFs regenerados (98/126 KB) con `generar_pdf.ps1` (norma 0.11: backups
  `.bak-20260815-1729`). Backups: README/manuales `.bak-20260815-1729`.
- **2026-08-15 (27ª parte) — DESCARGO AMPLIADO + PUBLICACIÓN FINAL DEL DÍA**: README (ES/EN)
  y `Manual_uso_NavaTastic_4.2.md` (ES + apéndice EN) reciben el **descargo ampliado**: las
  instalaciones deben cumplir la normativa aplicable (nacional/autonómica/local/europea:
  emplazamiento, permisos, seguridad, medio ambiente); dónde y cómo se monta es
  responsabilidad exclusiva del instalador; el proyecto queda **desvinculado** de montajes o
  usos de terceros que no se ajusten a la legislación. PDFs regenerados. Publicado a GitHub
  (rama huérfana → main + PDFs del release v2.6.1 sustituidos). Detalle: BITACORA.
- **2026-08-15 (28ª parte) — PROTOCOLO DE RESCATE ACTUALIZADO (dato del operador)**: ya NO
  hace falta la app 2.7.10: la app **actual** de Meshtastic (Play Store) permite cambiar la
  clave privada. Operativa (manual de uso ES+EN y README ES+EN): **Ajustes → Seguridad** del
  nodo de mando → borrar el campo **"Clave Privada"** → pegar la privada del proyecto →
  **guardar/enviar** → la clave pública correcta **se regenera sola**. Si no se queda
  aplicada, repetir (bug conocido de la app de Meshtastic). PDFs regenerados. Backups
  `.bak-20260815-1812`.
- **2026-08-15 (29ª parte) — 4 DE 6 PLACAS VERIFICADAS + hallazgos de código anotados**:
  - **Verificadas por el operador**: Xiao Kit i2c (SX1262) y Xiao Kit i2c + E22P (15/08) y
    Faketec HT-RA62 (14/08). El "fallo" del Xiao+E22P era el pico del E22P en TX — **L29:
    aplica a TODAS las placas E22P** (regla de banco ya documentada en L14/L18/README);
    SX1262 = 22 dBm siempre (decisión del operador). Pendientes: Seed y T114.
  - **Hallazgos de código (anotados, NO tocados)**:
    1. Contador de baja ASIMÉTRICO: `>4` (≈100s) solo Promicro/Faketec vs `>10` (≈220s) en
       Seed/T114/Xiao×2 (Power.cpp); la macro `USERPREFS_LOW_BATTERY_READINGS_COUNT` está
       "medio huérfana" (la usa el pre-check de main.cpp, NO el monitor runtime) → candidato
       **F18**: decisión del operador (15/08) = **8 lecturas para TODAS** (≈160s), perfiles a
       `=8` + Power.cpp leyendo la macro.
    2. `USERPREFS_LORACONFIG_TX_POWER` de los perfiles es **configuración MUERTA** (0 refs en
       src): el default real de TX vive en Channels.cpp (8 E22P / 22 SX1262) y es IDÉNTICO
       recién flasheado y tras factory reset (mismo camino `resetRadioConfig`→`initDefaults`).
    3. Xiao: `isVbusIn()` = tensión >4200 mV (sin detección VBUS real) → en banco con fuente a
       4,2 V el contador puede resetearse silenciosamente.
    4. Diff 4.3 (que funcionaba) → actual: el sueño pasa por `handleLowBatteryEvent` (TX del
       [Sueño] ANTES de dormir) y el pre-check de arranque tiene 3 bandas; LPCOMP/hyst/
       delay(3000) idénticos.
  - **Decisión del operador**: SX1262 SIEMPRE a 22 dBm (no alinear con el 8 del E22P); con
    SX1262 no hace falta bajar potencia en banco. **Faketec HT-RA62 probada por el operador el
    14/08 → verificada** (4 de 6 placas verificadas; pendientes Seed y T114). Detalle: BITACORA.
- **2026-08-15 (30ª parte) — POSIBLES AMPLIACIONES ANOTADAS (esperan orden)**:
  - **BLOQUE R — Resets remotos** (agrupado por decisión del operador): FASE R1 = F19
    (`/nava full_reset`: factory_reset + `/resilience.bin` a defaults, conserva claves PKI) +
    F21     (`/nava wipe`: erase total + regeneración del par PKI, purga de compromiso; el
    NodeNum se conserva solo — deriva de la MAC del hardware, NodeDB.cpp:1269). FASE R2 =
    F20 (claves admin en `/resilience.bin`, la última: toca modelo de seguridad + formato del
    struct; SOLO puede existir con F21 desplegado — el wipe es su botón de purga; L31).
    **F22** (idea del operador): etiqueta `NAVATASTIC_BUILD` compilada + línea en `/nava
    status` (y opcional en [Boot]) para saber por radio qué versión lleva un nodo. Detalle en
    BITACORA.
- **2026-08-15 (31ª parte) — CIERRE DE SESIÓN (handover a sesión de implementación)**:
  sesión cerrada con el estado completo persistido. PROMPT DE RETOMA reescrito en PLAN como
  **prompt de sesión de IMPLEMENTACIÓN** (F18 8-lecturas / BLOQUE R fase R1 / F22), con
  apertura canónica + instrucción del operador: analizar la viabilidad ANTES de implementar,
  explicar el plan en lenguaje fácil, pedir permiso, y PREGUNTAR dudas (el operador traslada
  al agente de esta sesión, con más contexto). Nueva sección "GUÍA PARA LA SESIÓN DE
  IMPLEMENTACIÓN" en PLAN con pistas de diseño (ubicaciones de código, mecanismos, patrones)
  para no re-derivar todo. Estado: V2.6 verificada, 4/6 placas verificadas (Promicro, Faketec,
  Xiao×2; pendientes Seed y T114), GitHub v2.6.1 publicado. Pendientes de implementar: F18,
  Bloque R (R1 → R2), F22; anotados: F16b/d/e, F17.
- **2026-08-16 (35ª parte) — CIERRE "NavaTastic Eclipse V3" (4.3.2) + PUBLICACIÓN COMPLETA**:
  - **Publicado a GitHub**: release **v4.3.2** (id 371184462) con **26 assets** — 12 UF2 + 12
    OTA (builds V3, 12/12 SUCCESS) + 2 PDFs (cartel HD como portada + títulos "NavaTastic
    Eclipse V3"). Rama huérfana UN commit (`1f07ddeaa`) → main; releases v2.6/v2.6.1
    conservados. **Verificación de seguridad final**: clon del repo público escaneado — **0
    tokens** (ghp_/github_pat_/gho_/ghs_/AWS), main = 1 solo commit (historial local con
    claves Propia NUNCA subido), `build_propia.ps1` solo referencia variables de entorno.
  - **Carteles del operador**: README con `cartel_navatastic_github.jpg` (1024×572) como
    primera imagen (escudo retirado); PDFs con el flyer HD (1792×2398) como **página 1**
    (ajustado al área de texto, centrado, sin página en blanco — L36) y portada estilizada
    "NavaTastic Eclipse V3" en la 2ª. Plantilla LaTeX + `generar_pdf.ps1` (copia del HD a
    `%TEMP%`, ruta sin espacios — L34).
  - **Títulos de los manuales**: "NavaTastic Eclipse V3" (antes 4.2/4.2.1/4.3) en portada,
    H1 ES/EN y adenda de versión; el tip `/nava help` por nodo en manual y README.
  - **Lecciones**: L34 (`\includegraphics` con espacios escapados → `Missing endcsname`;
    nonstopmode puede dar "OK" sin imagen — usar %TEMP%), L35 (edición sin stagear → commit
    incompleto → imagen 404; verificar con `git show HEAD:<fichero>` antes de publicar),
    L36 (`titlepage` emite `\newpage` → primera página en blanco; portada-cartel con página
    simple + `\vfill`). Verificación PDF con pypdf (pag1=imagen, pag2=portada, pag3=índice).
  - **Estado**: BLOQUE R completo (R1+R2), F18/F22/F20 cerrados y verificados, GitHub al día.
    Pendientes: banco Seed y T114, F16b (BLE)/F16d/F16e/F17, rotación del token GitHub (L26),
    publicación en el grupo de Telegram (decisión del operador), próximos releases (bump de
    `NAVATASTIC_BUILD` en NavaCLIModule.h por release).
- **2026-08-16 (36ª parte) — README EN: 10 caracteres mojibake corregidos (publicación retoma)**:
  el README público mostraba 10 caracteres rotos "�" (U+FFFD, bytes EF BF BD) en la sección
  EN — guiones largos y el "×" de "6 boards/radios × 2 branches" grabados mal en la
  traducción del 15/08. Fix: `README.md` (backup `.bak-20260816-0335`) sustituyendo los 10
  U+FFFD por U+2014 (—) ×9 y U+00D7 (×) ×1 (verificado: 0 restos). Decisión del operador:
  la clave privada del Master Node publicada en los PDFs NO es un fallo de diseño — es la
  "última bala" de rescate, con instrucciones claras para desautorizarla/cambiarla al
  recuperar el nodo; respuesta lista si lo critican en el grupo. Republicado: rama huérfana
  UN commit → main; L24 aplicada (distribucion\ + PDFs repoblados).
- **README descargo (misma sesión)**: reescrito el punto "pensada para la malla de este
  proyecto" (decisión del operador: aunque existan nodos montados por él, no deben asociarse
  con su persona) → el proyecto **no tiene ni opera ninguna malla**, los nodos con este
  firmware no tienen relación con el proyecto ni con su autor, y hay que revisar la
  configuración por defecto antes de desplegar. ES+EN, verificado 0 restos.
- **Guía de compilación pública (misma sesión)**: creado `docs/Compilar_NavaTastic.md`
  (ES+EN) adaptado del texto de compilación de la documentación oficial de Meshtastic
  (desechados "Adding Custom Hardware" y "Hardware Model Acceptance Policy" — no aplican a
  un fork con 6 placas fijas) y de lo que había en el README: requisitos (Git/PlatformIO/
  raíz corta Windows MAX_PATH), clonar+submodules, actualizar, compilar (12 envs + CLI),
  flashear (enlace), Propia (build_propia.ps1/variables de entorno), ajustes de hardware,
  enlaces. El README (ES+EN) queda más limpio: las secciones "Compilar" y "Rama propia"
  sustituidas por una corta con enlace a la guía. Manuales INTACTOS (norma 0.11 no aplica).
  Guia_para_agente: una línea de enlace a la guía pública (§2, su contenido no se duplica).
  Backups `.bak-20260816-0345/0346`. `generar_pdf.ps1`: `Compilar_NavaTastic.md` a
  `$Excluir` (norma 11/08 — solo manuales; el PDF extra se retiró, la rama pública lleva
  los 2 PDFs).
- **Agradecimientos en el README (misma sesión, texto del operador)**: nueva sección
  "Agradecimientos"/"Acknowledgments" al pie (ES y EN): JBAU92 y su firmware_solar_fix
  (https://github.com/JBAU92/firmware_solar_fix, origen del proyecto) + amig@s y conocid@s
  de la malla de Navarra + grupo Meshtastic España (Telegram). Backup `.bak-20260816-0350`.
- **Nota del autor en el README (misma sesión)**: tras la tabla "Estado de pruebas (banco)"
  (ES+EN), nota en cursiva del operador: "algo podría fallar... es reparable... el firmware
  puede tener 'un poquito de sueño'" (guiño al ciclo de sueño/despertar, frase entre
  comillas). Backup `.bak-20260816-0355`.
- **2026-08-16 (37ª parte) — RETOMA: builds Propia R2IP #1 + F20 aclarado en banco**:
  - **Propia R2IP #1**: compilados 4 envs con las claves del operador y PIN propio vía
    variables de entorno en memoria (`navarrico_promicro_e22p_r2ip`,
    `navarrico_xiao_e22p_r2ip`, `navarrico_xiao_kit_sx1262_r2ip`,
    `navarrico_faketec_sx1262_r2ip`; 4/4 SUCCESS; K0/K1/PIN verificados por byte-scan en
    los UF2). Destino: `Desktop\Navatastic V3 Eclipse Infraestructura Propia\Rama 2
    Routers\UF2|OTA` + `PROMPT_BUILD_PROPIA.md` reutilizable (sin claves). Nada commiteado
    ni subido; repo intacto.
  - **F20 aclarado (operador en banco + confirmado por código)**: (1) `keys_clear` solo
    purga la copia persistida; la config sigue autorizando hasta el reset — volver a
    fábrica = `full_reset` (conserva PKI/bonds) / `factory_reset` (PKI nuevo) / `wipe`.
    (2) **No existe el slot 3** (nanopb `admin_key[3]`): una 4ª clave se descarta, no
    autoriza y no se persiste. (3) Re-persistir = escribir desde la app.
  - **Pendiente de publicación**: `generar_pdf.ps1` ($Excluir, commit 8b5824fed) + esta
    ronda de docs irán en la próxima publicación.
- **2026-08-16 (40ª parte) — REORGANIZACIÓN Y LIMPIEZA DEL REPO (TIDY UP)**:
  `Guia_para_agente_sobre_NavaTastic.md`, `BITACORA_TECNICA.md`, `PLAN_DE_TRABAJO.md` y
  `PORTING_NUEVO_FORK.md` movidos a `docs/`. `cartel_navatastic_github.jpg` y
  `flyer_navatastic_eclipse_v3_hd.jpg` movidos a `branding/`. `HerramientasPropiasIA/` renombrada a
  `herramientas/`. `AGENTS.md` canónico en la raíz apuntando a `docs/`. `README.md` y `generar_pdf.ps1`
  actualizados. Raíz del repositorio 100% limpia y profesional.
- **2026-08-16 (39ª parte) — KO-FI Y PATROCINIO VOLUNTARIO ALTRUISTA**: integrado
  Ko-fi (`https://ko-fi.com/ea2oy`) en `README.md` (badge superior + secciones dedicadas ES/EN
  tras Agradecimientos) y `.github/FUNDING.yml` (`ko_fi: ea2oy` → botón «💖 Sponsor» de GitHub).
  Texto explicita que el proyecto es y será **100% libre, gratuito y totalmente altruista**;
  donar es estrictamente voluntario para cubrir costes de hardware/banco de pruebas.
  **Publicado a GitHub (main)**: rama huérfana regenerada con título público limpio (`NavaTastic v4.3.2 — Actualización 16/08/2026 14:15...`)
  y regla L24 ejecutada (distribucion\ repoblada + 2 PDFs regenerados).
- **2026-08-16 (38ª parte) — CIERRE DE LA RETOMA (sesión documental post-V3)**: sesión
  cerrada: commits locales en master (README + docs), publicación final con rama huérfana
  regenerada → main (lleva el $Excluir pendiente y toda la ronda de docs). Nada de código
  tocado; V3 intacta. Siguientes pasos en §5.5 handover y PROMPT DE RETOMA de PLAN.
- **2026-08-16 (34ª parte) — NOMENCLATURA DE VERSIONES UNIFICADA (decisión del operador)**:
  el changelog del manual de uso se reestructura en 3 hitos públicos: **4.3.0** = NavaTastic +
  control remoto sin PC (auto-fav y NodeDB en RAM ya presentes) · **4.3.1 = "NavaTastic
  Eclipse"** (12/08, primera distribución a colegas: muchos comandos + cola de mensajes y
  descartables en RAM) · **4.3.2 = "NavaTastic Eclipse V3"** (15-16/08, actual: reajuste de
  sueño/despertar sin afectar RF ni lecturas, avisos de estado al dormir/despertar, resets
  remotos, resiliencia de claves admin F20, 8 lecturas, etiqueta `NAVA V3`; en cualquier nodo
  `/nava help` lista los comandos de SU release). **Mapeo**: "NavaTastic Eclipse V3" = 4.3.2
  = etiqueta compilada `NAVA V3` (no recompilar). Las iteraciones internas (V2.2/V2.3/V2.4/
  V2.6/F18/F20...) quedan SOLO como historial técnico en BITACORA/cerebro, nunca en manuales
  ni en la etiqueta. Manuales/README actualizados (ES+EN); PDFs regenerados.
- **2026-08-16 (33ª parte) — F20 (FASE R2) COMPLETADA Y VERIFICADA EN BANCO 7/7 (V3)**:
  **F20** = claves admin PÚBLICAS del usuario persistidas en `/resilience.bin` para que
  sobrevivan a los resets de fábrica (hoy se perdían, L10). Struct `ResiliencePrefs` 84→180 B
  (`keySlot1/2/0Own`, marcador "NAV3" `0x4E415633`); migración legacy con adopción (dedupe
  contra claves del proyecto vía macros, `#ifdef` por clave); restauración en el primer tick
  (DESPUÉS de NodeDB::init) con **regla final enmendada "slot 0 = estado previo del usuario"**
  (`keySlot0Own` vuelve al slot 0 desplazando la del proyecto; si no existe, queda la del
  proyecto); sincronización merge desde `AdminModule::handleSetConfig` (vaciar en la app NO
  purga; purgar = keys_clear/wipe); comandos `/nava keys_ls` + `/nava keys_clear` (ACK
  diferido, sin reboot). **Hallazgo de banco H1**: el full_reset de R1 borraba el fichero
  entero (mataba las claves) → fix `navaFullResetKeepKeys()` (conserva SOLO las claves y
  resetea el resto a defaults de perfil). **Banco 7/7 PASS (Faketec)**: full_reset conserva
  claves (S0=propia desplaza proyecto, DM OK sin re-acreditar, Master Node NO AUTORIZADO —
  sin ventana de secuestro) + semi-persistentes resetean (rol client→ROUTER, sleepmsg off→ON)
  + re-autorización (S0=proyecto limpia la override) + factory_reset conserva claves (PKI
  reaprende, L11) + wipe purga total + keys_clear vacía la persistencia sin tocar config/
  química/rol (tras full_reset las claves NO vuelven) + regresión (status NAVA V3, help,
  canal 1 → SOLO DM SEGURO). **12/12 envs SUCCESS + distribuir -Todo -V2** + docs completas
  (manuales ES+EN con regla de claves y merge, transfer_context, guia_integracion, subnotas
  02/03/05, BITACORA, PLAN, README) + PDFs. Detalle: BITACORA "F20" + "F20 H1".
- **2026-08-15 (32ª parte) — SESIÓN DE IMPLEMENTACIÓN: F18 + BLOQUE R fase R1 + F22 (V3)**:
  FASE 1 validada por el operador (plan en lenguaje fácil + 5 respuestas del agente anterior).
  **F18**: contador de baja unificado a **8 lecturas (~160s)** para las 6 placas — 13 jsonc
  (`READINGS_COUNT` `"5"`→`"8"`) + Power.cpp comparando contra la macro (fallback `#ifndef
  → 8`; fuera el `#ifdef` `>4`/`>10`); el pre-check ya leía la macro (8×200ms ≈ 1,6s).
  **BLOQUE R R1**: `/nava full_reset` (ACK → diferido → remove `/resilience.bin` +
  `factoryReset(false)` → **conserva par PKI y bonds**) y `/nava wipe` (→ `factoryReset(true)`
  → **par PKI nuevo** + peers + bonds; NodeNum intacto por MAC); DM-PKI automático (fuera de
  whitelist canal 1); flags diferidos en runOnce; help/manuales con AVISO de re-aprendizaje
  PKI. F20 NO tocada (fase R2). **F22**: `#define NAVATASTIC_BUILD "V3"` (bump manual por
  release) + línea `NAVA V3 | fw …` en `status` + etiqueta en [Boot]. **L32**: dos errores
  propios de la sesión corregidos (usar `optstr(APP_VERSION)` — el `-D` es token crudo;
  typo `navarrico`→`navarico` en ResetReasonName). **12/12 envs SUCCESS**. Detalle:
  BITACORA "V3". Backups `.bak-20260815-2109`.
- **2026-08-14 (3ª parte) — CEREBRO PORTADO AL REPO UNIFICADO**: `docs\` reestructurado a
  layout canónico `docs\cerebro\` (cerebro.md + subnotas 01-12 + 2 PROMPTs, copias 1:1 de
  4.3, MD5 15/15 verificados). Este cerebro pasa a ser el VIVO del repo único: banner
  ESTADO 14/08, sección 5 (log + F1-F12 + VIGENTE vs OBSOLETO + handover). Subnotas 01-12
  y docs de contexto reciben banner/adenda de mapeo 4.3 → repo. El 4.3 queda SOLO LECTURA.
- **2026-08-14 (4ª parte) — JOYA DE LA CORONA**: creado `PORTING_NUEVO_FORK.md` (raíz del
  repo) — guía maestra para integrar TODAS las mejoras Navarrico en un fork NUEVO de
  Meshtastic: inventario fichero a fichero con anclas, catálogo de bloques con
  dependencias, procedimiento paso a paso y checklist de trampas (F1-F12).
- **2026-08-14 (5ª parte) — SISTEMA DE PDF PORTADO AL REPO**: `HerramientasPropiasIA\`
  `generar_pdf.ps1` + `plantilla_navatastic.tex` (copia 1:1 de 4.3, SOLO LECTURA allí).
  Entrada = `docs\`, salida = `docs\pdf\` (**gitignored**). `$Excluir` = docs de contexto
  (transfer_context, guia_integracion, GUIA_AGENTE_NAVTASTIC, INSTRUCCION_AUDITORIA_CLAUDE)
  — norma 11/08: solo manuales de firmware y comandos. Verificado: 2 PDFs generados sin
  errores (Pandoc + MiKTeX presentes). **Decisión operador**: se mantiene como herramienta
  inerte; `docs\*.md` siguen siendo la fuente única de los manuales. El 14/08 se añadió la
  **norma §0.11**: backup `*.md.bak-AAAAMMDD-HHMM` del manual antes de tocarlo (rollback si
  se detecta error) + regenerar PDF tras cada cambio.
- **2026-08-14 (6ª parte) — SNAPSHOT BASELINE "NavaTastic 4.3 Eclipse Edition - Unificado"**:
  creado `_archivo\NavaTastic 4.3 Eclipse Edition - Unificado.zip` (5,3 MB, 1872 entradas,
  HEAD `644d09e68`): fuentes + config + perfiles + scripts + docs/cerebro completos, SIN
  `.pio`/`.git`/binarios (los binarios buenos = Desktop\NavaTastic 4.3 120826 + `distribucion\`).
  **Baseline conocido-funciona**: con él, cualquier cambio futuro (comandos NavaCLI, energía,
  etc.) es reversible: descomprimir sobre la raíz o `git checkout 644d09e68`. La paridad MD5
  12/12 vs Desktop es la prueba de que el snapshot reproduce exactamente los binarios de Eclipse.
- **2026-08-14 (7ª parte) — NORMA 0.12 + DESTINO V2 + FASE 2 (V2)**: nueva norma §0.12:
  los builds NUEVOS (post-snapshot) se distribuyen a `Desktop\NavaTastic Eclipse Edition V2`
  (`distribuir.ps1 -V2`, misma estructura Rama×LIPO|NIMH×UF2|OTA). Eclipse
  (`Desktop\NavaTastic 4.3 120826`) SOLO LECTURA, no se toca. En FASE 2 (V2) se implementan:
  (A) mensajes de sueño/vivo/listo al canal Navadmin + gate `/nava sleepmsg` + INA219 cuando
  presente + ampliar `/nava power` (±mA cargando/descargando); (B) fix favoritos en `status`/
  `fav ls` (distinción Auto/Manual persistida en `ResiliencePrefs.autoFavIds`); (C) fix
  fragmentación de respuestas multilínea (corte en `\n`). MD5 baseline congelado (Eclipse).
- **2026-08-14 (8ª parte) — FASE 2 V2 COMPLETADA Y DISTRIBUIDA (12/12 compilados, 16:20-16:50)**:
  implementado A+B+C (ver BITACORA "V2"). Lógica de sueño: al venir de sueño, el pre-check
  usa el despertar LPCOMP como umbral → 3 bandas: V<corte OCV = silencio+re-sueño; corte≤V<LPCOMP
  = [Vivo]+re-sueño tras TX; V≥LPCOMP = boot normal ([Listo]). Sueño en runtime delega en
  NavaCLI (envía [Sueño] y duerme al drenar la cola). **F13**: `distribuir.ps1` copiaba el
  artefacto MÁS ANTIGUO de `.pio/build` (orden alfabético) → distribuía binarios de era Eclipse;
  fix: siempre el más reciente por LastWriteTime. Distribuido a `distribucion\` + V2 Desktop
  (32+32 ficheros, MD5 nuevos; `distribucion\` ya NO es Eclipse, es V2). Manual actualizado
  (backup 0.11: `.bak-20260814-1651`) + PDFs regenerados. Pendiente banco: ciclo real
  solar/LPCOMP + ATtiny + status tras reinicio. V2.1 (14/08): rol semi-permanente EXTENDIDO a
  Rama 2 (simetría set_role, campo `role` fuera del `#ifdef NAVARICO_RAMA_1`; R1 sin cambios).
- **2026-08-14 (9ª parte) — V2.2: FIX F14 (mensajes sueño/vivo/listo NO llegaban) + precisión de lecturas**:
  probado en banco por el operador: no llegaba ningún aviso al canal Navadmin. Causa (F14):
  (1) jitter anticolisión del canal 1 de 0.5-6.5s aplicado ANTES del primer envío; (2) `sleepTime`
  se fijaba al ENCOLAR (+5s) y el re-sueño entraba justo tras `sendToMesh` (asíncrono) → la radio
  se apagaba con el paquete sin emitir o a mitad de ráfaga. Fix: `sleepTime` se recalcula DESPUÉS
  del envío real (+3s margen airtime SFNarrow); jitter corto 300-2300ms para estos mensajes
  (`enqueueResponse(..., quick=true)`). Precisión (subagente, verificado): el pre-check "5 lecturas
  200ms" era 1 medida ADC real + cache (throttle 5s `Power.cpp:331`); 1ª lectura cruda sin LPF y
  sin asentamiento post-reset. Fix: `readPowerStatus(force=true)` salta el throttle (5 medidas
  reales) + `delay(500)` de asentamiento en `main.cpp`; LPCOMP NO se tocó (ya estable: EVENTS_READY
  + delay(10) + histéresis + delay(3000) pre-sueño). Binarios V2 probados archivados en
  `_archivo\V2-testeados-antes-fix-timing-20260814.zip`. Promicro R2IG V2.2 en Desktop V2
  (MD5 20CDA06A...) para re-test del operador; distribución -Todo -V2 pendiente del resultado.
- **2026-08-14 (10ª parte) — TEST V2.2 EN BANCO FALLIDO (F15 ABIERTA)**: el operador probó el
  Promicro R2IG V2.2 (MD5 20CDA06A...) y los mensajes [Sueño]/[Vivo]/[Listo] SIGUEN SIN LLEGAR
  al canal Navadmin (pese al fix F14). Detalle del test: se hizo factory reset al nodo con el
  PRIMER binario (V2) para materializar el canal Navadmin — NO repetir salvo indicación.
  **F15 (investigación pendiente)**: por qué no se envía el mensaje antes/después del ciclo
  dormir/despertar. Pistas iniciales: (1) ¿el nodo llega a dormirse en el test? (el mensaje
  [Sueño] solo sale si la batería baja del corte OCV — verificar por serial: "Low voltage
  detected" / "Entering battery sleep"); (2) ¿llegan OTROS comandos por el canal 1 (ping/status)?
  Si sí → el envío funciona y el fallo está en el disparo; si no → el problema es el envío a
  canal 1 desde runOnce; (3) revisar si el primer runOnce (Vivo/Listo) ocurre antes de que la
  radio esté lista para TX; (4) `allocDataPacket`/`sendToMesh` con `channel=1` + `to=0`; (5) los
  LOGs de serial del nodo son la fuente de verdad. Distribución -Todo -V2 SIGUE PENDIENTE del
  fix.     Binarios V2 pre-fix archivados en `_archivo\V2-testeados-antes-fix-timing-20260814.zip`.
- **2026-08-15 (13ª parte) — F15 RESUELTA EN BANCO + 2 FRENTES NUEVOS (admin-nodedb + mensajes sueño/vivo/listo)**:
  **VERIFICADO EN BANCO (Promicro R2IG, COM15):**
  1. El gate de migración POR TAMAÑO era insuficiente: el nodo tenía un `/resilience.bin`
     "envenenado" de **84 bytes con role=0** (escrito por un build intermedio V2.3), que el
     tamaño no distinguía del formato bueno → role=CLIENT persistente.
  2. El manifiesto de ficheros reveló que el fichero medía **1252 bytes** (metadata LFS
     corrupta). Causa de fondo: **`FILE_O_WRITE` de Adafruit InternalFS NO trunca**
     (`LFS_O_RDWR|LFS_O_CREAT`, seek a 0, sin trunc) → los ficheros nunca encogen y, tras
     los resets/escrituras de la sesión, la metadata se corrompió.
  3. **FIX FINAL (compilado y flasheado)**: `FSCom.remove()` antes de cada escritura
     (fichero siempre de 84 bytes exactos) + gates de migración `fileSize != sizeof(prefs)
     || version != 0x4E415653` + saneado de campos fuera de rango (chemistry/vbat/vwake/
     flags) en loadResiliencePrefs, navaResiliencePeek y navaSetWasInSleep; fallback de
     navaSetWasInSleep completado (vbat/vwake/chemistry). Los campos legacy VÁLIDOS de
     ficheros 4.3 se preservan.
  4. **CICLO VERIFICADO**: migración 1252B→84B limpio ✓ · `set_role client`→CLIENT ✓ ·
     reboot→CLIENT persiste ✓ · **factory reset→CLIENT sobrevive (diseño cumplido)** ✓ ·
     `set_role router`→ROUTER ✓ · `nrf erase`→ROUTER (perfil) + fichero nuevo 84B ✓.
     El factory reset SOLO borra /prefs (`rmDir`); resilience.bin sobrevive (confirmado).
  5. **CDC mudo explicado**: los logs del boot se pierden antes de que enumere el CDC
     (normal en nRF52); en runtime los logs van como LogRecords protobuf SOLO con
     `debug_log_api_enabled=true` (el factory reset lo borra → se pierde el canal).
     La API USB es el canal de verdad.
  6. **BLE**: `PowerFSM::serialEnter` apaga la baliza mientras el CLI USB está conectado
     (comportamiento estándar); el "wedge" que parecía BLE se recuperó con power-cycle
     (anotado F16: revisar resumeAdvertising tras shutdown()).
  **FRENTE B (bug nuevo, NO cerrado):** la acreditación admin (bitfield+favorito) en
  `AdminModule.cpp:109-117` NO guarda la DB → tras reboot el nodo admin no responde hasta
  que re-anuncia nodeinfo. Fix propuesto: `saveToDisk(SEGMENT_NODEDATABASE)` tras
  acreditar/favoritear + verificación en banco. (El 4.3 funcionaba; la lógica de
  updateUser/H3-fix está intacta en el unificado.)
  **FRENTE A (test sueño, NO cerrado):** con fuente de laboratorio el MCU duerme/despierta
  pero NO envía [Sueño]/[Listo]. Parche TEMP flasheado (diagnósticos por canal Navadmin:
  HB cada 60s, disparo lowbat, pre-check) + banco montado a 869.545 privada con observador
  (COM9, Eclipse V1). PENDIENTE de ejecutar el test. **OJO: el nrf erase regeneró la clave
  del test node → el observador tiene la clave vieja → DM PKI falla
  (`PKI_SEND_FAIL_PUBLIC_KEY`) → hay que `--remove-node` en el observador y que reaprenda.**
  **INSTRUMENTACIÓN TEMP F15 PENDIENTE DE RETIRAR** (todo marcado `NAVARICO: TEMP F15`):
  F15DBG logs + breadcrumbs owner.short_name (AD/LR/SR/XX) en AdminModule/NavaCLIModule,
  HB 1s + HB canal-1 60s + bootDiag en runOnce, `return 1000` (original 60000), logs
  F15 precheck en main.cpp, contador Power.cpp a LOG_INFO, "F15: resilience.bin escrito".
  Backups código `.bak-20260814-2125`; docs `.bak-20260815-0024`.
- **2026-08-14 (11ª parte) — F15: CAUSA RAÍZ ENCONTRADA (bug parseo sleepmsg + compat /resilience.bin)**:
  el operador descubrió que `/nava sleepmsg` respondía OFF persistente (ni factory reset ni flasheo
  lo cambiaban) y que el R2IG aparecía como CLIENT pese al perfil ROUTER. Diagnóstico: (1) **bug de
  parseo**: `sleepmsg on|off` usaba `substr(8)` (8 letras + espacio → arg=" on") → nunca se pudo
  activar por comando → fix `substr(9)` (V2.3b, MD5 8D0B32…); (2) **compat de fichero**: el
  `/resilience.bin` de la era Eclipse (80 bytes, sin campos V2) hacía que `sleepMsgs` leyera el
  padding (OFF) y `role` leyera 0 (CLIENT) — la migración por tamaño no se disparaba (el struct V2
  también ocupa 80) → fix con campo `version` (84 bytes) → migración a sleepMsgs=1, role=0xFF,
  wasInSleep=0 (V2.3c, MD5 98A97F88). También corregidos `navaResiliencePeek` (lee fileSize) y la
  migración de `navaSetWasInSleep` (role=0xFF+version). Instrumentación temporal marcada
  `NAVARICO: TEMP F15` (pre-check main.cpp, contador Power.cpp a LOG_INFO, logs de [Sueño]/escritura
  en NavaCLIModule). **Pendiente (sesión nueva)**: reflashear V2.3c y verificar role=ROUTER +
  sleepmsg=ON por API/radio; verificar persistencia de escritura (si `sleepmsg on` no sobrevive al
  reboot → FS de escritura → `nrf erase`); explicar el **CDC mudo** (la API USB funciona:
  `meshtastic --port COM15 --info` → myNodeNum 551169628, NRF52_PROMICRO_DIY, firmware c7af16b,
  pero el serial no emite texto); retirar la instrumentación TEMP F15; distribución `-Todo -V2`
  pendiente del re-test. Anotado (no tocado, posible F16): `fav rm` con substr(8) se come el primer
  carácter del id; jitter quick muerto (cosmético); whitelist canal 1 sin sleepmsg (decisión del
  operador: consulta por DM). Backups: código `.bak-20260814-1819`, docs `.bak-20260814-1947`,
  binario V2.2 en `_archivo\Promicro R2IG V2.2 20CDA06A - antes de instrumentacion F15.uf2`.
- **2026-08-14 (anterior)**: unificación completa (12 envs, perfiles, scripts) + paridad
  12/12 + distribución a `distribucion\` + copia inicial de docs (ver `BITACORA_TECNICA.md`
  y `PLAN_DE_TRABAJO.md`).

### 5.3 Errores conocidos nuevos — proceso de unificación/paridad (F1-F12)

> Detalle completo, cronología y fixes: `BITACORA_TECNICA.md`. Resumen (todos resueltos):

| # | Error encontrado | Solución aplicada |
|---|---|---|
| F1 | APP_VERSION embebía el SHA del repo nuevo (`bfe547a` ≠ `54e0d8d`) | Override `NAVARICO_APP_VERSION` en `platformio-custom.py` (inerte si no existe) |
| F2 | BUILD_EPOCH = día de compilación (≠ 12/08) | Override `NAVARICO_BUILD_EPOCH` (variable de entorno, inerte) |
| F3 | APP_ENV = nombre del env (`navarrico_*` ≠ canónico) | `custom_meshtastic_app_env` por env |
| F4 | Libs embeben ruta `.pio\libdeps\<env>\...` (arduino-fsm usa `__FILE__`) | `-ffile-prefix-map` inyectado desde Python a **los LIB BUILDERS** (`lb.env`), no a projenv (Fix 4 tras 3 fallidos) |
| F5 | Mapa de libdeps de Promicro R2IG apuntaba a ruta ABSOLUTA (sonda confundida con R1IG) | R2IG embebe ruta RELATIVA `.pio\libdeps\<env>`; mapa corregido. Lección: verificar sonda contra el binario correcto |
| F6 | Override `__TIME__/__DATE__` con CPPDEFINES rompía (CommandLineToArgvW borra comillas simples) | Flags crudos `-D__TIME__=\"...\" -D__DATE__=\"...\"` como CCFLAGS |
| F7 | Cualquier línea añadida en NodeDB.cpp rompía paridad (1 byte: `saveToDisk` embebe `__LINE__`) | Cambios a 0 líneas netas; rol CLIENT vía perfil, comentarios NAVARICO inline |
| F8 | "El orden de libs difería" — falso positivo (orden alfabético del script vs `piolib.py:1106`) | Verificar con el BINARIO (diff byte a byte), no con heurísticas de carpetas |
| F9 | Build limpio del repo original NO reproducía Eclipse… salvo 6 bytes | Era la marca temporal (`05:24:42Aug 14 2026` vs `07:35:34Aug 12 2026`); Eclipse ES reproducible en limpio |
| F10 | Morrilla y scripts con errores de PowerShell (Join-Path 3 args, Select-String -Encoding Byte, CRLF en here-strings, ExecutionPolicy) | Aprendizajes de entorno; ver BITACORA |
| F11 | Cada repo original divergió en main-nrf52.cpp (bloques por placa solo en sus carpetas) | Unificado con TODOS los bloques en `#ifdef/#elif`; asserts (embeben `__LINE__`) fijados con `#line` por radio (451/454 SX1262 vs 456/459 E22P) |
| F12 | Los "UF2" de PIO para nRF52 llevan solo ~312 bloques UF2 + el resto crudo | La marca temporal puede quedar en zona cruda: buscar en el fichero crudo, no reconstruyendo la imagen |

### 5.4 VIGENTE vs OBSOLETO (qué queda del 4.3 y qué no)

**VIGENTE (conocimiento técnico que sigue aplicando al repo único):**

- Todas las normas de comportamiento: resiliencia energética (brownout de ascenso solar,
  pre-check, LPCOMP por divisor real, `delay(3000)`, storm, POFCON), protección Flash
  (RAM-only, filtros de guardado, eviction, auto-fav, TransmitHistory bypass, límite 10
  huérfanos), seguridad `/nava` (canal Navadmin slot 1, DM PKI, whitelist, rate-limit 30 s),
  fix H3 (a)+(a2), fix `updateUser`, auto-recuperación de claves (`local_sum==0`), fix
  #10873 (disableBluetooth después del reset), P0 lifepo4 rechazado, C8, C4 revertido,
  fav auto, ayuda/consultas, fragmentación por palabra, rol semi-permanente (R1).
- Valores por variante (subnota 01 y 10): potencia 12/22 dBm, cortes 3500/3400 mV,
  LPCOMP `9_16`/`3_8`/`2_8`, divisores ADC (0.5 / 1M-510k / 3.3 / 4.916), `RADIO_POWER_ENABLE_PIN`.
- Mapa de hardcodeos (subnota 10): el mapa como tal; los valores viven ahora en
  `profiles/*.jsonc` + `variant.h` por placa + envs `navarrico_*`.
- Claves: K0/K1 Propia (del operador, **valores no publicados** — se piden al compilar los
  envs `R2IP_*/R1IP_*` vía variables de entorno + `build_propia.ps1`, nunca almacenadas en el
  repo) y K0 = Master Node (General, implementada en los 12 perfiles). Regla: SOLO en perfiles/`userPrefs.jsonc`,
  nunca literales en código.
- Manuales de comandos `/nava` y de uso (docs/), subnotas 03/05/07/12, diagnósticos.

**OBSOLETO (sustituido por el repo único — no usar como fuente activa):**

- **Las 24 carpetas/repos viejos de 4.3** (`Rama 1 Clientes en Infraestructura\`,
  `Rama 2 Infraestructura\Infraestructura General|Propia\`, `felix puerto venecia\`,
  etc.): sustituidas por el repo único. Solo referencia histórica.
- **LAB** (`08_diagnostico_lab.md`): instrumento descartado (12/08), NO migrado al repo.
- **Build Felix "Puerto Venecia"**: build puntual fuera del repo; no participa en el
  unificado (su jsonc con claves propias NO se sube a GitHub).
- **Distribución al Desktop** (`Desktop\NavaTastic 4.3 120826\` + `distribuir_desktop.ps1`):
  sustituida por `distribucion\` dentro del repo. El Desktop de Eclipse queda SOLO como
  referencia de paridad MD5 (NO se toca). Los builds V2 nuevos van a
  `Desktop\NavaTastic Eclipse Edition V2` (norma 0.12, `distribuir.ps1 -V2`).
- **`HerramientasPropiasIA\`** (4.3): `distribuir_binarios.ps1`/`distribuir_desktop.ps1` —
  obsoletas; el repo tiene `build.ps1`/`distribuir.ps1`. El **sistema de PDF SÍ está en el
  repo** (`HerramientasPropiasIA\generar_pdf.ps1` + `plantilla_navatastic.tex` → `docs\pdf\`,
  gitignored); los PDFs de contexto NO se generan (norma 11/08: solo manuales).
- **Rutas `C:\Firmware Navarrico 4.3\...`** citadas en secciones 1-4 y subnotas: leerlas
  como referencia; la verdad viva es el repo único.
- **`default_envs = tbeam`** (fallaba por toolchain ESP32): ya no existe — `default_envs`
  = los 12 envs `navarrico_*`.

### 5.5 Handover (estado actual y siguiente paso)

- **Estado (17/08/2026, Post-Release V4.3.3 / NavaTastic Eclipse V4)**: "NavaTastic V4" (4.3.3) implementado, verificado, compilado (12/12 SUCCESS), distribuido y publicado oficialmente en GitHub Release `v4.3.3`.
  - **Release Oficial GitHub `v4.3.3` (`EA2OY/NavaTastic`, ID `371753206`)**:
    - 27 assets optimizados y clarificados con nomenclatura URL-safe para usuario final:
      - 12 firmwares `ROUTER_Repetidor_Fijo` (UF2 y OTA zip).
      - 12 firmwares `CLIENTE_convertible_a_ROUTER` (UF2 y OTA zip).
      - 3 manuales oficiales PDF (`Manual_NavaTastic.pdf`, `Manual_uso_NavaTastic_4.2.pdf`, `INFORME_DOBLE_AUDITORIA_NAVATASTIC_Y_MESHNAVARRA.pdf`).
  - **Saneamiento de la Rama Pública `main` en GitHub**:
    - Saneada mediante rama huérfana limpia. `docs/` en GitHub expone exclusivamente manuales públicos de usuario, protegiendo las bitácoras y cerebros internos de desarrollo en `master` local.
    - Cartel oficial del proyecto (`branding/cartel_navatastic_github.jpg`) integrado y visible en `README.md`.
    - 57 enlaces directos a firmwares y PDFs en `README.md` comprobados al 100% (HTTP 200 OK).
  - **Blindaje Anti-Tormentas en Navadmin (Canal Público)**: Broadcast masivo no dirigido limitado a 7 comandos ligeros de 1 línea (`ping`, `status`, `bat`, `power`, `env`, `channel`, `noise`) con jitter escalonado. Menú `/nava help` general y comandos no permitidos en broadcast quedan en silencio total para evitar saturación de la malla LoRa.
  - **Escudo Anti-Tormentas NodeInfo**: Sincronización en arranque (`currentGeneration = radioGeneration`) que desactiva peticiones masivas de respuesta (`want_response=false`). Documentado en tablas comparativas de `README.md` (ES y EN).
  - **Consola Privada de Gestión de Flota en Lote**: Redirigiendo la CLI a slots 2..7 (`set_cli_chan <2-7>`), el operador puede lanzar órdenes en lote a toda la red con un solo mensaje (`set_ok_to_mqtt`, `set_pos_tx`, `set_nodeinfo_tx`, `set_telem_tx`, `ign add/del/clear/ls`, `set_beacon`, `set_tz`, `set_chem`, `set_vbat`, `set_vwake`, `sleepmsg`, `mute`, `test_tx`, `db_purge`, `nodeinfo`, `pos`, `sendtel`). Comandos individuales (`set_pos`, `set_name`, `set_pin`, `pos_clear`) y nucleares destructivos exigen `!ID` o DM.
  - **Lista Negra Global Persistente**: `ign add/del/clear/ls` respaldado en `/resilience.bin` V5 (NAV5) y descarte inmediato de paquetes en `Router.cpp`.
  - **Control de Difusión de Posición/Telemetría**: Nuevos comandos `set_pos_tx`, `set_nodeinfo_tx`, `set_telem_tx`, `pos_clear`.
  - **Cero Desgaste Flash (RAM-Only)**: `stats`, `log`, `mute` y `test_tx` operan exclusivamente en memoria RAM sin llamadas de escritura LittleFS.
  - **Persistencia Atómica `/resilience.bin` V5 (`NAV5`)**: Detección y migración automática de estructuras previas sin alteración de claves admin ni roles de hardware.
  - **Flasheo Faketec Test Node (`COM9`)**: Flasheado con `navarrico_faketec_sx1262_r2ig` V4 (SUCCESS).
  - **Compilaciones Infraestructura Propia (R2IP)**: Generados los 4 firmwares V4 en RAM y depositados en el Escritorio (`Navatastic V4 Eclipse Infraestructura Propia`), sin rastro en el repositorio ni en GitHub.
- **Estado Consolidado (18/08/2026, Auditoría Ultra-Exhaustiva V4 Finalizada 100% PASS)**:
  - **Dictamen Global**: **100% APROBADO (56 / 56 Casos Evaluados en Hardware Real — Cero Fallos)**.
  - **Fase 0 (Aislamiento & Handshake)**: 4/4 PASS (WiFi ADB `192.168.3.141:5555`, entorno de atenuación controlada `869.545 MHz / 1 dBm`, Traceroute bidireccional +12.5 dB / +11.5 dB).
  - **Fase 1 y Fase 3 (Sincronización Cruzada Bidireccional)**: 14/14 PASS (Demostrado que cambios vía NavaCLI `/nava set_hops`, `/nava set_role`, `/nava set_pos_tx`, etc. impactan en tiempo real en las preferencias binarias Protobuf de la flash y viceversa).
  - **Fase 2 (Batería Completa NavaCLI)**: 28/28 PASS (Diagnósticos de 1 línea, gestión de canales y URLs exportables, filtrado MQTT selectivo, difusiones de flota a 72h, favoritos categorizados `[AUTO]` / `[MAN]`, lista negra persistente, parámetros ejecutivos y seguridad).
  - **Fase 4 (Resiliencia Forense & Soft Reboot)**: 4/4 PASS (ACK LoRa previo a reinicio `/nava reboot`, reconexión RF inmediata a 12.0 dB SNR, preservación íntegra de `/resilience.bin` V5 [`NAV5`] y emisión diferida a los 2 minutos del aviso de diagnóstico `[Boot]`).
  - **Fase 5 y Fase 6 (Parámetros Fijos, Blindajes y Rescate)**: 6/6 PASS (Inamovilidad de Slot 1 Navadmin, rechazo de `ch_del 1` con `ERR: SLOT INVALIDO (SOLO 2-7)`, inmutabilidad de la clave de rescate del proyecto en `admin_key[1]` y prueba de purga `keys_clear` con recuperación garantizada).
  - **Análisis Forense del Switch Bluetooth**: Documentado por qué el switch BLE en la App oficial se restaura por seguridad en el arranque (el watchdog de `/resilience.bin` previene nodos huérfanos si `prefs.ble_disabled == 0`) y cómo `/nava ble off` realiza el apagado permanente y persistente en montaña.
  - **Sanitización Rigurosa de Claves**: Sustituidas todas las claves reales por claves dummy estándar de 44 caracteres Base64 diferenciadas por rol (Master: `K8mP2x9Lv4Qj7Nt1Ws3Yc0Zb5Fa6Ud9Re2Th4Gm7Xi8=`, Slave: `B7vN4w1Zq9Lp2Xm8Tc5Yd0Gf3Ja6Ks9Re1Th2Vm7Ui4=`, Rescate: `R3k9Qm2Wp8Xz4Vb7Nc1Yf0Ld6Ja5Ts8Ue2Gh4Pm9Xi1=`).
  - **Documentación y PDF Oficial**: Generados el informe técnico [docs/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.md](file:///c:/NavaTastic%20Codigo%20completo/docs/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.md) y el PDF profesional [docs/pdf/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.pdf](file:///c:/NavaTastic%20Codigo%20completo/docs/pdf/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.pdf).
  - **Guía Rápida en 5 Pasos en Portada `README.md`**: Integrada en la portada (español e inglés) con diagrama Mermaid, alertas visuales destacadas del Factory Reset (Paso 4 imprescindible) y canal Navadmin (PSK `AQ==`).
  - **Renombrado Canónico de Manuales**: `Manual_uso_NavaTastic_4.2.md` renombrado a `Manual_uso_NavaTastic.md` (eliminando el sufijo histórico obsoleto) y recompilado a `Manual_uso_NavaTastic.pdf` (2.49 MB).
  - **Capítulo de Coexistencia con App Oficial**: Añadida sección específica en ambos manuales (`Manual_NavaTastic.md` §12 y `Manual_uso_NavaTastic.md` §6) detallando por qué el motor `/resilience.bin` preserva y restaura ciertos ajustes frente a la App oficial (watchdog BLE anti-huérfano, blindaje Slot 1, admin_keys, rol semipermanente, RAM-Only NodeDB y cadencia 72h).
  - **Seguimiento Directo de PDFs en Git (Fix 404)**: Eliminada la exclusión `docs/pdf/` de `.gitignore`. Todos los PDFs maquetados (`Manual_NavaTastic.pdf`, `Manual_uso_NavaTastic.pdf`, informe de auditoría, etc.) se rastrean y publican directamente en el árbol de GitHub sin enlaces rotos.
  - **Flasheo de Faketec y Retorno a Producción SFN**: Flasheado el nodo físico Faketec (`COM17`) con `navarrico_faketec_sx1262_r2ig` (Rama 2 Routers LIPO) y restablecida la frecuencia oficial de España de ShortFast Narrow (`869.618 MHz / 22 dBm` / canal 4), saliendo del modo aislamiento de laboratorio (`869.545 MHz / 1 dBm`).
  - **Auditoría de Sanitización Criptográfica**: Verificada al 100% la ausencia de claves personales en todo el repositorio (0 coincidencias regex), utilizando únicamente claves dummy estándar de 44 caracteres (`K8mP2x9Lv...`, `B7vN4w1Zq...`, `R3k9Qm2Wp...`).
  - **Sincronización Total en GitHub (`EA2OY/NavaTastic:main`)**: Publicado con éxito el árbol saneado mediante el flujo seguro de rama huérfana (`github-public:main`), quedando la portada, manuales, PDFs y binarios inmediatamente accesibles para la comunidad.










