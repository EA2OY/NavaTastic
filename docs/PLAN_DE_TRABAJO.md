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
- [x] **BLOQUE R — FASE R2 (F20) CERRADA (16/08, banco 7/7 PASS)**: claves admin PUBLICAS del
      usuario persistidas en `/resilience.bin` (campos `keySlot1/2/0Own`, marcador NAV3
      0x4E415633, struct 180 B). Regla final de slot 0 (ENMIENDA del operador): "slot 0 =
      estado previo del usuario" — `keySlot0Own` se restaura EN el slot 0; si no existe,
      queda la del proyecto. Auto-recuperación de NodeDB INTACTA (solo configs vacías).
      Sincronización merge desde AdminModule::handleSetConfig (vaciar en la app NO purga;
      purgar = keys_clear/wipe). Comandos `/nava keys_ls` + `/nava keys_clear` (ACK
      diferido, sin reboot). Fix de banco H1: full_reset conserva las claves
      (`navaFullResetKeepKeys()`). **Banco 7/7 (Faketec)**: conservación, desplazamiento de
      slot 0 sin ventana de secuestro, re-autorización, semi-persistentes a defaults,
      purga wipe/keys_clear, regresión OK. **12/12 envs SUCCESS + distribuir -Todo -V2 +
      docs + PDFs**. BLOQUE R COMPLETO (R1 + R2).
- [ ] **F22 seguimiento**: bump de `NAVATASTIC_BUILD` en cada release futuro (NavaCLIModule.h)
  + publicación GitHub (release con los 24 binarios + PDFs, flujo de la Guía §9).

## GUÍA PARA LA SESIÓN DE IMPLEMENTACIÓN (F18 / BLOQUE R / F22) — COMPLETADA 16/08 (histórica)

> La sesión de implementación terminó: F18, BLOQUE R (R1+R2) y F22 están cerrados, verificados
> y publicados (estado arriba). Esta sección queda como referencia histórica de diseño.

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
NUEVA SESIÓN — NavaTastic (repo unificado C:\NavaTastic Codigo completo, rama master).
Estado de partida: "NavaTastic Eclipse V3" (4.3.2) CERRADA, verificada en banco y PUBLICADA
a GitHub (release v4.3.2, 26 assets, rama huérfana 1 commit). RETOMA 16/08 CERRADA (sesión
documental: README mejorado, guía de compilación, agradecimientos, ronda Propia R2IP #1,
F20 aclarado — ver cerebro 36ª-38ª / BITACORA). Nada pendiente de esas sesiones. Esta
sesión solo ejecuta lo que el operador pida. ANTES de escribir código: LEER, ANALIZAR,
explicar el plan en lenguaje fácil y PEDIR PERMISO (FASE 1 → confirmación → FASE 2).

PASO 0 (OBLIGATORIO, apertura canónica, en este orden):
  0. AGENTS.md (bloque NAVARICO) → docs\Guia_para_agente_sobre_NavaTastic.md §0 REGLAS OPERATIVAS.
  1. docs\Guia_para_agente_sobre_NavaTastic.md COMPLETA (12 envs + Propia, perfiles, scripts,
     §5 flujo Propia del Desktop, §9 GitHub).
  2. docs\BITACORA_TECNICA.md: F1-F20 (H1), V2.2-V2.6, L1-L36, publicación v4.3.2, carteles,
     nomenclatura, RETOMA 16/08, Ko-fi, Tidy Up.
  3. docs\PLAN_DE_TRABAJO.md: estado (V3 + retoma + Ko-fi + Tidy Up) + "Posibles ampliaciones" + este PROMPT.
  4. docs\cerebro\cerebro.md SECCIÓN 5 (partes 24ª-40ª; 5.4 VIGENTE vs OBSOLETO, 5.5 handover).
  5. docs\PORTING_NUEVO_FORK.md + docs de contexto según el caso. C:\Firmware Navarrico 4.3 y 4.2 =
     SOLO LECTURA.

POSIBLES TRABAJOS (el operador decidirá; NO son órdenes):
- **F21 (Plan de Trabajo)**: Creación remota de canales secundarios y redirección de NavaCLI / silenciamiento de Navadmin ([docs/PLAN_CANAL_PRIVADO_Y_REDIRECCION_NAVACLI.md](docs/PLAN_CANAL_PRIVADO_Y_REDIRECCION_NAVACLI.md)).
- Banco: ciclo de resiliencia completo en Seed Solar P1 y Heltec T114 (las 2 placas pendientes).
- Candidatos anotados: F16b (BLE no reaparece tras shutdown), F16d (jitter quick muerto,
  cosmético), F16e (sleepmsg fuera de whitelist canal 1 — decisión tomada: dejarlo), F17
  (PKI_SEND_FAIL_PUBLIC_KEY esporádico ~uptime 243s; explicación candidata en BITACORA).
- Publicación en Telegram (grupo Meshtastic España, decisión del operador).
- Rotación/revocación del token GitHub del Administrador de credenciales (L26).
- Siguiente release: bump de `NAVATASTIC_BUILD` en NavaCLIModule.h + compilar 12 envs +
  distribuir -Todo -V2 + flujo de publicación de la Guía §9 (rama huérfana → push -f main →
  release por API → L24: repoblar distribucion\ y regenerar PDFs).
- Rondas Propia nuevas: el operador rellena el `PROMPT_BUILD_PROPIA.md` de
  `Desktop\Navatastic V3 Eclipse Infraestructura Propia` (claves/PIN/placas) y lo pega.

REGLAS Y TRAMPAS VIGENTES:
- Normas del proyecto: dos fases, backups `.bak-AAAAMMDD-HHMM` antes de tocar archivos
  críticos, commits locales por hito, AÑADIR en docs, documentar EN CALIENTE, PDFs tras tocar
  manuales (norma 0.11), distribución -V2 (norma 0.12).
- NO TOCAR sin orden expresa: LPCOMP (main-nrf52.cpp), delay(3000), delay(500)+force del
  pre-check, OCV/cortes, NodeDB.cpp (paridad/__LINE__), SX1262=22dBm, F16d/e.
- Trampas recientes: L34 (includegraphics con espacios → %TEMP%), L35 (git add explícito de
  CADA fichero editado + verificar con `git show HEAD:<fichero>` antes de publicar), L36
  (titlepage de LaTeX crea página en blanco), L24 (checkout entre ramas borra ficheros
  force-add → distribuir.ps1 -Todo + generar_pdf tras volver a master), no paralelizar dos
  builds del MISMO env, TX 1 dBm en banco con E22P, avisos solo verificables con observador.
- GitHub: NUNCA subir el historial local (claves Propia del 14/08) — siempre rama huérfana
  con UN commit; el token de la API en memoria (fichero temporal borrado al terminar).
```

- [x] **NOMENCLATURA DE VERSIONES (16/08, decisión del operador)**: changelog público
      reestructurado en 4.3.0 / 4.3.1 "NavaTastic Eclipse" / 4.3.2 "NavaTastic Eclipse V3"
      (actual = etiqueta compilada `NAVA V3`; iteraciones internas solo en BITACORA/cerebro).
      Manuales ES+EN, README y PDFs actualizados.
- [x] **PUBLICACIÓN GITHUB "NavaTastic Eclipse V3" (16/08)**: release **v4.3.2** con 26
      assets (12 UF2 + 12 OTA V3 + 2 PDFs con cartel HD de portada y títulos Eclipse V3);
      main = rama huérfana UN commit (sin historial local); **verificado 0 tokens** en el
      árbol público (scan del clon). README con cartel del operador. L24 aplicada tras cada
      publicación. Lecciones L34-L36 en BITACORA.
- [x] **CIERRE ECLIPSE V3 (16/08)**: snapshot `_archivo\NavaTastic Eclipse V3 - FINAL
      20260816 (HEAD 55db4d4f5).zip` + cerebro 35ª parte + handover §5.5 + PROMPT DE RETOMA
      reescrito. Estado: V3 cerrada y publicada; pendientes para la próxima sesión: banco
      Seed/T114, F16b/d/e, F17, rotación token (L26), Telegram, siguientes releases.
- [x] **DOBLE AUDITORÍA CONSOLIDADA & RESILIENCIA SOLAR COMPLETA (17/08)**:
      * **Auditoría End-to-End**: 26/26 pruebas superadas (100% PASS) en banco real con Faketec Slave (`!43ca4c27`) y Faketec Master (`!8289015a`) + Xiaomi Mi 10 (ADB WiFi `RemoteControlReceiver`).
      * **Fix Canónico de Resiliencia**: Eliminadas llamadas a `cpuDeepSleep()` directo antes del init de radio. Creado el estado **`[Critico]`** (Nivel 2: $< 3.30\text{V}$) junto a **`[Vivo]`** (Nivel 1), garantizando 160s (8 lecturas) y apagado por SPI a **0.4 mA** (o 1.5 mA con booster E22P).
      * **Validación en Fuente Regulable**: Capturado `[Critico]` a 3.24 V $\rightarrow$ 139s $\rightarrow$ `[Sueño]` a 0.4 mA $\rightarrow$ Despertar por comparador LPCOMP a **3.77 V** reales (`ADC 3771 mV`, error 0.02%) y emisión de `[Listo]`.
      * **Documentación & Certificación**: Creada `GUIA_MAESTRA_PROCEDIMIENTO_AUDITORIA_NAVATASTIC.md`, informe consolidado de auditoría, 4 PDFs compilados y `README.md` naturalizado y simétrico en ES/EN.
      * **Compilación y Publicación**: 12/12 variantes compiladas con **`fw 2.7.26.f12f833`**; 28 assets publicados en GitHub Release `v4.3.2` y rama `main` actualizada.

- [x] **IMPLEMENTACIÓN Y VERIFICACIÓN COMPLETA DEL FRENTE F21 (17/08)**:
      * **16 Comandos Implementados**: `ch_ls`, `ch_set`, `ch_del`, `ch_url`, `set_cli_chan`, `navadmin_mute`, `ch_reset`, `ch_mqtt`, `set_ok_to_mqtt`, `set_pos`, `set_beacon`, `mute`, `set_pin`, `stats`, `test_tx`, `log`.
      * **Cero Desgaste de Flash**: `stats`, `log`, `mute` y `test_tx` operan de forma 100% volátil en memoria RAM sin llamadas a LittleFS.
      * **Persistencia Atómica V4 (`NAV4`)**: Migración automática y sin pérdidas de `/resilience.bin` preservando claves de admin y roles.
      * **Compilación 12/12 SUCCESS**: Verificación completa con `build.ps1` (22m 11s) para los 12 entornos.
      * **Distribución y Manuales**: 32 binarios generados en `distribucion\` con `distribuir.ps1 -Todo` y 5 PDFs generados con `generar_pdf.ps1`.
      * **Prueba en Banco Físico Faketec Slave (`COM9`) + Master Mi 10 (`192.168.3.141:5555`)**: Flasheo DFU exitoso, enlace LoRa bidireccional probado (`rxSNR=12.25`, `rxRSSI=-29`) y respuestas de `ch_ls`, `stats` y `log` verificadas sobre el aire.

- [x] **AMPLIACIÓN F22: CONSOLA DE FLOTA EN LOTE, BLINDAJE ANTI-TORMENTAS Y GENERACIÓN NAVA V4 (17/08)**:
      * **Blindaje Anti-Tormentas en Canal Público**: En broadcast no dirigido solo responden 7 comandos ligeros (`ping`, `status`, `bat`, `power`, `env`, `channel`, `noise`) en 1 línea con jitter escalonado; `/nava help` general y comandos no permitidos se silencian para evitar saturación de la malla LoRa.
      * **Consola Privada de Gestión de Flota en Lote**: Redirigiendo la CLI a slots 2..7 (`set_cli_chan <2-7>`), el operador puede lanzar órdenes en lote a toda la red con un solo mensaje (`set_ok_to_mqtt`, `set_pos_tx`, `set_nodeinfo_tx`, `set_telem_tx`, `ign add/del/clear/ls`, `set_beacon`, `set_tz`, `set_chem`, `mute`, `test_tx`, `db_purge`, `nodeinfo`, `pos`, `sendtel`).
      * **Lista Negra Global Persistente**: `ign add/del/clear/ls` respaldado en `/resilience.bin` V5 (NAV5) y descarte inmediato de paquetes en `Router.cpp`.
      * **Control de Difusión de Posición/Telemetría**: Nuevos comandos `set_pos_tx`, `set_nodeinfo_tx`, `set_telem_tx`, `pos_clear`.
      * **Generación NAVA V4**: Versión incrementada en firmware (`NAVATASTIC_BUILD "V4"` y `NAVS_RESILIENCE_VERSION = 0x4E415635`).
      * **Compilación y Distribución 12/12 SUCCESS**: 12 entornos compilados limpiamente y 32 binarios generados en `distribucion\`.

- [x] **PUBLICACIÓN GITHUB RELEASE V4.3.3 Y ENLACES DIRECTOS VERIFICADOS (17/08)**:
      * **Release v4.3.3 Publicada**: Creada en `EA2OY/NavaTastic` (ID `371753206`) con 27 assets optimizados y nombres URL-safe.
      * **Nombres de Binarios Clarificados**: 24 firmwares etiquetados con `ROUTER_Repetidor_Fijo` y `CLIENTE_convertible_a_ROUTER`.
      * **Documentación Oficial en Release**: Subidos `Manual_NavaTastic.pdf`, `Manual_uso_NavaTastic_4.2.pdf` e `INFORME_DOBLE_AUDITORIA_NAVATASTIC_Y_MESHNAVARRA.pdf`.
      * **Saneamiento de `main` en GitHub**: Rama huérfana limpia exponiendo exclusivamente documentación pública de usuario (`README.md`, manuales e informe de auditoría), protegiendo la memoria técnica interna en `master` local.
      * **Enlaces Directos en README.md**: 57 enlaces directos a firmwares y PDFs verificados al 100% (HTTP 200 OK).

- [ ] **EJECUCIÓN DE LA AUDITORÍA ULTRA-EXHAUSTIVA V4 (65 PRUEBAS, PERSISTENCIA & SINCRONIZACIÓN UI)**:
      * **Diseño Completo**: 7 fases y 65 casos de prueba sistemáticos detallados en `docs/cerebro/12_auditoria_navatastic.md` y `implementation_plan.md`.
      * **Alcance**: Validación cruzada App Oficial Meshtastic (Android Protobuf / AdminMessages) $\leftrightarrow$ Firmware NavaTastic V4 $\leftrightarrow$ NavaCLI (`/nava`) $\leftrightarrow$ UI.
      * **Detección Previa de Bug**: Identificada la anomalía de desacoplo del switch Bluetooth en la App oficial al no sincronizarse con `prefs.ble_disabled` en `/resilience.bin`.
      * **Regla Estricta**: Cero modificaciones de código, cero commits y cero subidas a GitHub durante la ejecución de las pruebas hasta completar el diagnóstico y debatir los hallazgos con el operador.

```
================================================================================
PROMPT DE APERTURA / HANDOVER PARA NUEVA SESIÓN (AUDITORÍA ULTRA-EXHAUSTIVA V4)
================================================================================
Eres el agente responsable de continuar el proyecto NavaTastic.
Repositorio único de trabajo: C:\NavaTastic Codigo completo (rama master).

⚠️ FASE 1 OBLIGATORIA: LECTURA COMPLETA DE MEMORIA ANTES DE TOCAR NADA
Antes de ejecutar comandos o editar código, DEBES leer OBLIGATORIAMENTE en este orden:
1. docs\Guia_para_agente_sobre_NavaTastic.md (§0 REGLAS OPERATIVAS: dieta de tokens,
   flujo en dos fases, backups .bak-AAAAMMDD-HHMM, 4.3 y Desktop SOLO LECTURA).
2. docs\BITACORA_TECNICA.md (historial técnico, fixes y recetas de paridad).
3. docs\PLAN_DE_TRABAJO.md (estado actual y contexto vivo).
4. docs\cerebro\cerebro.md (índice global y notas 01 a 13).
5. docs\cerebro\12_auditoria_navatastic.md (Plan Maestro de Auditoría de 65 Casos).
6. docs\Manual_NavaTastic.md (catálogo completo de comandos /nava vigentes).
7. docs\Manual_uso_NavaTastic.md (manual de uso, montaje de hardware y resiliencia).

🎯 ESTADO ACTUAL: AUDITORÍA ULTRA-EXHAUSTIVA V4 FINALIZADA AL 100% (100% PASS) Y PUBLICADA EN GITHUB
- Matriz completa de 65 casos de prueba de docs\cerebro\12_auditoria_navatastic.md ejecutada en banco físico.
- Nodo Slave (Faketec HT-RA62 !43ca4c27) y Master (Faketec HT-RA62 !8289015a) validados en 869.545 MHz / 1 dBm.
- Sincronización cruzada bidireccional, persistencia /resilience.bin tras soft reboot, aviso [Boot] y blindajes de seguridad verificados al 100%.
- Portada README.md actualizada con Guía Rápida de 5 Pasos y enlaces a los manuales oficiales (PDFs y Markdown).
- Repositorio publicado y sincronizado en https://github.com/EA2OY/NavaTastic (rama main).
================================================================================

```

- [ ] **MEJORA FUTURA MESHNAVARRA UTILITY (RECORDATORIO OPERADOR)**:
      * Actualizar el catálogo de botones predefinidos en la interfaz táctil de la app Android MeshNavarra Utility para incluir accesos directos a los nuevos comandos de NavaTastic V4 (`set_cli_chan`, `ign`, `set_ok_to_mqtt`, `stats`, `log`, `pos_clear`, etc.), permitiendo lanzar órdenes en lote a la flota con un solo toque desde el móvil.

## Datos de referencia
- Epoch 12/08/2026 00:00 +02:00: lo calcula build.ps1 (-Paridad)
- version.properties: 2.7.26, MD5 798B967F7152F34EFCCA88C3A0FCC722 (x12 idénticos)
- .pio heredado movido a _archivo/.pio_heredado (recompilar limpio)
- APP_REPO del binario = "meshtastic/firmware" (remote del .git) -> idéntico a los builds originales
