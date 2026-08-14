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
- [ ] GitHub (otra sesión): repo público solo General, docs saneadas de claves Propia
- [ ] Propia (R2IP/R1IP): perfiles + envs sin tocar código
- [ ] Opcional: progname por env para OTA zip byte-idéntico
- [ ] **⭐ JOYA DE LA CORONA (nueva sesión, prioridad alta)**: crear `PORTING_NUEVO_FORK.md`
      (o nombre similar) — guía maestra de portabilidad: (1) mapa COMPLETO de añadidos Navarrico
      vs Meshtastic prístino (Desktop\firmware como referencia), fichero por fichero, con anclas
      de búsqueda (nombres de funciones/macros/comentarios NAVARICO:) para localizarlos en un
      fork NUEVO donde las líneas difieran; (2) catálogo de las mejoras por bloque (Rama 1
      energía/resiliencia, Rama 2 Flash/infraestructura, NavaCLI /nava, canal Navadmin, H3,
      fav auto, ayuda/consultas, rol semi-permanente R1, paridad byte-a-byte) con su
      comportamiento y dependencias; (3) procedimiento paso a paso de portado: analizar fork
      nuevo -> aplicar bloques -> recompilar -> verificar paridad/regresión; (4) checklist de
      "qué debe saber el agente antes de tocar" (líneas mágicas __LINE__, PIO backslashes,
      projenv vs lib builders, UF2 mixto, marcas temporales). Material base: Guia_para_agente_sobre_NavaTastic.md
      §4 (mapa de ficheros), BITACORA_TECNICA.md (receta y fixes F1-F12), docs/ y el prístino.

## Datos de referencia
- Epoch 12/08/2026 00:00 +02:00: lo calcula build.ps1 (-Paridad)
- version.properties: 2.7.26, MD5 798B967F7152F34EFCCA88C3A0FCC722 (x12 idénticos)
- .pio heredado movido a _archivo/.pio_heredado (recompilar limpio)
- APP_REPO del binario = "meshtastic/firmware" (remote del .git) -> idéntico a los builds originales
