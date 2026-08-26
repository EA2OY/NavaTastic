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

## 2bis. Decisiones arquitectónicas y filosofía de supervivencia en montaña (VECTORES ASUMIDOS POR DISEÑO)

El auditor debe entender el contexto operativo real: repetidores solares en cumbres de montaña a 1.500m de altitud, aislados por nieve o climatología severa. En este escenario, **la recuperabilidad y supervivencia del nodo prima sobre el aislamiento paranoico**. Los siguientes puntos son decisiones de diseño conscientes y aceptadas:

1. **Canal "Navadmin" con PSK pública estándar `{0x01}` (`AQ==`)**:
   - **Propósito**: Es el canal de guardia, socorro, diagnóstico y auxilio público de la montaña. Permite que cualquier montañero o radioaficionado con un equipo Meshtastic de serie pueda pedir auxilio, consultar telemetría básica o coordinar comunicaciones sin requerir una clave privada previa.
   - **Salvaguarda**: En este canal rige un **whitelist estricto de solo lectura** (`ping`, `status`, `bat`, `power`, `env`, `channel`, `noise`). Todos los comandos de configuración, reconfiguración o destructivos (`wipe`, `factory_reset`, `reboot`, `set_lora`, `ch_set`) están **absolutamente prohibidos y bloqueados**.
   - **NO es un fallo de seguridad**: Es la puerta de rescate abierta de la red.

2. **Clave de fábrica "MasterNode" y recuperación de nodos huérfanos**:
   - **Propósito**: Pilar Anti-Lockout / Mando de Rescate. Si un nodo en cumbre sufre una corrupción grave o un `wipe`, jamás debe quedarse huérfano o brickeado obligando a una expedición física con nieve para reprogramarlo por USB. La clave MasterNode compartida en la Infraestructura General permite al operador de Navarra (EA2OY) rescatar y reconfigurar el nodo por radio.
   - **Desautorización inmediata por el usuario (Procedimiento de Manual)**: El Manual de Usuario de NavaTastic instruye y aconseja expresamente que, nada más desplegar un repetidor, el operador configure sus claves públicas personales en los slots de administración (Slot 0 / Slot 1). Al establecer una clave propia en el Slot 0 (`keySlot0Own`), la clave de fábrica MasterNode queda **automáticamente desautorizada y revocada** (regla "Slot 0 = clave propia del usuario"). La clave de fábrica solo actúa como red de seguridad temporal antes del aprovisionamiento o en caso de un borrado total deliberado (`wipe`).
   - **Aislamiento para redes privadas**: Para usuarios que deseen aislamiento criptográfico total sin compartir clave de rescate en ningún momento, existe la **Rama Propia (R2IP / R1IP)**, que inyecta claves privadas únicas del operador en compilación.
   - **NO es un fallo de seguridad**: Es la garantía de supervivencia en montaña de la Infraestructura General combinada con revocación inmediata por parte del usuario final.

3. **Comandos grupales en el "Canal Privado de Flota" (Slots 2..7)**:
   - **Propósito**: Operatividad táctica colectiva mediante canales cifrados simétricos (AES-128 / AES-256). Permite enviar órdenes simultáneas a una constelación de repetidores.
   - **Salvaguarda**: Si se requiere administración nuclear con no-repudio individual, se utiliza Mensaje Directo (DM) con autenticación asimétrica por PKI (Curve25519) y comprobación de clave pública registrada.

4. **Protocolo "Botón del Pánico" (Evacuación de Flota) y Consolidación (`panic_ok`)**:
   - **Propósito**: Salto sincronizado de toda la red a una frecuencia/preset de emergencia ante interferencias o catástrofes.
   - **Salvaguarda**: La orden inicial textual `/nava panic` **exige obligatoriamente DM cifrado por PKI** (clave privada del administrador).
   - **Desacople del chat de usuario**: El pulso binario se emite por el puerto privado del sistema (`meshtastic_PortNum_PRIVATE_APP`, 256) y con firma mágica independiente de endianness (`char magic[4] = "PANC"`). Esto evita que los 28 bytes de la trama binaria aparezcan como caracteres basura en las aplicaciones móviles de los usuarios en el canal Navadmin. Los usuarios solo reciben el aviso textual limpio en español (`[Panico] EVACUACION a...`).
   - **Consolidación en red (`panic_ok`)**: Cuando el operador emite `/nava panic_ok` por DM, el nodo consolida su configuración y propaga un pulso de confirmación (`POK!`) por la malla para que todos los repetidores en periodo de prueba cancelen su rollback simultáneamente.

5. **Confirmación explícita en dos pasos para comandos destructivos**:
   - `/nava wipe CONFIRM` y `/nava full_reset CONFIRM` exigen el parámetro `CONFIRM` de forma mandatoria. Un resbalón tipográfico o un comando `/nava wipe` sin confirmar es rechazado de inmediato, impidiendo dejar huérfano un nodo en montaña.

6. **Validación defensiva de umbral de despertar solar (`set_vwake`)**:
   - `set_vwake` rechaza cualquier nivel cuyo voltaje de despertar sea inferior o igual al corte de batería activo (`vbat_cutoff`), blindando el nodo contra estados inconsistentes de sueño perpetuo.

7. **Tiempo de pantalla OLED en rol ROUTER**:
   - Upstream de Meshtastic fuerza por defecto `screen_on_secs = 1` segundo en rol `ROUTER` vía macro `IF_ROUTER(1, 60*10)` en `Default.h` para proteger la batería y evitar quemar píxeles.
   - NavaTastic adopta ese valor de 1 segundo por defecto, pero **respeta y preserva cualquier ajuste manual superior configurado por el usuario** (ej. 15s o 30s) a través de reinicios y recargas de rol, permitiendo el mantenimiento y diagnóstico visual en campo.

8. **Capa de Persistencia Atómica `/resilience.bin` (NAV7)**:
   - Escritura atómica mediante fichero temporal (`/resilience.tmp`) y posterior `rename()` atómico, garantizando que un corte de alimentación en un repetidor solar jamás corrompa ni pierda la configuración persistida.
   - Todas las operaciones de lectura/escritura en LittleFS están blindadas con `concurrency::LockGuard g(spiLock)` para evitar colisiones en el bus SPI con el chip LoRa.
   - Validación integral de integridad mediante checksum CRC32 en el arranque.

9. **Escudo Anti-Tormentas (Supresión de 12h de NodeInfo repetidos)**:
   - El nodo no solicita respuesta en su anuncio propio de arranque (`want_response = false`) y suprime durante 12 horas las consultas repetidas de NodeInfo procedentes de un mismo emisor (salvo administradores). Esta es una optimización deliberada para proteger el espectro y la batería en cumbres solares frente a spam de consultas en la malla.

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
