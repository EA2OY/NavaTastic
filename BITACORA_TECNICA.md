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

- [x] Paridad Promicro R2IG 1/1 (MD5 idéntico a Eclipse).
- [ ] Verificación 12/12 en curso (verificar_paridad.ps1, ~1h).
- [ ] Distribución a `distribucion\` con nombres actuales.
- [ ] Propia (R2IP/R1IP) a futuro: añadir 12 perfiles + 12 envs, sin tocar código.
- [ ] GitHub: solo General (docs saneadas de claves Propia).

## DECISIONES DE DISEÑO CLAVE

1. **Los 12 envs navarrico_* son la interfaz de usuario** (VS Code). La paridad MD5 requiere
   el modo -Paridad (variables de entorno) — documentado en `Guia_para_agente_sobre_NavaTastic.md`.
2. **El perfil jsonc es la fuente de las diferencias de rama** (claves, canal, rol, BT);
   el código solo tiene macros ortogonales (radio, rama-1) para lo que no puede ir en el perfil.
3. **`custom_meshtastic_libdeps_map`** define la ruta canónica embebida; el `-ffile-prefix-map`
   se inyecta por Python porque el parser de build_flags de PIO destruye los backslashes.
4. **Todo override es inerte por defecto** (solo actúa con variables de entorno u opciones explícitas).
