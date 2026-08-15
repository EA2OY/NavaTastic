---
title: "Manual de Uso — Firmware NavaTastic 4.2"
subtitle: "Desarrollo optimizado para infraestructura solar en ubicaciones de difícil acceso"
author: "Modificación inicial por JBAU92 · Desarrollo continuado por EA2OY"
date: "Agosto 2026"
colorlinks: true
toc: true
toc-title: "Índice"
---

# Manual de Uso — Firmware NavaTastic 4.2

> **ADENDA 14/08/2026 — REPO UNIFICADO**: manual de usuario **VIGENTE** (comportamiento
> del firmware; la versión actual es NavaTastic 4.3 = repo unificado, 12 builds
> `navarrico_*`). Los binarios se distribuyen desde `distribucion\` del repo (32
> ficheros, nombres históricos). El PDF de este manual no se regenera en el repo.

Este firmware ha sido diseñado específicamente para nodos de la red **Meshtastic** que operan de forma aislada y autónoma. Su objetivo es garantizar la supervivencia del hardware ante caídas críticas de energía o corrupciones de memoria, permitiendo la recuperación y gestión de forma **100% remota** sin necesidad de intervenciones físicas en el emplazamiento.

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

1. **Monitoreo**: El firmware lee el voltaje de forma continua con filtro anti-falsos-positivos: para dormirse necesita **5 lecturas consecutivas por debajo del umbral crítico (3.4V - 3.5V) separadas ~20s** (~100s en total) — una sola lectura errónea del ADC (RF, temperatura, transitorios) no puede tumbar el nodo; cualquier lectura buena resetea el contador. (En el **arranque** el pre-check usa 5 lecturas rápidas de 200ms como protección anti-brownout.)
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

## 6. Laboratorio: Banco de Pruebas Técnico

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

## 7. Protocolo de Rescate Remoto (Paso a Paso)

Si un nodo remoto de la malla sufre un fallo crítico y vuelve al estado de fábrica, mantendrá su identificador único de Meshtastic (`idxxxxx`) pero operará bajo los parámetros genéricos introducidos por defecto (EU868, ShortFast Narrow, clave pública de rescate preconfigurada).

> Debido al funcionamiento del cifrado y el intercambio de claves públicas/privadas en Meshtastic, es **mandatorio seguir este protocolo estrictamente** para evitar bloqueos de llaves criptográficas.

### Paso 1: Preparación del Nodo de Mando (Rescate)

Necesitas un nodo físico cualquiera que actúe como "mando a distancia" para conectarte al nodo caído. Puede ser cualquier hardware compatible con Meshtastic.

1. **Aislamiento inicial**: Asegúrate de que este nodo de mando esté completamente apagado o blindado en un entorno donde no pueda emitir ni recibir balizas de la malla antes de ser configurado.
2. **Nueva recomendación**: A veces el método tradicional no funciona bien, ya que por un bug de la app de Meshtastic falla la restauración de la copia de seguridad. Por lo tanto:
3. **Primero**, instala en tu smartphone la versión **2.7.10** de la app de Meshtastic, ya que esta permite modificar la clave privada del nodo (el `.apk` está incluido en el archivo general del firmware Navarrico).
4. Abre la app, conéctate a tu nodo de mando y asegúrate de que haya estado aislado; si no, dará conflicto de claves con el nodo que quieres controlar.
5. Ve a **Ajustes de Seguridad** de ese nodo y, en el campo "Clave Privada", borra el contenido y escribe exactamente:

```
cJzjBkBwWid26swcnuOJ9v8EQcWC5fyugDhZddtnu04=
```

Guarda los cambios.

6. Reinicia el nodo normalmente y comprueba que ha generado la clave pública:

```
x9wN6W0TuoY/gtVKM/+lysx8Rewb5CAdZ9YfzIVRAFU=
```

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

## 8. Administración Remota (Resumen)

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
- `/resilience.bin` (química de batería, voltajes, estado TX/BLE) se guarda en la raíz del disco y **sobrevive a los resets de fábrica**.

---

## 9. Changelog de Versiones

| Versión | Descripción |
| :--- | :--- |
| **1.0** | Versión inicial, aplicado en versiones de firmware comprobado su uso en varios Routers de la malla que llegaban a sufrir brownouts y con esta solución dejaron de darlos. |
| **2.0** | Implementados ajustes hardcodeados después de un reset de fábrica; el nodo es capaz de sobrevivir a un fallo grave de memoria que lo devuelve a su estado de fábrica; solucionado que el nodo persistiera los cambios del usuario sobre las claves de gestión remota tras un reset. |
| **3.0** | Solucionado bug que surgía cuando el nodo, o bien arrancaba o bien era reseteado a mano o por un ATTINY13a cuando estaba alimentado a un voltaje por debajo de lo programado para dormirse: se despertaba constantemente nada más dormirse, en bucle. |
| **4.0** | Añadida compilación para Xiao Kit i2c y soporte en todas las compilaciones para Spreading Factor 5 y 6 (nuevos presets de trabajo LoRa que se están implementando en Madrid). |
| **4.1** | Corregidas variantes Faketec: no se detectaba el INA219, solucionado; variante Xiao NRF52 KIT i2c, modificado bus de salida a radio (conmuta a GND para menor consumo en Deep Sleep). |
| **4.2** | Portado el firmware Navarrico a Meshtastic 2.7.26 Beta y añadido soporte para más placas. Añadida la **Rama 2** para routers de infraestructura con protección de la Flash y auto-favoriteo de routers descubiertos en contacto directo de radio. La Rama anterior pasa a denominarse **Rama 1**. |
| **4.3** | Cambio de denominación a **NavaTastic** e implementada la arquitectura de seguridad híbrida y comandos extendidos de administración remota en Rama 2: canal Navadmin (Canal 1, PSK 0x01) para administración en lote; módulo `NavaCLIModule` que intercepta `/nava` silenciosamente; direccionamiento por ID/rol/geográfico con jitter anticolisión; acceso crítico solo por DM PKI; salvaguarda que impide silenciar a admins; corregido DoS por DB llena (límite 80) y límite de 10 favoritos huérfanos; resuelta tormenta de broadcasts de NodeInfo en el arranque. |

| **4.4 (12/08/2026)** ⭐ **distribuido como "NavaTastic 4.3 Eclipse Edition"** (build 17:09-17:15, entregado a colegas para pruebas; referencia de regresión) | Rama 2: homogeneizado el canal Navadmin (slot 1) en las 12 variantes; fix H3 de administración remota (rotación de clave: el NodeInfo del mando acredita admin al instante, canal y DM); **`/nava fav auto [on|off]`** (control del auto-favoriteo de routers directos, persistido en `/resilience.bin`); respuestas fragmentadas por palabra/línea (nunca se parten comandos a la mitad); ayuda y consultas: cualquier comando sin argumento muestra estado y opciones, interrogación con `/nava <comando> ?` o `help`, y aviso de rollback (`nrf erase`) en comandos persistentes. || **4.2.1 / 4.4** | **Endurecimiento de la ronda de auditoría**: whitelist estricta en el canal Navadmin (solo lectura, no-admins en silencio); normalización a minúsculas antes del filtro; guardas de longitud en `substr()` (elimina crash); `help <comando>`; respuestas en español; `factory_reset` diferido; `ign add` seguro; rate-limit de no-admins por DM; `ble` real; `bat` honesto (sin sag falso); **storm real** con RTC2 y apagado de radio (`storm test1`/`test2`); Secuencia Remota 2 completa (`set_chem`, `set_vbat`, `set_vwake`, `txoff`/`txon`, `ble`, `rxlog`, `afc`, `reset_reason`, `trace`, `route`, `msg`, `bell`, `pos`, `nodeinfo`, `sendtel`, `admin_ls`, `power`, `noise`). |
| **4.5 (12/08/2026)** | **Rama 1 (Clientes)**: nueva rama para nodos de infraestructura que no son routers — aparecen en la malla como **CLIENT** (`set_role` y el rol por defecto). El rol es ahora **semi-permanente**: `/nava set_role [client/mute/router]` se guarda en `/resilience.bin` y sobrevive al factory reset (un cliente puede convertirse en router por radio y revertirse cuando quiera; cuidado: tras un rescate, un nodo convertido a router seguirá retransmitiendo hasta que se le devuelva a client). Resto de la rama idéntico a Rama 2 (misma administración `/nava`, misma protección de Flash y energía). |
