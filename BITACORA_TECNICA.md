# BITÁCORA TÉCNICA — Unificación NavaTastic (14/08/2026)

Registro de éxitos, fallos y fixes del proceso de unificar los 24 repos en uno solo
con **salida byte-idéntica (MD5) a los binarios que funcionan** (Eclipse Edition R2IG + R1IG).
Complementa a `PLAN_DE_TRABAJO.md` (estado) y `Guia_para_agente_sobre_NavaTastic.md` (cómo funciona).

---

## RESUMEN DE LOGROS

- ✅ Un solo repositorio → 12 compilaciones (6 placas × R2IG/R1IG) desde `platformio.ini`.
- ✅ **MD5 byte-idéntico al build original (Eclipse Edition)** — verificado 1/12 a fondo
  (Promicro R2IG: `C93764F62C15063F72A963DE15C9CCB0` = referencia) y 12/12 en curso.
- ✅ El núcleo es ÚNICO; las diferencias entre versiones son declarativas (env + perfil).
- ✅ Sin MAX_PATH, sin `.pio` heredado, sin 24 carpetas duplicadas.
- ✅ Cero cambios de comportamiento: el código R2 compila idéntico al original (probado).

---

## LA RECETA DE LA PARIDAD BYTE-A-BYTE (lo que hubo que descubrir)

El firmware embebe 5 metadatos que hacen que "recompilar" produzca binarios distintos
aunque el código sea el mismo. Todos se controlan ahora:

| Metadato | Dónde vive | Cómo se fija |
|---|---|---|
| `APP_ENV` (nombre de placa) | `-DAPP_ENV=` en `platformio-custom.py` | `custom_meshtastic_app_env` en el env (los envs navarrico_* reportan el nombre canónico) |
| `APP_VERSION` (incluye SHA de git) | `bin/readprops.py` (git rev-parse) | variable `NAVARICO_APP_VERSION` (build.ps1 -Paridad la fija a `2.7.26.54e0d8d`) |
| `BUILD_EPOCH` (día del build) | `platformio-custom.py` (datetime.now) | variable `NAVARICO_BUILD_EPOCH` (build.ps1 -Paridad fija 12/08/2026) |
| Marca `__TIME__/__DATE__` (Crypto/RNG.cpp y RadioLib/Module.cpp) | macros del compilador en LIBS | variables `NAVARICO_BUILD_TIME/DATE` → `-D__TIME__=\"...\" -D__DATE__=\"...\"` |
| Rutas de libdeps en `__FILE__` (arduino-fsm y otros libs con asserts) | path `.pio\libdeps\<env>\...` embebido | `-ffile-prefix-map` inyectado desde Python (custom_meshtastic_libdeps_map) |

Regla de oro descubierta: **cualquier línea añadida en `NodeDB.cpp` cambia el binario**
(`saveToDisk` embebe `__LINE__`). Los cambios deben ser a 0 líneas netas o inexistentes.

---

## CRONOLOGÍA DE FALLOS Y FIXES

### F1. La versión embebía el SHA de git del repo nuevo (bfe547a ≠ 54e0d8d)
- **Síntoma**: APP_VERSION = `2.7.26.<sha del repo>`; los 24 repos viejos tenían `54e0d8d`.
- **Causa**: `bin/readprops.py` hace `git rev-parse --short=7 HEAD`.
- **Fix**: override `NAVARICO_APP_VERSION` en `platformio-custom.py` (inerto si la variable no existe).

### F2. BUILD_EPOCH = día de compilación (recompilar hoy ≠ 12/08)
- **Causa**: `datetime.now().replace(hour=0,...)` en platformio-custom.py.
- **Fix**: override `NAVARICO_BUILD_EPOCH` (variable de entorno, inerte por defecto).

### F3. APP_ENV = nombre del env (navarrico_* ≠ canónico)
- **Causa**: `-DAPP_ENV=` + PIOENV. Renombrar envs cambiaba el binario.
- **Fix**: `custom_meshtastic_app_env` (opción de env) → APP_ENV canónico.

### F4. Las LIBs embeben la ruta `.pio\libdeps\<env>\...` (arduino-fsm usa `__FILE__`)
- **Síntoma**: 1043 bytes de diff localizados en la string `...\arduino-fsm\Fsm.cpp`.
- **Diagnóstico (laborioso)**: comparación de `.o` por objeto mostró que TODAS las libs eran
  idénticas salvo esa string; el `.a` del framework difería solo en metadatos del archivo
  (los 63 miembros eran idénticos).
- **Fix 1 (fallido)**: `-ffile-prefix-map` en `build_flags` del ini → **PlatformIO ELIMINA los
  backslashes** de build_flags (el flag llegaba como `.piolibdeps...`, inservible).
- **Fix 2 (fallido)**: inyección en `projenv.Append(CCFLAGS=...)` → **`projenv` solo afecta al
  SRC, no a las librerías** (las libs se compilan en envs propios).
- **Fix 3 (fallido)**: `env.Append(CCFLAGS=...)` (env principal) → tampoco llega a las libs.
- **Fix 4 (FUNCIONA)**: `for lb in env.GetLibBuilders(): lb.env.Append(CCFLAGS=[flag])` —
  las librerías se compilan en `lb.env` y es ahí donde hay que inyectar.
- **Bonus**: `env` principal + `projenv` también reciben el flag (src, por si acaso).

### F5. El mapa de libdeps de Promicro R2IG apuntaba a la ruta ABSOLUTA (error de sonda)
- **Síntoma**: tras el remap correcto, 16456 bytes de diff (¡más que antes!).
- **Causa**: una sonda inicial (confundida con el binario R1IG) indicó que Eclipse R2IG embebía
  `C:\Users\Jesus\.platformio\libdeps\...`; en realidad **R2IG embebe la ruta RELATIVA**
  `.pio\libdeps\nrf52_promicro_diy_tcxo\...` (R1IG sí lleva las absolutas r1promic/r1xiaoki).
  Longitud distinta → cascada de layout.
- **Fix**: mapa de R2 promicro → `.pio/libdeps/nrf52_promicro_diy_tcxo` (relativo). Los demás
  mapas (R2: relativos; R1: absolutos r1xxx) ya eran correctos.
- **Lección**: verificar SIEMPRE la sonda contra el binario correcto (R2 vs R1).

### F6. El override de `__TIME__/__DATE__` con CPPDEFINES rompía la compilación
- **Síntoma**: `invalid conversion from 'int' to 'const char*'` y `12: No such file or directory`.
- **Causa**: CommandLineToArgvW (parser de línea de comandos de Windows) elimina las comillas
  simples del valor → `__TIME__` quedaba sin comillas (macros numéricas) y la fecha se partía.
- **Fix**: flags crudos `-D__TIME__=\"HH:MM:SS\"` / `-D__DATE__=\"Mon DD YYYY\"` (comillas
  escapadas con backslash) inyectados como CCFLAGS. Los warnings `builtin-macro-redefined`
  son inofensivos.

### F7. Cualquier línea añadida en NodeDB.cpp rompía la paridad (1 byte)
- **Síntoma**: `.o` de NodeDB.cpp con 1 byte distinto (inmediato MOVW 1866 vs 1861).
- **Causa**: `NodeDB::saveToDisk` embebe `__LINE__` (un assert/check interno). Mis comentarios
  y el `#ifdef` de Rama 1 desplazaban las líneas.
- **Fix**: el fallback de rol se revirtió a la línea ORIGINAL (0 líneas netas). El rol CLIENT
  de Rama 1 lo garantiza el PERFIL (`USERPREFS_CONFIG_DEVICE_ROLE=CLIENT` en R1IG), no el código.
  Los comentarios NAVARICO se pusieron INLINE (misma línea, +0 líneas).

### F8. "El orden de libs difería entre builds" — falso positivo
- **Síntoma**: listados `libNNN` con órdenes "distintos" entre builds.
- **Causa**: mi script de comparación ordenaba alfabéticamente (Get-ChildItem) mientras que
  PlatformIO enumera alfabéticamente desde `piolib.py:1106` (`sorted(os.listdir(...))`) —
  el orden REAL siempre fue el mismo.
- **Lección**: verificar con el BINARIO (diff byte a byte), no con heurísticas de carpetas.

### F9. El build limpio del folder original NO reproducía Eclipse… excepto 6 bytes
- **Experimento decisivo**: copia del repo original + `.git` en temp + build limpio → difiere
  de Eclipse SOLO en la marca temporal (`05:24:42Aug 14 2026` vs `07:35:34Aug 12 2026`).
- **Conclusión**: Eclipse ES reproducible en limpio; la paridad absoluta es alcanzable.
- (El falso "incremental = irreproducible" se descartó con este experimento.)

### F10. Morralla y scripts con errores de PowerShell (aprendizajes de entorno)
- `Join-Path` con 3 argumentos no existe en PS 5.1 (usar -Path/-ChildPath).
- `Select-String -Encoding Byte` no existe (usar lectura binaria propia).
- `$env` y `$LabelB:` son nombres problemáticos (variable reservada / parsing de `$x:`).
- Escritura de .ps1: el shell mangled `r`n` (CRLF) en cadenas — usar ficheros con el tool de
  escritura, no here-strings por shell.
- La política de ejecución bloquea .ps1 → invocar con `powershell -NoProfile -ExecutionPolicy Bypass -File`.

---

## HERRAMIENTAS DE DIAGNÓSTICO CREADAS (temp, no forman parte del repo)

- `diffuf2.ps1` — compara dos UF2 byte a byte y muestra los runs de diferencia con contexto.
- `findstr.ps1` / `probe.ps1` — búsqueda de strings/constantes en binarios.
- `proberefs2.ps1` — extrae rutas de libdeps embebidas en binarios de referencia.
- `cmplibs.ps1` — comparación de libs entre builds (¡usar con cuidado: orden alfabético!).
- `findstamp.ps1` — localiza qué `.o` embebe `__TIME__/__DATE__`.

---

## ESTADO

- [x] **PARIDAD 12/12 CONSEGUIDA** (14/08): los 12 UF2 del repo unificado son **byte-idénticos**
      (MD5) a los binarios originales (Eclipse Edition R2IG + R1IG). Verificar_paridad.ps1.
- [x] Distribución a `distribucion\` (Rama 1 Clientes / Rama 2 Routers × LIPO/NIMH × UF2/OTA,
      32 ficheros, nombres históricos). Los UF2 son los de paridad (byte-idénticos).
- [x] Docs de contexto copiados a `docs\` (cerebro + subnotas + 3 docs + manuales + GUIA).
- [ ] GitHub (otra sesión): solo General; saneado de claves Propia en las docs.
- [ ] Propia (R2IP/R1IP): añadir 12 perfiles + 12 envs, sin tocar código.

## RESULTADO FINAL (resumen de la receta completa)

1. Envs `navarrico_<placa>_<radio>_<rama>` declarados en `variants/nrf52840/navarrico.ini`.
2. Perfiles `profiles/<RAMA>_<Placa>.jsonc` = copias 1:1 de los jsonc originales.
3. `custom_meshtastic_app_env` = APP_ENV canónico; `custom_meshtastic_prefs` = perfil;
   `custom_meshtastic_libdeps_map` = ruta canónica embebida (por env, R2 relativas o absolutas,
   R1 r1promic/r1xiaoki — ¡el workaround de MAX_PATH cambió las rutas embebidas de 4 boards!).
4. `platformio-custom.py` inyecta (solo con variables de entorno, inerte por defecto):
   APP_VERSION, BUILD_EPOCH, __TIME__/__DATE__ y el -ffile-prefix-map (PIO destruye los
   backslash de build_flags → se inyecta desde Python a los LIB BUILDERS, no a projenv).
5. Código: `#line` en los asserts de main-nrf52 (E22P 456/459 vs SX1262 451/454), NodeDB.cpp
   a 0 líneas netas, bloques LPCOMP por placa en getActiveLpcompThreshold, RF95_RXEN por radio.

### Descubrimientos F11/F12 (última milla)
- **F11**: cada repo original divergió en main-nrf52.cpp (bloques SEEED/XIAO/T114 solo en sus
  carpetas; bloques RADIO_POWER_ENABLE_PIN solo en E22P) → el unificado lleva TODOS los bloques
  con #elif. Los asserts (que embeben __LINE__) se fijan con #line por radio.
- **F12**: los "UF2" de PIO para nRF52 llevan solo los primeros ~312 bloques codificados y el
  resto crudo → la marca temporal puede quedar en la zona cruda. Get-Stamp busca en el fichero
  crudo (no reconstruye la imagen).

### Cómo rehacerlo / verificarlo
```powershell
.\verificar_paridad.ps1                    # 12 builds en modo paridad + MD5 vs Desktop 120826
.\build.ps1 -EnvName <env> -Distribuir     # build normal + copia a distribucion\
.\distribuir.ps1 -Todo                     # repoblar distribucion\ desde los .pio/build
```
Nota OTA: los .zip difieren en el nombre interno del env (manifest/entry); el firmware (.uf2/.bin)
es byte-idéntico. Si se quiere zip idéntico, habría que fijar `progname` por env (pendiente opcional).

## DECISIONES DE DISEÑO CLAVE

1. **Los 12 envs navarrico_* son la interfaz de usuario** (VS Code). La paridad MD5 requiere
   el modo -Paridad (variables de entorno) — documentado en `Guia_para_agente_sobre_NavaTastic.md`.
2. **El perfil jsonc es la fuente de las diferencias de rama** (claves, canal, rol, BT);
   el código solo tiene macros ortogonales (radio, rama-1) para lo que no puede ir en el perfil.
3. **`custom_meshtastic_libdeps_map`** define la ruta canónica embebida; el `-ffile-prefix-map`
   se inyecta por Python porque el parser de build_flags de PIO destruye los backslashes.
4. **Todo override es inerte por defecto** (solo actúa con variables de entorno u opciones explícitas).

---

## SESIÓN 14/08 (3ª-8ª partes) — PORTABILIDAD DEL CONOCIMIENTO (cerebro VIVO + joya + PDF + V2)

- **6ª-8ª parte — SNAPSHOT BASELINE + FASE 2 V2 (sueño/vivo/listo + fav real + fragmentación)**:
  - **Baseline**: `_archivo\NavaTastic 4.3 Eclipse Edition - Unificado.zip` (HEAD 644d09e68).
    Todo cambio posterior rompe el MD5 de Eclipse a propósito; el snapshot es el rollback.
  - **A (sueño/vivo/listo)**: al venir de sueño (`ResiliencePrefs.wasInSleep`, seteado antes de
    cada `cpuDeepSleep` por batería), el pre-check de `main.cpp` usa el despertar LPCOMP real
    (`navaGetLpcompWakeMv()` en main-nrf52) como umbral: V<corte OCV (3500 E22P / 3400 SX1262
    = `power->OCV[10]`) → silencio + re-sueño; corte≤V<LPCOMP → [Vivo] + re-sueño tras drenar
    la cola (patrón storm, `sleepPending` en NavaCLI); V≥LPCOMP → boot normal → [Listo].
    Sueño en runtime (`Power.cpp` monitor) delega en `navaCLIModule->handleLowBatteryEvent()`
    → [Sueño] + sueño diferido (evita dormir con la cola sin drenar). Gate `/nava sleepmsg
    [on|off]` (persistido). INA219 solo si presente en el bus (auto-detectado); `power`
    muestra ADC + INA ±mA CARGANDO/DESCARGANDO.
  - **B (fav real)**: `ResiliencePrefs.autoFavIds[16]` persistido; reconciliación en `runOnce`
    (60s) con `activeDirectRouters`; `status`: Auto = fav ∩ autoFavIds, Manual = fav − autoFavIds
    (sin doble conteo ni pérdida tras reinicio); `fav ls` etiqueta [AUTO]/[MAN]; `fav rm` limpia.
  - **C (fragmentación)**: `enqueueResponse` corta en `\n` preferentemente (líneas enteras;
    si la línea >190, recae en espacio).
  - **F13 — distribuir.ps1 copiaba el artefacto MÁS ANTIGUO**: `.pio/build/<env>` acumula UF2/zip
    de builds previos (`54e0d8d`, `1a9937d`, `b9a1fa8`...) y `Get-ChildItem | First` = orden
    alfabético → se distribuyó el de era Eclipse (Promicro R2IG MD5 C93764F6 = ¡el de referencia!).
    **Fix**: `Sort-Object LastWriteTime -Descending | Select -First 1`. Detección: MD5 del
    Promicro distribuido = MD5 de Eclipse (imposible tras el cambio de código).
  - **Distribución**: `distribuir.ps1 -Todo -V2` → 32+32 ficheros (distribucion\ + Desktop V2),
    MD5 nuevos. `distribucion\` ya NO es Eclipse; Eclipse sigue en Desktop 120826 + snapshot.

- **3ª parte — CEREBRO VIVO MIGRADO AL REPO ÚNICO**:

- **5ª parte — SISTEMA DE PDF PORTADO AL REPO**:
  - `HerramientasPropiasIA\generar_pdf.ps1` + `plantilla_navatastic.tex` copiados 1:1
    desde 4.3 (SOLO LECTURA allí) al repo. Entrada por defecto = `docs\`, salida =
    `docs\pdf\` (**gitignored**, se crea sola). Plantilla intacta (estética "panel de
    control": fondo #0A1120, dorado #F1D38E, verde #39FF14, Segoe UI/Consolas).
  - `$Excluir` (norma 11/08): transfer_context, guia_integracion, GUIA_AGENTE_NAVTASTIC,
    INSTRUCCION_AUDITORIA_CLAUDE → solo se generan los 2 manuales.
  - Verificado: 2 PDFs generados sin errores (Pandoc local + MiKTeX x64 presentes).
  - Regla: los PDFs NO se regeneran en el Desktop; el repo es la única fuente.

- **3ª parte — CEREBRO VIVO MIGRADO AL REPO ÚNICO**:
  - `docs\` reestructurado a layout canónico `docs\cerebro\` (cerebro.md + subnotas 01-12
    + PROMPT_INICIALIZACION/PROMPT_DESPLIEGUE). Las subnotas 01-11 que faltaban por el
    fallo de la copia inicial (comodines con -LiteralPath) se trajeron desde 4.3.
    **Verificado MD5 1:1 = 15/15 vs 4.3** (el 4.3 no se tocó).
  - `cerebro.md` VIVO: banner ESTADO 14/08 + sección 5 nueva (repo unificado, log de la
    sesión, **tabla F1-F12**, sección VIGENTE vs OBSOLETO, handover). Log histórico y
    subnotas conservados íntegros (AÑADIR, no reescribir).
  - Banners ESTADO 14/08 en subnotas 01-12 + 2 PROMPTs (qué sigue vigente, qué quedó
    obsoleto: 24 carpetas, LAB, Felix, distribución al Desktop, HerramientasPropiasIA\).
  - Adendas "REPO UNIFICADO" en `transfer_context.md`, `guia_integracion_navarrico.md`,
    `Manual_NavaTastic.md`, `Manual_uso_NavaTastic_4.2.md`, `GUIA_AGENTE_NAVTASTIC.md`.
  - Regla aplicada: `C:\Firmware Navarrico 4.3` = SOLO LECTURA (archivo histórico).
- **4ª parte — JOYA DE LA CORONA (COMPLETADA)**: creado `PORTING_NUEVO_FORK.md` (raíz del
  repo) — la guía maestra de portabilidad a forks nuevos: (1) inventario fichero a fichero
  con anclas de búsqueda; (2) catálogo de bloques (E1 energía, E2 Flash, S seguridad,
  N NavaCLI, P paridad) con comportamiento y dependencias; (3) procedimiento paso a paso
  (0 preparación → 1 análisis → 2 pases → 3 compilar → 4 verificar); (4) checklist de
  trampas (F1-F12 + MAX_PATH + .pio heredado + no paralelizar). Material base: diffs reales
  vs prístino (git diff 54e0d8d0a..HEAD, 69 ficheros), anclas localizadas por grep
  (NAVARICO:, USERPREFS_*, FIX_NATIVE_CORE_RESET, funciones), BITACORA F1-F12 y docs/.
