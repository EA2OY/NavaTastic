---
title: "Manual de Uso — Firmware NavaTastic Eclipse V4"
subtitle: "Desarrollo optimizado para infraestructura solar en ubicaciones de difícil acceso"
author: "Modificación inicial por JBAU92 · Desarrollo continuado por EA2OY"
date: "Agosto 2026"
colorlinks: true
toc: true
toc-title: "Índice"
---

# Manual de Uso — Firmware NavaTastic Eclipse V4

> **ADENDA 17/08/2026 — REPO UNIFICADO V4 (F21/F22)**: manual de usuario **VIGENTE** (comportamiento
> del firmware; la versión actual es NavaTastic Eclipse V4 (4.3.3) = repo unificado, 12 builds
> `navarrico_*`). Los binarios se distribuyen desde `distribucion\` del repo (32
> ficheros, nombres históricos).

Este firmware ha sido diseñado específicamente para nodos de la red **Meshtastic** que operan de forma aislada y autónoma. Su objetivo es garantizar la supervivencia del hardware ante caídas críticas de energía o corrupciones de memoria, permitiendo la recuperación y gestión de forma **100% remota** sin necesidad de intervenciones físicas en el emplazamiento.

> **⚠️ RESPONSABILIDAD DEL MONTAJE Y CUMPLIMIENTO NORMATIVO**: toda instalación con este
> firmware debe cumplir la normativa que le sea de aplicación (nacional, autonómica, local y
> europea): emplazamiento, permisos de acceso y obra, seguridad y medio ambiente. Dónde y cómo
> se monta el equipo (árboles, estructuras, propiedades ajenas...) es decisión y responsabilidad
> exclusiva de quien lo instala. El proyecto queda **desvinculado** de cualquier montaje o uso de
> terceros y no asume responsabilidad por usos que no se ajusten a la legislación vigente.

---

## 1. Especificaciones de Compatibilidad y Variantes

El paquete comprimido incluye binarios en formatos **`.uf2`** y **`.zip`** (para actualizaciones inalámbricas OTA), así como los bootloaders específicos que incorporan parches de seguridad para recuperarse de transmisiones Bluetooth fallidas.

### Arquitecturas Soportadas

| Variante | Hardware | Notas |
| :--- | :--- | :--- |
| **Xiao nRF52840 + E22P** | Seeed Xiao + módulo Ebyte E22P-868M30S | 2.7.26 |
| **Xiao nRF52840 Kit i2c** | Seeed Xiao + OLED I2C | 2.7.26 |
| **Faketec Estándar** | nRF52840 + radio **HT-RA62** (SX1262 estándar) | Compatible con cualquier versión HT-RA62 y variantes Faketec Vx |
| **nRF52840 + E22P** | Hardware estándar nRF52840 + E22P | Pad de selección de radio en E22 |
| **Seed Solar Node P1** | Seeed Solar Node | 2.7.26 |
| **Heltec T114** | Heltec T114 | 2.7.26 (compilada en Rama 2, sin testar) |

### Soporte de Sensores e Instrumentación (v4.2)

- **Monitoreo de energía**: Integración corregida y funcional para el chip **INA219**.
- **Telemetría ambiental**: Soporte nativo para sensores **BMP280 + AHT20, BME680**.

---

## 2. Requisitos Críticos de Hardware y Conexionado

Para que las salvaguardas de bajo consumo y gestión de energía funcionen correctamente, se deben respetar de forma estricta las siguientes normas de montaje:

- **Regla del conexionado de radio (E22P)**: El conexionado físico debe realizarse siguiendo el esquema del modelo E22 estándar. Los módulos E22P realizan la conmutación de transmisión y recepción de forma automática. Por este motivo, se ha modificado el comportamiento del **GPIO 017**: ya no conmuta TX/RX, sino que actúa como el **interruptor de encendido de la radio**, manteniéndose en alto (HIGH) para despertar el módulo y derivándose a masa (GND) para dormirlo.
- **Compatibilidad en PCBs dedicadas**: Si el módulo E22P se monta sobre placas base compatibles (como **Albatastic** o **Xiaowa**), el pad de selección de radio que se debe soldar es obligatoriamente el correspondiente al **E22**.
- **Divisor ADC 2.0**: Es necesario que el divisor de resistencias que mide el voltaje de la batería esté compuesto por **dos resistencias de 1 Megaohmio** (NRF52/Faketec/Albatastic/XiaoWa).
  > **TIP — divisor distinto**: si tu placa lleva un divisor con otros valores, puedes ajustarlo antes de compilar en `variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h` (macro `ADC_MULTIPLIER`, valor `VBAT_DIVIDER_COMP`). **AVISO importante**: ese mismo divisor alimenta el comparador **LPCOMP**, que es el que decide el **despertar del modo de resiliencia por batería baja** (`set_vwake`, niveles 1-5). Los niveles de despertar están calibrados para divisor 2.0 (1M+1M): con otro divisor, el nodo despertará a una tensión distinta de la indicada — hay que recalibrar `getActiveLpcompThreshold()` en `src/platform/nrf52/main-nrf52.cpp` (o usar el umbral fijo de fábrica).
- **Líneas de bus optimizadas (v4.1 Xiao Kit i2c)**: Se ha modificado el bus de salida a la radio para que conmute a **GND** durante el estado de Deep Sleep, reduciendo drásticamente cualquier fuga residual de corriente.

---

## 3. Configuración del Nodo (Firmware Hardcoded)

El firmware NavaTastic viene **hardcodeado** para la red SFNarrow (preset LoRa de uso nacional en España): región EU868, preset ShortFast Narrow, canal SFNarrow y canal Navadmin, potencia y umbrales de batería ya configurados por variante. **No requiere configuración del usuario** para funcionar en la malla.

La única intervención opcional del operador es:

- **Ajustar el nombre** del nodo (para identificarlo en la malla).
- **Activar la lectura de sensores de telemetría** de energía (INA219) y de clima (BMP280 + AHT20 / BME680), si la placa los incorpora.

**Conexión Bluetooth**: el nodo emite con **PIN fijo `654321`** (modo FIXED_PIN de Meshtastic) — la app lo pide al emparejar. (Los builds Propia usan un PIN propio del operador.)

Todo lo demás (canales, región, administración remota, protección de batería) está preconfigurado en compilación.

> **⚙️ Despliegue (nodos nuevos o reflasheados)**: el flasheo conserva los `/prefs` antiguos. Si el nodo es nuevo de fábrica (o viene de un firmware sin el canal Navadmin), hacer **un factory reset tras flashear** para materializar el canal Navadmin (slot 1). Sin ese canal, los avisos [Sueño]/[Vivo]/[Listo] y los comandos de consulta por canal abierto no llegarán.

> **📄 Los comandos `/nava` para ajustar nombre, sensores y resto de parámetros están en el manual de administración remota** (`Manual_NavaTastic.md`).

## 4. Dimensionado y Recomendaciones Solares

Para que el nodo soporte los ciclos de hibernación/despertar y los temporales de nieve, se recomienda el siguiente dimensionado mínimo según la radio:

| Radio | Batería mínima | Placa solar (pico) |
| :--- | :--- | :--- |
| **SX1262 / HT-RA62** | **+3000 mAh** | ~**300 mA** |
| **E22P-868M30S** | **+6000 mAh** | ~**1000 mA** |

> La E22P con etapa de amplificación consume más en TX, por eso requiere el doble de capacidad y más aporte solar.

### 🔋 Químicas de batería soportadas

El firmware soporta **4 químicas** configurables con `/nava set_chem` (solo DM). Cada una ajusta el corte de apagado, la curva OCV y el nivel de despertar solar (LPCOMP):

| Química | Corte | Despertar | Notas |
| :--- | :--- | :--- | :--- |
| **LiPo / Li-Ion** | 3500 mV | ~3.7V | Estándar |
| **NiMH (3 celdas)** | 3400 mV | ~3.7V | |
| **Sodio (Na-Ion)** | 2600 mV | ~3.7V | Carga máx ~4.0V |
| **LiFePO4** | 2800 mV | **~3.3V** | Fosfato de hierro-litio — **solo Promicro fix y Faketec** |

> El nivel de despertar (`set_vwake`) del LiFePO4 usa ~3.3V, alcanzable porque esta química carga hasta ~3.65V. Los niveles 1-4 del LPCOMP corresponden a ~2.1V, ~2.5V, ~3.7V y ~4.5V respectivamente. Ver `/nava help set_vwake`.

> **⚠️ Compatibilidad de química por placa**: en **Seed Solar P1, Xiao Kit i2c, Xiao E22P y Heltec T114** la química **`lifepo4` está rechazada** (el comando responde `ERR: LIFEPO4 NO COMPATIBLE, UMBRAL LPCOMP FIJO`). Su LPCOMP es **fijo por hardware** y su umbral de despertar (~3.67V–4.04V) supera el voltaje máximo físico de una celda LiFePO4 (~3.65V): si se aceptara, un nodo apagado por batería baja **jamás despertaría por solar** (ladrillo remoto). En esas placas solo están disponibles `lipo`, `nimh` y `sodium`. En **Promicro fix y Faketec** (LPCOMP dinámico) las 4 químicas están disponibles y `lifepo4` despierta a ~3.3V.

### ⚠️ Encendido inicial de un nodo solar

Un problema común al encender un nodo solar por primera vez es la **corriente sucia generada por el panel**: si el panel está expuesto a la luz durante el arranque, el nodo puede entrar en **brownout** y no encender bien (ciclos de reinicio).

**Procedimiento correcto:**

1. **Tapar completamente el panel solar** (papel, tela opaca) antes de conectar la batería.
2. Conectar la batería con el panel tapado.
3. Esperar a que el nodo arranque correctamente y se estabilice.
4. Destapar el panel solar solo después de la confirmación de arranque.

Si el nodo entra en brownout por no haber tapado el panel, **desconectar la corriente, tapar el panel y repetir el proceso** desde el principio.

---

## 5. Lógica de las Salvaguardas Automáticas

### A. Sistema Anti-Brownout (Protección de Batería)

1. **Monitoreo**: El firmware lee el voltaje de forma continua con filtro anti-falsos-positivos: para dormirse necesita **8 lecturas consecutivas por debajo del umbral crítico (3.4V - 3.5V) separadas ~20s** (~160s en total, V3 para las 6 placas) — una sola lectura errónea del ADC (RF, temperatura, transitorios) no puede tumbar el nodo; cualquier lectura buena resetea el contador. (En el **arranque** el pre-check usa 8 lecturas rápidas de 200ms como protección anti-brownout.)
2. **Secuencia de apagado**: El nodo salva el estado en la memoria no volátil, apaga la radio poniendo el GPIO asignado en estado bajo y configura el comparador de bajo consumo integrado (**LPCOMP**) del microcontrolador nRF52840.
3. **Consumo mínimo en hibernación**: El sistema entra en un sueño profundo donde el nRF52840 consume apenas **0.4 mA**. El consumo total del nodo (incluyendo un elevador de tensión/booster **MT3608**) se sitúa en un máximo de **1.5 mA** (o **2 mA** si el diseño incluye un ATTINY13a para reseteos cíclicos).
4. **Colchón de seguridad**: Al cortar el consumo a estos niveles, se preserva un colchón de aproximadamente el **30%** de la capacidad de la batería. Esto evita la degradación química prematura por descarga profunda y otorga un margen de varios meses para que el sistema solar reciba radiación suficiente y recupere el sistema.
5. **Resurrección automática**: En cuanto el sistema de carga solar eleva el voltaje de la batería por encima de los **+3.7V** (pudiendo requerir hasta 4V dependiendo de las tolerancias de la electrónica local), el nRF52840 despierta, reactiva el módulo de radio y el nodo vuelve a operar plenamente en la malla.

> **⚠️ ADVERTENCIA CRÍTICA SOBRE LA ALIMENTACIÓN**
>
> La salvaguarda anti-brownout por baja batería **NO funcionará si el nodo se alimenta a través del puerto USB**. Si el dispositivo se queda sin energía por el USB, no podrá entrar en modo de reposo controlado de bajo consumo; en este escenario, el firmware únicamente te protegerá frente a un reinicio accidental a valores de fábrica.

### B. Salvaguarda en Caso de Catástrofe (Hardcoded Recovery)

Si el nodo sufre una corrupción de memoria, un fallo de escritura o un reinicio eléctrico crítico provocado por el ATTINY13a justo en el momento de guardar datos, el firmware se verá obligado a realizar un **Factory Reset**. Para evitar que el nodo quede aislado e inaccesible, se han grabado a fuego (hardcoded) los siguientes parámetros por defecto en caso de reinicio forzado:

| Parámetro | Valor |
| :--- | :--- |
| **Región de operación** | EU868 |
| **Modem Preset** | ShortFast Narrow / 869.618 MHz / Slot 4 (bandwidth 62 kHz, SF7, CR5) |
| **Potencia máxima** | **22 dBm** para HT-RA62/SX1262 · **12 dBm** para E22P (30 dBm tras etapa de amplificación) |
| **Administración** | Clave de administración pública prefijada inyectada automáticamente |

> **🔴 Nota sobre potencia y antena**: El límite de potencia se establece para no generar espurias/armónicos. **Recuerda que la antena es vital que esté sintonizada/resonando correctamente en 869 MHz** para no dañar las radios.

---

## 6. Coexistencia y Diferencias con la App Oficial de Meshtastic

NavaTastic incorpora un motor de resiliencia (`/resilience.bin`) diseñado para garantizar la supervivencia de repetidores solares de montaña. Esto genera comportamientos intencionados que difieren de la App oficial:

| Parámetro / Interruptor | Comportamiento en App Oficial | Comportamiento en NavaTastic | Razón de Seguridad y Resiliencia | Manejo Canónico en NavaTastic |
| :--- | :--- | :--- | :--- | :--- |
| **Interruptor Bluetooth (BLE)** | Al apagar el switch se guarda en flash. | Al reiniciar, el firmware **vuelve a encender BLE** si no se usó `/nava`. | **Protección Anti-Huérfano**: Evita que un toque accidental en la pantalla del móvil deje el repetidor incomunicado sin Bluetooth en la montaña. | Para apagarlo de forma permanente: enviar por DM `/nava ble off` (guarda `prefs.ble_disabled = 1`). |
| **Canal 1 (Navadmin)** | La App permite borrarlo o editarlo. | **Inamovible y protegido**: Rechaza `/nava ch_del 1` y restaura PSK `AQ==`. | **Canal de Rescate Vital**: Garantiza que el nodo siempre emita avisos de ciclo solar, telemetría y diagnósticos. | Para silenciarlo en abierto: `/nava navadmin_mute on` o mover la consola con `/nava set_cli_chan <2-7>`. |
| **Claves Admin tras Reset** | Un Factory Reset borra todas las claves. | **Persistencia Criptográfica**: Las claves de usuario y rescate se restauran solas. | **Mantenimiento Remoto Seguro**: Evita perder el control administrativo tras un reset de configuración. | Consultar con `/nava admin_ls` o purgar copia persistida con `/nava keys_clear`. |
| **Rol de Hardware** | Un reset vuelve al rol del binario original. | **Rol Semi-Permanente**: El rol modificado persiste tras Factory Reset. | **Supervivencia de Malla**: Evita que un router reconvertido vuelva a cliente tras una tormenta eléctrica. | Conmutar con `/nava set_role router` o `/nava set_role client`. |
| **Base de Datos de Nodos** | La App espera que los nodos se guarden en flash. | **100% RAM-Only**: Nodos de paso viven en RAM y no se escriben en disco. | **Cero Desgaste de Flash**: Multiplica por 10 la vida útil del microcontrolador al no quemar las celdas flash. | Los favoritos manuales y automáticos se respaldan en `/resilience.bin`. |
| **Cadencia de Balizas en Routers** | Suele emitir cada 15 a 30 minutos. | Fijada por defecto a **72 horas** en routers de infraestructura. | **Anti-Saturación LoRa**: Protege el canal de spam de posición innecesario en repetidores fijos. | Ajustar intervalos de flota con `/nava set_pos_tx` y `/nava set_nodeinfo_tx`. |

---

## 7. Laboratorio: Banco de Pruebas Técnico

Antes de desplegar el nodo en una ubicación remota, es obligatorio validar el comportamiento de las salvaguardas en un entorno controlado:

```
[Fuente de Alimentación Regulable] -> [Entrada de Batería del Nodo]
                                     |
                                     +--> [Multímetro en modo Amperímetro]
```

1. Conecta el nodo a una fuente de alimentación de laboratorio regulable configurada inicialmente a **4.2V** a través de los pines de la batería (**nunca por USB**).
2. Intercala un multímetro en serie en modo de medición de corriente (**mA**) para monitorizar el consumo en tiempo real.
3. **Prueba de hibernación**: Baja lentamente el voltaje de la fuente hasta los **3.3V** y espera unos instantes (entre 1 y 3 minutos). Verifica que el firmware ejecute el guardado, apague la radio y el consumo caiga drásticamente hasta estabilizarse en los **0.4 mA** (o el rango de 1.5 - 2 mA si usas periféricos/boosters adicionales).
4. **Prueba de recuperación**: Comienza a subir el voltaje de la fuente de alimentación. Verifica que al rebasar el umbral de los **3.71V** aproximadamente, el nodo "resucita" de forma automática, inicia los módulos y vuelve a transmitir balizas a la potencia habitual.

---

## 8. Protocolo de Rescate Remoto (Paso a Paso)

Si un nodo remoto de la malla sufre un fallo crítico y vuelve al estado de fábrica, mantendrá su identificador único de Meshtastic (`idxxxxx`) pero operará bajo los parámetros genéricos introducidos por defecto (EU868, ShortFast Narrow, clave pública de rescate preconfigurada).

> Debido al funcionamiento del cifrado y el intercambio de claves públicas/privadas en Meshtastic, es **mandatorio seguir este protocolo estrictamente** para evitar bloqueos de llaves criptográficas.

### Paso 1: Preparación del Nodo de Mando (Rescate)

Necesitas un nodo físico cualquiera que actúe como "mando a distancia" para conectarte al nodo caído. Puede ser cualquier hardware compatible con Meshtastic.

1. **Aislamiento inicial**: Asegúrate de que este nodo de mando esté completamente apagado o blindado en un entorno donde no pueda emitir ni recibir balizas de la malla antes de ser configurado.
2. **Nueva recomendación**: A veces el método tradicional no funciona bien, ya que por un bug de la app de Meshtastic falla la restauración de la copia de seguridad. Por lo tanto:
3. **Usa la versión ACTUAL de la app de Meshtastic (Play Store)**: ya permite modificar la
   clave privada del nodo y, al guardarla, la clave pública correcta se **regenera sola**. No
   hace falta la versión 2.7.10 ni el `.apk` antiguo.
4. Abre la app, conéctate a tu nodo de mando y asegúrate de que haya estado aislado; si no, dará conflicto de claves con el nodo que quieres controlar.
5. Entra en **Ajustes → Seguridad** de ese nodo y, en el campo **"Clave Privada"**: borra el contenido y pega exactamente:

```
cJzjBkBwWid26swcnuOJ9v8EQcWC5fyugDhZddtnu04=
```

Pulsa **guardar/enviar**: con eso, la clave pública correcta se regenera sola.

6. Comprueba que se ha generado la clave pública:

```
x9wN6W0TuoY/gtVKM/+lysx8Rewb5CAdZ9YfzIVRAFU=
```

(Si la clave privada no se queda aplicada al guardar, hazlo de nuevo — es un bug conocido de la app de Meshtastic.)

### Paso 2: Interconexión en la Malla

1. Una vez cargada la configuración de rescate en el nodo de mando, enciéndelo o permite que acceda al espectro de radio. **¡NO PERMITAS QUE ANTES DE ESE MOMENTO ENVÍE NINGUNA BALIZA SUYA A LA RED!**
2. **El conflicto de claves**: Si el nodo de mando se hubiese encendido e identificado en la red antes de restaurar la copia de seguridad, habría intercambiado claves estándar con el nodo remoto. Al aplicar la copia posteriormente, las claves almacenadas en la lista interna no coincidirán y el sistema denegará el acceso remoto de administración.
3. **Resolución del conflicto**: Si sospechas que los nodos ya se han "visto" con claves incorrectas, debes purgar manualmente el nodo remoto de la lista de dispositivos de tu aplicación de usuario y esperar a recibir una baliza completamente limpia y nueva del nodo accidentado. Esto forzará el reconocimiento mutuo bajo la clave pública de rescate compartida. También se forzará el borrado de ese nodo cuando en el nodo remoto se "reciclen" los viejos, ya que conforme le aparecen nuevos va borrando los antiguos debido a la limitada capacidad de **80 nodos** en memoria.

### Paso 3: Reconfiguración Remota

1. Una vez que ambos dispositivos se reconozcan de forma recíproca a través de la clave pública inyectada, accede a la interfaz de **Gestión Remota** a través de tu nodo de mando.
2. Vuelve a volcar la configuración específica que tenía originalmente el nodo (canales privados, tasas de transmisión personalizadas, geolocalización, etc.).
3. **Verificación del ADC**: Durante la reconfiguración, asegúrate de comprobar que el parámetro del ADC (Conversor Analógico-Digital) esté ajustado en el valor correcto (por ejemplo, `2.0` u otro valor específico de tu hardware). Si este valor no se define correctamente, el nodo calculará de forma errónea el voltaje real de la celda y la salvaguarda anti-brownout podría fallar o no activarse a tiempo.
4. Aplica los cambios a distancia. El nodo remoto asimilará los datos y volverá a integrarse en la malla con sus funciones y nombres habituales.

---

## 9. Administración Remota (Resumen)

La administración remota se realiza con comandos **`/nava`** (módulo `NavaCLIModule`) de forma **100% silenciosa y headless**, por dos vías:

| Vía | Alcance | Seguridad |
| :--- | :--- | :--- |
| **Canal Abierto (Navadmin)** | Solo consulta y diagnóstico de lectura | Respuesta en lote con jitter 0.5-6.5s. No-admins en silencio total |
| **DM Privado Cifrado (PKI)** | Configuración, reinicio, DB, bloqueos, favoritos, energía | Firma criptográfica PKI obligatoria |

> **📄 El manual completo de comandos `/nava` se distribuye por separado** (`Manual_NavaTastic.md`): listado de los 40+ comandos, sintaxis de direccionamiento (`!ID`, `@router`, `@name:...`), ayuda por comando (`/nava help <comando>` e interrogación con `/nava <comando> ?`) y detalles de la Secuencia Remota 2 (químicas de batería, storm, energía).

### Notas de seguridad (v4.2.1)

- El canal Navadmin usa la PSK pública por defecto de Meshtastic: **cualquiera puede escucharlo**. Por eso solo admite lectura y nunca responde a no-admins.
- La suplantación del campo `from` en el canal es posible (PSK pública); la whitelist de solo-lectura es la mitigación. **Los comandos destructivos van SIEMPRE por DM PKI.**
- El canal Navadmin se identifica por su slot (índice 1), no por nombre: **no reordenar canales**.
- `/resilience.bin` (química de batería, voltajes, estado TX/BLE, **claves admin del usuario — F20**) se guarda en la raíz del disco y **sobrevive a los resets de fábrica**.
- **Claves admin y resets (F20/V3)**: las claves admin PÚBLICAS del usuario se persisten en `/resilience.bin` y vuelven tras un factory/full reset (slot 0 = estado previo del usuario). **Quitar una clave en la app NO la purga del nodo** — la copia persistida reaparecerá tras el próximo reset; para purgar de verdad: `/nava keys_clear` (borra solo la copia persistida, no la config actual) o `/nava wipe` (purga total). Tras `wipe`/`nrf erase` queda solo la clave del proyecto (canal de rescate garantizado).

---

## 10. Changelog de Versiones

| Versión | Descripción |
| :--- | :--- |
| **1.0** | Versión inicial, aplicado en versiones de firmware comprobado su uso en varios Routers de la malla que llegaban a sufrir brownouts y con esta solución dejaron de darlos. |
| **2.0** | Implementados ajustes hardcodeados después de un reset de fábrica; el nodo es capaz de sobrevivir a un fallo grave de memoria que lo devuelve a su estado de fábrica; solucionado que el nodo persistiera los cambios del usuario sobre las claves de gestión remota tras un reset. |
| **3.0** | Solucionado bug que surgía cuando el nodo, o bien arrancaba o bien era reseteado a mano o por un ATTINY13a cuando estaba alimentado a un voltaje por debajo de lo programado para dormirse: se despertaba constantemente nada más dormirse, en bucle. |
| **4.0** | Añadida compilación para Xiao Kit i2c y soporte en todas las compilaciones para Spreading Factor 5 y 6 (nuevos presets de trabajo LoRa que se están implementando en Madrid). |
| **4.1** | Corregidas variantes Faketec: no se detectaba el INA219, solucionado; variante Xiao NRF52 KIT i2c, modificado bus de salida a radio (conmuta a GND para menor consumo en Deep Sleep). |
| **4.2** | Portado el firmware Navarrico a Meshtastic 2.7.26 Beta y añadido soporte para más placas. Añadida la **Rama 2** para routers de infraestructura con protección de la Flash y auto-favoriteo de routers descubiertos en contacto directo de radio. La Rama anterior pasa a denominarse **Rama 1**. |
| **4.3** | Cambio de denominación a **NavaTastic** e integrado el **control remoto del nodo por comandos sin necesidad de PC**: se administra por radio (canal Navadmin para consultas y mensajes directos cifrados para los cambios), con protecciones frente a usos indebidos. El nodo ya mantenía el auto-favoriteo de routers directos y la base de nodos en **memoria RAM** (la Flash dejó de escribirse constantemente). |
| **4.3.1 — "NavaTastic Eclipse"** (12/08/2026) ⭐ primera distribución a colegas | Muchos comandos nuevos (`fav auto`, ayuda y consultas por radio, respuestas fragmentadas legibles, gestión de energía y diagnóstico). La **cola de mensajes y los datos descartables dejaron de escribirse en Flash**: pasaron a memoria RAM, protegiendo la vida útil del nodo. |
| **4.3.2 — "NavaTastic Eclipse V3"** (15-16/08/2026) | **Reajustados los tiempos de sueño profundo/despertar** para que no afecten a la radio ni se produzcan lecturas erróneas, y **el nodo avisa por radio de su estado al dormirse/despertarse** (con la causa en cada arranque). **Reforzada la resiliencia ante fallos**: claves de admin persistidas tras factory reset (`keys_ls`/`keys_clear`, `full_reset`/`wipe`). |
| **4.3.3 — "NavaTastic F21 / V4"** (17/08/2026, versión actual) | **Gestión Remota Avanzada de Canales e Infraestructura (F21)**: soporte completo para creación, borrado, listado y URL de canales secundarios (`ch_set`, `ch_del`, `ch_ls`, `ch_url`, `ch_reset`). **Redirección de NavaCLI** y silenciamiento opcional de Navadmin (`set_cli_chan`, `navadmin_mute`). **Control MQTT por canal** (`ch_mqtt`, `set_ok_to_mqtt`), **coordenadas fijas** (`set_pos`), **baliza ajustable** (`set_beacon`), **PIN Bluetooth** (`set_pin`), **modo silencioso temporal** (`mute`), **diagnósticos y log en RAM** (`stats`, `log`, `test_tx`). Persistencia atómica `/resilience.bin` V4 (`NAV4`). |

---

# NavaTastic User Manual — Eclipse V3 (English)

> English translation of the Spanish manual above. **The Spanish original is the authoritative
> version.** The firmware is designed for autonomous, isolated **Meshtastic** network nodes; its
> goal is hardware survival through critical energy drops or memory corruption, with **100%
> remote** recovery — no physical intervention needed at the site.

> **⚠️ INSTALLATION RESPONSIBILITY AND REGULATORY COMPLIANCE**: any installation using this
> firmware must comply with the regulations applicable to it (national, regional, local and
> European): site, access and works permits, safety and environment. Where and how the
> equipment is mounted (trees, structures, third-party property...) is the sole decision and
> responsibility of whoever installs it. The project is **dissociated** from any third-party
> installation or use and assumes no responsibility for uses that do not comply with current
> legislation.

## 1. Compatibility and variants

The distribution package contains `.uf2` binaries and `.zip` packages (OTA wireless updates),
plus board-specific bootloaders with security patches to recover from failed Bluetooth transfers.

| Variant | Hardware | Notes |
| :--- | :--- | :--- |
| **Xiao nRF52840 + E22P** | Seeed Xiao + Ebyte E22P-868M30S | 2.7.26 |
| **Xiao nRF52840 Kit i2c** | Seeed Xiao + I2C OLED | 2.7.26 |
| **Faketec Estándar** | nRF52840 + **HT-RA62** radio (standard SX1262) | Any HT-RA62 version and Faketec Vx variants |
| **nRF52840 + E22P** | Standard nRF52840 + E22P | Radio selector pad on E22 |
| **Seed Solar Node P1** | Seeed Solar Node | 2.7.26 |
| **Heltec T114** | Heltec T114 | 2.7.26 (built, not yet field-tested) |

**Sensors (v4.2)**: fixed INA219 power monitoring; native support for **BMP280 + AHT20, BME680**.

## 2. Critical hardware and wiring requirements

- **E22P radio wiring rule**: follow the standard E22 wiring. E22P modules switch TX/RX
  automatically, so **GPIO 017** no longer toggles TX/RX — it acts as the **radio power switch**:
  HIGH wakes the module, GND sleeps it.
- **Dedicated PCB compatibility**: on compatible carrier boards (e.g. **Albatastic**, **Xiaowa**)
  the radio selector pad must be soldered to **E22**.
- **ADC divider 2.0**: the battery voltage divider must use **two 1 MΩ resistors**
  (NRF52/Faketec/Albatastic/XiaoWa).
  > **TIP — different divider**: adjust before compiling in
  > `variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h` (`ADC_MULTIPLIER`,
  > `VBAT_DIVIDER_COMP`). **Important**: the same divider feeds **LPCOMP**, which decides the
  > **low-battery resilience wake-up** (`set_vwake`, levels 1-5) — levels are calibrated for 2.0;
  > with another divider recalibrate `getActiveLpcompThreshold()` in
  > `src/platform/nrf52/main-nrf52.cpp` (or use the board's factory fixed threshold).
- **Optimized bus lines (v4.1 Xiao Kit i2c)**: the radio output bus switches to **GND** during
  Deep Sleep to reduce leakage current.

## 3. Node configuration (hardcoded firmware)

The firmware comes **hardcoded** for the SFNarrow network (national LoRa preset in Spain): EU868
region, ShortFast Narrow preset, SFNarrow channel and Navadmin channel, power and battery
thresholds per variant. **No user configuration is required** to operate on the mesh. Optional
operator steps: set the node **name**, enable telemetry sensors (INA219, BMP280 + AHT20 / BME680).
**Bluetooth**: fixed PIN **`654321`** (FIXED_PIN mode; the app asks when pairing). Everything
else (channels, region, remote administration, battery protection) is preconfigured at build time.

> **⚙️ Deployment (new or reflashed nodes)**: flashing keeps old `/prefs`. Factory-new nodes (or
> nodes from firmware without the Navadmin channel) need **one factory reset after flashing** to
> materialize the Navadmin channel (slot 1) — without it, the [Sueño]/[Vivo]/[Listo] notices and
> open-channel queries will not arrive.
>
> **📄 The `/nava` commands** (name, sensors, all other parameters) are in the remote
> administration manual (`Manual_NavaTastic.md`).

## 4. Solar sizing recommendations

| Radio | Minimum battery | Solar panel (peak) |
| :--- | :--- | :--- |
| **SX1262 / HT-RA62** | **3000+ mAh** | ~**300 mA** |
| **E22P-868M30S** | **6000+ mAh** | ~**1000 mA** |

> The E22P's amplification stage draws more in TX — double the capacity and solar input.

### Supported battery chemistries

4 chemistries, configurable via `/nava set_chem` (DM only):

| Chemistry | Cutoff | Wake | Notes |
| :--- | :--- | :--- | :--- |
| **LiPo / Li-Ion** | 3500 mV | ~3.7 V | Standard |
| **NiMH (3 cells)** | 3400 mV | ~3.7 V | |
| **Sodium (Na-Ion)** | 2600 mV | ~3.7 V | Max charge ~4.0 V |
| **LiFePO4** | 2800 mV | **~3.3 V** | **Promicro fix and Faketec only** |

> **⚠️ Chemistry compatibility per board**: on **Seed Solar P1, Xiao Kit i2c, Xiao E22P and
> Heltec T114** `lifepo4` is **rejected** (`ERR: LIFEPO4 NO COMPATIBLE, UMBRAL LPCOMP FIJO`):
> their LPCOMP is hardware-fixed and its wake threshold (~3.67 V–4.04 V) exceeds the physical
> maximum of a LiFePO4 cell (~3.65 V) — an accepted LiFePO4 node would **never wake on solar**
> (remote brick). Only `lipo`, `nimh`, `sodium` there. On **Promicro fix and Faketec** (dynamic
> LPCOMP) all 4 chemistries are available; `lifepo4` wakes at ~3.3 V.

### ⚠️ Initial power-on of a solar node

The panel can produce **dirty current** while exposed to light during boot → **brownout** and
reset loops. Correct procedure:

1. **Fully cover the solar panel** (paper, opaque cloth) before connecting the battery.
2. Connect the battery with the panel covered.
3. Wait for the node to boot and stabilize.
4. Uncover the panel only after boot is confirmed.

If the node brownouts anyway: disconnect power, cover the panel, repeat from the start.

## 5. Automatic safeguard logic

### A. Anti-brownout system (battery protection)

1. **Monitoring**: continuous voltage reading with an anti-false-positive filter: sleeping needs
   **8 consecutive readings below the critical threshold (3.4 V–3.5 V) ~20 s apart (~160 s
   total; V3, all 6 boards)** — a single spurious ADC reading (RF, temperature, transients) cannot take the node
   down; any good reading resets the counter. (At boot, the pre-check uses 8 fast 200 ms
   readings as anti-brownout protection.)
2. **Shutdown sequence**: the node saves state to non-volatile memory, powers the radio down
   (GPIO LOW) and arms the nRF52840's low-power comparator (**LPCOMP**).
3. **Minimum hibernation consumption**: deep sleep at ~**0.4 mA** on the nRF52840; total node
   consumption (with an **MT3608** booster) max **1.5 mA** (or **2 mA** with an ATTINY13a for
   cyclic resets).
4. **Safety cushion**: cutting consumption preserves ~**30%** of battery capacity — avoids early
   chemical degradation from deep discharge and gives months of margin for solar recovery.
5. **Automatic resurrection**: once solar charging raises the battery above **+3.7 V** (up to 4 V
   depending on local electronics tolerances), the nRF52840 wakes, reactivates the radio and the
   node returns to full mesh operation.

> **⚠️ CRITICAL POWER WARNING**: the low-battery anti-brownout safeguard **will NOT work when the
> node is powered through USB**. Without battery power the node cannot enter the controlled
> low-power sleep; in that scenario the firmware only protects against an accidental reset to
> factory values.

### B. Catastrophe safeguard (hardcoded recovery)

If the node suffers memory corruption, a write failure or a critical electrical reset by the
ATTINY13a right while saving data, the firmware performs a **Factory Reset**. To avoid leaving
the node isolated and unreachable, these defaults are hardcoded:

| Parameter | Value |
| :--- | :--- |
| **Operating region** | EU868 |
| **Modem preset** | ShortFast Narrow / 869.618 MHz / Slot 4 (62 kHz BW, SF7, CR5) |
| **Maximum power** | **22 dBm** HT-RA62/SX1262 · **12 dBm** E22P (30 dBm after amplification) |
| **Administration** | Prefixed public admin key, injected automatically |

> **🔴 Power & antenna note**: the power limit avoids spurs/harmonics. **The antenna must be
> tuned/resonant at 869 MHz** to avoid damaging the radios.

## 6. Coexistence & Differences with the Official Meshtastic App

NavaTastic incorporates a resilience engine (`/resilience.bin`) designed to ensure the survival of solar mountain repeaters. This results in intentional behaviors that differ from standard Meshtastic firmware:

| Feature / Setting | Official App Behavior | NavaTastic Behavior | Safety & Resilience Rationale | Canonical NavaTastic Solution |
| :--- | :--- | :--- | :--- | :--- |
| **Bluetooth Switch (BLE)** | Disabling BLE saves `false` to flash. | Upon reboot, firmware **re-enables BLE** if `/nava` was not used. | **Anti-Orphan Protection**: Prevents accidental mobile taps from leaving a mountain repeater unreachable. | To disable permanently: send DM `/nava ble off` (writes `prefs.ble_disabled = 1`). |
| **Channel 1 (Navadmin)** | App allows renaming, editing or deleting. | **Locked and protected**: Rejects deletion and restores PSK `AQ==`. | **Vital Rescue Backbone**: Guarantees telemetry, solar notices and remote rescue availability. | To mute in open air: `/nava navadmin_mute on` or relocate CLI with `/nava set_cli_chan <2-7>`. |
| **Admin Keys on Reset** | A Factory Reset wipes all admin keys. | **Cryptographic Persistence**: User & rescue admin keys restore automatically. | **Safe Remote Maintenance**: Prevents losing administrative access after a config reset. | Inspect with `/nava admin_ls` or purge persisted copy with `/nava keys_clear`. |
| **Hardware Role** | A reset reverts to the compile-time binary role. | **Semi-Permanent Role**: Reconfigured roles persist across factory resets. | **Mesh Backbone Survival**: Prevents a converted router from reverting to client after a lightning reset. | Switch roles with `/nava set_role router` or `/nava set_role client`. |
| **Node Discovery Database** | Official app expects nodes stored in flash. | **100% RAM-Only**: Transit nodes live in RAM and never write to flash. | **Zero Flash Wear**: Multiplies hardware lifespan tenfold by preventing flash memory burnout. | Manual and auto favorites are backed up in `/resilience.bin`. |
| **Router Beacon Interval** | Usually broadcasts every 15 to 30 mins. | Set to **72 hours** by default on infrastructure routers. | **LoRa Airtime Throttling**: Keeps mesh channels clean of unnecessary position spam. | Adjust fleet intervals with `/nava set_pos_tx` and `/nava set_nodeinfo_tx`. |

## 7. Laboratory: technical test bench

Before deploying to a remote location, validate the safeguards in a controlled environment:

```
[Adjustable Lab Power Supply] -> [Node Battery Input]
                                     |
                                     +--> [Multimeter in Ammeter Mode]
```

1. Connect the node to an adjustable lab supply set to **4.2 V** through the battery pins
   (**never through USB**).
2. Wire a multimeter in series (mA mode) to monitor consumption in real time.
3. **Hibernation test**: slowly lower the supply to **3.3 V** and wait (1–3 min). Verify the
   firmware saves state, powers the radio down and consumption drops to **0.4 mA** (or
   1.5–2 mA with peripherals/boosters).
4. **Recovery test**: raise the supply voltage. Verify that past **~3.71 V** the node
   "resurrects" automatically, initializes modules and transmits beacons at normal power.

## 8. Remote rescue protocol (step by step)

If a remote node suffers a critical failure and returns to factory state, it keeps its unique
Meshtastic ID (`idxxxxx`) but operates under the generic default parameters (EU868, ShortFast
Narrow, preconfigured public rescue key).

> Because of how Meshtastic encryption and public/private key exchange work, **follow this
> protocol strictly** to avoid cryptographic key lockouts.

### Step 1: Preparing the control node (rescue)

You need any physical Meshtastic-compatible node acting as the remote control to reach the
fallen node.

1. **Initial isolation**: make sure this control node is fully powered off (or shielded where it
   cannot emit/receive mesh beacons) before configuring it.
2. **New recommendation**: the traditional method sometimes fails because a Meshtastic app bug
   breaks backup restoration. Therefore:
3. **Use the CURRENT Meshtastic app (Play Store)**: it already allows editing the node's
   private key and, when saved, the correct public key **regenerates by itself**. No need for
   the 2.7.10 version or the old `.apk`.
4. Open the app, connect to your control node and make sure it stayed isolated; otherwise there will be a key conflict with the node you want to control.
5. Go to that node's **Settings → Security** and, in the **"Private Key"** field: clear the
   content and paste exactly:

```
cJzjBkBwWid26swcnuOJ9v8EQcWC5fyugDhZddtnu04=
```

Press **save/send**: the correct public key regenerates by itself.

6. Verify that the public key has been generated:

```
x9wN6W0TuoY/gtVKM/+lysx8Rewb5CAdZ9YfzIVRAFU=
```

(If the private key does not stay saved, do it again — it is a known Meshtastic app bug.)

### Step 2: Mesh Interconnection

1. Once the rescue configuration is loaded into the control node, turn it on or allow it to
   transmit. **DO NOT ALLOW IT TO EMIT ANY OF ITS OWN BEACONS BEFORE THIS!**
2. **Key conflict**: If the control node turned on before restoring the backup, it exchanged
   standard keys with the remote node, causing key mismatches.
3. **Resolving the conflict**: If they already saw each other with wrong keys, purge the remote
   node from your device list and wait for a fresh beacon.

### Step 3: Remote Reconfiguration

1. Once both nodes recognize each other via the injected public key, access **Remote Management**.
2. Restore the specific original configuration (private channels, transmission rates, GPS, etc.).
3. **ADC verification**: check that the ADC parameter is set correctly (e.g. `2.0`).
4. Apply changes remotely.

## 9. Remote administration (summary)

Remote administration runs with **`/nava`** commands (module `NavaCLIModule`), **100% silent and
headless**, through two channels:

| Channel | Scope | Security |
| :--- | :--- | :--- |
| **Open channel (Navadmin)** | Read-only query and diagnostics | Batch reply with 0.5-6.5 s jitter. Non-admins get total silence |
| **Encrypted DM (PKI)** | Configuration, reboot, DB, blocklist, favorites, energy | Mandatory PKI cryptographic signature |

> **📄 The complete `/nava` command manual is distributed separately** (`Manual_NavaTastic.md`):
> 40+ commands, batch addressing (`!ID`, `@router`, `@name:...`), per-command help
> (`/nava help <command>`, `/nava <command> ?`).

### Security notes (v4.2.1)

- The Navadmin channel uses Meshtastic's default **public PSK**: anyone can listen. Read-only;
  never replies to non-admins.
- `from` spoofing on the channel is possible (public PSK); the read-only whitelist is the
  mitigation. **Destructive commands ALWAYS go through DM PKI.**
- The Navadmin channel is identified by its **slot (index 1)**, not by name: do not reorder
  channels.
- `/resilience.bin` (chemistry, voltages, TX/BLE state, **user admin keys — F20**) lives at the disk root and **survives
  factory resets**.
- **Admin keys and resets (F20/V3)**: the user's PUBLIC admin keys are persisted in
  `/resilience.bin` and return after a factory/full reset (slot 0 = the user's previous state).
  **Removing a key in the app does NOT purge it from the node** — the persisted copy reappears
  after the next reset; to truly purge: `/nava keys_clear` (clears only the persisted copy, not
  the current config) or `/nava wipe` (total purge). After `wipe`/`nrf erase` only the project
  key remains (guaranteed rescue channel).

## 10. Version changelog

| Version | Description |
| :--- | :--- |
| **1.0** | Initial version, proven on several mesh routers that suffered brownouts — the solution stopped them. |
| **2.0** | Hardcoded defaults after factory reset; the node survives critical memory failures; user changes no longer persist over the remote-management keys after a reset. |
| **3.0** | Fixed a wake/sleep loop when booting or being reset (by hand or ATtiny13a) below the programmed sleep voltage. |
| **4.0** | Xiao Kit i2c build; Spreading Factor 5 and 6 support in all builds (new LoRa presets being deployed in Madrid). |
| **4.1** | Fixed Faketec INA219 detection; Xiao NRF52 Kit i2c radio output bus switches to GND for lower Deep Sleep consumption. |
| **4.2** | Ported to Meshtastic 2.7.26 Beta, more boards supported. Added **Branch 2** for infrastructure routers (Flash protection + auto-favoriting of direct radio routers). Previous branch renamed **Branch 1**. |
| **4.3** | Renamed **NavaTastic** and integrated **remote control of the node by commands, no PC needed**: it is administered over the radio (Navadmin channel for queries, encrypted direct messages for changes), with protections against misuse. The node already had direct-router auto-favoriting and the node database in **RAM** (Flash no longer written constantly). |
| **4.3.1 — "NavaTastic Eclipse"** (12/08/2026) ⭐ first distribution to colleagues | Many new commands (`fav auto`, on-air help and queries, readable fragmented replies, energy and diagnostic management). The **message queue and disposable data stopped being written to Flash**: moved to RAM, protecting the node's lifespan. |
| **4.3.2 — "NavaTastic Eclipse V3"** (15-16/08/2026) | **Deep-sleep/wake timings readjusted** so they do not affect the radio nor produce erroneous readings, and **the node announces its state over the radio when it sleeps/wakes** (with the reset cause on every boot). Strengthened resilience: admin keys survive factory reset (`keys_ls`/`keys_clear`, `full_reset`/`wipe`). |
| **4.3.3 — "NavaTastic F21 / V4"** (17/08/2026, current) | **Advanced Remote Channels & Infrastructure Management (F21)**: Full secondary channel management (`ch_set`, `ch_del`, `ch_ls`, `ch_url`, `ch_reset`), CLI listener and notice redirection (`set_cli_chan`, `navadmin_mute`), MQTT controls (`ch_mqtt`, `set_ok_to_mqtt`), static coordinates (`set_pos`), adjustable beacon interval (`set_beacon`), Bluetooth PIN (`set_pin`), temporary RF mute (`mute`), RAM-only diagnostics and logs (`stats`, `log`, `test_tx`). Atomic `/resilience.bin` V4 (`NAV4`) persistence. |
