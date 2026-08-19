# Guía para agente sobre NavaTastic (repo unificado)

Único punto de entrada para cualquier agente de IA o humano que retome este repositorio.
Este repo sustituye a los 24 repos duplicados del proyecto "Firmware Navarrico 4.3":
**todo el código es único; las 12 compilaciones se eligen por env.**

---

## 0. REGLAS OPERATIVAS (normas principales — leer primero)

Heredadas del cerebro 4.3 (`PROMPT_INICIALIZACION.md`). Son obligatorias en toda sesión:

1. **Comunicación**: directa y técnica, mínimas palabras, sin introducciones, cortesías ni relleno.
2. **Flujo en dos fases**: ante cualquier tarea, FASE 1 = diagnóstico + plan técnico conciso +
   método de verificación (p. ej. `pio run -e <env>`), SIN editar archivos; esperar confirmación
   explícita → FASE 2 = ejecución.
3. **Filtrado de ruido**: no devolver salidas crudas de terminal, diffs completos ni logs masivos —
   solo líneas de error relevantes y fragmentos de código modificados (dieta de tokens).
4. **Código mínimo**: solución con menos líneas y cero dependencias nuevas; verificar primero si
   la funcionalidad ya existe en el proyecto.
5. **Handover**: si la conversación se alarga o lo pide el operador, actualizar `docs\cerebro\cerebro.md`
   y generar bloque de traspaso (Objetivo, Decisiones, Estado, Siguiente paso).
6. **Actualización continua del cerebro**: anotar en `docs\cerebro\cerebro.md` (log de estado,
   errores→soluciones, tareas, handover) todo lo que se haga, **sobre la marcha**, no solo al final.
7. **Autorización de proyectos**: solo se escribe en `C:\NavaTastic Codigo completo`.
   `C:\Firmware Navarrico 4.3` y `C:\Users\Jesus\Desktop\firmware` son SOLO LECTURA (archivo
   histórico y prístino). Otros proyectos requieren orden explícita y puntual.
8. **Preservar el trabajo existente**: al actualizar docs/cerebro, AÑADIR (errores + soluciones)
   en vez de reescribir; no destruir contexto útil.
9. **Backup/rollback por marca de tiempo (NO por "sesión")**: antes de tocar archivos no
   recuperables (`variant.h`, `userPrefs.jsonc`, `NodeDB.cpp`, `platformio.ini`, fuentes/contexto
   críticos) crear copia `nombre.bak-AAAAMMDD-HHMM` (una por archivo y día) y/o snapshot
   `snap-AAAAMMDD-HHMMSS.zip` (fuentes clave + config + cerebro, sin binarios). En un rollback:
   LISTAR las copias disponibles de ese día/hora y restaurar la más cercana, diciendo
   exactamente qué se restauró. Los backups COMPLEMENTAN a git (no commitear sin orden expresa).
10. **Cierre de sesión**: actualizar `PLAN_DE_TRABAJO.md` y `BITACORA_TECNICA.md`; commits
    locales por hito. Al tocar código común, recompilar las variantes/envs afectados y verificar.
11. **Backup del manual antes de tocarlo (norma 14/08)**: antes de modificar cualquier manual
    (`docs\Manual_NavaTastic.md`, `docs\Manual_uso_NavaTastic.md` u otro documento de
    entrega), crear copia `nombre.md.bak-AAAAMMDD-HHMM` (misma convención de la norma 9) para
    poder revertir si se detecta un error tras el cambio. Tras editar el manual, regenerar el
    PDF con `.\herramientas\generar_pdf.ps1` y verificar la salida.
12. **Distribución V2 al Desktop (norma 14/08)**: los builds NUEVOS (cambios posteriores al
    snapshot baseline "NavaTastic 4.3 Eclipse Edition - Unificado") se distribuyen con
    `distribuir.ps1 -V2` a `Desktop\NavaTastic Eclipse Edition V2` (misma estructura
    Rama×LIPO|NIMH×UF2|OTA, nombres históricos). El Desktop de Eclipse
    (`Desktop\NavaTastic 4.3 120826`) es SOLO LECTURA y NO se toca nunca.

> Detalle completo (original histórico): `docs\cerebro\PROMPT_INICIALIZACION.md` y `PROMPT_DESPLIEGUE.md`.

---

## 1. Qué es esto

Fork de Meshtastic v2.7.26 (base `54e0d8d`) para repetidores solares de infraestructura (malla
SFNarrow, Madrid). Con un solo repositorio se generan **12 firmwares distintos**:

- **6 placas/radios**: Promicro+E22P, Faketec (HT-RA62/SX1262), Seed Solar Node P1, Heltec T114,
  Xiao Kit i2c (SX1262), Xiao+E22P.
- **2 ramas**: Rama 2 (Routers, rol ROUTER) y Rama 1 (Clientes, rol CLIENT + rol semi-permanente).
- **1 configuración**: General (clave admin Master Node, BT 654321). La rama Propia (claves
  del operador + PIN BT propio) se compila con los 12 envs `R2IP_*/R1IP_*` + `build_propia.ps1`:
  las claves y el PIN se piden al compilar (variables de entorno `NAVARICO_PROPIA_KEY_0/1`,
  `NAVARICO_PROPIA_BT`) y **NO se almacenan en ningún fichero del repo**.

Las diferencias entre las 24 versiones antiguas eran SOLO: `variant.h` (radio/potencia/OCV),
`userPrefs.jsonc` (claves/canales/rol/BT) y 2 ficheros con deltas de Rama 1. Todo eso está ahora
**declarado** en envs y perfiles; el núcleo es único.

---

## 2. Cómo compilar

Requisito: PlatformIO (VS Code con la extensión PlatformIO, o `pio` en línea de comandos).

En VS Code: el desplegable de PlatformIO (PROJECT TASKS → `pio run -e ...`) lista los 12 envs.

En CLI (desde la raíz del repo):

```bash
pio run -e navarrico_promicro_e22p_r2ig     # uno solo
pio run                                     # los 12 (default_envs)
```

Los 12 envs (definidos en `variants/nrf52840/navarrico.ini`):

| Env | Placa | Radio | Rama | Rol | Perfil |
|---|---|---|---|---|---|
| navarrico_promicro_e22p_r2ig | Promicro NRF52+E22P | E22P (12 dBm) | R2 | ROUTER | profiles/R2IG_Promicro.jsonc |
| navarrico_faketec_sx1262_r2ig | Faketec | HT-RA62 (22 dBm) | R2 | ROUTER | profiles/R2IG_Faketec.jsonc |
| navarrico_seed_sx1262_r2ig | Seed Solar P1 | SX1262 (22 dBm) | R2 | ROUTER | profiles/R2IG_Seed.jsonc |
| navarrico_t114_sx1262_r2ig | Heltec T114 | SX1262 (22 dBm) | R2 | ROUTER | profiles/R2IG_T114.jsonc |
| navarrico_xiao_kit_sx1262_r2ig | Xiao Kit i2c | SX1262 (22 dBm) | R2 | ROUTER | profiles/R2IG_XiaoKitI2c.jsonc |
| navarrico_xiao_e22p_r2ig | Xiao Kit +E22P | E22P (12 dBm) | R2 | ROUTER | profiles/R2IG_XiaoKitI2cE22P.jsonc |
| navarrico_promicro_e22p_r1ig | ídem | E22P | R1 | CLIENT | profiles/R1IG_Promicro.jsonc |
| navarrico_faketec_sx1262_r1ig | ídem | HT-RA62 | R1 | CLIENT | profiles/R1IG_Faketec.jsonc |
| navarrico_seed_sx1262_r1ig | ídem | SX1262 | R1 | CLIENT | profiles/R1IG_Seed.jsonc |
| navarrico_t114_sx1262_r1ig | ídem | SX1262 | R1 | CLIENT | profiles/R1IG_T114.jsonc |
| navarrico_xiao_kit_sx1262_r1ig | ídem | SX1262 | R1 | CLIENT | profiles/R1IG_XiaoKitI2c.jsonc |
| navarrico_xiao_e22p_r1ig | ídem | E22P | R1 | CLIENT | profiles/R1IG_XiaoKitI2cE22P.jsonc |

Salida: `.pio/build/<env>/` (`.uf2`, `.hex`, `.zip` OTA, `.mt.json`).

Además hay 12 envs **Propia** (`navarrico_<placa>_<radio>_r2ip/r1ip`, extienden a los General):
sus claves admin y PIN BT **no se almacenan** — se piden al compilar (variables de entorno
`NAVARICO_PROPIA_KEY_0/1`, `NAVARICO_PROPIA_BT` o el script interactivo `build_propia.ps1`).
Sin las variables, el build aborta con instrucciones.

Helpers PowerShell (Windows):
```powershell
.\build.ps1 -EnvName navarrico_promicro_e22p_r2ig -Distribuir
.\build.ps1 -Paridad          # reproduce builds del 12/08/2026 byte a byte (solo verificación)
.\distribuir.ps1 -Todo        # copia a distribucion\Rama 1 Clientes|Rama 2 Routers × LIPO|NIMH × UF2|OTA
.\distribuir.ps1 -Todo -V2    # ADEMAS copia a Desktop\NavaTastic Eclipse Edition V2 (norma 0.12)
.\verificar_paridad.ps1 -RefR2IG <carpeta> -RefR1IG <carpeta>   # MD5 12/12 contra builds originales
```

> Guía pública de compilación para usuarios (enlazada desde el README): `docs/Compilar_NavaTastic.md`.

---

## 3. Cómo funciona la selección de versión (mecánica)

Tres ejes ortogonales, todos declarados en el env:

1. **Placa/radio** → `extends` del env upstream + macros:
   - `NAVARICO_RADIO_E22P` / `NAVARICO_RADIO_SX1262`: elige en el `variant.h` fusionado
     (potencia máx 12/22 dBm, pin de alimentación o RXEN, curva OCV clamp 3500/3400 mV) y en
     `src/mesh/Channels.cpp` (TX base 8/22) y `src/modules/NavaCLIModule.cpp` (`set_txpower` 0-12/0-22).
   - Las macros por placa ya existentes (`SEEED_SOLAR_NODE`, `SEEED_XIAO_NRF52840_KIT`,
     `HELTEC_T114`, `NRF52_PROMICRO_DIY`, `RADIO_POWER_ENABLE_PIN`) siguen funcionando igual.
2. **Rama** → `NAVARICO_RAMA_1` (solo Rama 1):
   - `src/mesh/NodeDB.cpp`: fallback de rol CLIENT (vs ROUTER).
   - `src/modules/NavaCLIModule.h/.cpp`: campo `role` en `ResiliencePrefs` (semi-permanente,
     sobrevive a factory reset) + `set_role` lo persiste.
   - Sin la macro, el binario es exactamente el de Rama 2.
3. **Claves/canales/BT/rol** → perfil `profiles/<RAMA>_<Placa>.jsonc`:
   - `platformio-custom.py` lo inyecta como macros `-D` (mecanismo original de Meshtastic,
     `userPrefs.jsonc` → `bin/platformio-custom.py`).
   - El `userPrefs.jsonc` de la raíz es el perfil por defecto (R2IG Promicro) y se usa si un
     env no define `custom_meshtastic_prefs`.

Metadatos del binario (importante para paridad):
- `custom_meshtastic_app_env` → el binario reporta el APP_ENV canónico de la placa
  (p. ej. Faketec reporta `nrf52_promicro_diy_tcxo`), idéntico a los builds originales.
- `NAVARICO_BUILD_EPOCH` (variable de entorno) → fija el sello de fecha; sin ella se usa la
  medianoche de hoy (comportamiento original). `verificar_paridad.ps1` la fija al 12/08/2026.

---

## 4. Ficheros tocados por Navarrico (mapa de cambios)

Todo el código Navarrico está marcado con comentarios `NAVARICO:`.

| Fichero | Cambio | Macro que lo controla |
|---|---|---|
| variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h | Fusion Promicro (E22P) + Faketec (HT-RA62): pin radio, potencia, OCV | NAVARICO_RADIO_E22P / NAVARICO_RADIO_SX1262 |
| variants/nrf52840/seeed_xiao_nrf52840_kit/variant.h | Fusion Xiao Kit (SX1262) + Xiao E22P: pin D5, potencia, OCV, LPCOMP | NAVARICO_RADIO_* |
| variants/nrf52840/seeed_solar_node/variant.h | Bloque Seed (FIX_NATIVE_CORE_RESET, 22 dBm, LPCOMP 3_8, ADC_CTRL, OCV 3400) | — (siempre) |
| variants/nrf52840/heltec_mesh_node_t114/variant.h | LPCOMP activado, OCV 3400, 22 dBm, FIX_NATIVE_CORE_RESET | — (siempre) |
| variants/nrf52840/seeed_solar_node/platformio.ini | lib_ldf_mode deep+, lib_ignore sensores, EXCLUDE magnet/accel | — (siempre) |
| variants/nrf52840/heltec_mesh_node_t114/platformio.ini | EXCLUDE magnet/accel | — (siempre) |
| src/mesh/Channels.cpp | TX base 8 (E22P) / 22 (SX1262) | NAVARICO_RADIO_* |
| src/mesh/NodeDB.cpp | Fallback rol CLIENT (R1) vs ROUTER (R2) | NAVARICO_RAMA_1 |
| src/Power.cpp | F18/V3: contador de baja contra `USERPREFS_LOW_BATTERY_READINGS_COUNT` (fallback 8; antes `#ifdef` `>4`/`>10`) | — (perfil) |
| src/modules/NavaCLIModule.h | Campo `role` en ResiliencePrefs + flags `fullResetPending`/`wipePending` + `NAVATASTIC_BUILD "V3"` (bump manual por release, F22) + F20: `keySlot1/2/0Own` + `NAVS_RESILIENCE_VERSION` | NAVARICO_RAMA_1 |
| src/modules/NavaCLIModule.cpp | set_txpower 0-12/0-22 + rol semi-permanente (4 bloques) + V3: `/nava full_reset` (factoryReset(false)+`navaFullResetKeepKeys`) y `/nava wipe` (factoryReset(true)+remove) + etiqueta en `status`/[Boot] + F20: persistencia/restauración de claves admin (`keys_ls`/`keys_clear`) | NAVARICO_RADIO_* + NAVARICO_RAMA_1 |
| src/modules/AdminModule.cpp | F20: gancho `syncAdminKeysFromConfig()` en `handleSetConfig` (caso security, merge) | — |
| bin/platformio-custom.py | `custom_meshtastic_prefs`, `custom_meshtastic_app_env`, `NAVARICO_BUILD_EPOCH` | opciones de env / variable de entorno (inertes por defecto) |
| platformio.ini | default_envs = 12 envs navarrico (sustituye a tbeam) | — |
| variants/nrf52840/navarrico.ini | Los 12 envs (extienden los upstream) | — |

**Regla**: para cambiar de versión NUNCA se edita código: se elige env/perfil.
Para cambiar un valor físico (potencia, ADC, corte) → `variant.h` de su placa.
Para claves/canales/rol → perfil jsonc. Para añadir una rama (Propia) → perfiles + 2 envs.

---

## 5. Seguridad y claves

- Las claves admin de `profiles/` son claves **PÚBLICAS** (el firmware solo lleva públicas).
- Las claves Propia (K0/K1 del operador + PIN BT) **NO existen en el repo**: se piden al
  compilar los envs `R2IP_*/R1IP_*` (variables de entorno, script `build_propia.ps1`), nunca
  se almacenan. Los patrones `profiles/*R2IP*`/`*R1IP*` están gitignored por si acaso.
  **Flujo operativo de rondas Propia (16/08)**: carpeta
  `Desktop\Navatastic V3 Eclipse Infraestructura Propia` con `PROMPT_BUILD_PROPIA.md`
  reutilizable (huecos `<K0>/<K1>/<PIN>/<ENVS>`, sin claves; registro de rondas); el
  operador pega el prompt relleno en una sesión nueva.
- El JSON con la clave privada del Master Node NO debe distribuirse nunca.
- El fuzzer (`.clusterfuzzlite/router_fuzzer.cpp`) usa la clave General (1 clave, Master Node).

---

## 6. Verificación y regresión

- **PARIDAD 12/12 VERIFICADA (14/08/2026, del BASELINE)**: `verificar_paridad.ps1` compila los 12
  envs en modo paridad (BUILD_EPOCH 12/08, APP_VERSION 2.7.26.54e0d8d, marca temporal y rutas de
  libdeps de cada referencia) y compara MD5 contra `Desktop\NavaTastic 4.3 120826` → **12/12
  byte-idénticos**. ⚠️ **La paridad vale para el código del BASELINE (14/08)**: los builds V2/V2.3/
  V2.6 posteriores divergen a propósito (mensajes de sueño, [Boot], F15/F16...). Para reproducir
  Eclipse byte a byte hay que volver al commit baseline (`git checkout 80e9f7e14~...` o el snapshot
  `_archivo\NavaTastic 4.3 Eclipse Edition - Unificado.zip` / el V2 de 15/08) y ejecutar
  `verificar_paridad.ps1`. La paridad ES la prueba de que la unificación reproduce los binarios
  originales; la V2 se construye sobre esa base.
- Detalles de la receta, fallos y fixes: `BITACORA_TECNICA.md`.
- Los `.zip` OTA pueden diferir en el nombre interno del env (manifest/entry): el firmware
  (`.uf2`/`.bin`) es byte-idéntico. Comparar siempre el `.uf2`.
- Referencia histórica "Eclipse Edition": build del 12/08 17:09-17:15 de Rama 2 (R2IG).
- No paralelizar dos builds del MISMO env (corrompe la caché de PlatformIO).
- MAX_PATH (error #13 del pasado): resuelto de raíz (raíz corta); no usar rutas profundas.
  Ojo: el workaround histórico (r1promic/r1xiaoki) dejó rutas embebidas distintas en R1 —
  ya absorbido en `custom_meshtastic_libdeps_map`.
- **Flash nRF52**: `pio run -e <env> -t upload --upload-port COM<x>` (touch 1200bps + nrfutil,
  protocolo del board); alternativamente, con el bootloader en modo DFU (doble reset) aparece la
  unidad **NICENANO** y se copia el `.uf2` a mano. NO hay unidad UF2 con el touch de 1200bps.
- **Pruebas en banco**: con USB, TX a 1 dBm en el E22P (picos de corriente); la detección de
  batería baja exige SIN USB (alimentar solo por fuente); los avisos NO se ecoan en la API del
  propio emisor (verificar con un nodo observador).

---

## 7. Morrilla y archivo

`_archivo/` contiene material histórico (backups `.bak-*`, builds viejos, notas obsoletas).
Está gitignored y no forma parte del código.

## 8. Documentación

- `herramientas\generar_pdf.ps1` + `plantilla_navatastic.tex` — genera los PDFs de
  los manuales (Pandoc + MiKTeX) en `docs\pdf\` (gitignored). Excluye los docs de desarrollo
  por defecto (norma 11/08: solo manuales de firmware y comandos). Uso:
  `.\herramientas\generar_pdf.ps1` o con `-Archivo Manual_NavaTastic.md`.
- `Manual_NavaTastic.md` (docs/) — manual de comandos `/nava`.
- `profiles/README.md` — perfiles.
- Este fichero es la guía de retoma para agentes.

## 9. Publicación a GitHub (EA2OY/NavaTastic) — flujo obligatorio

> El repo público es https://github.com/EA2OY/NavaTastic (solo General, sin claves Propia).
> Proyecto hermano: https://github.com/EA2OY/MeshNavarra-Utility (app del operador).

1. **Nunca subir el historial local** (contiene claves Propia en commits del 14/08). Publicar
   SIEMPRE una rama huérfana con UN solo commit del árbol saneado:
   `git branch -D github-public` → `git checkout --orphan github-public` →
   `git add -A` → `git add -f distribucion docs/pdf` → **borrar** `.github\workflows\*.yml`
   del disco → `git commit` → `git push -f <url-token> github-public:main` →
   `git checkout master` → **`distribuir.ps1 -Todo`** (el cambio de rama borra los ficheros
   force-add del disco — L24).
2. **Actions desactivadas** en el repo (los workflows upstream envían correos de error).
   Si se recreara el repo: quitar workflows + `PUT /repos/EA2OY/NavaTastic/actions/permissions`
   con `{"enabled":false}`.
3. **Binarios**: subirlos como assets de un Release (API `uploads.github.com`), no como
   enlaces a carpetas. El README ya apunta a Releases.
4. **Credenciales**: usar un token puntual del operador (revocar al terminar) o `gh` CLI con
   login. Nunca commitear el token.
