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

### PROTOCOLO DE RESCATE ACTUALIZADO (15/08, dato del operador)
- **Ya no hace falta la app 2.7.10**: la app ACTUAL de Meshtastic (Play Store) permite cambiar
  la clave privada del nodo. Operativa documentada (manual de uso §7 ES+EN y README ES+EN):
  **Ajustes → Seguridad** → borrar el campo **"Clave Privada"** → pegar la privada del proyecto
  → **guardar/enviar** → la clave pública correcta **se regenera sola** (sin reinicio). Si no
  se queda aplicada al guardar, repetir la operación — bug conocido de la app de Meshtastic.
  Backups `.bak-20260815-1812` (README + manual de uso).

### XIAO×2 VERIFICADAS (15/08) + HALLAZGOS DE CÓDIGO (candidatos)
- **L29 — el pico del E22P en TX aplica a TODAS las placas E22P** (confirmado por el operador
  con el Xiao+E22P; regla de banco de TX bajo ya documentada en L14/L18/README). Con SX1262 NO
  hace falta bajar potencia (decisión del operador: SX1262 SIEMPRE 22 dBm).
- **Verificado por el operador**: Xiao Kit i2c (SX1262) y Xiao Kit i2c + E22P (15/08) y
  **Faketec HT-RA62 (14/08)** — 4 de 6 placas verificadas (pendientes Seed y T114). Tabla del
  README actualizada.
- **Candidato F18 (NO tocado, espera orden)**: contador de baja ASIMÉTRICO — `>4` (~100s) solo
  con `PROMICRO_DIY_TCXO`; Seed/T114/Xiao×2 usan `>10` (~220s) (Power.cpp:1023-1029). La macro
  `USERPREFS_LOW_BATTERY_READINGS_COUNT` está "MEDIO HUÉRFANA": la usa el pre-check de arranque
  (main.cpp) pero NO el monitor runtime (Power.cpp). Fix (decisión del operador 15/08):
  **8 lecturas para TODAS** (≈160 s) — perfiles a `=8` + Power.cpp leyendo la macro; el
  pre-check también queda a 8; actualizar manuales/docs ("5 lecturas" → "8").
- **L30 — `USERPREFS_LORACONFIG_TX_POWER` de los perfiles es configuración MUERTA** (0 refs en
  src); el default real de TX vive en `Channels.cpp:107-111` (8 E22P / 22 SX1262) y es el MISMO
  recién flasheado y tras factory reset (mismo camino `resetRadioConfig`→`channels.initDefaults`
  cuando no hay /prefs). No documentar "TX distinta tras reset": es falso.
- **Nota Xiao**: `isVbusIn()` = `getBattVoltage() > 4200 mV` (sin detección VBUS real, a
  diferencia del Promicro con `powerHAL_isVBUSConnected()`) → en banco con fuente a 4,2 V (o
  batería cargada) el monitor puede resetearse (getHasUSB()=true).
- **Diff 4.3 funcional → actual (Xiao)**: Power.cpp monitor idéntico (umbral y gate); las
  diferencias son la capa V2: `handleLowBatteryEvent` (TX de [Sueño] ANTES de dormir) y el
  pre-check de 3 bandas; LPCOMP/hyst/delay(3000) idénticos. El 4.3 dormía directo por PowerFSM
  sin TX previo.
- Backups `.bak-20260815-2025` (README, cerebro, BITACORA, PLAN, subnota 04, transfer_context).

### POSIBLES AMPLIACIONES (anotadas 15/08, esperan orden del operador)
- **BLOQUE R — Resets remotos** (agrupadas 15/08 por decisión del operador; FASE R1 = F19+F21
  juntas, FASE R2 = F20 sola). **Riesgo por pieza (precisado 15/08): F19 BAJO** (factory_reset
  existente + remove de un fichero que ya se borra en cada guardado; arranque sin fichero =
  defaults verificados) · **F21 MEDIO** (el comando más destructivo: par PKI nuevo → TODOS los
  peers fallan DM hasta re-aprender, L11; mitigaciones: secuencia idempotente borrar→regenerar
  →reboot, gates de arranque autocuran, test de banco del ciclo completo con observador) ·
  **F20 MEDIO-ALTO** (modelo de seguridad + formato del struct + doble fuente de verdad).
  - **F19 — `/nava full_reset`**: factory_reset + borrar `/resilience.bin` (semi-persistentes
    → defaults), **conservando claves PKI** → revert remoto sin PC y sin romper la malla.
  - **F21 — `/nava wipe`** (purga de compromiso): erase total + **regeneración del par PKI**
    (equivalente remoto de `nrf erase`). **El NodeNum se conserva SOLO**: deriva de la MAC del
    hardware (`pickNewNodeNum`, NodeDB.cpp:1269-1291 — verificado 15/08), no vive en flash →
    un erase/wipe re-deriva el MISMO id; los peers re-aprenden la pubkey nueva del mismo id
    (camino H3/updateUser). Excepciones: colisión con peer de MAC distinta → aleatorio; cambio
    de módulo de radio/MCU → identidad nueva.
  - **F20 — claves admin en `/resilience.bin`** (FASE R2, la última): hoy el factory reset las
    borra (L10) y solo vuelve la del proyecto; persistirlas las haría sobrevivir. **Riesgo
    principal (L31)**: el factory reset pierde su función de PURGA → por eso R2 solo puede
    existir con F21 ya desplegado. Mitigaciones: solo claves PÚBLICAS; slot 0 SIEMPRE =
    clave del proyecto (no pisable); struct versionado (gates F15); resolver doble fuente de
    verdad con /prefs.
  - **Por qué en 2 fases**: F19/F21 comparten armazón (comandos + ACK diferido, sin cambios
    de struct) y son de riesgo bajo/medio; F20 toca el modelo de seguridad y el FORMATO del
    struct (bump `version` → migración en todos los nodos desplegados) → aislada, evaluada con
    el flujo de compromiso real. Escalera final: `factory_reset` → `full_reset` → `wipe`.
- **L31 — factory reset = herramienta de purga**: cualquier diseño que haga sobrevivir
  acreditaciones a un factory reset debilita su uso como "expulsar a un admin comprometido".
  Evaluar siempre esa pérdida antes de persistir claves admin fuera de /prefs.
- **F22 — etiqueta de build en `/nava status`** (idea del operador): hoy ningún comando
  `/nava` reporta la versión NavaTastic (V2/V2.6 eran nombres de iteración no compilados; la
  app solo ve el 2.7.26 genérico). Propuesta: macro `NAVATASTIC_BUILD` (inyectada desde
  platformio-custom.py, inerte por defecto, bump manual por release) + línea en `status`
  (`NAVA V2.7 | fw 2.7.26 (54e0d8d)`) + opcionalmente en el [Boot]. Coste: NavaCLIModule
  .h/.cpp + 12 envs + banco; riesgo bajo.

### CIERRE DE SESIÓN 15/08 (handover a sesión de implementación)
- PROMPT DE RETOMA de PLAN reescrito como prompt de IMPLEMENTACIÓN (F18/Bloque R/F22) con la
  instrucción del operador: analizar viabilidad antes de implementar (no es orden ciega),
  explicar el plan en lenguaje fácil, pedir permiso y preguntar dudas (canal de traslado al
  agente de la sesión anterior). Nueva sección "GUÍA PARA LA SESIÓN DE IMPLEMENTACIÓN" en
  PLAN con pistas de diseño: F18 (Power.cpp:1020-1041 `>4`/`>10`; macro ya usada en
  main.cpp:544-548; perfiles ×12 a "=8"; ojo `>N` vs `>=N`), Bloque R R1 (patrón
  factory_reset diferido; F19 = remove resilience.bin + factory reset; F21 = + regenerar par
  PKI vía blank de security.private_key o factoryReset completo; NodeNum estable por MAC),
  F22 (macro vía platformio-custom.py). Estado: 4/6 placas verificadas; GitHub v2.6.1
  publicado; commits del día en master (ver git log).

### V3 (15/08, sesión de IMPLEMENTACIÓN) — F18 + BLOQUE R fase R1 + F22
- **FASE 1 validada con el operador** (plan en lenguaje fácil + 5 preguntas trasladadas al
  agente de la sesión anterior): F22 = `#define` manual en NavaCLIModule.h (NO env-var: la
  vía platformio-custom.py tiene fallo silencioso si se olvida la variable; los NAVARICO_*
  de entorno existen solo por paridad) · etiqueta inicial **V3** · ambos comandos en help
  [E] + helpForCommand + manuales ES/EN con AVISO de re-aprendizaje PKI en wipe · etiqueta
  en [Boot] SÍ, en [Sueño]/[Vivo]/[Listo] NO · wipe = `eraseBleBonds=true` + `FSCom.remove
  ("/resilience.bin")` explícito (el factory reset no lo borra, por diseño) · ACK + 3s +
  reboot idéntico a factory_reset.
- **F18 (8 lecturas, ~160s)**: los 13 jsonc (12 perfiles + userPrefs.jsonc raíz)
  `USERPREFS_LOW_BATTERY_READINGS_COUNT` `"5"`→`"8"`. Power.cpp: eliminado el `#ifdef`
  asimétrico (`>4` PROMICRO_DIY_TCXO / `>10` resto) → comparación única
  `low_voltage_counter >= USERPREFS_LOW_BATTERY_READINGS_COUNT` con fallback
  `#ifndef → 8` a nivel de fichero (cubre envs sin perfil, p. ej. native). LOG `%d/%d`.
  El pre-check de main.cpp ya leía la macro → queda a 8 sin tocar código (8×200ms ≈ 1,6s).
- **BLOQUE R fase R1**: comandos `/nava full_reset` (ACK → diferido → `FSCom.remove
  ("/resilience.bin")` + `nodeDB->factoryReset(false)` → **conserva par PKI y bonds BLE**,
  `installDefaultConfig(preserveKey=true)`) y `/nava wipe` (ACK → diferido → remove
  resilience.bin + `nodeDB->factoryReset(true)` → **par PKI nuevo** (private_key.size=0 →
  regen al boot) + peers borrados (`installDefaultNodeDatabase`) + bonds BLE). Flags
  `fullResetPending`/`wipePending` en NavaCLIModule.h; ejecución en el bloque diferido de
  runOnce junto a factoryResetPending. **DM-PKI automático**: fuera de la whitelist del
  canal 1 → `ERR: SOLO DM SEGURO` por canal. NodeNum intacto (deriva de la MAC). Escalera:
  `factory_reset` → `full_reset` → `wipe`. F20 (claves admin en resilience.bin) NO tocada
  (fase R2).
- **F22**: `#define NAVATASTIC_BUILD "V3"` en NavaCLIModule.h (bump manual por release,
  visible en el commit) + primera línea de `/nava status` (`NAVA V3 | fw 2.7.26.31cd4cc` vía
  `optstr(APP_VERSION)`) + etiqueta en el [Boot].
- **L32 — dos errores de compilación al primer intento, ambos de la sesión** (corregidos):
  (1) usé `APP_VERSION` directamente en `snprintf` — es un **token crudo** (`-DAPP_VERSION=
  2.7.26.54e0d8d` sin comillas): `error: too many decimal points in number`. El código
  upstream SIEMPRE lo consume con `optstr(APP_VERSION)`/`xstr()` (stringify). (2) typo
  `navarricoResetReasonName` → la definición es `navaricoResetReasonName` (una r; NavaCLIModule
  .cpp:299). Detección: build del env banco antes de la ronda completa.
- **Verificación**: `navarrico_promicro_e22p_r2ig` SUCCESS (banco) → grep (13/13 jsonc a 8,
  0 restos `>5`, 0 restos `low_voltage_counter >`) → **12/12 envs SUCCESS** (lotes paralelos
  de envs DISTINTOS). Pendiente: test en banco (full_reset conserva clave → DM sigue OK sin
  re-aprender; wipe → peers fallan PKI hasta `--remove-node` + NodeInfo nuevo; F18 → ~160s
  operando antes de [Sueño]) + distribuir -Todo -V2 + PDFs + commit.
- Backups: `.bak-20260815-2109` (19 ficheros: Power.cpp, NavaCLIModule.h/.cpp, 13 jsonc, 2
  manuales).

### F20 (15/08, FASE R2) — claves admin persistidas en /resilience.bin (IMPLEMENTADO, pendiente banco+docs)
- **Diseño final (validado por operador + sesión anterior, con ENMIENDA de la regla de slot 0)**:
  - Struct `ResiliencePrefs` += `keySlot1[32]`, `keySlot2[32]`, `keySlot0Own[32]` (84→180 B);
    marcador `version` bump a `NAVS_RESILIENCE_VERSION 0x4E415633` ("NAV3"); los 8 literales
    `0x4E415653` sustituidos (queda 1 solo en comentario explicativo).
  - **Regla final de slot 0**: "slot 0 = estado previo del usuario" — `keySlot0Own` (clave propia
    que el dueño puso en slot 0 desautorizando la de fábrica) se restaura EN el slot 0 desplazando
    a la del proyecto; si no existe, slot 0 queda con la del proyecto (como estaba). La
    auto-recuperación de NodeDB (local_sum==0) sigue cubriendo solo configs vacías (wipe/nrf
    erase) — INTACTA, NodeDB.cpp no se toca.
  - **Migración legacy (84 B "NAVS")**: campos legacy preservados, campos de claves a cero +
    **adopción** (copia de las claves de usuario ya presentes en el config, con dedupe contra
    las claves del proyecto vía macros USERPREFS_USE_ADMIN_KEY_0/1/2 — General solo define K0).
    La adopción también corre en el fichero inexistente (tras wipe no adopta nada: config solo
    proyecto). No-op en boot normal.
  - **Restauración**: primer tick de runOnce (`applyPersistedAdminKeys`, DESPUÉS de
    NodeDB::init): slot 0 ← keySlot0Own; slots 1-2 ← solo si vacíos; recomputa
    `admin_key_count`; `saveToDisk(SEGMENT_CONFIG)` SOLO si cambió.
  - **Sincronización**: gancho en `AdminModule::handleSetConfig` caso security (tras aplicar
    config, `if (navaCLIModule)`): merge — slot entrante NO vacío se persiste; vacío NUNCA
    borra lo persistido; slot 0 entrante == clave del proyecto → limpia keySlot0Own
    (re-autorización del rescate). Purga real: `keys_clear`/wipe/nrf erase.
  - **Comandos nuevos (DM-PKI, fuera de whitelist canal 1)**: `/nava keys_ls` (persistidas en
    base64) y `/nava keys_clear` (ACK diferido → runOnce con cola vacía tras 3 s → cero SOLO
    los 3 campos → sin reboot; patrón ANEXO). Help [E] + helpForCommand actualizados.
  - **Edge conocido (documentado)**: si el primer boot tras flashear V3 duerme por batería
    baja en el pre-check, la migración la hace `navaSetWasInSleep` (config aún no cargada) →
    adopción diferida al siguiente sync de seguridad. Sin pérdida de claves.
  - **Verificación**: `navarrico_faketec_sx1262_r2ig` SUCCESS (208 s). UF2:
    `.pio\build\navarrico_faketec_sx1262_r2ig\firmware-navarrico_faketec_sx1262_r2ig-2.7.26.e1c3179.uf2`
    (MD5 FED443632EFD127EA4CEE6752009ADE7). Pendiente: banco (operador) → 12 envs →
    distribuir -Todo -V2 → pasada de docs completa (cerebro 33ª, BITACORA, PLAN,
    transfer_context, guia_integracion, subnotas 02/03/05, manuales ES+EN + PDFs, README).
  - Backups: `.bak-20260815-2301` (NavaCLIModule.h/.cpp, AdminModule.cpp, 2 manuales).

### F20 H1 (15/08, hallazgo de banco) — full_reset borraba las claves persistidas (FIX)
- **Síntoma (banco, operador)**: prueba 2a FALLIDA — tras `/nava full_reset`, `keys_ls`
  VACÍA y `admin_ls` solo con la clave del proyecto. Causa: el full_reset de R1 ejecutaba
  `FSCom.remove("/resilience.bin")` ANTES de `factoryReset(false)` → con F20 ese fichero
  es donde viven las claves admin persistidas. `factory_reset` (que NO borra el fichero)
  sí las conservaba (2b OK).
- **Fix**: la ejecución diferida de full_reset ya NO borra el fichero: llama a
  `navaFullResetKeepKeys()` (NavaCLIModule.cpp): conserva SOLO los 3 campos de claves
  (`keySlot1/2/0Own`) y resetea el resto a defaults de perfil (química/vbat/vwake del
  perfil, rol=0xFF, sleepMsgs=1, auto_fav=1, autoFavIds a cero, wasInSleep=0, tx/ble a
  defaults, version=NAV3), con la escritura L7 (remove+write vía saveResiliencePrefs).
  Después `factoryReset(false)` + reboot, como antes. wipe y keys_clear SIN cambios
  (siguen purgando).
- **Re-verificación banco pedida**: repetir 2a con este build (keys_ls conserva las 3
  claves tras full_reset) + verificar que full_reset SIGUE reseteando semi-persistentes
  (p. ej. `set_role client` antes → tras full_reset vuelve al rol del perfil).
- Backups: `.bak-20260815-2335`. Build Faketec SUCCESS (UF2 MD5 79842458471A82629C964ECDCDAE21CE).

### F20 CIERRE (16/08) — BANCO 7/7 PASS + pasada completa
- **Banco (operador, Faketec con el fix de full_reset): 7/7 PASS**: (2a) full_reset conserva las
  claves del usuario (S0=propia desplaza a la del proyecto, S1/S2 correctas), DM OK sin
  re-acreditar, Master Node → NO AUTORIZADO (desautorización persistente, sin ventana de
  secuestro) · semi-persistentes resetean (rol client→ROUTER, sleepmsg off→ON) · re-autorización
  (slot 0=proyecto → S0 se limpia → tras reset slot 0=proyecto) · factory_reset conserva claves
  (re-aprendizaje PKI esperado, L11) · wipe purga total (keys_ls vacío, solo proyecto, DM falla
  hasta re-aprender) · keys_clear vacía la persistencia sin tocar config/química/rol (tras
  full_reset las claves NO vuelven) · regresión (status NAVA V3, help con keys_ls/keys_clear,
  canal 1 → ERR SOLO DM SEGURO).
- **L33 — los comandos que reescriben /resilience.bin deben decidir QUÉ campos purgan**: el
  propósito del comando define la política de campos — full_reset = defaults de perfil
  conservando claves admin (F20); keys_clear = cero SOLO los 3 campos de claves (conserva
  química/vbat/vwake/rol); wipe = borrar el fichero entero (purga total). Una implementación
  que borre el fichero entero "para resetear los semi-persistentes" rompe por construcción
  cualquier campo que un feature posterior decida persistir ahí (le pasó a full_reset con F20).
- **12/12 envs SUCCESS** (lotes de envs distintos) + `distribuir.ps1 -Todo -V2` (32+32 ficheros)
  + docs completas (cerebro 33ª, BITACORA, PLAN, transfer_context, guia_integracion, subnotas
  02/03/05, manuales ES+EN con keys_ls/keys_clear/regla de claves/merge, README ES+EN, Guia) +
  PDFs regenerados. Backups docs `.bak-20260816-0100`.

### PUBLICACIÓN GITHUB — "NavaTastic Eclipse V3" v4.3.2 (16/08, con assets actualizados)
- **Push**: rama huérfana `github-public` regenerada (UN commit `aca051dd4`, árbol saneado sin
  `.github/workflows` upstream) → `push -f ...github-public:main` (main actualizada).
- **Release v4.3.2** (id 371184462, tag v4.3.2, target main, name "NavaTastic Eclipse V3
  (4.3.2)"): **26 assets subidos por API** (`uploads.github.com`) — 12 UF2 + 12 OTA
  (nombres históricos de `distribucion\`; NIMH = mismos binarios que LIPO) + 2 PDFs
  (bilingües, regenerados 16/08). Releases v2.6/v2.6.1 anteriores conservados.
- **Body del release**: novedades ES/EN (etiqueta NAVA V3, avisos sueño/despertar, 8
  lecturas, full_reset/wipe, claves admin persistidas banco 7/7, tip `/nava help`).
- **Credencial**: la del Administrador de credenciales de Windows (usuario EA2OY); el token
  se usó en memoria y se eliminó el fichero temporal. **Recomendado rotar/revocar (L26)**.
- **L24 aplicada**: tras volver a master se repobló `distribucion\` (`distribuir.ps1 -Todo
  -V2`) y se regeneraron los 2 PDFs. README actualizado ("Última versión" = Eclipse V3) en
  master y en la rama pública.
- **README re-publicado (16/08, 2ª ronda)**: tras la auditoría del operador se matizó la
  semántica de rescate con F20 (la clave de rescate se inyecta automáticamente SOLO sin estado
  previo: fallo completo/wipe/nrf erase o nodo que nunca desautorizó; desautorización en slot 0
  persistente entre resets; quitar clave en la app NO purga → keys_clear/wipe) + tabla de
  comandos (full_reset conserva claves admin, wipe borra las persistidas) + estado de pruebas
  (Faketec 7/7). Rama huérfana regenerada (UN commit `13cf0b8cd`) → push -f a main. L24
  aplicada de nuevo (distribucion\ + PDFs repoblados).

### CARTEL DEL OPERADOR (16/08) — README + portadas de los PDFs
- **Flyer "NavaTastic Eclipse V3"** (JPEG del operador, 1792×2398): el README lo muestra como
  **primera imagen** (versión optimizada 1200px, 380 KB) — retirados el escudo
  (`escudo_navatastic.png`, recuperable en git) y el titular `# NavaTastic` (el cartel ya lleva
  la marca).
- **Portada de los PDFs**: la plantilla LaTeX (`plantilla_navatastic.tex`) añade
  `\makeflyerpage` — el cartel HD (2,7 MB, en el repo como `flyer_navatastic_eclipse_v3_hd.jpg`)
  ocupa la **primera página a página completa** (`height=0.92\paperheight, keepaspectratio`,
  `\thispagestyle{empty}`) antes de la portada estilizada. `generar_pdf.ps1` copia el HD a
  `%TEMP%` (ruta sin espacios) y lo pasa con `-V flyer=...`.
- **Trampa resuelta (L34)**: `\includegraphics` con ruta que contiene espacios escapados (`\ `)
  da `Missing endcsname inserted` y, sin `-halt-on-error`, xelatex en nonstopmode puede "OK"
  SIN imagen (PDF sin portada) — usar SIEMPRE copia en `%TEMP%` sin espacios.
- **YAML de título**: `Manual_NavaTastic.md` no tenía metadatos YAML → la plantilla solo dibuja
  portada con `$if(title)$`; añadido bloque title/subtitle/author/date/toc.
- Cabecera de página y portada de la plantilla: "NavaTastic 4.3" → "NavaTastic Eclipse V3".
- **Publicado**: rama huérfana regenerada (UN commit `7f327b647`) → main; los 2 PDFs del
  release v4.3.2 sustituidos por los nuevos (2,4 MB con cartel; vía API: borrado + subida).
  L24 aplicada (distribucion\ + PDFs repoblados). Backups `.bak-20260816-0159` (plantilla +
  generar_pdf) y `.bak-20260816-0212` (manual de comandos).
- **Carteles sustituidos (16/08, 3ª ronda)**: el operador entregó versiones nuevas —
  `cartel_navatastic_github.jpg` (1024×572, README de GitHub; retirado el flyer optimizado
  anterior) y el flyer de portada de los PDFs actualizado (1792×2398, sobre
  `flyer_navatastic_eclipse_v3_hd.jpg`). PDFs regenerados y sustituidos en el release v4.3.2
  (vía API). Rama huérfana `6a44fd022` → main. L24 aplicada de nuevo.
- **FIX README roto (16/08, L35)**: la edición del README (cartel) quedó SIN STAGEAR — el
  commit llevaba el `git rm` del flyer viejo pero no el cambio de README; el `git checkout
  master` posterior restauró la referencia antigua → imagen 404 en GitHub. Regla (L35): tras
  editar ficheros, `git add` explícito de CADA fichero editado y verificar con
  `git show HEAD:<fichero>` antes de publicar. Fix: README → `cartel_navatastic_github.jpg`,
  main `418768227`.
- **FIX portada PDF (16/08, L36)**: el entorno `titlepage` de la portada-cartel emitía un
  `\newpage` inicial → primera página EN BLANCO y el cartel en la 2ª. Fix: `\makeflyerpage` sin
  `titlepage` (`\thispagestyle{empty}` + `\vfill` + `center` + `\includegraphics[width=\textwidth,
  height=\textheight, keepaspectratio]` + `\vfill` + `\clearpage`) → cartel en la página 1,
  ajustado al área de texto (margen de geometry) y centrado en ambos ejes. Verificado con
  pypdf: pag1=imagen sin texto, pag2=portada, pag3=índice, sin páginas en blanco. PDFs del
  release v4.3.2 sustituidos; rama huérfana `71b7eee31` → main; L24 aplicada.

### CIERRE "NAVASTASTIC ECLIPSE V3" (16/08) — build/commit cerrado, sesión lista para retoma
- **Títulos de los manuales** → "NavaTastic Eclipse V3" (portada, H1 ES/EN, adenda de
  versión en Manual_uso; antes 4.2/4.2.1/4.3). PDFs regenerados y sustituidos en el release
  v4.3.2 (verificado con pypdf: pag1=cartel sin texto, pag2=portada Eclipse V3, pag3=índice,
  sin páginas en blanco).
- **Verificación de seguridad del repo público**: clon de main escaneado — **0 tokens**
  (ghp_/github_pat_/gho_/ghs_/AWS), main = UN commit huérfano (el historial local con claves
  Propia del 14/08 nunca sube), `build_propia.ps1` solo referencia variables de entorno.
- **Docs de cierre**: cerebro 35ª parte + handover §5.5 actualizado (estado V3 cerrada y
  publicada; siguientes pasos), PLAN (cierre + PROMPT DE RETOMA reescrito para retoma; la
  GUÍA DE IMPLEMENTACIÓN queda histórica), transfer_context ronda de cierre.
- **Snapshot**: `_archivo\NavaTastic Eclipse V3 - FINAL 20260816 (HEAD 55db4d4f5).zip`
  (git archive del árbol limpio: fuentes + config + perfiles + scripts + docs/cerebro +
  carteles, SIN .pio/.git/binarios/distribucion/PDFs/baks — mismo criterio de los
  snapshots anteriores).
- **Pendientes para la próxima sesión** (ver PLAN/cerebro §5.5): banco Seed y T114, F16b/d/e,
  F17, rotación del token GitHub (L26), Telegram (decisión del operador), próximos releases
  (bump `NAVATASTIC_BUILD`). Backups del día: `.bak-20260816-0100/0128/0159/0212/0227/0230/0259`.

### NOMENCLATURA DE VERSIONES (16/08, decisión del operador)- **Changelog público (manual de uso) = 3 hitos**: **4.3.0** (NavaTastic + control remoto sin
  PC), **4.3.1 = "NavaTastic Eclipse"** (12/08, distribuida a colegas), **4.3.2 = "NavaTastic
  Eclipse V3"** (actual). Mapeo oficial: **"NavaTastic Eclipse V3" = 4.3.2 = etiqueta
  compilada `NAVA V3`** — no recompilar por esto. La carpeta destino del Desktop
  ("NavaTastic Eclipse Edition V2", norma 0.12) conserva su nombre histórico y recibe los
  builds de Eclipse V3.
- **Iteraciones internas** (V2.2/V2.3/V2.4/V2.6, F18, Bloque R, F20, F22): SOLO historial
  técnico de esta bitácora/cerebro; nunca en manuales ni en la etiqueta.
- Docs actualizadas ES+EN (manual de uso changelog, manual de comandos adenda 16/08 con el
  tip `/nava help` por nodo, README) + PDFs. Backups `.bak-20260816-0128`.

### RETOMA 16/08 — README EN: mojibake corregido + republicación (publicación post-V3)
- **Síntoma**: el README público (release v4.3.2, rama huérfana) mostraba 10 caracteres rotos
  "�" (U+FFFD, bytes EF BF BD en el fichero) en la sección EN: guiones largos (9×) y el "×"
  de "6 boards/radios × 2 branches" (1×) — mal grabados en la traducción EN del 15/08.
- **Fix**: `README.md` con backup `.bak-20260816-0335`; los 10 U+FFFD sustituidos por
  U+2014 (—) y U+00D7 (×) según contexto (verificado por códigos de carácter: 0 restos de
  U+FFFD, 44 em-dash + 6 × totales en el fichero, coherente con el original ES).
- **Nota de diseño (decisión del operador)**: la clave privada del Master Node expuesta en
  los PDFs públicos NO es un fallo — es la "última bala" de rescate, con instrucciones
  claras de desautorización/cambio al recuperar el nodo; si lo critican en el grupo de
  Telegram, la respuesta es esa.
- **Publicado**: rama huérfana `github-public` regenerada (UN commit) → push -f a main
  (credencial del Administrador de credenciales de Windows, en memoria, sin ficheros de
  token); L24 aplicada (distribucion\ repoblada + PDFs regenerados). Backups
  `.bak-20260816-0335` (README + BITACORA + cerebro).
- **Descargo del README (decisión del operador)**: reescrito el punto de "la malla de este
  proyecto" — el proyecto NO tiene ni opera ninguna malla; los nodos que corran este
  firmware no se asocian al proyecto ni a su autor (aunque los haya montado él); la
  configuración por defecto debe revisarse antes de desplegar. ES+EN (README líneas
  279-282/552-556), backup `.bak-20260816-0340`. Republicado con la misma rama huérfana;
  L24 aplicada.
- **Guía de compilación pública (decisión del operador)**: creado `docs/Compilar_NavaTastic.md`
  (ES+EN) — adaptación del texto de compilación de la doc oficial de Meshtastic, desechando
  lo que no aplica (Adding Custom Hardware / Hardware Model Acceptance Policy) y absorbiendo
  lo que estaba en el README: requisitos (Git, PlatformIO, aviso MAX_PATH de Windows),
  clonado + `git submodule update --init` (protobufs), actualización, compilación (12 envs
  VS Code/CLI), flasheo (enlace), Propia (build_propia.ps1 + NAVARICO_PROPIA_*), ajustes de
  hardware, enlaces. README ES+EN: secciones "Compilar"/"Building" y "Rama propia"/
  "Private branch" sustituidas por una sección corta con enlace a la guía (README más
  limpio). Manuales intactos. Guia_para_agente: línea de enlace en §2 (sin duplicar).
  Backups `.bak-20260816-0345/0346`.
