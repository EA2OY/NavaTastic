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
- [ ] **F18 (candidato, espera orden)**: unificar contador de baja a
      `USERPREFS_LOW_BATTERY_READINGS_COUNT` para las 6 placas (hoy: `>4` Promicro/Faketec vs
      `>10` resto en Power.cpp; la macro SOLO la usa el pre-check de main.cpp — "medio
      huérfana"). **Decisión del operador (15/08, confirmada): 8 lecturas para AMBAS**
      (monitor runtime ≈160 s Y pre-check de arranque): perfiles a `=8` + Power.cpp leyendo
      la macro; manual/docs a actualizar ("5 lecturas" → "8"). Nota:
      `USERPREFS_LORACONFIG_TX_POWER` es config muerta (L30); SX1262 = 22 dBm SIEMPRE
      (decisión del operador, no tocar).

## Posibles ampliaciones (anotadas 15/08, esperan orden)
- [ ] **BLOQUE R — Resets remotos** (una opción de plan, DOS fases; idea del operador).
  Riesgo por pieza: F19 BAJO · F21 MEDIO (destructivo: par PKI nuevo → peers re-aprenden,
  L11) · F20 MEDIO-ALTO (modelo de seguridad + formato del struct):
  - **Fase R1 = F19 + F21 juntas** (comparten armazón: comandos `/nava`, patrón diferido con
    ACK, sin cambios de struct ni migración; un solo ciclo de build+banco):
    - `F19 /nava full_reset`: factory_reset + borrar `/resilience.bin` (semi-persistentes →
      defaults de perfil) **CONSERVANDO claves PKI** → revertir ajustes remotos sin PC.
    - `F21 /nava wipe` (purga de compromiso): erase total + **regeneración del par PKI**
      (equivalente remoto del `nrf erase`). El NodeNum se conserva SOLO (deriva de la MAC del
      hardware, NodeDB.cpp:1269 — verificado): los peers re-aprenden la pubkey nueva del mismo
      id (camino H3/updateUser). Escalera: `factory_reset` → `full_reset` → `wipe`.
  - **Fase R2 = F20 sola** (persistir claves admin en `/resilience.bin` para que sobrevivan a
    resets de fábrica — hoy se pierden, L10). Es la que toca el MODELO DE SEGURIDAD y el
    FORMATO del struct (bump de `version` → migración en todos los nodos desplegados) → va
    última, evaluada con el flujo de compromiso real, y SOLO puede existir con F21 ya hecho.
    Mitigaciones: solo claves PÚBLICAS; slot 0 SIEMPRE = clave del proyecto; versionado;
    sincronía con /prefs. Regla: F20 sin F21 NO se hace (el wipe es su botón de purga).
- [ ] **F22 — etiqueta de build en `/nava status`** (idea del operador): hoy no hay forma de
      saber por radio qué versión NavaTastic lleva un nodo (V2/V2.6 eran nombres de iteración
      no compilados). Propuesta: macro `NAVATASTIC_BUILD` (inyectada desde platformio-custom.py,
      inerte por defecto, bump manual en cada release) + mostrarla en `status`
      (`NAVA V2.7 | fw 2.7.26 (54e0d8d)`) y opcionalmente en el [Boot] junto a la causa.
      Coste: NavaCLIModule.h/.cpp + 12 envs + banco. Riesgo bajo.

## PROMPT DE RETOMA (pegar tal cual en una sesión nueva)

```
NUEVA SESIÓN — NavaTastic (repo unificado C:\NavaTastic Codigo completo) — retoma tras la
sesión del 15/08/2026 (V2.6 cerrada, GitHub publicado, snapshot FINAL creado).

PASO 0 (OBLIGATORIO, lectura en este orden ANTES de tocar nada — apertura canónica):
  0. AGENTS.md (bloque NAVARICO) → Guia_para_agente_sobre_NavaTastic.md §0 REGLAS
     OPERATIVAS: dieta de tokens, flujo en dos fases (FASE 1 plan → esperar confirmación
     → FASE 2), backup/rollback `.bak-AAAAMMDD-HHMM`, solo se escribe en este repo,
     commits locales por hito, normas 0.11 (manuales+PDF) y 0.12 (distribución V2).
  1. Guia_para_agente_sobre_NavaTastic.md COMPLETA: 12 envs navarrico_* (+12 Propia),
     perfiles, macros, scripts, mapa de cambios, seguridad de claves, y SECCIÓN 9
     (flujo de publicación a GitHub).
  2. BITACORA_TECNICA.md — F1-F15, V2.2-V2.6, lecciones L1-L26, publicación GitHub.
  3. PLAN_DE_TRABAJO.md — estado + este PROMPT.
  4. docs\cerebro\cerebro.md — SECCIÓN 5 PRIMERO (3ª-22ª partes = todo lo ocurrido) y
     después lo que haga falta (5.4 VIGENTE vs OBSOLETO, 5.5 handover).
  5. PORTING_NUEVO_FORK.md (joya de la corona) y docs de contexto según el caso
     (transfer_context.md, guia_integracion_navarrico.md, manuales, subnotas 01-12).
     El 4.3 original (C:\Firmware Navarrico 4.3) es SOLO LECTURA — sirve para diffear.

NORMAS: flujo en dos fases, solo escribir en este repo, backups por marca de tiempo,
commits locales por hito, dieta de tokens, AÑADIR (no reescribir) en cerebro/BITACORA.
BACKUPS/ROLLBACK (obligatorio en cada hito): copia `nombre.bak-AAAAMMDD-HHMM` JUNTO al
fichero tocado; binarios históricos y morralla → `_archivo\`; documentar cada cambio EN
CALIENTE en cerebro (log) + BITACORA (fallos/fixes/lecciones) + PLAN (estado), con
referencias cruzadas. Actualizar los docs de contexto cuando cambie comportamiento,
comandos o parches (Guia/transfer_context/guia_integracion/manuales+PDF si procede).

ESTADO CLAVE (15/08/2026, sesión cerrada):
- V2.6 VERIFICADA EN BANCO (Promicro E22P R2IG): ciclo sueño/despertar definitivo —
  [Vivo] (banda corte−100..corte) opera ~100s → [Sueño] (5 lecturas monitor) →
  doDeepSleep completo → ~1 mA → LPCOMP ~3.7-3.8V → [Listo] → [Boot] a los 2 min con
  causa (WDT/RESETPIN/SOFT...). Avisos con ADC + CPU (chip). LED apagado en sueño.
- 12/12 IG compilados SUCCESS y distribuidos (`distribucion\` + Desktop V2).
- GITHUB PUBLICADO: https://github.com/EA2OY/NavaTastic (rama main, UN solo commit,
  release v2.6 con 26 assets, Actions desactivadas). Historial local NUNCA se sube
  (claves Propia en commits viejos). Proyecto hermano: EA2OY/MeshNavarra-Utility.
- SNAPSHOT FINAL (rollback de referencia): `_archivo\NavaTastic Eclipse Edition V2 -
  FINAL 20260815 (HEAD 9d45c2bbf).zip` — versión final y completa (V2.6 + README +
  docs GitHub). Rollback: `git checkout` del commit o descomprimir sobre la raíz.
- MECANISMO PROPIA listo: 12 envs R2IP/R1IP + `build_propia.ps1` (claves y PIN BT se
  piden al compilar, NUNCA almacenadas). Para compilar Propia: el script, no perfiles.

TRABAJAR SOBRE EL CÓDIGO (añadir funciones, cambios, compilar):
- Cambios de comportamiento → código común src/ + docs EN CALIENTE + build del env del
  banco → verificar en banco → SOLO ENTONCES los 12 envs (lotes paralelos de envs
  DISTINTOS) → `distribuir.ps1 -Todo -V2` → manuales+PDF si procede → commit local.
- CADA BUILD ES DIFERENTE (12 envs ≠ mismo binario): placa/radio (NAVARICO_RADIO_*:
  potencia 12/22, OCV 3500/3400, LPCOMP por placa), rama (NAVARICO_RAMA_1), perfil
  (claves, canal, BT, rol), rutas libdeps (R2 relativas, R1 r1promic/r1xiaoki). El banco
  SOLO valida promicro+E22P+R2IG: anotar qué queda sin validar en las otras 11.
- NO TOCAR sin orden expresa: LPCOMP (main-nrf52.cpp), delay(3000) pre-sueño,
  delay(500)+force del pre-check, OCV/cortes, NodeDB.cpp (paridad/__LINE__), F16d/e.
- F16b BLE resumeAdvertising: pendiente anotado (BITACORA). F17 PKI_SEND_FAIL: pendiente
  con candidato — explicación del auditor (15/08): comportamiento estándar de Meshtastic
  (clave pública del destino aún no aprendida, Router.cpp:669); vigilar timing de
  aprendizaje de claves si reaparece.
- MÁS ALLÁ (si se pide): regresión Eclipse en campo, opcional progname OTA, Felix.

TRAMPAS CONOCIDAS: reboot automático 7s tras --set (esperar ≥30s); polls --info lentos;
factory reset borra debug_log_api_enabled y claves admin añadidas; FILE_O_WRITE no trunca
(remove antes de escribir); nrf erase regenera claves (limpiar peers); serialEnter apaga
baliza BLE con USB; TX 1 dBm en banco con USB (picos E22P); los avisos NO se ecoan en la
API del propio emisor (verificar con observador); al alternar rama master↔github-public
el checkout BORRA del disco los ficheros force-add (repoblar con distribuir.ps1 -Todo).
```

## Datos de referencia
- Epoch 12/08/2026 00:00 +02:00: lo calcula build.ps1 (-Paridad)
- version.properties: 2.7.26, MD5 798B967F7152F34EFCCA88C3A0FCC722 (x12 idénticos)
- .pio heredado movido a _archivo/.pio_heredado (recompilar limpio)
- APP_REPO del binario = "meshtastic/firmware" (remote del .git) -> idéntico a los builds originales
