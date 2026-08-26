# SALIDA DE EMERGENCIA — TRASPASO DE SESIÓN NAVATASTIC

> **INSTRUCCIÓN PARA EL NUEVO AGENTE DE IA:**
> Lee este archivo en primer lugar. Te proporciona el estado exacto del proyecto, los archivos clave que debes consultar y los próximos pasos inmediatos.

---

## 1. Archivos Obligatorios de Memoria y Contexto a Leer
Antes de tocar nada o responder al operador, lee estos archivos en orden:
1. `docs/Guia_para_agente_sobre_NavaTastic.md` (Reglas operativas, no tocar sin autorización, estructura).
2. `docs/cerebro/cerebro.md` (Índice maestro de arquitectura y memoria viva).
3. `docs/INSTRUCCION_AUDITORIA_CLAUDE.md` (Directrices de seguridad, filosofía de montaña y puntos de auditoría).
4. `docs/Manual_NavaTastic.md` (Catálogo de comandos y sintaxis de administración).

---

## 2. Estado Actual del Proyecto (NavaTastic V5.2 — Commit `be5e9e879`)
El firmware es el fork unificado sobre **Meshtastic 2.7.26 (base `54e0d8d0a`)**.
Todo el código está sincronizado y subido al repositorio GitHub `EA2OY/NavaTastic`:
* Rama **`master`**: Código activo de producción con todas las mejoras.
* Rama **`main`**: Enlazada directamente con el commit oficial `54e0d8d0a` para auditoría y diff en 1 clic.

---

## 3. Trabajo Realizado en Esta Sesión (100% Completado y Compilado)
1. **Solución del Bug de Propagación del Botón del Pánico (Endianness)**:
   - `pulse.magic` era `0x50414E43` (`uint32_t`). En procesadores ARM Cortex-M4 (Little-Endian) se invertía en memoria física a `"CNAP"`. Los nodos receptores comparaban `memcmp(..., "PANC", 4)` y rechazaban el pulso en silencio el 100% de las veces.
   - **Fix aplicado**: Cambiado a `char magic[4]` con `memcpy(pulse.magic, "PANC", 4)`. Independiente de endianness.
2. **Desacople de Chat de Navadmin (Puerto Privado)**:
   - El pulso binario ahora se transmite por `meshtastic_PortNum_PRIVATE_APP` (puerto 256) con `ccToPhone = false`. Las apps móviles ya no imprimen la línea basura con caracteres ilegibles en el chat.
   - El aviso textual `[Panico] EVACUACION a...` viaja en texto limpio por `TEXT_MESSAGE_APP` con eco local (`ccToPhone = true`).
3. **Propagación en Malla de `panic_ok`**:
   - Al ejecutar `/nava panic_ok` por DM al nodo maestro, este consolida su configuración y difunde un pulso binario `POK!` y un aviso por radio a toda la cordillera. Todos los repetidores en prueba cancelan su rollback simultáneamente.
4. **Tiempo de Pantalla OLED en Rol ROUTER**:
   - Meshtastic oficial fuerza 1 segundo (`IF_ROUTER(1, 60*10)`).
   - **Fix aplicado en `NodeDB.cpp` y `NavaCLIModule.cpp`**: Si el usuario configura un tiempo superior (> 1s) en la app, el firmware lo preserva a través de reinicios y recargas de rol sin machacarlo.
5. **Incorporación de Mejoras de la Auditoría de Claude**:
   - **`wipe CONFIRM` y `full_reset CONFIRM`**: Exigen obligatoriamente el argumento `CONFIRM` para ejecutar comandos destructivos.
   - **Validación defensiva de `set_vwake`**: Impide fijar un voltaje de despertar menor o igual al corte de batería (`vbat_cutoff`).
6. **Binarios Compilados y Disponibles en el Escritorio del Operador**:
   - `C:\Users\Jesus\Desktop\NavaTastic_Test_V5_1\Faketec_SX1262_R2IG.uf2`
   - `C:\Users\Jesus\Desktop\NavaTastic_Test_V5_1\Promicro_E22P_R2IG.uf2`

---

## 4. Qué Falta / Próximos Pasos Inmediatos
1. **Prueba física en banco con los 4 nodos**:
   - El operador tiene 4 nodos en su banco de pruebas (frecuencia de trabajo en malla: **869.618 MHz**).
   - Flashear los nuevos binarios `.uf2` del Escritorio en los nodos.
   - Repetir la prueba de pánico: `/nava panic medium_fast 5 6` por DM al maestro.
   - Verificar que:
     a) No sale texto basura en el chat de Navadmin.
     b) Los otros 3 nodos capturan el pulso `"PANC"` e inician su cuenta atrás simultánea.
     c) Al enviar `/nava panic_ok`, toda la red cancela el rollback y se consolida en `MEDIUM_FAST`.
2. **Generación de binarios finales**:
   - Si la prueba en banco es satisfactoria, compilar las 12 variantes con `./build.ps1` y empaquetar la versión final en `distribucion\`.

---

> **RECORDATORIO VITAL PARA EL AGENTE:**  
> **NUNCA modifiques código ni compiles sin la autorización previa y explícita del operador.**
