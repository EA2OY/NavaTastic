# PROMPT DE DESPLIEGUE — NavaTastic (nueva sesión / retoma)

> **ESTADO 14/08/2026 — REPO UNIFICADO**: bloque histórico 4.3 (rutas y estructura de
> 24 carpetas **OBSOLETAS**). Retomar el repo único = leer `Guia_para_agente_sobre_
> NavaTastic.md` + `docs\cerebro\cerebro.md` (sección 5) + `BITACORA_TECNICA.md` +
> `PLAN_DE_TRABAJO.md` + `PORTING_NUEVO_FORK.md`. Compilar desde la raíz con
> `pio run -e navarrico_<placa>_<radio>_<rama>`; distribuir con `distribuir.ps1`.
> `C:\Firmware Navarrico 4.3` es SOLO LECTURA (archivo histórico).

Copia y pega este bloque en una nueva conversación de agente para retomar el trabajo sin perder contexto. Generado 2026-08-11 (noche), tras la sesión de fixes de auditoría y verificación en hardware.

---

```
Retoma el proyecto "Firmware Navarrico 4.3" (fork de Meshtastic v2.7.26 para repetidores
solares de infraestructura, malla SFNarrow, Madrid).

PASO 0 (OBLIGATORIO) — lee en este orden, antes de tocar nada:

1. C:\Firmware Navarrico 4.3\GUIA_AGENTE_NAVTASTIC.md
   (guía raíz de inicialización de agente)

2. C:\Firmware Navarrico 4.3\Contexto y Manuales\cerebro\cerebro.md
   (memoria canónica: visión, índice, log de estado, errores→soluciones, handover)
   + TODAS las subnotas de C:\Firmware Navarrico 4.3\Contexto y Manuales\cerebro\ (01..10):
     01_ramas_variantes.md        ramas General/Propia, 6 variantes, envs, hardware
     02_claves_admin.md           claves K0/K1, regla hardcodeo, fix updateUser, auto-recuperación
     03_seguridad_nava.md         módulo /nava, canal Navadmin, DM PKI, whitelist
     04_energia_bateria.md        LPCOMP, químicas, storm, deep sleep, divisores reales por placa
     05_nodedb_flash.md           protección Flash, RAM-only, favoritos, desalojo
     06_compilar_distribuir.md    build por variante (-e obligatorio), distribución binarios
     07_version_desplegada_estella.md  snapshot desplegado en Tierra Estella (pre-Secuencia 2)
     08_diagnostico_lab.md        instrumento LAB + PRUEBAS POR USB con CLI (comandos validados)
     09_general_vs_propia.md      normas de diferenciación General (1 clave admin = Master Node)
     10_hardcodeos_nodo.md        mapa de hardcodeos (dónde vive cada valor del nodo)

3. C:\Firmware Navarrico 4.3\Contexto y Manuales\cerebro\PROMPT_INICIALIZACION.md
   (pautas de operación: dieta de tokens, flujo en dos fases, backup por marca de tiempo,
   autorización de proyectos, actualización continua del cerebro)

4. Documentos de contexto (C:\Firmware Navarrico 4.3\Contexto y Manuales\):
   - transfer_context.md            (doc 1: arquitectura, variantes, parches, divisores reales)
   - guia_integracion_navarrico.md  (doc 2: bloques copy-paste, LPCOMP corregido por variante)
   - Manual_NavaTastic.md           (doc 3: comandos /nava, notas por variante)
   - Manual_uso_NavaTastic_4.2.md   (manual de uso, con compatibilidad de químicas por placa)

5. Referencia del proyecto hermano (SOLO LECTURA — la app que controla este firmware):
   - C:\Users\Jesus\Desktop\MeshKachoUtility\Cerebro_MeshKachoUtility\cerebro.md
   - C:\Users\Jesus\Desktop\MeshKachoUtility\GUIA_AGENTE.md

RUTAS CLAVE DEL PROYECTO:
- Código + binarios: C:\Firmware Navarrico 4.3\Rama 2 Infraestructura\Infraestructura <General|Propia>\
  (6 variantes por rama, sufijos R2IG/R2IP; UF2\ y OTA\ a nivel de rama)
- Compilar SIEMPRE: pio run -e <env real> DESDE la carpeta de la variante
  (pio.exe en C:\Users\Jesus\.platformio\penv\Scripts\pio.exe; default_envs=tbeam falla, NO tocar)
- Herramientas: HerramientasPropiasIA\distribuir_binarios.ps1 y generar_pdf.ps1
- Diagnóstico LAB: C:\Firmware Navarrico 4.3\LAB\ (NO desplegar en campo)

MODO DE OPERACIÓN:
- Mínimo consumo de tokens, directo y técnico.
- FASE 1 (plan + método de verificación) → esperas confirmación → FASE 2 (ejecución).
- Backup/rollback por marca de tiempo: nombre.bak-AAAAMMDD-HHMM y/o snap-AAAAMMDD-HHMMSS.zip
  antes de tocar archivos no recuperables (variant.h, userPrefs.jsonc, NodeDB.cpp, platformio.ini).
- Solo puedes modificar/crear contenido en Firmware Navarrico 4.3; otros proyectos requieren
  orden explícita puntual.
- Actualiza cerebro.md continuamente (log, errores→soluciones, tareas, handover).

ESTADO ACTUAL (2026-08-12, tras sesión 12/08):
- Propia (referencia): 6 variantes compiladas SUCCESS y distribuidas con:
  - Fix LPCOMP por divisor real (Seed/Xiao×2/T114: #ifdef → BATTERY_LPCOMP_THRESHOLD)
  - P0 lifepo4 rechazado en LPCOMP fijo; P1 rate-limit 30s canal Navadmin; P2 Manual db_clear
  - C8 macro huérfana eliminada; C4 REVERTIDO (rompía factory reset); fix #10873 aplicado
    (disableBluetooth después del reset → factory reset USB/BLE verificado en hardware)
  - Claves K0/K1 de fábrica con auto-recuperación en cada boot (local_sum==0)
  - Canal Navadmin homogeneizado en las 12 variantes (jsonc, 12/08)
- General: ACTIVA desde 12/08 — K0=Master Node {0xc7,0xdc,...0x00,0x55} (1 sola clave), BT 654321,
  canal Navadmin; primera ronda compilada y distribuida (12/08)
- ⚠️ SIN COMPILAR (a la espera de que el operador decida sobre una NUEVA FEATURE): canal Navadmin
  (jsonc ×10) + fix H3 (NodeDB.cpp ×12, bitfield al aceptar clave admin) → se compilará todo junto
- LAB Promicro (diagnóstico sleep) y zip de auditoría v3 disponibles
- Verificación en hardware completada: factory reset USB/BLE, persistencia tras soft reset,
  auto-recuperación de claves (detalle en 08_diagnostico_lab.md)
- Diagnóstico de admin remota: Desktop\problema_administracion.md · Plan fix H3: Desktop\plan_h3_agente_fix.md

PRÓXIMOS PASOS (a esperar orden del operador):
1. **Compilar las 12 variantes** (Propia + General) — pendiente: homogeneización canal Navadmin + fix H3 ya aplicados en fuentes, SIN COMPILAR (el operador estudia añadir una nueva feature y compilar todo junto)
2. Test en banco de P0/P1 (lifepo4 rechazado, rate-limit 30s, ping normal)
3. Test en banco del fix H3 (§7 de Desktop\plan_h3_agente_fix.md)
4. Despliegue: flashear + factory reset en nodos ya configurados (materializar canal 1) → NodeInfo del mando → verificar canal Navadmin + DM
5. Verificaciones en campo: Estella (NodeInfo→DM PKI con mando de rescate); LAB Promicro (sleep 3.8V)
6. P3-doc pendiente menor (PDFs transfer_context/guia_integracion si el operador los quiere)

Respóndeme con el diagnóstico del PASO 0 (coherencia del cerebro vs árbol real, lagunas,
aclaraciones) antes de tocar nada.
```

---

**Nota de generación**: este prompt asume el estado del 12/08/2026 (tras la sesión del 12/08). Si ha habido cambios posteriores, el propio PASO 0 (lectura del cerebro) los reflejará.
