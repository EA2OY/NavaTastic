# INSTRUCCIONES DE AUDITORÍA — NAVATASTIC (repo unificado, para Claude/agentes)

> **LEER ANTES DE CUALQUIER COSA.** El operador exige una auditoría **QUIRÚRGICA**: no ahorres
> tokens no leyendo, no des nada por hecho, no asumas uniformidad entre placas.
> **ESTADO 15/08/2026 — REPO UNIFICADO**: el paquete a auditar es el árbol ACTUAL del repo
> único (`C:\NavaTastic Codigo completo`): `src/`, `variants/`, `profiles/` (General), `bin/`,
> scripts y docs. El `codigo_diff/` de la versión 4.3 ya no existe; el código real es `src/`.
> Las rutas viejas de `C:\Firmware Navarrico 4.3\...` citadas en los docs históricos son
> SOLO LECTURA (referencia, no auditar allí).

---

## 0. Orden de lectura obligatorio

1. `AGENTS.md` (bloque NAVARICO + normas upstream)
2. `Guia_para_agente_sobre_NavaTastic.md` §0 REGLAS OPERATIVAS
3. `docs\cerebro\cerebro.md` **sección 5** (repo unificado, log 3ª-22ª partes)
4. `BITACORA_TECNICA.md` (fallos/fixes F1-F15, V2.x, lecciones L1-L26) + `PLAN_DE_TRABAJO.md`
5. `PORTING_NUEVO_FORK.md` (joya de la corona: inventario fichero a fichero + bloques)
6. `docs\transfer_context.md`, `docs\guia_integracion_navarrico.md`, `docs\Manual_NavaTastic.md`
   y subnotas `docs\cerebro\01-12` (con sus banners ESTADO 14/08 — qué es histórico)

## 0bis. MATERIAL PERSONAL — FUERA DE LA AUDITORÍA

La carpeta local puede contener material personal del operador que NO forma parte del
proyecto a auditar. **Descartalo por completo y NO lo audites, NO lo inspecciones, NO lo
menciones en tu informe y NO lo reproduzcas en ningún sitio:**

- El historial git local (`.git`): contiene **claves privadas de la rama "Propia"** (red
  personal del operador) en commits antiguos. No hagas `git log`, no revises commits
  antiguos, no diffs históricos. Audita SOLO el árbol de trabajo actual.
- Los ficheros de backup `*.bak-*` (norma interna de respaldo por marca de tiempo).
- La carpeta `_archivo/` (material histórico archivado).
- Cualquier otro fichero con claves privadas, PINs o datos personales que encuentres de
  forma incidental: ignóralo y avisa al operador de que existe, **sin copiarlo**.

El objeto de la auditoría es únicamente el CÓDIGO ACTUAL y su DOCUMENTACIÓN. Las claves
públicas de la configuración General y las del Master Node (incluida su **privada**, por
decisión expresa del operador) están aceptadas y NO son hallazgos de seguridad. El canal
Navadmin va **sin cifrar por diseño** (PSK pública, solo lectura) — tampoco es hallazgo.

## 1. Modo de operación obligatorio

1. **LEE TODO, en orden.** No saltes ficheros "porque ya te suenan".
2. **NO ASIMAS NADA UNIFORME ENTRE PLACAS.** ADC, divisor de batería, LPCOMP, GPIOs, radio y
   potencias difieren por variante. La selección es declarativa (env `navarrico_*` + perfil
   `profiles/*.jsonc` + macros `NAVARICO_RADIO_*`/`NAVARICO_RAMA_1`) — nunca se edita código
   para cambiar de versión.
3. **Verifica contra el código real.** Si un documento dice X y `src/` dice Y, **el código es
   la fuente de verdad** y el documento debe corregirse.
4. **Solo lectura salvo orden expresa**: flujo en dos fases (plan → esperar OK → ejecutar).
   No recompilar ni tocar LPCOMP/delay(3000)/NodeDB.cpp sin orden (ver Guia §0).
5. **Sé honesto**: si no hay evidencia para clasificar algo, di "unknown" y lista qué lo
   desambiguaría. No rellenes con especulación.

## 2. Puntos críticos a auditar con lupa

- **`getActiveLpcompThreshold()`** (`src/platform/nrf52/main-nrf52.cpp`): `#ifdef` por placa
  (SEEED/XIAO/T114 → umbral fijo de fábrica) y `switch` dinámico SOLO Promicro/Faketec
  (divisor 0.5). Divisores reales: Promicro/Faketec 0.5 · Xiao 0.3377 · Seed ≈0.303 · T114 0.204.
- **Ciclo sueño/despertar V2.6** (`src/main.cpp` pre-check + `src/Power.cpp` contador +
  `src/modules/NavaCLIModule.cpp` avisos): bandas [corte−100, corte), contador SOLO con
  `!force`, dormir por `doDeepSleep`, LED off antes de System OFF. Verificado en banco.
- **`updateUser`** (NodeDB.cpp): fix rotación de clave admin + acreditación persistente
  (AdminModule `saveToDisk(SEGMENT_NODEDATABASE)`).
- **Claves admin**: solo en perfiles/`userPrefs.jsonc` (macros), nunca literales en src.
  Propia: claves por variables de entorno (`NAVARICO_PROPIA_*`), nunca almacenadas.
- **Seguridad NavaCLI**: whitelist canal 1, rate-limit 30s, DM PKI obligatorio, guards
  `substr()`, rate-limit de no-admins.
- **Protección Flash**: guardados filtrados, auto-favoritos, desalojo híbrido,
  `TransmitHistory` sin escritura.
- **T114**: LPCOMP activo en el fork (Meshtastic lo desactiva por fuga 2.9mA, issue #8801) —
  decisión de diseño del operador, no "corregirlo".

## 3. Candidatos YA ANOTADOS (no son novedad si los encuentras)

F16c `fav rm` substr(8) · F16d jitter quick muerto (cosmético) · F16e whitelist canal 1 sin
`sleepmsg` (decisión del operador) · F16b BLE no reaparece tras shutdown() · F17
PKI_SEND_FAIL_PUBLIC_KEY esporádico (origen sin identificar). Detalle: BITACORA.

## 4. Entrega esperada

- Lista de hallazgos ordenada por severidad, cada uno con: archivo:línea, evidencia, impacto
  y fix propuesto.
- NO toques código sin pedir confirmación.
- Si todo está correcto, dilo explícitamente y justifica brevemente por qué.

---

*Original 2026-08-11 (era 4.3). Actualizado 15/08/2026 al repo unificado (V2.6, GitHub
EA2OY/NavaTastic) con el material personal excluido por orden del operador.*
