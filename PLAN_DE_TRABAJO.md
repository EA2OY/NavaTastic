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
- [ ] Propia (R2IP/R1IP): perfiles + envs sin tocar código
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

## Datos de referencia
- Epoch 12/08/2026 00:00 +02:00: lo calcula build.ps1 (-Paridad)
- version.properties: 2.7.26, MD5 798B967F7152F34EFCCA88C3A0FCC722 (x12 idénticos)
- .pio heredado movido a _archivo/.pio_heredado (recompilar limpio)
- APP_REPO del binario = "meshtastic/firmware" (remote del .git) -> idéntico a los builds originales
