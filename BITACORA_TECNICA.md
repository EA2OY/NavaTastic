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

### F14 (V2.2). Los mensajes [Sueño]/[Vivo]/[Listo] no llegaban al canal Navadmin- **Síntoma**: probado en banco (operador): ningún aviso recibido.
- **Causa**: (1) el jitter anticolisión del canal 1 (500-6500ms, `enqueueResponse`) retrasaba
  el primer envío; (2) `sleepTime = millis()+5000` se fijaba al ENCOLAR y el re-sueño entraba
  justo después de `sendToMesh` (ASÍNCRONO) → `cpuDeepSleep` apagaba la radio con el paquete
  sin emitir o a mitad de la ráfaga (~1s airtime SFNarrow).
- **Fix**: el sueño se programa DESDE el envío real del último fragmento (`sleepTime = millis()+3000`
  tras `sendToMesh`); jitter corto 300-2300ms con el flag `quick` (solo los 3 mensajes de sueño).
- **Bonus precisión (subagente + verificado)**: el pre-check de arranque hacía "5 lecturas 200ms"
  pero el throttle de 5s de `AnalogBatteryLevel::getBattVoltage` devolvía cache → 1 sola medida
  ADC real, cruda (sin LPF) y sin asentamiento post-reset. Fix: `Power::readPowerStatus(bool force)`
  + `getBattVoltage(bool force)` (default false, inerte) + `force=true` en el pre-check + `delay(500)`
  de asentamiento. LPCOMP NO se tocó (ya estable: EVENTS_READY + delay(10) + histéresis 50mV).
- **Ojo**: los overrides de `getBattVoltage` de otros sensores (MAX17048/CW2015/BQ/meshSolar/serial)
  recibieron el param `force` con `(void)force;` (comportamiento idéntico).

### F15 (V2.3, 14/08). Los mensajes [Sueño]/[Vivo]/[Listo] no llegaban — CAUSA RAÍZ en /resilience.bin
- **Síntoma del banco**: el nodo (Promicro R2IG) reporta **role CLIENT** pese al perfil ROUTER, y
  `/nava sleepmsg` responde **OFF** persistente (ni factory reset ni flasheo lo cambian). La app no
  puede cambiar el rol (el fichero pisa /prefs en cada boot). F15: no llegaba ningún aviso.
- **Causa 1 (bug de parseo)**: `sleepmsg on|off` usaba `substr(8)` pero "sleepmsg" son 8 letras +
  espacio separador → `arg = " on"` ≠ `"on"` → caía siempre en la rama de estado (SLEEPMSGS: OFF).
  El gate NUNCA se pudo cambiar por comando. **Fix**: `substr(9)` (V2.3b).
- **Causa 2 (compat de fichero — la raíz real)**: `/resilience.bin` escrito por Eclipse R2IG (12/08)
  tiene **80 bytes SIN los campos V2** (`sleepMsgs`/`wasInSleep`): el byte 11 era padding (0) y los
  bytes 77-79 padding (0). El struct V2 R2IG también ocupa 80 bytes (el byte 11 lo ocupa `role`) →
  la migración por tamaño (`fileSize < sizeof(prefs)`) NO se dispara → `sleepMsgs` lee padding = OFF
  y `role` lee 0 = CLIENT. Como resilience.bin sobrevive a flasheo Y factory reset (por diseño),
  el nodo queda CLIENT+OFF "para siempre". **Fix (V2.3c)**: campo `uint32_t version` al final del
  struct (84 bytes) → todo fichero ≤80 migra con `sleepMsgs=1`, `role=0xFF` (sin fijar → perfil),
  `wasInSleep=0`. También: `navaResiliencePeek` lee `fileSize` (no `sizeof`) y la migración de
  `navaSetWasInSleep` rellena `role=0xFF`+`version` (el byte 11 del fichero viejo era padding=0 →
  sin esto, role=0=CLIENT).
- **Pendiente de verificar (sesión nueva)**: (1) que el V2.3c migre el fichero del nodo y la API
  reporte ROUTER (a última vista seguía CLIENT — ¿no corrió el build o no se mantuvo el flasheo?);
  (2) la persistencia de escritura de resilience.bin (el `sleepmsg on` con V2.3b daba OK pero OFF
  tras reboot — sospecha de FS de escritura; el log TEMP `F15: resilience.bin escrito 84/84` debía
  discriminarlo pero **el CDC del nodo NO emite serial**); (3) explicar el **CDC mudo** (sin texto a
  115200/57600/921600 pese a que la API USB funciona: `meshtastic --port COM15 --info` responde,
  myNodeNum 551169628, NRF52_PROMICRO_DIY, firmware c7af16b). Si la escritura falla → `nrf erase`.
- **Instrumentación TEMP F15 (marcada `NAVARICO: TEMP F15`, retirar al confirmar)**: logs en
  pre-check main.cpp (`F15 precheck: wasInSleep/gate/lecturas/rama`), `Low voltage counter` a
  LOG_INFO en Power.cpp, `F15: [Sueño] encolado` y `F15: resilience.bin escrito %u/%u bytes` en
  NavaCLIModule.cpp.
- **Anotado (NO tocado, posible F16)**: `fav rm` usa `substr(8)` con "fav rm" de 6 letras → se
  come el primer carácter del id. `fav add`/`fav auto` OK. Jitter quick muerto (setIntervalFromNow
  pisado por OSThread::run→setInterval; cosmético). Whitelist canal 1: sleepmsg fuera (solo lectura)
  — decisión del operador: dejarlo, la consulta va por DM PKI.
- **Builds**: V2.3b MD5 `8D0B32672BF01BBBD081A2427F4A6F47` (fix parseo) y V2.3c MD5
  `98A97F888D32E2CF617D3681B93A7276` (+fix migración) en `Desktop\NavaTastic Eclipse Edition V2`
  (Rama 2 Routers\LIPO\UF2). Backup binario V2.2 en `_archivo\Promicro R2IG V2.2 20CDA06A - antes
  de instrumentacion F15.uf2`. Backups código: `.bak-20260814-1819` (main/Power/NavaCLIModule).

### F15 (cierre 15/08). El gate por tamaño no bastaba: fichero envenenado 84B + InternalFS sin trunc
- **Verificado en banco (Promicro R2IG, sesión 14-15/08)**: la migración V2.3c (gate
  `fileSize < sizeof`) NO curó el nodo porque su `/resilience.bin` medía **1252 bytes**
  (el manifiesto de ficheros lo delató): metadata LFS corrupta + **`FILE_O_WRITE` de
  Adafruit InternalFS NO trunca** (`LFS_O_RDWR | LFS_O_CREAT`, seek a 0, tamaño nunca
  encoge) → el fichero "envenenado" (84B role=0, escrito por un build intermedio V2.3)
  sobrevivía a todo. Con tamaño raro, el read del boot era impredecible → role=CLIENT
  permanente y sleepmsg OFF.
- **Fix FINAL (a7ab507, compilado y flasheado)**: `FSCom.remove("/resilience.bin")`
  antes de CADA escritura (saveResiliencePrefs + navaSetWasInSleep) → fichero SIEMPRE de
  84 bytes exactos; gates de migración `fileSize != sizeof(prefs) || version !=
  0x4E415653` en loadResiliencePrefs/navaResiliencePeek/navaSetWasInSleep; saneado de
  campos fuera de rango; fallback de navaSetWasInSleep completado. Legacy VÁLIDO de
  ficheros 4.3 se preserva.
- **CICLO VERIFICADO**: 1252B→84B ✓ · `set_role client`→CLIENT ✓ · reboot→persiste ✓ ·
  **factory reset→CLIENT sobrevive** ✓ · `set_role router`→ROUTER ✓ · `nrf erase`→
  ROUTER (perfil) + fichero nuevo 84B ✓. Factory reset solo rmDir("/prefs"): el
  resilience.bin sobrevive (diseño confirmado). El nrf erase REGENERA la clave del nodo
  (¡PKI: los peers guardan la vieja → limpiar sus entradas!).
- **CDC mudo explicado**: logs de boot se pierden antes de la enumeración USB (normal
  nRF52); en runtime los LogRecords protobuf SOLO con `debug_log_api_enabled=true`
  (el factory reset lo borra). API USB = canal de verdad.
- **Falsos positivos de la sesión (no repetir)**: (1) el "flip de rol en 1s" era
  artefacto de polls `--info` lentos (3-10s cada uno) + el **reboot automático 7s tras
  cualquier setConfig con requiresReboot** (AdminModule→saveChanges→reboot(7) →
  powerCommandsCheck en loop()); (2) el "factory reset borra resilience.bin" era el
  fichero corrupto, no el reset.
- **BLE**: `PowerFSM::serialEnter` apaga la baliza BLE mientras el CLI USB está
  conectado (estándar); "no emite baliza" tras la batería de resets se recuperó con
  power-cycle → candidato F16 (resumeAdvertising tras shutdown()).

### LECCIONES DE SESIÓN 14/08 (para no repetir fallos)
- **L1 — AÑADIR, no reescribir (cerebro §5.2)**: al añadir una entrada de log el agente pisó
  la 8ª parte (al insertar la 9ª) y la 9ª (al insertar la 10ª); ambas restauradas. Regla: el
  `oldString` debe ser la entrada anterior COMPLETA y la nueva se inserta DESPUÉS, sin borrar.
- **L2 — Firmas**: al añadir un parámetro (p. ej. `enqueueResponse(..., bool quick)`), tocar
  SIEMPRE declaración (.h) Y definición (.cpp) a la vez (el compilador: "no declaration matches").
- **L3 — resilience.bin**: los helpers estáticos que reescriben el fichero deben preservar
  TODOS los campos (leer struct completo → modificar 1 campo → escribir); un write con struct
  parcial borra chemistry/vbat_cutoff/role/autoFavIds. Compat: ficheros viejos → migrar campos
  nuevos con defaults en la misma escritura.
- **L4 — NodeNum = uint32_t**: nunca guardar ids de nodo en arrays uint8_t (trunca a 8 bits).
- **L5 — PowerShell 5.1**: `Start-Process -PassThru` devuelve `ExitCode` VACÍO tras WaitForExit
  (falsos FALLOS); verificar el éxito con el log de pio (`1 succeeded`) o timestamps de los
  artefactos, no con ExitCode.
- **L6 — Artefactos acumulados**: `.pio/build/<env>` acumula UF2/zip de builds previos;
  distribuir SIEMPRE el más reciente (LastWriteTime) — ver F13.

### LECCIONES DE SESIÓN 14-15/08 (L7-L12, para no repetir fallos)
- **L7 — Adafruit InternalFS `FILE_O_WRITE` NO trunca** (`LFS_O_RDWR|LFS_O_CREAT`):
  escribir menos bytes deja el fichero con su tamaño máximo histórico; el tamaño no es
  fiable como gate de formato. Regla: **`FSCom.remove()` antes de abrir para escribir**
  cualquier fichero binario de estado (resilience.bin) y gatear por `fileSize !=
  sizeof(struct) || marcador de versión`, no por `<`.
- **L8 — El manifiesto de ficheros delata el FS**: `--listen` (con debug_log_api_enabled)
  entrega "File: X (N) bytes" — la forma más rápida de ver tamaños/corrupción LFS por API.
- **L9 — `--set` con requiresReboot REBOTA SOLO a los 7s** (reboot(7) + powerCommandsCheck):
  cualquier verificación post-write debe esperar ≥30s o mirar el HB/uptime; los polls
  `--info` encadenados son lentos (3-10s) y crean líneas de tiempo falsas.
- **L10 — El factory reset borra `debug_log_api_enabled`** (y las admin_key añadidas por
  app): tras un reset, re-activar el canal de logs y re-añadir claves admin si procede.
- **L11 — `nrf erase` regenera el par de claves del nodo**: los peers con la clave vieja
  fallan el DM PKI (`PKI_SEND_FAIL_PUBLIC_KEY`) → `--remove-node` en el peer + reaprender
  el nodeinfo nuevo. El fabric/reset de config NO toca las claves.
- **L12 — Instrumentación por MALLA para nodos sin USB**: con la fuente de laboratorio no
  puede haber USB (el cargador sube VBUS) → los diagnósticos TEMP se encolan al canal
  Navadmin (patrón enqueueResponse(0,1,...)) y se reciben en un nodo observador.

### CANDIDATOS F16 (anotados, NO tocados — esperan orden)
- **F16a — Admin no persiste tras reboot**: **CERRADO (15/08)** — `saveToDisk(SEGMENT_NODEDATABASE)`
  tras acreditar/favoritear (solo si cambia) + verificado en banco (PONG antes/después de reboot).
- **F16b — BLE**: baliza no reaparece tras shutdown() sin power-cycle (revisar
  resumeAdvertising; mitigado: serialEnter apaga BLE con CLI USB — comportamiento normal).
- **F16c — `fav rm` substr(8)**: **CERRADO (15/08)** — auditoría externa (pack 14/08) confirmó
  que el código actual usa `substr(7)` correcto ("fav rm"=6 + espacio en 6 → id en 7,
  NavaCLIModule.cpp:752). El aviso de la 11ª parte quedó obsoleto; nunca hubo bug en el drop actual.
- **F16d — jitter quick 300-2300ms muerto** (setIntervalFromNow pisado por
  OSThread::run→setInterval; cosmético).
- **F16e — whitelist canal 1 sin `sleepmsg`** (solo lectura por canal 1; consulta por DM
  PKI — decisión del operador: dejarlo).
- **F16f — estado del banco (15/08)**: test node Promicro (a7ab507 + diags TEMP) en fuente
  de laboratorio a 869.545 (override duty cycle ON, clave observador añadida como admin);
  observador Promicro (COM9, Eclipse V1 54e0d8d) a 869.545. Pendiente: `--remove-node` de
  la entrada stale del test node en el observador (clave nueva) + ejecutar el test del
  sueño observando por COM9.

### PUBLICACIÓN GITHUB (15/08) — EA2OY/NavaTastic + MeshNavarra-Utility
- **Flujo**: rama huérfana `github-public` = UN commit del árbol saneado → push -f a `main`.
  El historial local (con claves Propia del 14/08) NO sube. Se excluye `.github/workflows`
  (CI upstream) y se desactivan Actions por API (`{"enabled":false}` en
  `PUT /repos/EA2OY/NavaTastic/actions/permissions` — sin `allowed_actions` cuando enabled=false).
- **Release v2.6**: assets por API (24 binarios + 2 PDFs = 26); las descargas del README
  apuntan a Releases, no a carpetas. `distribucion/` y `docs/pdf` se suben con `git add -f`.
- **README bilingüe ES/EN**: comandos /nava, integración con **MeshNavarra-Utility** (repo
  hermano del operador), divisor ADC, químicas, PIN BT 654321, backup y gestión de claves
  admin, estado de pruebas, licencia+disclaimer.
- **L24 — checkout de rama borra ficheros force-add**: `distribucion/` y `.github/workflows`
  se pierden del disco al alternar master ↔ huérfana (cada rama "borra" lo que la otra no
  trackea). Regla: repoblar con `distribuir.ps1 -Todo` tras cada publicación.
- **L25 — workflows upstream en un fork sin secretos = correos de error**: quitarlos de la
  rama pública y desactivar Actions.
- **L26 — token GitHub compartido por chat**: revocar tras la sesión; usar token puntual o gh CLI.

### V2.6 (15/08): ciclo sueño/despertar DEFINITIVO — verificado en banco ("EUREKA" del operador)
- **Correcciones sobre V2.4** (el diseño intermedio dormía mal): (1) [Vivo] ya NO re-duerme:
  anuncia y el nodo OPERA; el monitor runtime decide (5 lecturas reales ~100s). (2) Fix
  contador pre-cargado: `readPowerStatus(true)` (force, pre-check ×5) incrementaba
  `low_voltage_counter` → el primer tick (~20s) dormía el nodo recién arrancado con batería
  baja. Fix: el contador solo cuenta con `!force` (Power.cpp). (3) Dormir TODO por
  `doDeepSleep(portMAX_DELAY, false, true)` (preflight + RadioInterface::sleep + GPS +
  pantalla) — el `cpuDeepSleep` directo dejaba las SX1262 consumiendo ~5-10 mA dormidas.
  (4) LED apagado antes de System OFF en cpuDeepSleep (main-nrf52.cpp, tras estabilización;
  ~10 mA de LED enclavado). (5) Avisos con solo ADC + CPU (chip, reintento si BUSY; INA/I2C
  fuera — no disponible en esos momentos). Verificado: [Vivo] → 100s → [Sueño] → ~1 mA →
  LPCOMP 3.73V → [Listo] → [Boot] 2 min. **= comportamiento Eclipse V1 + avisos encima.**
- **L19** lecturas forzadas NO pre-cargan el contador de baja. **L20** dormir por
  `doDeepSleep`, `cpuDeepSleep` directo solo en el pre-check (radio nunca inicializada).
  **L21** GPIO enclavados en System OFF: apagar LED justo antes de `sd_power_system_off`.
- **SNAPSHOT V2 (15/08)**: commit local `80e9f7e14` + `_archivo\NavaTastic Eclipse Edition V2 -
  Unificado 20260815 (HEAD 80e9f7e14).zip` (5.26 MB, 2204 entradas, `git archive` del árbol
  limpio). Rollback: `git checkout 80e9f7e14`. Pendiente: 12 envs + distribuir -Todo -V2.
- **L22** el nodo NO ecoa sus propios avisos a la API local (sendToMesh sin ccToPhone):
  verificar SIEMPRE con observador externo. **L23** factory reset devuelve SFN 869.618
  (útil tras auditorías con frecuencia privada). Backups `.bak-20260815-0301/0315/0352/
  0418/0517`. Docs: cerebro 20ª parte, subnota 04 (comportamiento actual + referencia
  histórica Eclipse), transfer_context (ronda V2.6 + L19-L21), guia_integracion (bloque A
  actualizado), manuales + PDFs.

### V2.4 (15/08): rediseño [Vivo] + [Boot] diferido + temp chip
- **[Vivo] rediseñado**: el gate al despertar de sueño pasa de LPCOMP a **corte OCV**:
  V < corte−100 → silencio+re-sueño; [corte−100, corte) → [Vivo]+re-sueño (E22P 3400-3500 /
  SX1262 3300-3400); V ≥ corte → boot normal → [Listo] y opera. El LPCOMP solo decide el
  despertar FÍSICO (~3.71V), no la banda.
- **[Boot] diferido 2 min (idea del operador)**: arranques sin wasInSleep → aviso a los 2 min
  con causa RESETREAS decodificada (WDT/RESETPIN/SOFT/LOCKUP/LPCOMP/VBUS). Anti-bucle: un
  ciclo de resets nunca alcanza los 2 min → no inunda. Verificado: operador lo recibió por
  Navadmin en su nodo personal (869.618 tras factory reset que devolvió SFN por defecto).
- **Temp de chip** en los 3 avisos + [Boot] (sd_temp_get; I2C no disponible en esos momentos).
- **L16 corregida**: el eco de TX propios NO aplica a envíos del NavaCLI (sendToMesh sin
  ccToPhone) → verificar avisos SIEMPRE con observador externo, no con el emisor.
- **L18**: banco con USB → TX 1 dBm (regla operador; los picos del E22P corrompen frames).
  El factory reset devuelve la frecuencia por defecto SFNarrow 869.618 (util para volver a la
  red nacional tras auditorías con frecuencia privada).
- Backups código `.bak-20260815-0301/0315`; docs `.bak-20260815-0335`.

### F16a/f — FRENTES A y B (15/08, sesión de banco): CAUSA RAÍZ del FRENTE A encontrada; fix B aplicado
- **CIERRE (15/08, tras verificación en banco)**: retirada TODA la instrumentación TEMP F15
  (0 restos en src, verificado por grep): logs F15DBG (LR/AD/SR/XX), watchdog 1s, HB-60s canal 1,
  bootDiag, logs de escritura, `return 1000`→60000, contador Power a LOG_DEBUG. Los fixes reales
  quedan (remove-antes-de-escribir + gates + saneado + substr(9) + NODENUM_BROADCAST + F16a save).
  Build banco LIMPIO SUCCESS 66.11s (UF2 MD5 f5cb93cd6f1d23c653be8a796cf90211), sin flashear.
  Docs actualizadas (backups .bak-20260815-0137): Manual (adenda 15/08 + set_role ambas ramas +
  sección 8 + notas despliegue), transfer_context (ronda 15/08), guia_integracion (O.6),
  GUIA_AGENTE_NAVTASTIC (adenda 15/08), PORTING (inventario + trampas 16-18), cerebro 17ª parte.
  Pendiente: PDFs + flash limpio + smoke + 12 envs + distribuir -Todo -V2 + commit.
- **FRENTE A — causa raíz (código)**: los mensajes [Sueño]/[Vivo]/[Listo] y los diags TEMP por
  canal 1 usaban `enqueueResponse(0, 1, ...)` con **destino 0** en vez de `NODENUM_BROADCAST`.
  `isBroadcast(0)=false` (NodeDB.cpp:542) → el paquete se emite al aire pero NADIE lo entrega
  (ni nodos ni API local): invisible. El PONG de `/nava ping` sí llegaba (usa NODENUM_BROADCAST).
  Presente desde FASE 2 V2 (verificado en .bak-20260814-1819). Fix: 6 ocurrencias
  (NavaCLIModule.cpp 351/369/1465/1477/1488/1498) → NODENUM_BROADCAST.
- **FRENTE A — causa contribuyente (RF, operador)**: el E22P del Promicro genera picos de
  corriente al TXear; a 8 dBm los frames del test node llegaban corruptos al observador
  (110 RX / 109 bad). Bajando TX a 1 dBm (`--set lora.tx_power 1`) el enlace quedó estable
  (SNR ~12, todo decodifica). Regla de banco: test node SIEMPRE a 1 dBm.
- **FRENTE B (F16a) aplicado**: `saveToDisk(SEGMENT_NODEDATABASE)` tras acreditar/favoritear en
  AdminModule.cpp (con flag `accChanged` — solo escribe si cambia algo). El save filtrado ya
  persiste favoritos/admins/direct routers/ignored; loadFromDisk los restaura (bitfield 0x08
  incluido). Pendiente verificación en banco tras flash.
- **L13 — NODENUM_BROADCAST, nunca 0**: `to=0` no es broadcast en Meshtastic 2.7
  (`isBroadcast` solo NODENUM_BROADCAST/NODENUM_BROADCAST_NO_LORA); un paquete `to=0` se emite
  y nadie lo entrega. Los TX no-broadcast NO se ecoplean a la API local (sendLocal solo
  handleReceived para broadcasts) → el propio nodo no muestra sus TX con to inválido.
- **L14 — Picos de corriente E22P**: con fuente/USB justos, TX >1 dBm corrompe frames
  (llegaban CRC-ok pero indescifrables/109 bad). El consejo del operador (bajar potencia)
  resolvió un enlace "asimetrico" que parecía bug de código.
- **L15 — `--listen` con Start-Process y RedirectStandardOutput pierde el buffer** al matar el
  proceso (Python bufferea stdout sin TTY): usar `PYTHONUNBUFFERED=1` y stderr, o fichero+kill
  con flush. `--nodes` falla con encoding cp1252: `PYTHONIOENCODING=utf-8` +
  `[Console]::OutputEncoding=UTF8`.
- **L16 — Los TX propios NO siempre aparecen en el stream API del nodo**: solo los broadcast se
  ecoplean (Router::sendLocal → handleReceived solo si isBroadcast). Para verificar TX propios
  con to inválido hay que mirar el contador airUtilTx o un observador externo.
- **Abierto**: PKI_SEND_FAIL_PUBLIC_KEY esporádico en el test node (~uptime 243s tras boot,
  coincide con el ciclo de nodeinfos) — origen sin identificar (F17).
- **4.5 VERIFICADO EN BANCO (15/08) — ciclo sueño/despertar completo**: test node SOLO en fuente
  (usb=0): bajada a ~3.4V → 5 lecturas bajas → `F15DBG lowbat: ... bat=3375 usb=0` → **[Sueno] ...
  INA 3.40 V -51 mA DESCARGANDO** → silencio (dormido). Subida a 4V → LPCOMP (~3710) despierta →
  precheck `wasInSleep=1 gate=3710 corte=3500` → **[Listo] ... ADC 3772 mV** → HBs de vuelta.
  **FRENTE A CERRADO** ([Vivo] no salió: V saltó de 3.4 a 4V; rama [Vivo] = V en [corte, LPCOMP)).
- **Saneado de claves pre-GitHub (15/08, orden del operador)**: auditoría de fuga: encontrada la
  **K1 Propia completa** comentada en `userPrefs.jsonc` + 12 perfiles → eliminada (0 restos);
  prefijos truncados K0/K1 sustituidos por "valores no publicados" en 02/07/09/transfer_context;
  referencias a BT 123457 eliminadas (Propia: PIN propio, se pide). Master Node (privada+pública)
  se conserva por decisión del operador (General pública). `.gitignore` += `*.bak-*`.
  **Mecanismo Propia sin almacenar**: `custom_meshtastic_propia_keys` en platformio-custom.py
  (vars `NAVARICO_PROPIA_KEY_0/1` + `NAVARICO_PROPIA_BT` obligatorias, error claro si faltan),
  12 envs `R2IP_*/R1IP_*` (extends General), `build_propia.ps1` (interactivo, no guarda nada).
  Probado 3/3 (IG OK / IP sin vars = error / IP con vars = SUCCESS). **GitHub pendiente**: repo
  NUEVO con un solo commit (el historial actual contiene claves desde el 14/08).
- **VERIFICADO EN BANCO (15/08)**: flash via `pio -t upload --upload-port COM15` (nrfutil, 113.93s,
  touch 1200bps automático — NO usar UF2-por-drive: el bootloader no monta unidad, el protocolo
  del board es nrfutil). Observador COM9 recibe: bootDiag al primer tick + HB-60s cada minuto
  (canal 1, SNR 12) → camino de [Sueño]/[Vivo]/[Listo] probado punta a punta. FRENTE B: DM ping →
  PONG antes y después del reboot del test node (sin re-anuncio del observador) → síntoma resuelto;
  matiz: el save de updateUser/H3 (NodeDB.cpp:2153-2165, throttled 1 min) también persiste el
  bitfield cuando llega nodeinfo del admin; el fix F16a cubre el caso AdminMessage-sin-nodeinfo.
  Pendiente: test real de sueño (4.5) SIN USB en el test node.
- Backups código: `.bak-20260815-0100` (NavaCLIModule.cpp, AdminModule.cpp). Build banco:
  SUCCESS 62.99s (UF2 MD5 0ddd16a5a4c4bdd3153c4bdd50b360a7), pendiente de flashear.

## SESIÓN 14/08 (3ª-10ª partes) — PORTABILIDAD DEL CONOCIMIENTO (cerebro VIVO + joya + PDF + V2)

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

## RONDA AUDITORÍA EXTERNA 15/08 (Claude, fichero "auditoria claude 150826 1539.MD") — análisis y veredicto

> La auditoría trabajó sobre un pack del 14/08; contrastada contra el código VIVO (15/08).
> Resultado por hallazgo:

- **§1 [MEDIO] `navaGetLpcompWakeMv()` Seed reporta 3670 mV (propuesta del auditor: 4084)** —
  **RECHAZADO por medición de laboratorio del operador**: el Seed probado en banco (firmware 4.2,
  MISMO umbral `3_8`) despertaba a **~3,8 V** → divisor efectivo ~0,326, no 0,303 como asumía el
  auditor desde `ADC_MULTIPLIER 3.3`. La medición manda sobre la teoría. Verificado además que el
  hardware NO cambió entre 4.2 y el repo actual: `variant.h` idéntico en el bloque LPCOMP
  (`INPUT_7`, `3_8`, `ADC_CTRL BAT_READ` LOW, `ADC_MULTIPLIER 3.3`); `main-nrf52.cpp` usa el mismo
  `c.reference = 3_8` (vía `getActiveLpcompThreshold()`); única diferencia: `HYST_NOHYST` (4.2)
  → `HYST_ENABLED` 50 mV (diseño 4.3/V2, anti-rebotes) → despierta ~50 mV más tarde, nunca antes.
  El 3670 del aviso es SOLO informativo (`navaGetLpcompWakeMv` únicamente se usa en el texto de
  [Sueño], NavaCLIModule.cpp:347). **Pendiente opcional**: re-medir Seed en banco con fuente
  regulable y fijar el valor exacto del aviso (~3800) + docs.
- **§2 [BAJO] help listaba `power` en [Q] (canal 1) sin estar en la whitelist** — **FIX APLICADO
  (15/08)**: `power` movido de [Q] a [E] en el help (NavaCLIModule.cpp:649) + "SOLO DM SEGURO" en
  `helpForCommand("power")` (1600). Cosmético; la whitelist real nunca permitió `power` por canal 1.
- **§3 F16c** — confirmado: código correcto (`substr(7)`) → CERRADO (ver sección CANDIDATOS F16).
- **§4 instrumentación TEMP F15** — ya retirada el 15/08 (verificado grep: 0 restos en src).
- **§5 campo `version` nunca validado** — ya aplicado el 15/08: gates
  `fileSize != sizeof(prefs) || version != 0x4E415653` en loadResiliencePrefs/navaResiliencePeek/
  navaSetWasInSleep (NavaCLIModule.cpp:71/180/223).
- **§6 migración 80B V2.0-V2.2** — comportamiento POR DISEÑO (todo fichero de 80 bytes migra a
  defaults). Riesgo real ≈ 0: ningún nodo de campo corrió V2.0-V2.2 (la distribución -Todo -V2
  estuvo bloqueada por F14/F15 toda la sesión); solo el nodo de banco, ya reflasheado a V2.3c.
- **§8 F17 PKI_SEND_FAIL_PUBLIC_KEY** — anotada la explicación del auditor como candidato:
  comportamiento ESTÁNDAR de Meshtastic (Router.cpp:669 — la clave pública del destino aún no está
  en la NodeDB local, típico antes del primer intercambio de NodeInfo). Encaja con el síntoma
  (~uptime 243 s = ciclo de nodeinfos). NO cerrado sin evidencia; si reaparece, mirar el timing de
  aprendizaje de claves, no el cifrado.
- **Verificación de paridad/estructura**: `C:\Firmware Navarrico 4.2\Rama 1 General\Seed Studio
  Node P1 Rama 1` (referencia lab) vs repo actual — bloque LPCOMP de `variant.h` byte-idéntico;
  `main-nrf52.cpp` 4.2 usaba `c.reference = BATTERY_LPCOMP_THRESHOLD` directo; actual
  `getActiveLpcompThreshold()` → mismo `BATTERY_LPCOMP_THRESHOLD` bajo `#ifdef SEEED_SOLAR_NODE`.
- **L27 — el divisor efectivo no se deduce del ADC_MULTIPLIER**: el multiplicador es la calibración
  del ADC (incluye tolerancias); el divisor REAL del pin LPCOMP solo se conoce midiendo la tensión
  de despertar en banco. La teoría (multiplier 3.3 → divisor 0.303) falló ~300 mV contra la
  medición real (~3,8 V). Regla: dato empírico de banco > cálculo teórico.
- Backups de esta ronda: `.bak-20260815-1558` (NavaCLIModule.cpp, BITACORA, PLAN, cerebro,
  transfer_context, subnota 04).

### PUBLICACIÓN GITHUB v2.6.1 (15/08) — actualización post-auditoría
- **Push**: rama huérfana `github-public` regenerada (UN commit `1b72b99ce`, árbol saneado
  sin workflows upstream) → `push -f ...github-public:main`. Credencial usada: la del
  Administrador de credenciales de Windows (`git:https://github.com`, helper manager) —
  el token NO aparece en ningún fichero/commit; **recomendado: rotar/revocar ese token (L26)**.
- **Release v2.6.1** (id 371073616, tag v2.6.1, target main): 26 assets subidos por API
  (`uploads.github.com`) — 12 UF2 + 12 OTA (nombres históricos de distribucion\) + 2 PDFs.
  El release anterior v2.6 (id 370958405) se conserva como referencia.
- **Hallazgo de higiene (L28)**: `docs/Manual_NavaTastic.md.bak-20260814-1651` estaba
  TRACKED en master (creado antes de la regla `.gitignore *.bak-*` del 15/08) y se colaba en
  la rama pública → untracked con `git rm --cached` (el fichero en disco se conserva y ya
  queda cubierto por el .gitignore). Regla: antes de publicar, comprobar
  `git ls-tree master -r --name-only | Select-String "\.bak|_archivo"`.
- **L24 aplicada**: tras volver a master se repobló `distribucion\` (`distribuir.ps1 -Todo`)
  y se regeneraron los 2 PDFs (`generar_pdf.ps1`).

### DOCUMENTACIÓN PÚBLICA (15/08) — clave de rescate explicada + manuales bilingües ES/EN
- **README (ES/EN)**: la advertencia de "cambia la clave por defecto" se matizó con el propósito
  real: la clave admin pre-hardcodeada es la **herramienta de rescate integrada** — tras un
  restablecimiento duro (factory reset accidental, `nrf erase`, corrupción de configuración) el
  nodo vuelve con esa clave y el operador (poseedor de la privada) puede reentrar por DM,
  restaurarlo y **dejarlo de nuevo con la clave de su dueño**; por eso la auto-recuperación la
  re-inyecta si el slot 0 queda vacío. Se añadieron **instrucciones para cambiarla a mano con
  VS Code** (sin tocar C++): editar `profiles/<RAMA>_<Placa>.jsonc` → sustituir los 32 bytes de
  `USERPREFS_USE_ADMIN_KEY_0` por la clave pública propia (base64→hex) → `pio run -e <env>`; con
  el aviso de que se pierde el canal de rescate del proyecto. Versión README → V2.6.1.
- **Manuales bilingües**: traducción EN completa al FINAL de ambos manuales (el ES permanece
  arriba y es la fuente de verdad; el protocolo de rescate incluye la clave privada del Master
  Node por decisión del operador). PDFs regenerados con `generar_pdf.ps1` (98/126 KB).
- Backups norma 0.11: `.bak-20260815-1729` (README + 2 manuales). **REPUBLICADO (15/08)**:
  rama huérfana regenerada (UN commit `b49e79091`) → main; PDFs bilingües subidos al release
  v2.6.1 (borrados los antiguos). L24 aplicada: distribucion\ repoblada + PDFs regenerados.

### DESCARGO AMPLIADO + PUBLICACIÓN FINAL (15/08)
- **Descargo ampliado** en README (ES/EN) y en el manual de uso (`Manual_uso_NavaTastic_4.2.md`,
  cabecera ES + apéndice EN): las instalaciones deben cumplir la normativa que les sea de
  aplicación (nacional, autonómica, local y europea — emplazamiento, permisos de acceso y obra,
  seguridad, medio ambiente); dónde y cómo se monta el equipo (árboles, estructuras,
  propiedades ajenas) es responsabilidad exclusiva de quien lo instala; el proyecto queda
  **desvinculado** de montajes o usos de terceros que no se ajusten a la legislación vigente.
- **Publicado a GitHub**: rama huérfana regenerada (UN commit) → main; PDFs del release v2.6.1
  sustituidos por los nuevos (con el descargo). El árbol publicado incluye TODO lo documental
  (`docs\`: cerebro + subnotas + contexto + manuales). L24 aplicada de nuevo (distribucion\ +
  PDFs repoblados tras volver a master).
