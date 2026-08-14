# GUÍA DE INICIALIZACIÓN DE AGENTE — Firmware Navarrico 4.3

Punto de entrada único para cualquier agente (IA) que retome trabajo en este proyecto. Léela completa **antes de tocar cualquier fichero**.

---

## 1. Qué leer y en qué orden (OBLIGATORIO)

1. **`C:\Firmware Navarrico 4.3\Contexto y Manuales\cerebro\cerebro.md`** — memoria canónica del proyecto (visión, ramas, variantes, decisiones, log de estado y **handover**). Léela junto a sus subnotas de la misma carpeta:
   - `01_ramas_variantes.md` — ramas General/Propia, 6 variantes, envs, hardware. **General activa (12/08)**.
   - `02_claves_admin.md` — claves K0/K1, regla del hardcodeo, fix `updateUser`.
   - `03_seguridad_nava.md` — módulo `/nava`, canal Navadmin, DM PKI, whitelist.
   - `04_energia_bateria.md` — LPCOMP, químicas, storm, deep sleep, ADC, fixes por divisor.
   - `05_nodedb_flash.md` — protección Flash, RAM-only, favoritos, desalojo.
   - `06_compilar_distribuir.md` — build por variante, distribución de binarios, errores conocidos.
   - `07_version_desplegada_estella.md` — snapshot desplegado en Tierra Estella (pre-Secuencia 2).
   - `08_diagnostico_lab.md` — instrumento LAB de diagnóstico (NO desplegar).
   - `09_general_vs_propia.md` — normas de diferenciación General vs Propia (PSK Master Node, claves, canal).
   - `10_hardcodeos_nodo.md` — mapa de hardcodeos del nodo (dónde vive cada valor).
   - `11_rama1_plan.md` — **Rama 1 Clientes** (ejecutada 12/08): normas R1 vs R2, decisiones pendientes, compilación/distribución.
2. **Documentos de contexto del proyecto** (también importantes; dan el detalle técnico completo):
   - `C:\Firmware Navarrico 4.3\Contexto y Manuales\transfer_context.md` — arquitectura, variantes, parches, reglas (doc 1 de 3).
   - `C:\Firmware Navarrico 4.3\Contexto y Manuales\guia_integracion_navarrico.md` — portabilidad de parches, bloques copy-paste (doc 2 de 3).
   - `C:\Firmware Navarrico 4.3\Contexto y Manuales\Manual_NavaTastic.md` — manual de comandos `/nava` (doc 3 de 3).
   - `C:\Firmware Navarrico 4.3\Contexto y Manuales\cerebro\PROMPT_INICIALIZACION.md` — reglas de dieta de tokens y flujo en dos fases.

> El cerebro está **enlazado** a la documentación: cuando el detalle falte en una subnota, consulta el documento de contexto correspondiente. El `handover` (sección 4 de `cerebro.md`) marca el estado actual y el siguiente paso.

---

## 2. Reglas esenciales (resumen)

- **Compilar SIEMPRE con `-e <env real>`** desde la carpeta de la variante. `default_envs = tbeam` falla (toolchain ESP32) y NO debe cambiarse.
- Estructura 4.3: `Rama 2 Infraestructura\Infraestructura <General|Propia>\<Carpeta variante>` (sufijos `R2IG`/`R2IP`). UF2/OTA a nivel de rama. **General activa (12/08): K0=Master Node (1 clave), BT 654321; canal Navadmin homogeneizado.** **Rama 1 Clientes (12/08): `Rama 1 Clientes en Infraestructura\` (sufijos `R1IG`/`R1IP`), rol CLIENT + rol semi-permanente en resilience.bin (normas en subnota `11`).** ⚠️ Promicro×2/E22P×2 de R1 usan `libdeps_dir`/`build_dir` cortos (MAX_PATH, error #13) — no borrar esas líneas del platformio.ini.
- Claves admin SOLO en `userPrefs.jsonc` (macros), nunca literales en código. K0/K1 = Promicro en las 6 variantes.
- Núcleo común idéntico en las 6 variantes. No tocar ADCs de fábrica.
- XiaoKitI2c y XiaoKitI2c+E22P comparten env (`seeed_xiao_nrf52840_kit_i2c`); la diferencia vive en su `variant.h`. Compilar cada uno en su carpeta.
- No modificar C++ sin recompilar las variantes afectadas.
- Mantener actualizados: `cerebro.md` + subnotas, los 3 docs de contexto y los PDFs (via `HerramientasPropiasIA\generar_pdf.ps1`).

---

## 3. Herramientas del proyecto

| Herramienta | Ruta | Función |
|---|---|---|
| Compilación | `pio` (`C:\Users\Jesus\.platformio\penv\Scripts\pio.exe`) | `pio run -e <env>` desde la carpeta de la variante |
| Distribución de binarios | `C:\Firmware Navarrico 4.3\HerramientasPropiasIA\distribuir_binarios.ps1` | Copia `.uf2`/`.zip` a `UF2\`/`OTA\` de la rama (deduce General/Propia por sufijo) |
| Generación PDF | `C:\Firmware Navarrico 4.3\HerramientasPropiasIA\generar_pdf.ps1` | Convierte los `.md` de `Contexto y Manuales` a PDF con plantilla 4.3 |

---

## 4. Flujo de trabajo

1. Carga el cerebro y los docs (sección 1).
2. **FASE 1 (PLAN)**: diagnostica y expón plan técnico conciso + método de verificación. No edites archivos.
3. Espera confirmación explícita antes de **FASE 2 (EJECUCIÓN)**.
4. Al terminar: actualiza `cerebro.md` (log + handover) si hubo cambios de estado.
