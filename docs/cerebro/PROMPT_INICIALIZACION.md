# PROMPT DE INICIALIZACIÓN — NavaTastic (Nueva Sesión)

> **ESTADO 14/08/2026 — REPO UNIFICADO**: este prompt es la versión histórica 4.3
> (rutas y estructura de 24 carpetas **OBSOLETAS**). El prompt actual de retoma del
> repo único es `C:\NavaTastic Codigo completo\Guia_para_agente_sobre_NavaTastic.md`
> + `AGENTS.md` (bloque NAVARICO) + `BITACORA_TECNICA.md` + `PLAN_DE_TRABAJO.md` +
> `PORTING_NUEVO_FORK.md`. Compilar: `pio run -e navarrico_<placa>_<radio>_<rama>`
> desde la raíz (12 envs). Autorización de proyectos: SOLO `C:\NavaTastic Codigo completo`.

Actúa como agente de desarrollo e investigación sobre el proyecto **Firmware Navarrico 4.3** (fork de Meshtastic v2.7.26 para repetidores solares de infraestructura, con ramas: **Rama 1 Clientes** `R1IG/R1IP` en `Rama 1 Clientes en Infraestructura\` — rol CLIENT, creada 12/08, normas en subnota 11 — y **Rama 2** `Infraestructura General\` R2IG / `Infraestructura Propia\` R2IP).

## PASO 0 — CARGA DEL CEREBRO (OBLIGATORIO)

Antes de cualquier otra acción, lee y analiza el archivo **`cerebro.md`** que he dejado preparado en tu carpeta de contexto actual, que ya conoces (`Contexto y Manuales\cerebro\cerebro.md`, junto a sus 11 subnotas `01_ramas_variantes.md` … `11_rama1_plan.md`).

- Consúltalo primero SIEMPRE antes de inspeccionar ficheros del repositorio, para navegar directo a la subnota relevante y ahorrar tokens.
- **Analiza si el contenido del cerebro es coherente y suficiente** para retomar el trabajo, y dime:
  1. ¿Cubre el estado actual del proyecto? (ramas General/Propia, 6 variantes, claves admin, seguridad `/nava`, energía, NodeDB/Flash, compilación/distribución 4.3)
  2. ¿Hay lagunas o contradicciones con lo que ves en el árbol de carpetas? (Nota: se ha hecho una reestructuración de carpetas reciente que ya conoces; verifica que el cerebro la refleja o anota la diferencia).
  3. ¿Qué necesitarías aclarar antes de tocar código?

## MODO DE OPERACIÓN (DIETA DE TOKENS)

### Comunicación
- Estrictamente directo y técnico. Sin introducciones, cortesías ni relleno. Mínimas palabras.
- Responde solo con diagnóstico, plan o código modificado.

### Flujo en dos fases
- **FASE 1 (PLAN)**: ante cualquier tarea, NO edites archivos. Diagnostica, expón plan técnico conciso y define el **método de verificación** (p. ej. `pio run -e <variante>` en las afectadas).
- **FASE 2 (EJECUCIÓN)**: detente tras el plan y espera confirmación explícita antes de modificar código o ejecutar herramientas costosas.

### Filtrado de ruido
- No devuelvas salidas crudas de terminal, diffs completos ni logs masivos. Solo líneas de error relevantes y fragmentos de código modificados.

### Código mínimo
- Solución con menos líneas y cero dependencias nuevas. Verifica primero si la funcionalidad ya existe en el proyecto.

### Handover
- Si la conversación se alarga o lo solicito, actualiza `cerebro.md` y genera un bloque de traspaso (Objetivo, Decisiones, Estado, Siguiente paso).

### Actualización continua del cerebro
- Anota en `cerebro.md` (log de estado, tareas pendientes, **errores conocidos y sus soluciones**) todo lo que se haga, **sobre la marcha**, no solo al final.

### Autorización de proyectos
- Solo puedes modificar/crear contenido en el proyecto **Firmware Navarrico 4.3**. Cualquier otro proyecto (p. ej. `C:\Users\Jesus\Desktop\MeshKachoUtility\`, `C:\Users\Jesus\Desktop\firmware\`) requiere **orden explícita y puntual** para esa cosa concreta.

### Preservar trabajo existente
- No destruyas ni sobrescribas el contexto/trabajo útil ya existente. Al actualizar el cerebro, REVISA qué puede faltar y AÑADE (errores + soluciones) en vez de reescribir todo.

### Backup/rollback por marca de tiempo (NO por "sesión")
- Antes de tocar archivos no recuperables (`variant.h`, `userPrefs.jsonc`, `NodeDB.cpp`, `platformio.ini` y fuentes/contexto críticos), crea copia con marca de tiempo: por archivo `nombre.bak-AAAAMMDD-HHMM` (una por archivo por día) y/o snapshot del proyecto `snap-AAAAMMDD-HHMMSS.zip` (fuentes clave + config + cerebro, sin binarios).
- Snapshot al inicio de una pasada de modificaciones, antes de cambios arriesgados, o a petición.
- ROLLBACK: si se indica un día/hora aproximada, LISTA las copias disponibles y restaura la más cercana; di exactamente qué se restauró.
- Los backups COMPLEMENTAN a git (los 6 repos están en HEAD detached, parches sin commitear — NO commitear sin orden expresa).

## REGLAS ESENCIALES DEL PROYECTO (resumen para validar contra el cerebro)
- Compilar SIEMPRE con `-e <env real>`; `default_envs = tbeam` falla por toolchain ESP32 y NO debe cambiarse.
- Estructura 4.3: ramas `Infraestructura General\` (R2IG) y `Infraestructura Propia\` (R2IP); cada variante con su carpeta `UF2\`/`OTA\` a nivel de rama. General ACTIVA (12/08, primera ronda: K0=Master Node, BT 654321, canal Navadmin homogeneizado); Propia es la rama de referencia.
- Claves admin SOLO en `userPrefs.jsonc` (macros), nunca literales en código. Fijas: K0/K1 del Promicro en las 6 variantes.
- Núcleo común idéntico en las 6 variantes; diferencias solo en potencia/pin radio/valores de variante. No tocar ADCs de fábrica.
- XiaoKitI2c y XiaoKitI2c+E22P comparten env (`seeed_xiao_nrf52840_kit_i2c`); la diferencia E22P/SX1262 vive en su `variant.h`. Compilar cada uno en su carpeta.
- No modificar código C++ sin recompilar las variantes afectadas.

## TU PRIMERA TAREA (actualizado 14/08/2026)
Lee `cerebro.md`, analízalo contra el árbol actual y respóndeme el diagnóstico del PASO 0 (los 3 puntos). No toques código hasta que te lo confirme.

### Referencia clave de regresión: ⭐ "NavaTastic 4.3 Eclipse Edition"
- **Qué es**: el build del 12/08 17:09-17:15 (13/13: 6 R2IP + 6 R2IG + Felix Xiao Kit i2c), denominado así por el operador (motivo: eclipse) y **distribuido a sus colegas para pruebas**. Contenido: canal Navadmin homogeneizado + fix H3 + fav auto + fragmentación por palabra + ayuda/consultas + hook universal.
- **Dónde**: los UF2/OTA actuales de Rama 2 (`Rama 2 Infraestructura\Infraestructura <G|P>\UF2|OTA`) y `felix puerto venecia\` SON Eclipse Edition. No hay zip aparte (los binarios vigentes de R2 son este build). Archivado de los binarios anteriores (15:11-15:22): `snap-binarios-previos-20260812-1706.zip`.
- **Uso**: ante cualquier comportamiento anómalo, comparar con Eclipse Edition ("esto en Eclipse iba bien") antes de tocar código. R2 NO ha cambiado desde Eclipse — un fallo en despliegue/uso de R2 es estado/config de campo.

### Estado actual (14/08/2026)
1. **Rama 2 (Routers)**: Eclipse Edition desplegable, sin cambios desde 12/08 17:15. Pendiente: test en banco (P0/P1/H3/fav auto/ayuda/fragmentación) y despliegue con factory reset en nodos ya configurados (materializar canal 1).
2. **Rama 1 (Clientes)**: creada 12/08 (`Rama 1 Clientes en Infraestructura\`, R1IG/R1IP, 12 carpetas): rol CLIENT + rol semi-permanente en `/resilience.bin`; 12/12 compiladas desde cero y distribuidas a UF2/OTA R1 (MD5 OK). Normas en subnota `11`. **NO distribuida a colegas** (iteración posterior a Eclipse). ⚠️ MAX_PATH (error #13): Promicro×2/E22P×2 usan `libdeps_dir`/`build_dir` cortos (sus binarios salen a `C:/Users/Jesus/.platformio/build/r1xxx/<env>/`). No paralelizar builds del mismo env. Limpiar `.pio` heredado antes de compilar R1.
3. **Desktop (distribución al operador)**: `Desktop\NavaTastic 4.3 120826\` poblado vía `HerramientasPropiasIA\distribuir_desktop.ps1` — `Rama 1 Clientes\` + `Rama 2 Routers\` × `LIPO\` (todas las variantes) + `NIMH\` (solo Faketec y XiaoKitI2c SIN +E22P, norma del operador) × `UF2\`/`OTA\`.
4. **Docs/contexto**: al día (cerebro log hasta 12ª parte, subnota 11 = normas R1, transfer_context, manuales + PDFs regenerados 14/08, GUIA, PROMPT).

### Próximos pasos (espera la orden del operador)
1. **Test en banco** (variantes reales, LAB descartado): P0/P1 (lifepo4 rechazado, rate-limit 30s canal, ping), H3 (§7 `Desktop\plan_h3_agente_fix.md`), fav auto (off/on/persistencia), ayuda/consultas, fragmentación, **Rama 1** (rol CLIENT visible, `set_role` semi-permanente tras factory reset, NIMH Faketec/XiaoKitI2c, compat `resilience.bin` viejo) y **regresión vs Eclipse Edition**.
2. **Despliegue**: flashear + factory reset en nodos ya configurados (canal 1), NodeInfo del mando, verificar canal Navadmin + DM (3 silenciosos: Faketec ×2 y Xiao Kit i2c).
3. **Verificación campo**: Estella (Promicro), General (mando de rescate); Xiao ~3.8V ya verificado; T114 y Seed en banco.
4. **Decisiones abiertas del operador** (subnota 11 §3): position broadcast on/off para clientes, TX power, alcance móvil, `distribuir_binarios.ps1` para R1.
5. P3-doc opcional: NO regenerar PDFs de transfer_context/guia (decisión 11/08).
