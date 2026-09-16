---
title: "Manual de Uso — NavaTastic Eclipse V5.2"
subtitle: "Desarrollo optimizado para infraestructura solar en ubicaciones de difícil acceso"
author: "Modificación inicial por JBAU92 · Desarrollo continuado por EA2OY"
date: "Agosto 2026"
colorlinks: true
toc: true
toc-title: "Índice"
---

# Manual de Uso — NavaTastic Eclipse V5.2 (v4.3.9)

> **ADENDA 25/08/2026 — NAVATASTIC V5 (v4.3.4)**: manual de usuario **VIGENTE**. Incluye sincronización
> bidireccional transparente con la App Oficial de Meshtastic en el respaldo interno del nodo,
> Hop-Aware Timing con jitter adaptativo, protocolo de evacuación simultánea de pánico,
> persistencia física de capa LoRa/frecuencia y Canal 0 Primario. Distribución en 12 variantes
> `navarrico_*` desde `distribucion\` y `Desktop\NavaTastic V5 4.3.4`.

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


### 📥 Elegir tu archivo de firmware (descargas)

Cada Release de GitHub incluye un archivo por placa y rol. Regla rápida:

- **Busca tu placa en el nombre**: `Promicro...`, `Faketec...`, `Seed.Solar.Node.P1...`,
  `Heltec.T114...`, `XiaoKitI2c...`, `XiaoKitI2c+E22P...`, `HeltecV3...` o `HeltecV4...`.
- **Elige el rol**: sufijo `R2IG` (o `r2ig` en Heltec) = **Repetidor fijo** (router de
  infraestructura); sufijo `R1IG` (o `r1ig`) = **Cliente convertible a Repetidor**.
- **Elige el formato**: `.uf2` = por cable USB · `.zip` = actualización OTA por Bluetooth. En las
  placas Heltec V3/V4 los ficheros son `.APP.bin` y `.FACTORY.bin` (ver la guía de flasheo del
  repositorio).
- **Batería**: todos los firmwares funcionan con **LiPo**. Si vas a usar batería **NiMH**, elige
  una placa **Faketec o Xiao Kit i2c** (compatibilidad declarada por el autor) y configura la
  química con `/nava set_chem`. El mismo archivo sirve para ambas químicas.

**Estado de pruebas**: verificado en banco en **Faketec, Promicro NRF52+E22P, Xiao Kit i2c y Xiao
Kit i2c+E22P**; en pruebas de campo en **Seed Solar P1, Heltec T114 y Heltec V3/V4**.

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
  > **TIP — divisor distinto**: si tu placa lleva un divisor con otros valores, se puede adaptar el firmware a esa medida antes de compilarlo (el ajuste vive en el archivo de la placa, dentro de la carpeta `variants`). **AVISO importante**: ese mismo divisor alimenta el comparador de bajo consumo que decide el **despertar del modo de resiliencia por batería baja** (`set_vwake`, niveles 1-5). Los niveles de despertar están calibrados para divisor 2.0 (1M+1M): con otro divisor, el nodo despertará a una tensión distinta de la indicada, así que hay que recalibrar ese umbral (o dejar el umbral fijo de fábrica).
- **Líneas de bus optimizadas (v4.1 Xiao Kit i2c)**: Se ha modificado el bus de salida a la radio para que conmute a **GND** durante el estado de Deep Sleep, reduciendo drásticamente cualquier fuga residual de corriente.

---

## 3. Configuración del Nodo (Firmware Hardcoded)

El firmware NavaTastic viene **hardcodeado** para la red SFNarrow (preset LoRa de uso nacional en España): región EU868, preset ShortFast Narrow, canal SFNarrow y canal Navadmin, potencia y umbrales de batería ya configurados por variante. **No requiere configuración del usuario** para funcionar en la malla.

La única intervención opcional del operador es:

- **Ajustar el nombre** del nodo (para identificarlo en la malla).
- **Activar la lectura de sensores de telemetría** de energía (INA219) y de clima (BMP280 + AHT20 / BME680), si la placa los incorpora.

**Conexión Bluetooth**: el nodo emite con **PIN fijo `654321`** (modo FIXED_PIN de Meshtastic) — la app lo pide al emparejar. (Los builds Propia usan un PIN propio del operador.)

Todo lo demás (canales, región, administración remota, protección de batería) está preconfigurado en compilación.

> **⚙️ Despliegue (nodos nuevos o reflasheados)**: desde la V5 **no hace falta ningún reset de
> fábrica** — al primer arranque el firmware se despliega solo: materializa el canal de
> administración (Navadmin, slot 1), aplica las buenas prácticas y **respeta las claves del
> dueño** si el nodo ya tenía alguna. Solo espera un minuto tras flashear.

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

## 6. Sincronización Bidireccional y Coexistencia con la App Oficial de Meshtastic

A partir de **NavaTastic V5**, la interacción entre la App Oficial de Meshtastic (Android / iOS) y el respaldo interno del nodo es **completamente transparente y bidireccional**. Ya no existen restricciones que obliguen a usar exclusivamente comandos `/nava` para las configuraciones cotidianas:

| Parámetro / Interruptor | Comportamiento en App Oficial | Comportamiento en NavaTastic V5 | Razón de Seguridad y Resiliencia |
| :--- | :--- | :--- | :--- |
| **Nombre del Nodo (App vs NavaCLI)** | En la App cambia el nombre en la memoria del nodo. | Con `/nava set_name` se fija **a fuego en el respaldo interno** y sobrevive a cualquier reset de fábrica. **Desde V5.1**: si el nombre no está fijado, el nombre cambiado en la App **también se recuerda** (sobrevive a reinicios y restablecimientos de fábrica). | **Identidad Fiable**: fija el nombre por radio con `set_name` (manda hasta `flush`) o déjalo en modo App — nunca se pierde el nombre del repetidor. |
| **Rol del Dispositivo** | Al cambiar el rol en la App se guarda en el nodo. | Se sincroniza en el respaldo interno, actualiza la identidad pública y sobrevive a resets. | **Supervivencia de Malla**: Evita que un router vuelva a cliente tras una tormenta eléctrica. |
| **OK to MQTT** | Se conmuta en *Settings -> LoRa*. | Sincronizado automáticamente hacia el respaldo interno. | **Transparencia Total**: El estado elegido en la App se conserva permanentemente. |
| **Intervalos de Telemetría, NodeInfo y Posición** | Se modifican desde los menús de la App. | Sincronizados en el respaldo interno (sin escrituras parásitas si no cambia). | **Cero Desgaste Flash**: El nodo apenas escribe en su memoria de trabajo. |
| **Posición Fija y Coordenadas GPS** | Se fijan en *Device -> Location*. | Se guardan en el respaldo interno y se dispara baliza inmediata a la red y mapas. | **Refresco Instantáneo**: Las pasarelas y MeshMap reciben la posición sin demoras. |
| **Canales 0 al 7** | Se crean o editan en la pestaña de canales. | Se respaldan (incluyendo Canal 0 Primario y secundarios 2-7). | **Persistencia de Canales**: Los canales configurados en la App sobreviven a reinicios. |
| **Preset LoRa y Frecuencia** | Se ajustan en *LoRa Config*. | Se validan y se guardan en el bloque de radio física. | **Reconfiguración Blindada**: Evita desconfiguraciones accidentales de frecuencia. |
| **PIN Bluetooth (BLE)** | Se define en *Bluetooth Config*. | Se sincroniza en el respaldo interno para emparejamientos fijos. | **Acceso Ininterrumpido**: El PIN configurado persiste tras caídas de energía. |
| **Canal 1 (Navadmin)** | Protegido contra borrado en el nodo. | Inamovible con la clave pública del canal de rescate. | **Canal de Rescate Vital**: Garantiza que el repetidor siempre pueda ser tele-diagnosticado. |

---


### 🚫 NO uses "Restaurar copia de seguridad" de la App de Meshtastic

Los nodos NavaTastic se blindan solos: su respaldo interno guarda claves, rol,
canales y radio, y **sobrevive a los reinicios y a los restablecimientos de fábrica**. Ese respaldo
**es** la red de seguridad del nodo. La función de copia de seguridad de la App oficial es **otra
cosa distinta** y, en la práctica, **se ha llevado nodos por delante**: ha obligado a **subir a la
montaña** a repararlos a mano más de una vez, en varias versiones de la App.

**Qué pasa exactamente** (verificado en el firmware, 12/09/2026):

- La App puede pedir al nodo *"restaurar preferencias"* desde su copia. El firmware **acepta y
  sobrescribe toda la configuración** con la del fichero de copia (rol incluido), y reinicia.
- Pero **ese camino no sincroniza el respaldo interno del nodo**, que se aplica
  **después**, al arrancar. Resultado: **la restauración se deshace sola**. El rol vuelve al que
  había antes, el nodo queda con el fichero de configuración y el funcionamiento en desacuerdo, y
  desde la App parece que "no ha hecho nada" o que "está raro".
- Con **otras configuraciones de la copia** (canales, radio, claves) el desajuste puede dejarte un
  nodo que responde mal o **no responde**, y sin acceso por radio toca subir físicamente.

**Qué hacer en su lugar**:

| En vez de… | Usa… |
|---|---|
| Restaurar una copia de seguridad desde la App | **`/nava`**: no hace falta. El nodo ya recupera solo sus ajustes críticos tras un reset |
| Copiar la configuración a otro nodo | Configurar el nodo nuevo con su perfil, y ajustar por `/nava` o desde la App **sin** restaurar copias |
| Guardar una copia "por si acaso" | El respaldo interno del nodo **ya es** esa copia, y es la que sobrevive |

> **Es diseño del firmware, no un fallo a corregir**: el respaldo interno del nodo tiene prioridad
> sobre la configuración restaurada, a propósito, porque es lo que mantiene un repetidor de montaña
> en pie tras una caída de energía. Se documenta para que no se use, no para cambiarlo.

---

### 🔐 Haz tuyo el nodo: pon TU clave de administración

* El nodo trae de fábrica **una clave de administración de emergencia** (la del proyecto). Es una
  red de seguridad: sirve para poder recuperar por radio un nodo que se haya quedado sin dueño tras
  un fallo grave.
* Si el nodo es tuyo, **pon tu propia clave de administración**: mientras esté en el nodo, **la de
  emergencia no autoriza nada** (queda desplazada, y sigue así después de un restablecimiento de
  fábrica).
* La clave de emergencia **solo se reinyecta sola** en un caso: si el nodo sufre un **fallo
  catastrófico de memoria o un restablecimiento total** y se queda **sin ninguna clave tuya**.
* **Cómo ponerla**: si compilas tu firmware, edítala en tu perfil de compilación
  (`profiles/<RAMA>_<Placa>.jsonc`, clave `USERPREFS_USE_ADMIN_KEY_0`, en hexadecimal) antes de
  compilar (ver `Compilar_NavaTastic.md` del repositorio). En un nodo ya desplegado, añade la
  **clave pública de tu mando** en el lugar de la de emergencia desde tu cliente de administración
  y verifica con `/nava admin_ls` qué claves mandan.

---

### 🔑 Quién manda con las claves: la regla, en una frase

> **Si el nodo tiene alguna clave tuya, manda lo que tú digas. La copia interna solo entra cuando el
> nodo se queda SIN ninguna clave.**

El nodo guarda una **copia interna** de tus claves de administración que sobrevive a los reinicios y
a los restablecimientos de fábrica. Esa copia es la que hace que un reset indeseado **no te deje
fuera**. Pero es importante entender **cuándo** actúa:

| Lo que haces | Qué pasa |
|---|---|
| **Añades** una clave nueva desde el móvil | Se guarda en la copia interna. Sigue mandando la App |
| **Cambias** una clave por otra | Igual: se guarda y manda la App |
| **Borras** una clave | **Funciona**: el nodo te hace caso y no la recupera |
| **Borras TODAS** tus claves | El nodo no te deja sin acceso: repone la **clave de emergencia del proyecto**. **No hay aviso** (es deliberado): compruébalo tú cuando quieras con `/nava admin_ls` |
| **Reset de fábrica** (o flasheo) | El nodo se queda vacío → **repone tus claves desde la copia interna** |
| **`/nava keys_clear`** | Borra **la copia interna**, no tu configuración actual. Sirve para purgar el respaldo a propósito |
| **`/nava wipe`** o **`nrf erase`** | Borra todo, incluida la copia. Es un borrado total, sin vuelta atrás |

### 🧪 Casos para probar en banco (cambio de regla del 15/09/2026)

> **NUNCA probar esto con un nodo remoto.** Un fallo aquí deja el nodo **sin acceso por radio**.
> Hacerlo con **dos nodos delante**, uno de ellos actuando de mando.

| # | Qué preparar | Qué hacer | Qué debe pasar |
|---|---|---|---|
| 1 | Nodo vacío (recién flasheado) | Arrancar | Tiene la clave de emergencia. `admin_ls` la muestra |
| 2 | Nodo con **una** clave tuya | Reiniciar | La clave sigue. **No** aparece la de emergencia en el slot 0 |
| 3 | Nodo con tu clave | **Borrarla** desde la App → reiniciar | **Sigue borrada** (antes volvía) |
| 4 | Nodo con tu clave | **Cambiarla** por otra desde la App → reiniciar | Manda la nueva (antes volvía la vieja) |
| 5 | Nodo con **dos** claves tuyas | Borrar una → reiniciar | Queda la otra. **No** se duplica ninguna |
| 6 | Nodo con tu clave | **Borrar todas** → reiniciar | Se repone la **de emergencia**. **Sin aviso** (deliberado): confirmar con `/nava admin_ls` |
| 7 | Nodo con tu clave | `/nava keys_clear` → reiniciar | Tu clave **sigue mandando**; el respaldo queda vacío |
| 8 | Nodo con tu clave | `/nava keys_clear` + borrar todas → reiniciar | Se repone la **de emergencia** (no hay nada que rescatar) |
| 9 | Nodo con tu clave | **Reset de fábrica** → reiniciar | **Vuelven tus claves** (la red de seguridad) |
| 10 | Nodo con tu clave | **Flashear de nuevo** → arrancar | Igual que el 9 |
| 11 | Copia **vieja** del móvil con claves antiguas | Restaurarla → reiniciar | ⚠️ **Manda la copia vieja** (el respaldo ya no la corrige). Se arregla volviendo a poner tu clave desde la App |

**El caso 11 es el precio de la regla**: se gana que borrar funcione de verdad, se pierde que el
nodo corrija solo una restauración de copia antigua. Se nota enseguida (el nodo no te obedece) y
tiene arreglo manual.


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
- El respaldo interno del nodo (química de batería, voltajes, estado TX/BLE, **claves admin del usuario**) se guarda aparte de la configuración normal y **sobrevive a los resets de fábrica**.
- **Claves admin y resets**: las claves admin PÚBLICAS del usuario se guardan en el respaldo interno y vuelven tras un factory/full reset (el slot 0 conserva la clave previa del usuario). **Quitar una clave en la app NO la borra del nodo** — la copia respaldada reaparecerá tras el próximo reset; para purgar de verdad: `/nava keys_clear` (borra solo la copia respaldada, no la configuración actual) o `/nava wipe` (purga total). Tras `wipe`/`nrf erase` queda solo la clave de rescate del proyecto.

---

## 10. Cómo cambiar la frecuencia de toda la malla (Botón del Pánico V5)

El **Botón del Pánico** permite a un administrador mover todos los repetidores de una cordillera a una nueva frecuencia o velocidad LoRa a la vez, sin tener que desplazarse físicamente a las cumbres a reprogramarlos.

### 10.1 ¿Cuándo se utiliza?
- Si la frecuencia habitual tiene muchas interferencias o está saturada de tráfico.
- Si el grupo o la comunidad decide cambiar la velocidad de la red (ej. de *ShortFast Narrow* a *MediumFast*).
- Si necesitas evacuar temporalmente la flota a un canal seguro.

> 🛡️ **Seguridad (4.3.5)**: `panic` y `panic_ok` **solo funcionan por DM cifrado (PKI)** o por el **canal privado de flota** (Slots 2-7 con clave propia). Están **bloqueados en el canal público Navadmin** — solo quien posee la clave del canal privado o la clave privada de admin puede disparar una evacuación. Los pulsos de propagación viajan cifrados por ese canal privado (los nodos sin la clave no pueden ni leerlos ni falsificarlos).

### 10.2 Pasos para realizar la migración

1. **Lanzar la orden**: Envías el comando indicando la frecuencia o preset destino, los minutos de aviso previo y los minutos de prueba:  
   `/nava panic <preset> [minutos_aviso] [minutos_prueba]`  
   - *Ejemplo con red de seguridad (Recomendado)*:  
     `/nava panic medium_fast 15 120`  
     *(Avisa durante 15 minutos por toda la montaña, cambia a MediumFast y te da 2 horas para probarlo)*.
   - *Ejemplo de retorno definitivo a SFNarrow*:  
     `/nava panic sfnarrow 10 0`  
     *(Avisa durante 10 minutos y se queda fijado para siempre en SFNarrow)*.
2. **Qué ocurre durante el aviso**: Los repetidores se reenvían el aviso entre ellos. Durante el último minuto antes del cambio, la red se silencia para que todos los nodos salten a la vez limpiamente y sin colisiones.
3. **El cambio**: En el minuto pactado, todos los repetidores se reinician y arrancan en la nueva frecuencia.
4. **Confirmar que todo va bien**:
   - Cambias tu teléfono o nodo de control a la nueva frecuencia.
   - Mandas el comando **`/nava panic_ok`** por el canal de administración.
   - Todos los repetidores reciben tu confirmación y se quedan fijos en el nuevo preset para siempre.

### 10.3 ⚠️ ¿Qué pasa si algo sale mal? (Auto-Rescate de seguridad)
- **Si el nuevo preset no tiene cobertura o te equivocas**: No te preocupes. Si pasan los minutos de prueba (ej. 2 horas) sin que mandes `/nava panic_ok`, **los repetidores cancelan el cambio y vuelven solos automáticamente a la frecuencia estándar de fábrica (SFNarrow)**. ¡Nunca perderás el control de un nodo en la montaña!
- **Si un nodo se apaga o reinicia durante la prueba**: Al arrancar seguirá en la nueva frecuencia y reiniciará su tiempo de espera, dándote margen suficiente para mandar el `panic_ok`.

---

## 11. Changelog de Versiones

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
| **4.3.3 — "NavaTastic V4"** (17/08/2026) | **Gestión remota avanzada de canales e infraestructura**: crear, borrar, listar y compartir canales secundarios con enlace QR (`ch_set`, `ch_del`, `ch_ls`, `ch_url`, `ch_reset`); redirigir la consola `/nava` a un canal privado (`set_cli_chan`) y silenciar el canal público (`navadmin_mute`); control MQTT por canal (`ch_mqtt`), coordenadas fijas (`set_pos`), baliza ajustable (`set_beacon`, **retirado el 15/09/2026**: usar `set_nodeinfo_tx` y `set_pos_tx`), PIN Bluetooth (`set_pin`), modo silencioso temporal (`mute`) y diagnósticos en memoria (`stats`, `log`, `test_tx`). El respaldo interno pasa a escribirse de forma segura (no se estropea si se corta la alimentación a mitad). |
| **4.3.4 — "NavaTastic V5"** (25/08/2026) | **Sincronización transparente con la App oficial**: los 12 ajustes cotidianos (rol, MQTT, tiempos de telemetría/presencia/posición, posición fija, canales, radio, PIN, lista de bloqueados, claves) se sincronizan solos entre la App y el nodo, en ambos sentidos. **Respuestas adaptadas a la distancia** (más espera cuantos más saltos) y traceroute desacoplado (8 s). Persistencia de la configuración de radio y del canal principal (`set_preset`, `set_lora`, `set_freq`, `ch_set 0`). Nombre fijado a fuego (`set_name` y `flush`). Telemetría por defecto cada 12 h. **Botón del Pánico** para evacuar toda la malla a otra frecuencia. Corregidos 4 desajustes de sincronización (rol, nombre, posición y mensajes directos). Auto-favoritos ampliados a 32 nodos. |
| **4.3.5 — "NavaTastic V5.2"** (26/08/2026) | **Endurecimiento de seguridad**: el canal público pasa a ser **solo lectura y solo para administradores verificados** (silencio total ante desconocidos; configuración y pánico bloqueados en él). El **título de administrador solo se concede con el primer mensaje cifrado descifrado** (un anuncio público ya no basta). El **Botón del Pánico** solo se dispara por mensaje cifrado o por el canal privado de la flota: los pulsos de propagación viajan cifrados y solo la flota puede emitirlos o falsificarlos. **Respaldo de claves sagrado**: borrar una clave en la app ya no la purga (la purga real es `/nava keys_clear` o `/nava wipe`) y el nodo **nunca se queda sin administrador**. `storm`/`mute` con ventana de gracia de 60 s. Confirmación `CONFIRM` sin distinguir mayúsculas. Respaldo interno escrito de forma segura (comprobación de integridad y acceso protegido). |
| **4.3.6 — "NavaTastic V5.2 (auditoría funcional)"** (27/08/2026) | Corregidos fallos de la versión anterior: la **posición fija** ahora sí sobrevive a los resets de fábrica; el **botón del pánico** vuelve siempre a la frecuencia oficial de España (869.618 MHz); el canal público ya no se conecta a internet por error tras restablecer los canales. **Novedad: puedes poner un tiempo distinto** para cada tipo de telemetría (batería, clima, energía, aire, salud) desde la App; el comando `/nava set_telem_tx` sigue cambiándolos todos a la vez. ⚠️ **Al instalar esta versión** el respaldo interno se regenera limpio por seguridad: hay que volver a configurar la química de batería, la lista negra y los auto-favoritos (las claves, canales y rol se recuperan solos; la telemetría vuelve a 12 h). |
| **4.3.7 — "NavaTastic Eclipse V5 (revisión 28/08)"** | **Tú mandas**: todo lo que configures (rol, modo de retransmisión, tiempos de posición/presencia/telemetría — incluido apagarlos) se guarda y se mantiene tras reinicios y resets. **Instalación sin complicaciones**: al instalar sobre cualquier firmware oficial, el nodo se configura solo con los valores recomendados (aviso de posición y presencia cada 72 h, sensores cada 12 h) y **respeta tus claves de administración** si ya tenías alguna. **Nuevo comando** `/nava set_rebroadcast` para elegir cómo retransmite el nodo los mensajes ajenos. Al actualizar desde la versión anterior **no pierdes nada** (migración automática). |
| **4.3.8 — "NavaTastic Eclipse V5.1"** (09/09/2026) | **El nombre de tu nodo ya no se pierde**: si lo cambias con la App oficial de Meshtastic, el nodo lo recuerda (incluso tras un restablecimiento de fábrica). Si lo fijaste con `/nava set_name`, ese sigue mandando hasta que hagas `flush`. Al instalar sobre otro firmware (oficial o NavaTastic anterior), tu nombre se conserva. **Nuevo contador de salud**: el nodo cuenta los restablecimientos de fábrica que ha sufrido y lo muestra en `/nava status` (y en `reset_reason`) como `FR` — solo si es mayor que cero —, muy útil para saber si un repetidor se restableció sin que tú lo ordenaras. **El trazado de rutas ya devuelve el resultado**: `/nava trace !ID` te responde con la ruta completa (nodos recorridos y señal de cada tramo, en ida y vuelta) por el mismo canal por el que preguntaste, y avisa si el destino no contesta. **Más robustez**: si la App pone un rol avanzado que el nodo no gestiona, este ya no se restablece solo (el ajuste se conserva con seguridad). El aviso de arranque ([Boot]) ahora llega **a los 3 minutos** (antes 2), con más margen para que la malla esté asentada. |
| **4.3.9 — "NavaTastic Eclipse V5.2"** (15/09/2026) | **Corrección sobre el fichero de resiliencia**, el respaldo interno donde el nodo guarda sus ajustes: a partir de ahora, cada vez que el nodo escribe ahí, **comprueba que lo guardado ha quedado bien antes de darlo por válido**, de modo que el respaldo no se queda a medias. **Lo que cambias en la pantalla del nodo, ahora sí se queda**: el rol, el preset de radio, el canal y la cadencia de posición que elijas en el menú de la pantalla ya no se revierten al reiniciar (antes parecía que "no hacía nada": el nodo reiniciaba y volvía al valor viejo). **Apagar avisos ya es posible**: si pones el aviso de presencia (NodeInfo) en `off`, el nodo lo respeta — antes lo volvía a encender solo con el mínimo de 1 hora. **Las claves de administrador se pueden retirar de verdad**: si quitas una clave (en la App), ese nodo deja de poder mandar órdenes inmediatamente; antes conservaba el permiso para siempre. **Órdenes a prueba de interrupciones**: si mandas un reinicio, un cambio de radio o un borrado y el nodo se queda sin luz justo después, la orden **no se pierde**: se recupera y se ejecuta al volver, y el nodo **avisa por radio** de que la está recuperando. **Arreglo importante para las placas Heltec**: ya no se duermen de forma que no pudieran despertar. **Y una corrección de batería**: el modo de química de sodio quedaba configurado de forma que el nodo no podía volver a arrancar; corregido. |
| **V5 (revisión 29/08)** | **El Botón del Pánico, redondo**: el mensaje de confirmación (`/nava panic_ok`) ya llega durante la prueba y **consolida el cambio** en toda la flota; si un repetidor se reinicia durante el aviso, **se reincorpora a la evacuación** en vez de quedarse atrás; corregido el salto a LONG_FAST. **Los cambios de preset escriben todos los ajustes de una vez** (se acabaron los parámetros "a medias" que rompían el enlace). Tras una vuelta atrás automática, **el modo de retransmisión vuelve al valor recomendado**. Los **nombres de canal respetan mayúsculas y minúsculas**. **Nuevas placas: Heltec V3 y Heltec V4** (ESP32-S3). El firmware ocupa menos memoria (más margen de seguridad). |


---

# NavaTastic User Manual — V5.2 (v4.3.9) (English)

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


### 📥 Choosing your firmware file (downloads)

Every GitHub Release contains one file per board and role. Quick rule:

- **Look for your board in the name**: `Promicro...`, `Faketec...`, `Seed.Solar.Node.P1...`,
  `Heltec.T114...`, `XiaoKitI2c...`, `XiaoKitI2c+E22P...`, `HeltecV3...` or `HeltecV4...`.
- **Choose the role**: suffix `R2IG` (or `r2ig` on Heltec) = **Fixed Repeater** (infrastructure
  router); suffix `R1IG` (or `r1ig`) = **Client convertible to Repeater**.
- **Choose the format**: `.uf2` = USB cable · `.zip` = OTA update over Bluetooth. On Heltec V3/V4
  boards the files are `.APP.bin` and `.FACTORY.bin` (see the flashing guide in the repository).
- **Battery**: every firmware works with **LiPo**. If you are going to use **NiMH** batteries,
  choose a **Faketec or Xiao Kit i2c** board (compatibility declared by the author) and set the
  chemistry with `/nava set_chem`. The same file serves both chemistries.

**Bench-test status**: verified on the bench on **Faketec, Promicro NRF52+E22P, Xiao Kit i2c and
Xiao Kit i2c+E22P**; field-testing on **Seed Solar P1, Heltec T114 and Heltec V3/V4**.

## 2. Critical hardware and wiring requirements


- **E22P radio wiring rule**: follow the standard E22 wiring. E22P modules switch TX/RX
  automatically, so **GPIO 017** no longer toggles TX/RX — it acts as the **radio power switch**:
  HIGH wakes the module, GND sleeps it.
- **Dedicated PCB compatibility**: on compatible carrier boards (e.g. **Albatastic**, **Xiaowa**)
  the radio selector pad must be soldered to **E22**.
- **ADC divider 2.0**: the battery voltage divider must use **two 1 MΩ resistors**
  (NRF52/Faketec/Albatastic/XiaoWa).
  > **TIP — different divider**: if your board uses a divider with other values, the firmware can be
  > adapted to that measurement before compiling (the setting lives in the board file, inside the
  > `variants` folder). **Important**: the same divider feeds the low-power comparator that decides
  > the **low-battery resilience wake-up** (`set_vwake`, levels 1-5) — levels are calibrated for 2.0;
  > with another divider that threshold must be recalibrated (or leave the factory fixed threshold).
- **Optimized bus lines (v4.1 Xiao Kit i2c)**: the radio output bus switches to **GND** during
  Deep Sleep to reduce leakage current.

## 3. Node configuration (hardcoded firmware)

The firmware comes **hardcoded** for the SFNarrow network (national LoRa preset in Spain): EU868
region, ShortFast Narrow preset, SFNarrow channel and Navadmin channel, power and battery
thresholds per variant. **No user configuration is required** to operate on the mesh. Optional
operator steps: set the node **name**, enable telemetry sensors (INA219, BMP280 + AHT20 / BME680).
**Bluetooth**: fixed PIN **`654321`** (FIXED_PIN mode; the app asks when pairing). Everything
else (channels, region, remote administration, battery protection) is preconfigured at build time.

> **⚙️ Deployment (new or reflashed nodes)**: since V5 **no factory reset is needed** — on first
> boot the firmware deploys itself: it materializes the Navadmin channel (slot 1), applies the
> recommended settings and **respects the owner's keys** if the node already had any. Just wait a
> minute after flashing.
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

NavaTastic incorporates an internal backup engine designed to ensure the survival of solar mountain repeaters. This results in intentional behaviors that differ from standard Meshtastic firmware:

| Feature / Setting | Official App Behavior | NavaTastic Behavior | Safety & Resilience Rationale | Canonical NavaTastic Solution |
| :--- | :--- | :--- | :--- | :--- |
| **Bluetooth Switch (BLE)** | Disabling BLE saves `false` to flash. | Upon reboot, firmware **re-enables BLE** if `/nava` was not used. | **Anti-Orphan Protection**: Prevents accidental mobile taps from leaving a mountain repeater unreachable. | To disable permanently: send DM `/nava ble off` (writes `prefs.ble_disabled = 1`). |
| **Channel 1 (Navadmin)** | App allows renaming, editing or deleting. | **Locked and protected**: Rejects deletion and restores PSK `AQ==`. | **Vital Rescue Backbone**: Guarantees telemetry, solar notices and remote rescue availability. | To mute in open air: `/nava navadmin_mute on` or relocate CLI with `/nava set_cli_chan <2-7>`. |
| **Admin Keys on Reset** | A Factory Reset wipes all admin keys. | **Cryptographic Persistence**: User & rescue admin keys restore automatically. | **Safe Remote Maintenance**: Prevents losing administrative access after a config reset. | Inspect with `/nava admin_ls` or purge persisted copy with `/nava keys_clear`. |
| **Hardware Role** | A reset reverts to the compile-time binary role. | **Semi-Permanent Role**: Reconfigured roles persist across factory resets. | **Mesh Backbone Survival**: Prevents a converted router from reverting to client after a lightning reset. | Switch roles with `/nava set_role router` or `/nava set_role client`. |
| **Node Discovery Database** | Official app expects nodes stored in flash. | **100% RAM-Only**: Transit nodes live in RAM and never write to flash. | **Zero Flash Wear**: Multiplies hardware lifespan tenfold by preventing flash memory burnout. | Manual and auto favorites are backed up in the node's internal backup. |
| **Router Beacon Interval** | Usually broadcasts every 15 to 30 mins. | Set to **72 hours** by default on infrastructure routers. | **LoRa Airtime Throttling**: Keeps mesh channels clean of unnecessary position spam. | Adjust fleet intervals with `/nava set_pos_tx` and `/nava set_nodeinfo_tx`. |


### 🚫 Do NOT use the Meshtastic App's "Restore backup"

NavaTastic nodes protect themselves: their internal backup holds keys, role,
channels and radio settings, and **survives reboots and factory resets**. That backup **is** the
node's safety net. The official App's backup feature is **something else entirely** and, in
practice, **has taken nodes down**: it has forced a **trip up the mountain** to repair them by hand
more than once, across several App versions.

**What actually happens** (verified in the firmware, 2026-09-12):

- The App can ask the node to *"restore preferences"* from its backup. The firmware **accepts it and
  overwrites the whole configuration** with the file's contents (role included), then reboots.
- But **that path does not sync the node's internal backup**, which is applied
  **afterwards**, at boot. Result: **the restore undoes itself**. The role reverts to the previous
  one, the config file and the running behaviour end up disagreeing, and from the App it looks like
  "nothing happened" or "it's behaving oddly".
- With **other settings from the backup** (channels, radio, keys) the mismatch can leave you with a
  node that answers badly or **does not answer**, and with no radio access you have to go there
  physically.

**What to do instead**:

| Instead of… | Use… |
|---|---|
| Restoring a backup from the App | **`/nava`**: you don't need it. The node already recovers its critical settings by itself after a reset |
| Copying the configuration to another node | Configure the new node with its profile, and adjust via `/nava` or the App **without** restoring backups |
| Saving a backup "just in case" | The node's internal backup **already is** that copy, and it is the one that survives |

> **This is firmware design, not a bug to fix**: the node's internal backup takes priority over a
> restored configuration, on purpose, because that is what keeps a mountain repeater standing after a
> power loss. It is documented so it is not used, not so it gets changed.

---

### 🔐 Make the node yours: set YOUR admin key

* The node ships with **one emergency administration key** (the project's). It is a safety net: it
  exists so a node left without an owner after a severe fault can still be recovered over the air.
* If the node is yours, **set your own administration key**: while it is on the node, **the
  emergency key authorizes nothing** (it is displaced, and stays displaced after a factory reset).
* The emergency key **only reappears by itself** in one case: if the node suffers a **catastrophic
  memory fault or a total reset** and is left **without any of your keys**.
* **How to set it**: if you build your own firmware, set it in your build profile
  (`profiles/<BRANCH>_<Board>.jsonc`, key `USERPREFS_USE_ADMIN_KEY_0`, in hexadecimal) before
  building (see `Compilar_NavaTastic.md` in the repository). On an already-deployed node, add
  **your controller's public key** in place of the emergency one from your admin client and check
  with `/nava admin_ls` which keys are in charge.

---

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
- The node's internal backup (chemistry, voltages, TX/BLE state, **user admin keys**) lives apart from the normal configuration and **survives
  factory resets**.
- **Admin keys and resets**: the user's PUBLIC admin keys are stored in the internal backup
  and return after a factory/full reset (slot 0 keeps the user's previous key).
  **Removing a key in the app does NOT purge it from the node** — the backed-up copy reappears
  after the next reset; to truly purge: `/nava keys_clear` (clears only the backed-up copy, not
  the current config) or `/nava wipe` (total purge). After `wipe`/`nrf erase` only the project
  key remains (guaranteed rescue channel).

## 10. How to change the frequency of the whole mesh (Panic Button V5)

The **Panic Button** allows an administrator to move all mountain repeaters to a new frequency or LoRa speed at once, without climbing any summits to reprogram them physically.

### 10.1 When to use it
- If the current frequency is jammed, noisy, or overloaded with traffic.
- If the community decides to change fleet speed (e.g. from *ShortFast Narrow* to *MediumFast*).
- If you need to temporarily move your repeaters to a clean channel.

> 🛡️ **Security (4.3.5)**: `panic` and `panic_ok` **only work via encrypted DM (PKI)** or through the **private fleet channel** (Slots 2-7 with its own key). They are **blocked on the public Navadmin channel** — only the holder of the private channel key or the admin private key can trigger an evacuation. Propagation pulses travel encrypted on that private channel (nodes without the key can neither read nor forge them).

### 10.2 Step-by-step procedure

1. **Send the command**: Specify the target preset, warning minutes, and test/trial minutes:  
   `/nava panic <preset> [warning_mins] [test_mins]`  
   - *Example with safety net (Recommended)*:  
     `/nava panic medium_fast 15 120`  
     *(Warns across the mountain for 15 minutes, switches to MediumFast, and gives you 2 hours to test it)*.  
   - *Example permanent return to SFNarrow*:  
     `/nava panic sfnarrow 10 0`  
     *(Warns for 10 minutes and stays permanently on SFNarrow)*.
2. **What happens during the warning**: Repeaters relay the warning to each other. During the last minute before the jump, the mesh silences itself so that all nodes switch cleanly without collisions.
3. **The jump**: At the scheduled minute, all repeaters reboot and start operating on the new frequency.
4. **Confirming that everything works**:
   - Switch your phone or control device to the new frequency.
   - Send the command **`/nava panic_ok`** on the admin channel.
   - All repeaters receive your confirmation and lock in the new preset permanently.

### 10.3 ⚠️ What happens if something goes wrong? (Automatic Self-Rescue)
- **If the new preset has no coverage or you made a mistake**: Don't worry. If the test period expires (e.g. 2 hours) without receiving `/nava panic_ok`, **the repeaters will cancel the change and automatically revert on their own to the default factory rescue channel (SFNarrow)**. You will never lose control of a mountain repeater!
- **If a node loses power or resets during the test**: It will reboot into the new frequency and reset its trial timer, giving you plenty of time to send `panic_ok`.

---

## 11. Version changelog

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
| **4.3.3 — "NavaTastic F21 / V4"** (17/08/2026) | **Advanced Remote Channels & Infrastructure Management (F21)**: Full secondary channel management (`ch_set`, `ch_del`, `ch_ls`, `ch_url`, `ch_reset`), CLI listener and notice redirection (`set_cli_chan`, `navadmin_mute`), MQTT controls (`ch_mqtt`, `set_ok_to_mqtt`), static coordinates (`set_pos`), adjustable beacon interval (`set_beacon`, **removed 15/09/2026**: use `set_nodeinfo_tx` and `set_pos_tx`), Bluetooth PIN (`set_pin`), temporary RF mute (`mute`), RAM-only diagnostics and logs (`stats`, `log`, `test_tx`). Safe internal-backup writes, so the node's saved settings survive power interruptions. |
| **4.3.4 — "NavaTastic V5"** (25/08/2026) | **App Bidirectional Sync, hop-aware timing & resilience**: continuous two-way synchronization of 12 everyday settings with the node's internal backup. Hop-Aware adaptive timing (300ms to 3.5s) and decoupled traceroute (8s). LoRa PHY and Primary Channel 0 persistence (`set_preset`, `set_lora`, `set_freq`, `ch_set 0`). Hardcoded persistent node naming in flash (`set_name` and `flush`). Default 12-hour telemetry cadence (43200s). Panic Button Protocol (`panic`, `panic_ok`). 100% resolution of the 4 desync bugs (Role, Name, Position, DM). Auto-favorites capacity expanded to 32 direct nodes. |
| **4.3.5 — "NavaTastic V5.2"** (26/08/2026) | **Security hardening (Audit)**: The Navadmin channel is now **read-only and only for verified administrators** (total silence for non-admins; `set_lora`/`set_freq`/`set_preset`/`panic`/`panic_ok` blocked there). **Admin status is only granted with the first successfully decrypted PKI DM** (a NodeInfo no longer suffices). The **Panic Button** is triggered exclusively by encrypted DM or through the private fleet channel (Slots 2-7 with its own key): propagation pulses travel encrypted on that channel and only the fleet can emit or forge them. **Sacred key backup**: deleting a key in the app no longer purges the node's internal backup (real revocation only with `keys_clear`/`wipe`), and the node **never ends up without an administrator** (automatic rescue-key re-injection). `storm`/`mute` now have a 60-second grace window. Case-insensitive `CONFIRM`. Safe internal-backup writes protected against corruption. |
| **4.3.6 — "NavaTastic V5.2 (functional audit)"** (27/08/2026) | Fixed issues from the previous version: **fixed position** now truly survives factory resets; the **Panic Button** always returns to the official Spanish frequency (869.618 MHz); the public channel no longer opens an internet gateway by mistake after restoring channels. **New: a different cadence per telemetry type** (battery, climate, energy, air, health) from the App; `/nava set_telem_tx` still changes them all at once. ⚠️ **When installing this version** the internal backup is regenerated clean for safety: battery chemistry, blacklist and auto-favorites must be set again (keys, channels, name and role recover automatically; telemetry returns to 12 h). |
| **4.3.7 — "NavaTastic Eclipse V5 (revision 28/08)"** | **You are in charge**: everything you configure (role, rebroadcast mode, position/presence/telemetry cadences — including turning them off) is saved and survives reboots and resets. **Easy installation**: when installing over any official firmware, the node configures itself with the recommended values (position and presence every 72 h, sensors every 12 h) and **keeps your admin keys** if you already had any. **New command** `/nava set_rebroadcast` to choose how the node relays foreign messages. Updating from the previous version **loses nothing** (automatic migration). |
| **4.3.8 — "NavaTastic Eclipse V5.1"** (09/09/2026) | **Your node name is no longer lost**: if you change it with the official Meshtastic app, the node remembers it (even after a factory reset). If you fixed it with `/nava set_name`, that name keeps priority until you run `flush`. When installing over other firmware (official or previous NavaTastic), your name is preserved. **New health counter**: the node counts the factory resets it has suffered and shows them in `/nava status` (and `reset_reason`) as `FR` — only when greater than zero — very useful to tell if a repeater reset itself without your command. **Route tracing now returns the result**: `/nava trace !ID` replies with the full route (nodes visited and signal of each hop, outbound and return) on the same channel you asked from, and warns if the destination does not answer. **More robustness**: if the app sets an advanced role the node does not manage, it no longer resets itself (the setting is kept safely). The boot notice ([Boot]) now arrives **after 3 minutes** (previously 2), giving the mesh more time to settle. |
| **4.3.9 — "NavaTastic Eclipse V5.2"** (15/09/2026) | **Fix to the resilience file**, the internal backup where the node stores its settings: from now on, every time the node writes to it, it **checks that what was saved actually landed correctly before accepting it**, so the backup can never be left half-written. **What you change on the node's own screen now sticks**: the role, radio preset, channel and position interval you pick in the screen menu are no longer reverted on reboot (before it looked like the menu "did nothing": the node rebooted and went back to the old value). **Turning notices off is now possible**: if you set the presence notice (NodeInfo) to `off`, the node respects it — before it silently turned it back on with a 1 hour minimum. **Admin keys can truly be revoked**: if you remove a key (in the app), that node immediately loses the ability to send commands; before it kept the permission forever. **Commands survive interruptions**: if you send a reboot, a radio change or a wipe and the node loses power right after, the command is **not lost**: it is recovered and carried out on the next start, and the node **announces by radio** that it is recovering it. **Important fix for Heltec boards**: they no longer go to sleep in a way that they could not wake from. **And a battery fix**: the sodium chemistry mode was configured so that the node could never start again; fixed. |
