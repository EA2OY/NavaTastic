# INSTRUCCIONES DE AUDITORÍA — FIRMWARE NAVARRICO 4.3 (para Claude/agentes)

> **LEER ANTES DE CUALQUIER COSA.** Este paquete contiene la memoria técnica canónica del proyecto. El operador exige una auditoría **QUIRÚRGICA**: no ahorres tokens no leyendo, no des nada por hecho, no asumas uniformidad entre placas.

---

## 1. Modo de operación obligatorio

1. **LEE TODO, en orden.** No saltes ficheros "porque ya te suenan". Cada documento se editó a propósito y contiene matices por variante.
2. **NO ASIMES NADA UNIFORME ENTRE PLACAS.** Cada nodo es un entorno distinto: **ADC, divisor de batería, LPCOMP, GPIOs, radio, potencias** pueden diferir. Especialmente en las variantes **DIY**.
3. **Verifica contra el código real.** Si un documento dice X pero el `variant.h`/`main-nrf52.cpp` de una carpeta concreta dice Y, **el código es la fuente de verdad** y el documento debe corregirse.
4. **No modifiques código sin recompilar** las variantes afectadas. Compilar SIEMPRE con `-e <env real>` desde la carpeta de la variante (`default_envs = tbeam` falla y NO se cambia).
5. **Sé honesto**: si no hay evidencia para clasificar algo, di "unknown" y lista qué lo desambiguaría. No rellenes con especulación.

## 2. Puntos críticos a auditar con lupa

- **`getActiveLpcompThreshold()`** (`src/platform/nrf52/main-nrf52.cpp`): debe tener los `#ifdef` por variante (`SEEED_SOLAR_NODE`, `SEEED_XIAO_NRF52840_KIT`, `HELTEC_T114`) devolviendo `BATTERY_LPCOMP_THRESHOLD`, y el `switch` dinámico SOLO en Promicro/Faketec (divisor 0.5). Verifica el divisor real de cada placa.
- **Divisores de batería**: Promicro/Faketec 0.5; Xiao 0.3377; Seed ≈0.303; T114 0.204. No asumas 0.5.
- **`updateUser`** (NodeDB.cpp): fix de rotación de clave admin (nueva clave == admin_key → aceptar + re-favoritear).
- **Claves admin**: solo en `userPrefs.jsonc` (macros), nunca literales en src.
- **`default_envs = tbeam`** en los 6 `platformio.ini`: NO tocar.
- **Heltec T114**: LPCOMP activo en el fork (Meshtastic lo desactiva por fuga 2.9mA dormido, issue #8801) — decisión de diseño del operador, no "corregirlo".
- **Pre-check de batería** (main.cpp) + `waitUntilPowerLevelSafe` + POFCON: las 3 capas anti-brownout (brownout de ascenso solar documentado Nordic/ESP32).

## 3. Documentos de este paquete (léelos todos)

| Fichero | Contenido |
|---|---|
| `transfer_context.md` | Memoria técnica canónica: arquitectura, variantes, parches, divisores reales, historial de auditoría |
| `guia_integracion_navarrico.md` | Bloques copy-paste de parches (con el LPCOMP corregido por variante) |
| `Manual_NavaTastic.md` | Manual de comandos `/nava` (con notas por variante) |
| `cerebro/cerebro.md` + subnotas 01-09 | Capa de conocimiento portable del agente (índice global, log de estado, handover) |
| `codigo_diff/*.diff` | **CÓDIGO REAL** (git diff HEAD→working de las 6 variantes, base `54e0d8d0a` stock). Incluye TODOS los parches Rama 1+2, NavaCLIModule y fixes. Ver `NOTA_CODIGO.md` |
| `INSTRUCCION_AUDITORIA_CLAUDE.md` | Este fichero |

## 3bis. Código real — obligatorio

**Este paquete SÍ incluye el código fuente real** en `codigo_diff/` (6 diffs unificados). Audita el código, no solo la documentación:
- Contrasta cada afirmación de los `.md` contra el `.diff` correspondiente.
- Resuelve los "unknown" de rondas anteriores (guardas null, macros `#ifdef`, overrides).
- No des nada por hecho: si el código contradice un documento, **el código manda** y el documento debe corregirse.

## 4. Entrega esperada

- Lista de hallazgos ordenada por severidad, cada uno con: archivo:línea, evidencia, impacto y fix propuesto.
- NO toques código sin pedir confirmación (flujo en dos fases: plan → esperar OK → ejecutar).
- Si todo está correcto, dilo explícitamente y justifica brevemente por qué.

---

*Generado 2026-08-11. Actualizado tras la ronda de fixes de LPCOMP (Seed, Xiao×2, Heltec T114) y el resto del historial 2026-08-10/11.*
