# PLAN DE TRABAJO - Unificación NavaTastic (sesión 14/08/2026)

Estado de avance y decisiones. Cualquier sesión nueva debe leer este fichero + Guia_para_agente_sobre_NavaTastic.md.

## Objetivo
Un solo repositorio (C:\NavaTastic Codigo completo) que genere las 12 compilaciones
(6 placas x R2IG/R1IG) sin romper ninguna. GitHub y rama Propia se harán después.
## Decisiones del operador (14/08)
- Solo General (R2IG/R1IG). Propia se generará después con el mismo código (perfiles).
- Felix FUERA (build puntual, no participa).
- GitHub: solo sube lo General (claves Propia NO). Se hará más adelante.
- Distribución: NO al Desktop; solo a `distribucion\` dentro del repo, con nombres históricos.
- Comentarios `NAVARICO:` en cada línea tocada (qué hace y qué cambiar por versión).
- Repo git nuevo (historia no importa; commit inicial = estado prístino del volcado).

## Avance
- [x] Volcado verificado (Promicro R2IG General) + commit baseline e379d8ab
- [x] Morralla movida a _archivo/ (baks, Bloque*.md, BUG_*.md, Compilados, .pio heredado, meshtestic vacía)
- [x] Extracción: variant.h de 6 placas, 12 userPrefs.jsonc -> profiles/ (MD5 registrados),
      deltas R1 (NodeDB 1 línea + NavaCLI 10+1), version.properties idéntico x12,
      remotes: todos meshtastic/firmware (APP_REPO = meshtastic/firmware)
- [x] Ing. variant.h: promicro (E22P/HT-RA62), xiao kit (SX1262/E22P) fusionados con #ifdef;
      seed y t114 aplicados (bloques + inis con EXCLUDE flags)
- [x] Ing. código: Channels.cpp (TX radio), NodeDB.cpp (fallback rol a 0 líneas netas),
      NavaCLIModule.h/.cpp (set_txpower radio + 4 bloques Rama 1),
      main-nrf52.cpp (bloques LPCOMP #elif por placa + #line asserts por radio + RF95_RXEN)
- [x] platformio-custom.py: prefs/app_env/libdeps-map/APP_VERSION/BUILD_EPOCH/__TIME__/__DATE__ (inertes)
- [x] variants/nrf52840/navarrico.ini: 12 envs + platformio.ini default_envs
- [x] Scripts: build.ps1, distribuir.ps1, verificar_paridad.ps1, profiles/README.md, Guia_para_agente
- [x] **PARIDAD 12/12 BYTE-IDENTICA** (verificar_paridad.ps1 vs Desktop\NavaTastic 4.3 120826)
- [x] Distribución a distribucion/ (32 ficheros, nombres históricos, UF2 byte-idénticos)
- [x] Docs de contexto copiados a docs/ + BITACORA_TECNICA.md (receta completa para rehacer)
- [x] **CEREBRO VIVO MIGRADO AL REPO (14/08, 3ª parte)**: docs\ reestructurado a layout
      canónico `docs\cerebro\` (cerebro.md + subnotas 01-12 + 2 PROMPTs; copias 1:1 de
      4.3, MD5 15/15 verificados; subnotas 01-11 que faltaban, traídas desde 4.3).
      cerebro.md VIVO: banner ESTADO 14/08 + sección 5 (repo unificado, log sesión,
      tabla F1-F12, VIGENTE vs OBSOLETO, handover). Banners ESTADO 14/08 en subnotas
      01-12 y PROMPTs. Adendas "REPO UNIFICADO" en los 5 docs de contexto
      (transfer_context, guia_integracion, 2 manuales, GUIA_AGENTE_NAVTASTIC).
      El 4.3 queda SOLO LECTURA (archivo histórico).
- [x] **GitHub (15/08)**: https://github.com/EA2OY/NavaTastic — rama `main` con UN solo commit (árbol saneado + binarios `distribucion/` + PDFs + README bilingüe ES/EN con tabla de comandos `/nava`). **Release v2.6** con 26 assets (12 UF2 + 12 OTA + 2 PDFs). Actions desactivadas y workflows upstream fuera de la rama pública. **Proyecto hermano**: https://github.com/EA2OY/MeshNavarra-Utility (la app envía los comandos como mensajes predefinidos — enlazado desde el README). Flujo de publicación y trampas L24-L26 en BITACORA/cerebro 22ª parte.
- [x] **Propia (R2IP/R1IP, 15/08)**: 12 envs `R2IP_*/R1IP_*` (extends General) +
      `build_propia.ps1` + opción `custom_meshtastic_propia_keys` — claves y PIN BT se piden
      al compilar (variables de entorno) y **NO se almacenan** (verificado grep 0 restos).
      Backups `.bak-20260815-0200`.
- [ ] Opcional: progname por env para OTA zip byte-idéntico
- [x] **⭐ JOYA DE LA CORONA (14/08, 4ª parte)**: creado `PORTING_NUEVO_FORK.md` (raíz del
      repo) — guía maestra de portabilidad: (1) inventario COMPLETO fichero a fichero
      (69 ficheros, +6036/-114 vs prístino) con anclas de búsqueda (NAVARICO:,
      USERPREFS_*, FIX_NATIVE_CORE_RESET, funciones) y qué hace cada bloque;
      (2) catálogo de mejoras por bloque (E1 energía, E2 Flash, S seguridad, N NavaCLI,
      P paridad) con comportamiento, ficheros y dependencias (H1+H3 juntos, canal
      Navadmin+fix H3, LPCOMP por placa, externs de NavaCLI...);
      (3) procedimiento paso a paso (FASE 0 preparación → FASE 1 análisis → FASE 2
      pases → FASE 3 compilar → FASE 4 verificar); (4) checklist de trampas (F1-F12 +
      MAX_PATH + .pio heredado + no paralelizar).
- [x] **SISTEMA PDF (14/08, 5ª parte)**: `HerramientasPropiasIA\` portado de 4.3
      (generar_pdf.ps1 + plantilla_navatastic.tex, copia 1:1) → salida `docs\pdf\`
      gitignored; `$Excluir` = 4 docs de contexto (norma 11/08: solo manuales);
      verificado: 2 PDFs generados OK (Pandoc + MiKTeX).
- [x] **SNAPSHOT BASELINE (14/08, 6ª parte)**: `_archivo\NavaTastic 4.3 Eclipse Edition -
      Unificado.zip` (HEAD 644d09e68, 5,3 MB, sin binarios). Rollback de cualquier cambio
      futuro: descomprimir sobre la raíz o `git checkout 644d09e68`; binarios buenos en
      Desktop 120826 + `distribucion\`.
- [x] **NORMA 0.12 + V2 (14/08, 7ª parte)**: `Desktop\NavaTastic Eclipse Edition V2` creado;
      `distribuir.ps1 -V2` copia los builds nuevos ahí (misma estructura). Eclipse 120826
      SOLO LECTURA. FASE 2 en curso: (A) sueño/vivo/listo + sleepmsg; (B) fix fav status;
      (C) fix fragmentación.
- [x] **FASE 2 V2 COMPLETADA (14/08, 8ª parte)**: A+B+C implementados y compilados 12/12
      (16:20-16:50, 0 fallos). Distribuido 32+32 a `distribucion\` + `Desktop\NavaTastic
      Eclipse Edition V2` (F13 fix: distribuir cogía el artefacto viejo). Manual actualizado
      + PDFs. Pendiente: test en banco (ciclo solar/LPCOMP real, ATtiny, status tras reinicio).
      V2.1: rol semi-permanente extendido a Rama 2 (simetría set_role).
- [x] **V2.2 (14/08, 9ª parte)**: F14 — mensajes sueño/vivo/listo no llegaban (jitter 0.5-6.5s +
      sueño programado al encolar → radio apagada antes de emitir). Fix: sleepTime tras envío real
      +3s, jitter corto quick, force-read + delay(500) asentamiento. 12/12 compilados. Promicro
      R2IG V2.2 en Desktop V2 (MD5 20CDA06A...) para re-test del operador. Distribución -Todo -V2
      pendiente del resultado del test.
- [ ] **F15 (ABIERTA, prioridad)**: test V2.2 en banco FALLIDO — los mensajes sueño/vivo/listo
      siguen sin llegar al canal Navadmin. Investigar (ver cerebro 10ª parte): ¿el nodo llega a
      dormirse? ¿llegan ping/status por canal 1? ¿primer runOnce antes de radio lista? Serial =
      fuente de verdad. Factory reset YA hecho en el primer binario (materializar canal 1) — NO
      repetir salvo indicación.
- [x] **F15 (CIERRE PARCIAL 15/08)**: causa raíz confirmada y SUPERADA en banco: (1) bug
      parseo sleepmsg (substr(9), V2.3b); (2) fichero envenenado 84B role=0 + metadata LFS
      corrupta (1252B) + `FILE_O_WRITE` de InternalFS SIN trunc → fix final: remove-antes-
      de-escribir + gates `fileSize != sizeof || version != NAVS` + saneado de campos.
      **Verificado**: `set_role client` → persiste → SOBREVIVE factory reset → `set_role
      router` → ROUTER; `nrf erase` → ROUTER + fichero 84B limpio. CDC mudo explicado
      (logs boot pre-enumeración + debug_log_api_enabled). (Detalle: BITACORA F15 cierre +
      cerebro 12ª parte.)
- [x] **V2.6 (15/08) — CICLO SUEÑO/DESPERTAR DEFINITIVO VERIFICADO EN BANCO**: [Vivo]
      opera ~100s (5 lecturas monitor) → [Sueño] → doDeepSleep completo → **~1 mA** (LED
      apagado, radio sleep para las 6 placas) → LPCOMP ~3.73V → [Listo] → [Boot] 2 min con
      causa. Fixes: contador solo con !force, dormir por doDeepSleep, LED off pre-System OFF,
      avisos ADC+CPU. = Eclipse V1 + avisos. Docs al día (cerebro 20ª-21ª, subnota 04,
      contexto, manuales+PDFs). **Commit `80e9f7e14` + SNAPSHOT V2 en `_archivo\`**.
      **12/12 V2.6 compilados SUCCESS y distribuidos -Todo -V2 (15/08)**.
- [x] **4.5 CICLO SUEÑO/DESPERTAR VERIFICADO EN BANCO (15/08)**: test node solo en fuente (usb=0):
      ~3.4V → [Sueno] (INA -51 mA DESCARGANDO) → dormido (HBs en silencio) → subir a 4V → LPCOMP
      ~3710 despierta → precheck wasInSleep=1 → [Listo] (ADC 3772) → HBs de vuelta. **FRENTE A
      CERRADO** (rama [Vivo] = V en [corte, LPCOMP): test opcional a ~3.55V).
- [x] **FRENTE A (15/08) — CAUSA RAÍZ + FIX VERIFICADO EN BANCO**: (1) RF: TX 8 dBm del E22P
      corrompía frames → TX a 1 dBm: enlace estable. (2) Código: [Sueño]/[Vivo]/[Listo] y diags
      encolados con `to=0` en vez de NODENUM_BROADCAST → se emiten pero nadie los entrega.
      Fix: 6 ocurrencias en NavaCLIModule.cpp. **Flash vía `pio -t upload` (nrfutil)**: observador
      recibe bootDiag + HB-60s por canal 1 → camino probado punta a punta. Pendiente SOLO el test
      real de sueño (4.5, fuente SIN USB).
- [x] **FRENTE B (15/08) — fix aplicado + verificado a nivel síntoma**: saveToDisk(SEGMENT_NODEDATABASE)
      tras acreditar/favoritear admin (AdminModule.cpp, solo si cambia). Banco: DM ping → PONG
      antes y DESPUÉS de reboot del test node (observador sin re-anunciar) → síntoma resuelto.
      (Matiz: updateUser/H3 también guarda al recibir nodeinfo; el fix cubre AdminMessage sin
      nodeinfo posterior.)
- [x] **BUILD BANCO (15/08)**: `navarrico_promicro_e22p_r2ig` SUCCESS 62.99s (UF2 MD5
      0ddd16a5a4c4bdd3153c4bdd50b360a7) con fixes A+B, flasheado y verificado.
- [x] **DOCS CIERRE SESIÓN 14-15/08**: cerebro 12ª parte + BITACORA (F15 cierre, L7-L12,
      F16a-f) + PLAN (este fichero). Backups docs `.bak-20260815-0024`, código
      `.bak-20260814-2125`.
- [x] **RONDA AUDITORÍA EXTERNA 15/08 (Claude, pack 14/08) — analizada y resuelta**:
      2 hallazgos ya obsoletos en el código vivo (§4 TEMP F15 retirada, §5 gate version
      aplicado), 1 RECHAZADO por medición de laboratorio del operador (§1 Seed 4084 mV:
      el Seed despierta a ~3,8 V con el mismo `3_8` → divisor efectivo ~0,326 ≠ 0,303;
      hardware idéntico al 4.2 probado en banco), 1 APLICADO (§2 help `power` movido de
      [Q] a [E] + "SOLO DM SEGURO" en NavaCLIModule.cpp — cosmético), F16c CERRADO
      (código correcto `substr(7)`), F17 anotado con explicación candidata (PKI estándar,
      clave no aprendida), §6 migración 80B por diseño con riesgo 0. Docs actualizados
      (BITACORA ronda + L27, cerebro 24ª parte, transfer_context §8, subnota 04 nota
      empírica Seed). Backups `.bak-20260815-1558`. Pendiente opcional: re-medir Seed en
      banco (fuente regulable) para fijar el valor exacto del aviso (~3800).
- [ ] Opcional: re-medición Seed en banco → fijar `navaGetLpcompWakeMv` Seed a valor
      empírico (~3800) si difiere del actual 3670.
- [x] **GITHUB v2.6.1 (15/08)**: rama huérfana regenerada (UN commit) → `main` actualizada;
      **release v2.6.1** con 26 assets (12 UF2 + 12 OTA + 2 PDFs) por API. v2.6 anterior
      conservado. Higiene: `.bak` histórico untracked (git rm --cached; L28) para que no se
      cuele en publicaciones futuras. L24: distribucion\ repoblada + PDFs regenerados.
      Recomendado al operador: rotar/revocar el token guardado en el Administrador de
      credenciales de Windows (L26).
- [x] **DOCUMENTACIÓN PÚBLICA (15/08)**: README ES/EN — la clave admin de fábrica explicada
      como **herramienta de rescate integrada** (reentrar tras restablecimiento duro y devolver
      el nodo a la clave de su dueño) + instrucciones VS Code para cambiar la clave
      pre-hardcodeada (`profiles/*.jsonc` → `USERPREFS_USE_ADMIN_KEY_0`). Manuales **bilingües
      ES+EN** (traducción EN al final de ambos documentos; ES = fuente de verdad) y PDFs
      regenerados. Backups `.bak-20260815-1729`. **Republicado a GitHub (15/08)**: main
      actualizada (rama huérfana, UN commit) + PDFs bilingües en el release v2.6.1.
- [x] **DESCARGO AMPLIADO + PUBLICACIÓN FINAL (15/08)**: README y manual de uso (ES+EN) con
      descargo normativo ampliado (normativa nacional/autonómica/local/europea del montaje,
      responsabilidad exclusiva del instalador, proyecto desvinculado de usos de terceros
      ilegales). PDFs regenerados y publicados (main + release v2.6.1).
- [x] **PROTOCOLO DE RESCATE ACTUALIZADO (15/08, dato del operador)**: sin app 2.7.10 — la
      app actual (Play Store) cambia la clave privada; operativa: Ajustes → Seguridad → pegar
      privada → guardar/enviar → pública se regenera sola; si no se aplica, repetir (bug de la
      app). Documentado en manual de uso ES+EN y README ES+EN; PDFs regenerados.
- [x] **XIAO×2 VERIFICADAS (15/08, operador)**: Xiao Kit i2c (SX1262) y Xiao Kit i2c + E22P
      funcionan bien (ciclo + avisos). El "fallo" del Xiao+E22P era el pico del E22P en TX
      (L29). **Faketec HT-RA62 probada por el operador el 14/08** → verificada. README/estado
      de pruebas actualizado. **4 de 6 placas verificadas** (Promicro, Faketec, Xiao×2);
      pendientes: Seed y T114.
- [x] **F18 (V3, 15/08) — 8 lecturas para TODAS las placas**: 13 jsonc
      (12 perfiles + `userPrefs.jsonc`) `USERPREFS_LOW_BATTERY_READINGS_COUNT` `"5"`→`"8"` +
      Power.cpp unificado contra la macro (`>= 8`, fallback `#ifndef → 8`; eliminado el
      `#ifdef` asimétrico `>4` Promicro/Faketec vs `>10` resto). El pre-check de main.cpp
      ya leía la macro → 8 lecturas rápidas (~1,6s) sin tocar código. Monitor runtime
      ≈160s. Manuales/docs actualizados ("5 lecturas ~100s" → "8 ~160s").
- [x] **BLOQUE R fase R1 (V3, 15/08) — `/nava full_reset` + `/nava wipe`**: comandos con
      ACK diferido (patrón factory_reset): full_reset = `FSCom.remove("/resilience.bin")` +
      `factoryReset(false)` (**conserva par PKI y bonds BLE** → revert remoto sin romper la
      malla); wipe = remove resilience.bin + `factoryReset(true)` (**par PKI nuevo** +
      peers + bonds; NodeNum intacto por MAC). DM-PKI automático (fuera de la whitelist del
      canal 1). Help/manuales ES+EN con AVISO de re-aprendizaje PKI (L11). Escalera:
      `factory_reset` → `full_reset` → `wipe`. **F20 (R2) NO tocada** — pendiente, solo con
      R1 desplegado (el wipe es su botón de purga, L31).
- [x] **F22 (V3, 15/08) — etiqueta de build**: `#define NAVATASTIC_BUILD "V3"` en
      NavaCLIModule.h (bump manual por release, visible en el commit) + primera línea de
      `/nava status` (`NAVA V3 | fw <versión>` vía `optstr(APP_VERSION)`) + etiqueta en el
      aviso [Boot] (en [Sueño]/[Vivo]/[Listo] NO — decisión del operador). Verificado:
      12/12 envs SUCCESS. Pendiente: test en banco + distribuir -Todo -V2 + release GitHub
      cuando lo ordene el operador.

## Posibles ampliaciones (anotadas 15/08, esperan orden)
- [x] **BLOQUE R — FASE R1 HECHA (V3, 15/08)**: F19 `full_reset` + F21 `wipe` implementados
  (ver arriba). Riesgo por pieza: F19 BAJO · F21 MEDIO (destructivo: par PKI nuevo → peers
  re-aprenden, L11).
- [ ] **BLOQUE R — FASE R2 (F20) IMPLEMENTADA (V3, 15/08) — pendiente banco + docs**:
      claves admin PUBLICAS del usuario persistidas en `/resilience.bin` (campos
      `keySlot1/2/0Own`, marcador NAV3 0x4E415633, struct 180 B). Regla final de slot 0
      (ENMIENDA del operador): "slot 0 = estado previo del usuario" — `keySlot0Own` se
      restaura EN el slot 0; si no existe, queda la del proyecto. Auto-recuperación de
      NodeDB INTACTA (solo configs vacías). Sincronización merge desde
      AdminModule::handleSetConfig (vaciar en la app NO purga; purgar = keys_clear/wipe).
      Comandos `/nava keys_ls` + `/nava keys_clear` (ACK diferido, sin reboot).
      Build Faketec SUCCESS; pendiente: banco del operador → 12 envs → distribuir -Todo
      -V2 → pasada de docs completa (cerebro 33ª, manuales ES+EN + PDFs, README).
      Regla: F20 solo existe con F21 (wipe) desplegado — cumple (R1 hecho antes).
- [ ] **F22 seguimiento**: bump de `NAVATASTIC_BUILD` en cada release futuro (NavaCLIModule.h)
  + publicación GitHub (release con los 24 binarios + PDFs, flujo de la Guía §9).

## GUÍA PARA LA SESIÓN DE IMPLEMENTACIÓN (F18 / BLOQUE R / F22)

> **Instrucción del operador (15/08)**: estas propuestas NO son órdenes quirúrgicas ciegas.
> La sesión que las implemente tiene LIBERTAD para analizar la viabilidad ANTES de tocar
> código, debe explicar el plan en lenguaje fácil de entender y PEDIR PERMISO, y puede
> PREGUNTAR cualquier duda — el operador trasladará las preguntas (copia-pega) al agente de
> la sesión anterior (15/08), que tiene más contexto sobre el diseño.

**Pistas de implementación (para no re-derivar todo):**

- **F18 — 8 lecturas para TODAS**: hoy `Power.cpp:1020-1041` usa `#if defined(PROMICRO_DIY_TCXO)
  → contador >4` / resto `>10`; la macro `USERPREFS_LOW_BATTERY_READINGS_COUNT` YA la lee el
  pre-check de `main.cpp:544-548` pero no el monitor. Fix previsto: los 12 perfiles jsonc a
  `"USERPREFS_LOW_BATTERY_READINGS_COUNT": "8"` + Power.cpp comparando contra la macro
  (fallback 8 si no existe; ojo: el contador actual dispara con `>N` → para 8 lecturas
  exactas usar `>= 8`). Manuales/docs dicen "5 lecturas ~100s" → actualizar a 8 (~160s).
- **BLOQUE R — fase R1** (F19 `full_reset` + F21 `wipe`; F20 NO se toca):
  - Patrón de comando: como `factory_reset` existente (NavaCLIModule `executeCommand` →
    flag diferido → `runOnce` ejecuta; ACK antes de ejecutar; whitelist DM-PKI solo).
  - F19: `FSCom.remove("/resilience.bin")` + ejecutar el factory_reset existente (conserva
    claves PKI por diseño actual, L11).
  - F21: además de lo anterior, regenerar el par PKI — el mecanismo conocido: borrar/blankear
    `security.private_key` (el firmware regenera el par al arranque — AGENTS.md "Encryption at
    a glance") o la ruta de factory reset completa con `eraseBleBonds`; el NodeNum NO cambia
    (deriva de la MAC, NodeDB.cpp:1269). Verificar en banco: wipe → nodo vuelve con clave del
    proyecto (auto-recuperación) → observador re-aprende pubkey (H3/updateUser) → DM OK.
  - Escalera: `factory_reset` → `full_reset` → `wipe`. Nunca implementar F20 sin F21 previo.
- **F22 — etiqueta de build**: inyectar macro `NAVATASTIC_BUILD` desde
  `bin/platformio-custom.py` (patrón de los `NAVARICO_*` existentes: inerte si la variable no
  existe) o `#define` en NavaCLIModule.h con bump manual; mostrarla en `status`/`usageAndState`
  y opcionalmente en el [Boot].

## PROMPT DE RETOMA (pegar tal cual en una sesión nueva)

```
NUEVA SESIÓN — NavaTastic (repo unificado C:\NavaTastic Codigo completo) — sesión de
IMPLEMENTACIÓN (15/08/2026): F18 (8 lecturas de baja), BLOQUE R fase R1 (full_reset + wipe)
y/o F22 (etiqueta de build). ANTES de escribir código: LEER, ANALIZAR, explicar el plan en
lenguaje fácil y PEDIR PERMISO. Puedes PREGUNTAR lo que necesites: el operador trasladará
tus preguntas (copia-pega) al agente de la sesión anterior, que tiene más contexto.

PASO 0 (OBLIGATORIO, apertura canónica, en este orden):
  0. AGENTS.md (bloque NAVARICO) → Guia_para_agente_sobre_NavaTastic.md §0 REGLAS
     OPERATIVAS: dieta de tokens, flujo en dos fases (FASE 1 plan → esperar confirmación
     → FASE 2 ejecución), backup/rollback `.bak-AAAAMMDD-HHMM`, solo se escribe en este
     repo, commits locales por hito, normas 0.11 (manuales+PDF) y 0.12 (distribución V2).
  1. Guia_para_agente_sobre_NavaTastic.md COMPLETA: 12 envs navarrico_* (+12 Propia),
     perfiles, macros, scripts, mapa de cambios, seguridad de claves, SECCIÓN 9 (GitHub).
  2. BITACORA_TECNICA.md: F1-F17, V2.2-V2.6, L1-L31, publicación GitHub, candidatos.
  3. PLAN_DE_TRABAJO.md: estado + "Posibles ampliaciones" (F18, BLOQUE R, F22) + sección
     "GUÍA PARA LA SESIÓN DE IMPLEMENTACIÓN" (pistas de diseño por cada ítem) + este PROMPT.
  4. docs\cerebro\cerebro.md SECCIÓN 5 PRIMERO (24ª-31ª partes) y lo que necesites
     (5.4 VIGENTE vs OBSOLETO, 5.5 handover).
  5. PORTING_NUEVO_FORK.md + docs de contexto según el caso (transfer_context,
     guia_integracion, manuales, subnotas 01-12). C:\Firmware Navarrico 4.3 y 4.2 = SOLO
     LECTURA (referencias para diffear).

TRABAJO PROPUESTO (ANALÍZALO ANTES DE EJECUTAR — no es orden ciega):
- F18: 8 lecturas de batería baja para TODAS las placas (monitor runtime Y pre-check):
  perfiles ×12 a "USERPREFS_LOW_BATTERY_READINGS_COUNT": "8" + Power.cpp comparando contra
  la macro (hoy: #ifdef >4 Promicro/Faketec vs >10 resto, Power.cpp ~1020-1041; el
  pre-check de main.cpp ~544-548 ya la lee). Manuales/docs: "5 lecturas" → "8" (~160s).
- BLOQUE R fase R1: /nava full_reset (factory_reset + borrar /resilience.bin, CONSERVA
  claves PKI) y /nava wipe (erase total + regeneración del par PKI; el NodeNum NO cambia,
  deriva de la MAC — NodeDB.cpp:1269). Patrón diferido con ACK como factory_reset; DM-PKI.
  F20 (claves admin en resilience.bin) NO se toca — es fase R2, solo después de R1.
- F22: etiqueta NAVATASTIC_BUILD compilada (patrón NAVARICO_* de platformio-custom.py o
  #define con bump manual por release) + línea en /nava status + opcional en [Boot].

REGLAS ESPECIALES DE ESTA SESIÓN:
- Las NORMAS del proyecto (dos fases, backups, commits, AÑADIR en docs, documentar EN
  CALIENTE, dieta de tokens) siguen vigentes.
- Las propuestas de arriba son PUNTOS DE PARTIDA con pistas (ver la GUÍA en PLAN): si al
  analizar ves un problema o una vía mejor, DÍLO en la FASE 1. No improvises en silencio.
- FASE 1 = diagnóstico + plan técnico + método de verificación (p. ej. pio run -e <env>),
  explicado en lenguaje FÁCIL para el operador, SIN editar archivos; esperar permiso
  explícito → FASE 2 = ejecución.
- DUDAS: pregunta todo lo que haga falta; el operador trasladará las preguntas al agente
  de la sesión anterior (que diseñó estas opciones). Si algo no cuadra, NO sigas.
- NO TOCAR sin orden expresa: LPCOMP (main-nrf52.cpp), delay(3000) pre-sueño,
  delay(500)+force del pre-check, OCV/cortes, NodeDB.cpp (paridad/__LINE__), SX1262=22dBm,
  F16d/e. F16b BLE y F17 PKI: pendientes anotados, no son trabajo de esta sesión.

TRAMPAS CONOCIDAS: reboot automático 7s tras --set (esperar ≥30s); polls --info lentos;
factory reset borra debug_log_api_enabled y claves admin añadidas; FILE_O_WRITE no trunca
(remove antes de escribir); nrf erase regenera claves (limpiar peers); serialEnter apaga
baliza BLE con USB; TX bajo en banco con E22P (picos); los avisos NO se ecoan en la API del
propio emisor (verificar con observador); al alternar rama master↔github-public el checkout
BORRA del disco los ficheros force-add (repoblar con distribuir.ps1 -Todo); no paralelizar
dos builds del MISMO env.
```

## Datos de referencia
- Epoch 12/08/2026 00:00 +02:00: lo calcula build.ps1 (-Paridad)
- version.properties: 2.7.26, MD5 798B967F7152F34EFCCA88C3A0FCC722 (x12 idénticos)
- .pio heredado movido a _archivo/.pio_heredado (recompilar limpio)
- APP_REPO del binario = "meshtastic/firmware" (remote del .git) -> idéntico a los builds originales
