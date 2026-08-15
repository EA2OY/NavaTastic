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
- [ ] GitHub (otra sesión): repo público solo General, docs saneadas de claves Propia
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

## PROMPT DE RETOMA (pegar tal cual en una sesión nueva)

```
NUEVA SESIÓN — NavaTastic V2 (repo unificado C:\NavaTastic Codigo completo) — retoma
tras la sesión 14-15/08 (F15 casi cerrada, 2 frentes abiertos).
PASO 0 (OBLIGATORIO, lectura en este orden ANTES de tocar nada — apertura canónica):
  0. AGENTS.md → Guia_para_agente_sobre_NavaTastic.md §0 REGLAS OPERATIVAS (dieta de
     tokens, flujo en dos fases: FASE 1 plan → esperar confirmación → FASE 2,
     backup/rollback .bak-AAAAMMDD-HHMM, solo se escribe en este repo, commits locales
     por hito, norma 0.11 manuales+PDF, norma 0.12 distribución V2).
  1. Guia_para_agente_sobre_NavaTastic.md COMPLETA (12 envs navarrico_*, perfiles,
     macros, scripts, mapa de cambios, seguridad de claves).
  2. BITACORA_TECNICA.md — F1-F15 (cierre incluido) + LECCIOnES L1-L12 + F16a-f.
  3. PLAN_DE_TRABAJO.md — F15 + FRENTES A/B + CIERRE + este PROMPT.
  4. docs\cerebro\cerebro.md — SECCIÓN 5 PRIMERO (13ª parte = sesión 14-15/08 con el
     detalle de TODO lo ocurrido) y después las secciones que hagan falta.
  5. PORTING_NUEVO_FORK.md (joya de la corona: inventario fichero a fichero + bloques
     E1/E2/S/N/P + trampas) y, si el contexto lo pide, los docs de contexto de docs\
     (transfer_context.md, guia_integracion_navarrico.md, manuales) y subnotas
     docs\cerebro\ (01-12, con sus banners ESTADO 14/08). El 4.3 original
     (C:\Firmware Navarrico 4.3) es SOLO LECTURA — sirve para diffear.
NORMAS: flujo en dos fases, solo escribir en este repo, backups por marca de tiempo,
commits locales por hito, dieta de tokens, AÑADIR (no reescribir) en cerebro/BITACORA.
BACKUPS/ROLLBACK (obligatorio en cada hito): copia `nombre.bak-AAAAMMDD-HHMM` JUNTO al
fichero tocado (código y docs, misma convención que las normas 9/0.11); binarios/UUF2
históricos y morralla → `_archivo\`; documentar cada backup y cada cambio EN CALIENTE en
cerebro (log de estado) + BITACORA (fallos/fixes/lecciones) + PLAN (estado), referencias
cruzadas entre los tres para mantener coherencia.
ESTADO CLAVE:
- Fix F15 de rol/migración VERIFICADO en banco (set_role persiste + sobrevive factory
  reset; nrf erase → ROUTER + fichero 84B). Fix en código: remove-antes-de-escribir +
  gates version/tamaño + saneado.
- FRENTE A CERRADO (15/08): causa raíz = avisos [Sueño]/[Vivo]/[Listo] encolados con
  `to=0` (no es broadcast) + RF E22P inestable a 8 dBm en banco. Fix: NODENUM_BROADCAST
  (6 sitios) + TX 1 dBm en banco. Ciclo completo VERIFICADO ([Sueño] 3375 → dormido →
  LPCOMP 3710 → [Listo] 3772).
- FRENTE B CERRADO (15/08): `saveToDisk(SEGMENT_NODEDATABASE)` tras acreditar/favoritear
  (solo si cambia). Verificado: PONG antes/después de reboot sin re-anuncio del admin.
- INSTRUMENTACIÓN TEMP F15 RETIRADA del código (0 restos, verificado). Build banco
  LIMPIO SUCCESS (UF2 MD5 f5cb93cd6f...). **PENDIENTE**: flashear build limpio en banco +
  smoke (ping/sleepmsg) → compilar 12 envs → `distribuir.ps1 -Todo -V2` → PDFs (norma
  0.11, docs ya actualizadas 17ª parte) → docs cierre → commit local.
- PENDIENTES ANOTADOS (leer BITACORA F16a-f; NO tocar sin orden expresa): F16c `fav rm`
  substr(8), F16d jitter quick muerto, F16e whitelist canal 1 sin sleepmsg, F16b BLE
  resumeAdvertising, F17 PKI_SEND_FAIL_PUBLIC_KEY esporádico (origen sin identificar).
  MÁS ALLÁ: Propia (12 perfiles+envs), GitHub (solo General), opcional progname OTA,
  regresión Eclipse en campo.
- NO TOCAR: LPCOMP (main-nrf52.cpp), delay(3000) pre-sueño, delay(500)+force del
  pre-check, OCV/cortes, NodeDB.cpp (paridad/__LINE__), F16c/d/e sin orden expresa.
- CADA BUILD ES DIFERENTE (los 12 envs NO son el mismo binario): difieren en placa/radio
  (macros NAVARICO_RADIO_E22P/SX1262 → potencia 12/22 dBm, curvas OCV 3500/3400, LPCOMP
  por placa, pin de radio), rama (NAVARICO_RAMA_1 → rol por defecto CLIENT vs ROUTER),
  perfil (claves admin, canal Navadmin, BT, rol; química SODIUM en Seed/T114; LIFEPO4
  incompatible con LPCOMP fijo de Seed/Xiao/T114) y rutas embebidas de libdeps (R2
  relativas, R1 r1promic/r1xiaoki). El test de banco SOLO valida la combinación
  promicro+E22P+R2IG: cada fix debe ser ortogonal por macros/perfil y hay que ANOTAR
  qué queda sin validar en las otras 11 combinaciones (banco/campo). No desplazar líneas
  en NodeDB.cpp (paridad __LINE__) ni tocar variant.h sin backup previo (norma 9).
- TRAMPAS CONOCIDAS: reboot automático 7s tras --set (esperar ≥30s antes de verificar);
  polls --info lentos; factory reset borra debug_log_api_enabled y claves admin añadidas;
  FILE_O_WRITE no trunca (remove antes de escribir); nrf erase regenera claves (limpiar
  entradas en peers); serialEnter apaga baliza BLE con CLI USB conectado.
```

## Datos de referencia
- Epoch 12/08/2026 00:00 +02:00: lo calcula build.ps1 (-Paridad)
- version.properties: 2.7.26, MD5 798B967F7152F34EFCCA88C3A0FCC722 (x12 idénticos)
- .pio heredado movido a _archivo/.pio_heredado (recompilar limpio)
- APP_REPO del binario = "meshtastic/firmware" (remote del .git) -> idéntico a los builds originales
