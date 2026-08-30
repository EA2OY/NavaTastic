<div align="center">

<img src="branding/cartel_navatastic_github.jpg" alt="Cartel NavaTastic V5" width="640"/>

<br/>

[![Ko-fi](https://img.shields.io/badge/Ko--fi-Caf%C3%A9%20voluntario-FF5E5B?logo=ko-fi&logoColor=white)](https://ko-fi.com/ea2oy)
[![Auditoría V5](https://img.shields.io/badge/Auditor%C3%ADa%20V5-Realizada%20en%20banco-brightgreen?logo=checkmarx&logoColor=white)](docs/pdf/Informe_Auditoria_NavaTastic_V5.pdf)
[![Estado: ALPHA](https://img.shields.io/badge/Estado-ALPHA-orange?logo=github&logoColor=white)](https://github.com/EA2OY/NavaTastic/releases)

</div>

> ⚠️ **AVISO IMPORTANTE — V5 ESTÁ EN FASE ALPHA**:  
> La versión **NavaTastic V5** se encuentra en **fase ALPHA**, siendo probada en campo por el
> autor. Incluye funciones potentes y nuevas (el Botón del Pánico consolidable, la instalación
> automática, el reenvío configurable...), pero todavía no ha pasado una validación final:
> **úsala con criterio y no la consideres "estable"** hasta que el autor confirme su
> comportamiento tras las pruebas reales.  
> La versión de producción oficial, 100% auditada y recomendada para los repetidores e
> infraestructura sigue siendo **[NavaTastic V4 (v4.3.3)](https://github.com/EA2OY/NavaTastic/releases/tag/v4.3.3)**.  

---

**NavaTastic** es un firmware optimizado y endurecido sobre la base de [Meshtastic](https://meshtastic.org) v2.7.26, diseñado específicamente para **repetidores solares autónomos de alta montaña e infraestructura fija** en la red LoRa de España (**SFNarrow / EU_868**).

Con un solo código fuente genera **16 firmwares listos para usar** (6 tipos de placas/radios nRF52840 + **Heltec V3 y V4** ESP32-S3, en ramas de Routers y Clientes).

<div align="center">

[![Descargar Firmware](https://img.shields.io/badge/📥%20Descargar%20Firmware-Todos%20los%20Releases%20(V5%20·%20V4%20·%20anteriores)-blue?style=for-the-badge&logo=github)](https://github.com/EA2OY/NavaTastic/releases)

</div>

---

## 🧠 ¿Qué le añade NavaTastic al firmware normal?

Un repetidor solar instalado en una cumbre aislada no puede fallar: si se bloquea por una caída
de tensión, si quema su memoria flash interna o si pierde su configuración tras un reinicio,
exigiría subir a la montaña a pie para repararlo. NavaTastic resuelve de raíz los grandes
problemas del firmware estándar:

☀️ **Resiliencia energética.** Si el nodo se queda sin batería, ya no entra en brownout: te
escribe un mensaje avisando de que tiene batería baja, se programa para despertar cuando la
tensión suba de cierto umbral y se pasa a sueño profundo — microcontrolador y radio dormidos,
con un consumo de **0.4 mA** — hasta que salga el sol, recupere energía suficiente y vuelva a
arrancar. El despertar lo decide un comparador analógico por hardware (LPCOMP) cuando la batería
se ha recargado de verdad (≥ 3.77 V), sin bucles de amanecer. Entonces despierta y te escribe
otro mensaje diciendo que está listo para trabajar.

💾 **Protección de la memoria flash.** En una malla grande, todos los nodos mandan NodeInfos y
mensajes; en el firmware oficial eso provoca escrituras constantes en la flash y acorta su vida
útil — a medio plazo acaba rompiendo el microcontrolador por desgaste. NavaTastic elimina las
escrituras innecesarias: la base de datos de nodos y los diagnósticos (`stats`, `log`, `mute`,
`test_tx`) operan al 100% en memoria RAM, protegiendo y alargando la vida del nodo.

⭐ **Auto-favoritos activado por defecto.** Si el nodo es Router, detecta los otros routers que
ve en directo y los agrega a favoritos (hasta 32), beneficiándose del sistema *zero-hop* de
Meshtastic para que los routers de infraestructura no resten saltos. Los favoritos también se
gestionan a distancia por radio (`/nava fav`), sin cables.

📡 **Más de 50 comandos para gestionarlo todo sin ordenador.** Sin cable ni CLI: ver qué nodos
ve en directo, ver o agregar favoritos, ignorar nodos que molestan, ver el estado real, el nivel
de ruido, la batería, los sensores de clima y de energía... casi cualquier cosa que necesites.
Todo por mensajes cifrados, con sincronización transparente con la App oficial de Meshtastic y
soporte para **MeshNavarra Utility**.

🛡️ **Se recupera solo de los resets.** Si el nodo sufre un fallo que provoca un reset de
fábrica indeseado, no se queda aislado esperando a que vayas físicamente: vuelve a la malla
tratando de respetar los ajustes que ya le habías puesto — claves de administración, canales
secundarios, configuración de radio y parámetros de rescate quedan protegidos y sobreviven a
cualquier reset de configuración — hasta las claves de administración remota. Si el fallo es
fatal, tampoco se queda aislado: vuelve a la malla SFN con una clave de administración de
rescate que te permite dejarlo como estaba, evitando el desplazamiento.

📊 **Buenas Prácticas aplicadas.** El nodo se configura con las recomendaciones de una de las
mallas más grandes del mundo: prioridad y espacio para la mensajería de las personas. Los
NodeInfos, las posiciones GPS y las telemetrías se ajustan a 72h/72h/12h para optimizar la red,
y se eliminan los acuses de recibo que provocaban las "tormentas de NodeInfo" (que todos los
nodos intenten responder a la vez): el repetidor anuncia su identidad **sin exigir respuesta a
toda la red**, manteniendo el canal limpio.

🚨 **Botón del pánico.** Con una sola orden, toda tu malla migra al preset LoRa que elijas
(MediumFast, LongFast...), facilitando el aislamiento bajo demanda, puntual o fijo — con
reversión automática programable si lo necesitas. La orden se propaga de repetidor en
repetidor por el canal privado, aunque el mando solo tenga cobertura con uno.

---

## ⚡ Guía Rápida de Instalación en 5 Pasos (Para quien tiene prisa)

> 💡 **Ya NO hace falta el reset de fábrica**: al flashear, el firmware se configura solo
> (canal `Navadmin` + buenas prácticas) y respeta tus claves. Espera un minuto.

```
1. **Comprueba que tu hardware es compatible**
2. **Flashea el firmware** (UF2 / OTA)
3. **Guarda tu clave privada** *(opcional)*
4. **Sin reset: se despliega solo** — espera un minuto
5. **Añade el canal `Navadmin` (PSK `AQ==`)** en tu mando
```

### 1️⃣ Paso 1: Comprueba que tu Hardware es Compatible
* **Microcontrolador**: Compatible con placas **Nordic nRF52840** (Promicro DIY, Faketec, Seeed Xiao, Heltec T114) y **ESP32-S3** (Heltec V3, Heltec V4). *(Revisa la tabla de descargas abajo)*.
* **Divisor de Batería**: Las placas DIY deben llevar un divisor resistivo **1 MΩ + 1 MΩ (factor 2.0)** para que la medición ADC y el comparador de corte solar **LPCOMP** funcionen con precisión.

### 2️⃣ Paso 2: Flashea el Firmware NavaTastic
* **Vía Cable USB (.UF2)**: Conecta la placa al PC, haz **doble pulsación rápida en el botón RESET** para que aparezca la unidad de disco USB (`NICENANO` o similar) y arrastra el archivo `.uf2` correspondiente a tu placa.
* **Vía Actualización OTA (.zip)**: Si ya estás conectado por Bluetooth, actualiza desde la App oficial de Meshtastic seleccionando el `.zip` OTA.
* **Emparejamiento Bluetooth**: El PIN de conexión por defecto es **`654321`** (modo `FIXED_PIN`).

### 3️⃣ Paso 3: Respalda tu Clave Privada *(Opcional)*
* Si deseas **mantener la misma identidad de nodo y tus conversaciones previas**, copia tu clave privada (`private_key`) desde la App antes del reseteo para restaurarla después. Si es un nodo nuevo, salta este paso y el firmware generará una identidad limpia Curve25519.

### 4️⃣ Paso 4: ✅ YA NO HACE FALTA FACTORY RESET (desde la V5)
* **¿Por qué?** Desde la V5, al flashear NavaTastic el propio firmware **se despliega solo** en
  el primer arranque: materializa el canal `Navadmin` en el Slot 1, aplica las buenas prácticas
  (72h/72h/12h) y **respeta las claves del dueño** si el nodo ya tenía alguna.
* **Qué hacer**: nada. Espera un minuto tras flashear y el nodo queda operativo y configurado.

### 5️⃣ Paso 5: Añade el Canal `Navadmin` en tu Móvil Administrador
* Para gestionar el repetidor por radio desde tu móvil o mando de campo, crea en tu App de Meshtastic un canal secundario con estos parámetros:
  * **Nombre del canal**: `Navadmin` (respetando mayúsculas/minúsculas).
  * **Clave (PSK)**: `AQ==` (clave por defecto de Meshtastic `{ 0x01 }` / Default).
* ¡Listo! Ahora abre el canal `Navadmin` y envía `/nava ping` o abre la app oficial **[MeshNavarra](https://github.com/EA2OY/MeshNavarra)** para controlar tu repetidor con un solo toque.
* 🔄 **Sincronización Bidireccional V5**: A partir de NavaTastic V5, cualquier cambio que hagas en la App Oficial de Meshtastic (rol, canales, posición fija, telemetría, LoRa preset o PIN) **se sincroniza automáticamente en `/resilience.bin`**, por lo que tus ajustes persisten limpiamente sin revertirse al reiniciar.

### 🔧 ¿Prefieres hacerlo a mano? Compilar o flashear binarios

- [Guía de compilación desde el código fuente](Compilar_NavaTastic.md)
- [Guía de flasheo de binarios en placas Heltec V3/V4 (ESP32)](Guia_flasheo_binario_esp32.md)

---

## ☀️ Los 5 Estados del Ciclo Solar (Avisos Automáticos)

El repetidor informa a la red de su estado de salud en tiempo real a través del canal Navadmin o canal privado asignado:

1. **`[Listo]`** ($\ge 3.77\text{ V}$): Recuperación solar completada $\rightarrow$ El nodo anuncia *"despierto, cargando, listo para trabajar"* y opera de forma ininterrumpida.
2. **`[Vivo]`** ($3.30\text{V} - 3.40\text{ V}$): Batería al límite (Nivel 1) $\rightarrow$ Anuncia *"sigo vivo, al límite de carga"* y opera 160s. Si la batería no remonta, vuelve a dormir.
3. **`[Critico]`** ($< 3.30\text{ V}$): Capacidad crítica (Nivel 2) $\rightarrow$ Anuncia *"bateria en capacidad critica, operando 160s"* y se apaga limpiamente a **0.4 mA**. Permite al operador monitorizar día a día la rampa de recuperación solar en días nublados.
4. **`[Sueño]`**: Corte de batería $\rightarrow$ Emite el aviso de despedida con la tensión exacta del ADC y la temperatura del chip, apagando la radio por bus SPI.
5. **`[Boot]`**: Diagnóstico diferido a los 2 minutos de uptime exactos tras un reinicio en frío $\rightarrow$ Reporta la causa hardware del reinicio (`WDT`, `RESETPIN`, `SOFT`, `LPCOMP`, `VBUS`, etc.) y la versión `NAVA V5`. El retardo de 2 min actúa como anti-bucle de malla.

---

## 💻 NavaCLI: Gestiona todo desde el móvil, sin PC

Con un simple mensaje de texto (DM al repetidor o en canal privado de flota) manejas el nodo completo: **no hace falta cable, ni PC, ni abrir la interfaz normal de administración**. Toda la gestión del nodo — consultar su estado, ajustar energía, reiniciar con ventana de gracia de 6s, conmutar presets LoRa, evacuar en pánico o gestionar la flota en lote — cabe en un mensaje.

| Comando | Qué hace | Nivel de Acceso |
| :--- | :--- | :--- |
| **`ping` · `status` · `bat` · `power`** | Métricas rápidas: latencia · salud de memoria y versión `NAVA V5` · % OCV · energía ADC + INA219 | **Canal Abierto / Privado / DM** |
| **`env` · `channel` · `noise`** | Telemetría sensores I2C · ocupación de canal airtime % · piso de ruido LoRa en dBm | **Canal Abierto / Privado / DM** |
| **`peers` · `rxlog` · `afc` · `reset_reason`** | Vecinos directos · últimos 5 paquetes recibidos · deriva TCXO Hz · motivo del reinicio (RESETREAS) | **Broadcast con `!ID` / Privado / DM** |
| **`stats` · `log [n]` · `route` · `trace`** | Auditoría de extremos (temperatura, batería, tráfico RX/TX/Enrutados) · buffer RAM · traceroute desacoplado (8s) | **Broadcast con `!ID` / Privado / DM** |
| **`set_preset` · `set_lora` · `set_freq`** | **[Novedad V5]** Cambio de preset LoRa estándar · ajuste custom (BW/SF/CR/Freq/Slot/Power) · frecuencia | **Canal Privado / DM** |
| **`panic` · `panic_ok`** | **[Novedad V5]** Evacuación simultánea de toda la cordillera en $T$ minutos · consolidación / cancelación de retorno | **Canal Privado / DM** |
| **`ch_ls` · `ch_set` · `ch_del` · `ch_url`** | Gestión de Canal 0 y secundarios (0-7) · configuración con clave Base64 · exportación URL Meshtastic | **Canal Privado / DM** |
| **`set_cli_chan` · `navadmin_mute` · `ch_reset`** | Redirección de CLI a canal privado · silenciamiento de Navadmin · reset de canales a fábrica | **Canal Privado / DM** |
| **`set_ok_to_mqtt` · `ch_mqtt`** | Autorización global MQTT de la flota · conmutación granular de pasarela por canal | **Canal Privado (Lote) / DM** |
| **`set_pos_tx` · `set_nodeinfo_tx` · `set_telem_tx`** | Control de difusión periódica de posición (72h/off) · NodeInfo de flota · cadencia telemetría | **Canal Privado (Lote) / DM** |
| **`ign add/del/clear/ls`** | Lista negra persistente contra nodos saboteadores/spam (descarte inmediato en enrutador) | **Canal Privado (Lote) / DM** |
| **`set_beacon` · `mute` · `test_tx` · `db_purge`** | Intervalo balizas · silenciado temporal de reenvío · ráfaga de prueba RF · purga de memoria RAM | **Canal Privado (Lote) / DM** |
| **`set_pos` · `pos_clear` · `set_name` · `set_pin`** | Coordenadas fijas persistentes · borrado de posición fija · nombre persistente a resets (`set_name flush`) · PIN BLE fijo | **Individual (`!ID`) / DM** |
| **`set_chem` · `set_vbat` · `set_vwake`** | Cambio de química de batería (`lipo/nimh/sodium/lifepo4`) · umbral de corte mV · despertar LPCOMP | **Canal Privado / DM** |
| **`set_txpower` · `set_hops` · `set_role` · `set_tz`** | Potencia de transmisión LoRa · límite de saltos · cambio de rol semi-permanente · zona horaria | **Canal Privado / DM** |
| **`storm` · `txoff` · `txon` · `ble` · `sleepmsg`** | Hibernación por tormenta · control de transmisión LoRa · Bluetooth on/off · avisos solares | **Canal Privado / DM** |
| **`fav add/rm/ls/auto`** | Gestión de favoritos con bypass 0-Hop y auto-favoriteo de routers vecinos directos (hasta 32) | **Individual (`!ID`) / DM** |
| **`admin_ls` · `keys_ls` · `keys_clear`** | Consulta de claves admin activas · claves persistidas en disco · borrado de claves persistidas | **Solo DM de Administrador** |
| **`reboot` · `factory_reset` · `full_reset` · `wipe`** | Reinicio suave (gracia 6s) · reset de fábrica · reset completo sin perder claves PKI · purga nuclear | **Solo DM de Administrador** |

---



## Licencia

- **Firmware (código de este repositorio)**: **GPL v3** — heredada de
  [meshtastic/firmware](https://github.com/meshtastic/firmware), del que NavaTastic es un fork.
  Ver [LICENSE](LICENSE). Las modificaciones de NavaTastic se publican bajo la misma licencia.
- **Cumplimiento GPL**: los binarios distribuidos en los Assets de los Releases tienen su
  código fuente completo en **este mismo repositorio, en el mismo commit**.
- **Hardware**: los diseños de placas (cuando se publiquen) se licenciarán aparte.

## Descargo de responsabilidad

- Este firmware se distribuye **SIN GARANTÍA DE NINGÚN TIPO**, bajo los términos de la GPL v3. Úsalo **bajo tu propia responsabilidad**.
- Un repetidor solar es un dispositivo que se instala en altura, con baterías y alimentación solar: el montaje, el dimensionado de batería/panel y el mantenimiento son responsabilidad del instalador.
- **Cumplimiento normativo del montaje**: toda instalación debe cumplir la normativa nacional, autonómica, local y europea aplicable.
- El proyecto queda **desvinculado** de cualquier montaje o uso de terceros.

## Agradecimientos

Este proyecto nació gracias a **JBAU92** y su [firmware_solar_fix](https://github.com/JBAU92/firmware_solar_fix): sin esa base e inspiración, NavaTastic no existiría. Gracias también a tod@s l@s amig@s y conocid@s de la **malla de Navarra** y de las **mallas cercanas amigas**, a los que guardo mucho aprecio y valoro enormemente su apoyo. He hecho grandes amistades gracias a todo esto, y a toda la gente del grupo **Meshtastic España** (Telegram) que me ha echado una mano, probado el firmware e inspirado en este camino.

## ☕ Apoyo voluntario al proyecto

Este proyecto es y será siempre **100% libre, abierto, gratuito y desarrollado de forma totalmente altruista** y desinteresada para la comunidad.

Si alguien, de manera **estrictamente voluntaria**, desea invitar a un café para ayudar a sufragar los costes de placas de prueba y componentes de laboratorio, puede hacerlo aquí:

<div align="center">

[![Apoyar en Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/ea2oy)

</div>

---

# NavaTastic (English)

Firmware **NavaTastic** — an optimized and hardened [Meshtastic](https://meshtastic.org) v2.7.26 fork for **solar-powered infrastructure repeaters** on the **SFNarrow** LoRa preset (EU_868). A single repository produces **16 different firmwares** (6 nRF52840 boards + **Heltec V3 and V4** ESP32-S3, × 2 branches: Routers / Clients).

<div align="center">

[![Download Firmware](https://img.shields.io/badge/📥%20Download%20Firmware-All%20Releases%20(V5%20·%20V4%20·%20older)-blue?style=for-the-badge&logo=github)](https://github.com/EA2OY/NavaTastic/releases)

</div>

<div align="center">

[![Audit V5](https://img.shields.io/badge/Audit%20V5-Bench%20verified-brightgreen?logo=checkmarx&logoColor=white)](docs/pdf/Informe_Auditoria_NavaTastic_V5.pdf)
[![Status: ALPHA](https://img.shields.io/badge/Status-ALPHA-orange?logo=github&logoColor=white)](https://github.com/EA2OY/NavaTastic/releases)

</div>

> ⚠️ **IMPORTANT NOTICE — V5 IS IN ALPHA PHASE**:  
> The **NavaTastic V5** version is being field-tested by the author. It includes powerful new
> features, but it has not passed final validation yet: **use it with judgment and do not
> consider it "stable"** until the author confirms its behavior after real-world testing.  
> The official production version remains **[NavaTastic V4 (v4.3.3)](https://github.com/EA2OY/NavaTastic/releases/tag/v4.3.3)**.

---

## 🧠 What NavaTastic adds to the normal firmware

A solar repeater installed on an isolated peak cannot fail: if it locks up on a brownout, burns
its internal flash or loses its configuration after a reboot, someone has to hike up the
mountain to fix it. NavaTastic solves the big problems of the standard firmware at the root:

☀️ **Energy resilience.** If the node runs out of battery, it no longer goes into brownout: it
sends you a message warning that its battery is low, schedules a wake-up when the voltage rises
above a certain threshold and goes into deep sleep â€” microcontroller and radio asleep, drawing
only **0.4 mA** â€” until the sun comes out, it recovers enough energy and boots again. Waking up
is decided by an analog hardware comparator (LPCOMP) when the battery has truly recharged
(≥ 3.77 V), with no dawn lock-up loops. Then it wakes up and sends you another message saying it
is ready to work.

\U0001F4BE **Flash memory protection.** On a large mesh, all nodes send NodeInfos and messages; on the
official firmware that causes constant flash writes, shortening its lifespan â€” in the medium
term it ends up irreparably breaking the microcontroller from wear. NavaTastic removes the
unnecessary writes: the node database and the diagnostics (`stats`, `log`, `mute`, `test_tx`)
run 100% in RAM, protecting and extending the life of the node.

⭐ **Auto-favorites enabled by default.** If the node is a Router, it detects the other routers
it sees directly and adds them to favorites (up to 32), taking advantage of Meshtastic's
*zero-hop* system so that infrastructure routers don't consume hops. Favorites can also be
managed over the air (`/nava fav`), no cables needed.

\U0001F4E1 **More than 50 commands for complete management without a computer.** No cable or CLI: see
which nodes it sees directly, view or add favorites, ignore nodes that disturb the mesh, see
real status, noise level, battery, climate and energy sensors... almost anything you might
need. All over encrypted messages, with transparent sync with the official Meshtastic App and
support for **MeshNavarra Utility**.

\U0001F6E1️ **It recovers on its own from resets.** If the node suffers a fault that causes an unwanted
factory reset, it doesn't stay isolated waiting for you to go there physically: it returns to
the mesh trying to respect the settings you already gave it â€” admin keys, secondary channels,
radio settings and rescue parameters stay protected and survive any configuration reset â€”
including the remote administration keys. If the fault is fatal, it still doesn't stay
isolated: it returns to the SFN mesh with a rescue administration key that lets you restore it
as it was, avoiding the trip.

\U0001F4CA **Best Practices applied.** The node is configured with the recommendations of one of the
largest meshes in the world: priority and space for people's messaging. NodeInfos, GPS
positions and telemetry are set to 72h/72h/12h to optimize the network, and the
acknowledgements that caused "NodeInfo storms" (every node trying to answer at once) are
removed: the repeater announces its identity **without demanding an answer from the whole
network**, keeping the channel clean.

\U0001F6A8 **Panic Button.** With a single command, your whole mesh migrates to the LoRa preset you
choose (MediumFast, LongFast...), enabling on-demand isolation, temporary or permanent â€” with
optional automatic reversal if you need it. The command propagates repeater to repeater over
the private channel, even if the controller only has coverage with one node.

---

## ⚡ Quick Start Installation Guide (5 Steps)

> 💡 **No factory reset needed**: after flashing, the firmware configures itself
> (`Navadmin` channel + best practices) and respects your keys. Wait a minute.

```
1. **Check your hardware is compatible**
2. **Flash the firmware** (UF2 / OTA)
3. **Backup your private key** *(optional)*
4. **No reset needed: auto-deploys** — wait a minute
5. **Add the `Navadmin` channel (PSK `AQ==`)** on your controller
```

### 1️⃣ Step 1: Ensure Hardware Compatibility
* **Microcontroller**: Compatible with **Nordic nRF52840** boards (Promicro DIY, Faketec, Seeed Xiao, Heltec T114) and **ESP32-S3** (Heltec V3, Heltec V4).
* **Battery Divider**: DIY boards require a **1 MΩ + 1 MΩ (2.0 factor)** voltage divider for accurate ADC voltage telemetry and LPCOMP solar wake-up comparator.

### 2️⃣ Step 2: Flash NavaTastic Firmware
* **Via USB (.UF2)**: Connect to PC, **double-tap the RESET button** to enter DFU bootloader mode, and drag & drop the appropriate `.uf2` file.
* **Via OTA (.zip)**: If already connected over Bluetooth, use the Meshtastic App OTA update feature with the corresponding `.zip` file.
* **Bluetooth Pairing**: Default connection PIN is **`654321`** (`FIXED_PIN` mode).

### 3️⃣ Step 3: Backup Private Key *(Optional)*
* If you want to **keep the same node identity**, copy your private key before resetting. If setting up a fresh node, skip this step.

### 4️⃣ Step 4: ✅ NO FACTORY RESET NEEDED (since V5)
* **Why?** Since V5, after flashing NavaTastic the firmware **deploys itself** on first boot:
  it materializes the `Navadmin` channel on Slot 1, applies the best practices (72h/72h/12h) and
  **respects the owner's keys** if the node already had any.
* **What to do**: nothing. Wait a minute after flashing and the node is ready and configured.

### 5️⃣ Step 5: Add the `Navadmin` Channel on Your Admin Device
* Create a secondary channel on your mobile/controller app with:
  * **Channel Name**: `Navadmin` (case-sensitive).
  * **Pre-shared Key (PSK)**: `AQ==` (standard Meshtastic default key `{ 0x01 }`).
* 🔄 **Bidirectional App Sync**: In NavaTastic V5, settings configured in the official Meshtastic App (role, channels, fixed location, telemetry, LoRa preset, or PIN) **automatically synchronize to `/resilience.bin`** and persist cleanly across reboots!

### 🔧 Prefer to do it yourself? Build or flash binaries

- [Building guide from source](Compilar_NavaTastic.md)
- [Flashing guide for Heltec V3/V4 (ESP32) binaries](Guia_flasheo_binario_esp32.md)

---




## Acknowledgments

This project was born thanks to **JBAU92** and his [firmware_solar_fix](https://github.com/JBAU92/firmware_solar_fix). Thanks also to all friends across the **Navarra mesh** and neighboring mesh networks, and the **Meshtastic España** community.

## ☕ Voluntary Project Support

This project is and will always remain **100% free, open-source, and developed entirely altruistically** for the community.

If you wish to support development voluntarily:

<div align="center">

[![Support on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/ea2oy)

</div>