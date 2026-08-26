<div align="center">

<img src="branding/cartel_navatastic_github.jpg" alt="Cartel NavaTastic V5" width="640"/>

<br/>

[![Ko-fi](https://img.shields.io/badge/Ko--fi-Caf%C3%A9%20voluntario-FF5E5B?logo=ko-fi&logoColor=white)](https://ko-fi.com/ea2oy)
[![Auditoría V4](https://img.shields.io/badge/Auditor%C3%ADa%20V4-100%25%20PASS%20(Hardware%20Real)-brightgreen?logo=checkmarx&logoColor=white)](docs/pdf/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.pdf)
[![NavaTastic V4](https://img.shields.io/badge/Versi%C3%B3n%20Estable-v4.3.3%20(V4)-green?logo=github&logoColor=white)](https://github.com/EA2OY/NavaTastic/releases/tag/v4.3.3)

</div>

> ⚠️ **AVISO IMPORTANTE (26/08/2026) — PUBLICACIÓN DE V5 EN SUSPENSIÓN TÉCNICA**:  
> La versión **NavaTastic V5 (v4.3.4)** se encuentra actualmente **suspendida temporalmente y en proceso de auditoría y corrección técnica en banco de pruebas** tras detectarse anomalías en la máquina de estados del protocolo de evacuación/resiliencia.  
> La versión de producción oficial, 100% auditada y recomendada para todos los repetidores e infraestructura es **[NavaTastic V4 (v4.3.3)](https://github.com/EA2OY/NavaTastic/releases/tag/v4.3.3)**.  
> 📦 **[Descargar NavaTastic V4 Estable (v4.3.3)](https://github.com/EA2OY/NavaTastic/releases/tag/v4.3.3)** · 📜 **[Ver README Histórico de V4](docs/README_V4.md)** · 🛡️ **[Informe de Auditoría V4 (PDF)](docs/pdf/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.pdf)** · 📋 **[Leer Auditoría en GitHub](docs/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.md)**

---

**NavaTastic** es un firmware optimizado y endurecido sobre la base de [Meshtastic](https://meshtastic.org) v2.7.26, diseñado específicamente para **repetidores solares autónomos de alta montaña e infraestructura fija** en la red LoRa de España (**SFNarrow / EU_868**).

Con un solo código fuente genera **12 firmwares listos para usar** (6 tipos de placas/radios en ramas de Routers y Clientes).

---

## ⚡ Guía Rápida de Instalación en 5 Pasos (Para quien tiene prisa)

> ⚠️ **¡ATENCIÓN! La causa #1 de fallos es no hacer el Reset de Fábrica.** Si vienes de otro firmware o versión previa, Meshtastic conserva los archivos antiguos en flash y **NO** desplegará el canal `Navadmin` ni el perfil optimizado hasta que ejecutes el **Paso 4**.

```mermaid
graph LR
    A[1. Comprobar Hardware] --> B[2. Flashear UF2 / OTA]
    B --> C[3. Guardar Clave Privada]
    C --> D[4. Factory Reset OBLIGATORIO]
    D --> E[5. Añadir Navadmin PSK AQ==]
    E --> F[🎉 ¡A Disfrutar!]
    style D fill:#ff5555,stroke:#333,stroke-width:2px,color:#fff
    style F fill:#2ecc71,stroke:#333,stroke-width:2px,color:#fff
```

### 1️⃣ Paso 1: Comprueba que tu Hardware es Compatible
* **Microcontrolador**: Compatible con placas **Nordic nRF52840** (Promicro DIY, Faketec, Seeed Xiao, Heltec T114). *(Revisa la tabla de descargas abajo)*.
* **Divisor de Batería**: Las placas DIY deben llevar un divisor resistivo **1 MΩ + 1 MΩ (factor 2.0)** para que la medición ADC y el comparador de corte solar **LPCOMP** funcionen con precisión.

### 2️⃣ Paso 2: Flashea el Firmware NavaTastic
* **Vía Cable USB (.UF2)**: Conecta la placa al PC, haz **doble pulsación rápida en el botón RESET** para que aparezca la unidad de disco USB (`NICENANO` o similar) y arrastra el archivo `.uf2` correspondiente a tu placa.
* **Vía Actualización OTA (.zip)**: Si ya estás conectado por Bluetooth, actualiza desde la App oficial de Meshtastic seleccionando el `.zip` OTA.
* **Emparejamiento Bluetooth**: El PIN de conexión por defecto es **`654321`** (modo `FIXED_PIN`).

### 3️⃣ Paso 3: Respalda tu Clave Privada *(Opcional)*
* Si deseas **mantener la misma identidad de nodo y tus conversaciones previas**, copia tu clave privada (`private_key`) desde la App antes del reseteo para restaurarla después. Si es un nodo nuevo, salta este paso y el firmware generará una identidad limpia Curve25519.

### 4️⃣ Paso 4: 🔴 FACTORY RESET (Paso Imprescindible)
* **¿Por qué?** Meshtastic almacena la configuración en `/prefs/config.proto`. Al flashear un firmware nuevo, la flash vieja bloquea la inyección de los canales de fábrica.
* **Cómo hacerlo**: Entra en la App de Meshtastic $\rightarrow$ *Configuración* $\rightarrow$ *Radio Config* $\rightarrow$ *Device* $\rightarrow$ Pulsa **Factory Reset (Restablecer de fábrica)** (o ejecuta `meshtastic --factory-reset-config` por USB).
* Al reiniciar, NavaTastic inicializará automáticamente el motor `/resilience.bin` V6 (`NAV6`), blindará las claves y desplegará el canal **Navadmin en el Slot 1**.

### 5️⃣ Paso 5: Añade el Canal `Navadmin` en tu Móvil Administrador
* Para gestionar el repetidor por radio desde tu móvil o mando de campo, crea en tu App de Meshtastic un canal secundario con estos parámetros:
  * **Nombre del canal**: `Navadmin` (respetando mayúsculas/minúsculas).
  * **Clave (PSK)**: `AQ==` (clave por defecto de Meshtastic `{ 0x01 }` / Default).
* ¡Listo! Ahora abre el canal `Navadmin` y envía `/nava ping` o abre la app oficial **[MeshNavarra](https://github.com/EA2OY/MeshNavarra)** para controlar tu repetidor con un solo toque.
* 🔄 **Sincronización Bidireccional V5**: A partir de NavaTastic V5, cualquier cambio que hagas en la App Oficial de Meshtastic (rol, canales, posición fija, telemetría, LoRa preset o PIN) **se sincroniza automáticamente en `/resilience.bin`**, por lo que tus ajustes persisten limpiamente sin revertirse al reiniciar.

---

## 🏔️ Por qué NavaTastic para Repetidores de Montaña

Un repetidor solar instalado en una cumbre aislada no puede fallar. Si se bloquea por una caída de tensión, si quema su memoria flash interna o si pierde su configuración tras un reinicio, exige subir a la montaña a pie para repararlo. 

NavaTastic resuelve de raíz los 6 grandes problemas del firmware estándar:

| Problema en Firmware Oficial | Solución Exclusiva de NavaTastic V5 |
| :--- | :--- |
| **Bloqueo de amanecer (*Brownout*)**: Si la batería se agota de noche, la subida lenta de voltaje con los primeros rayos de sol bloquea el microcontrolador en un bucle infinito del que solo sale quitando la pila físicamente. | **Modo de Resiliencia Solar de 5 Estados**: El hardware se apaga por completo (**0.4 mA**) y solo despierta cuando el comparador analógico por hardware (**LPCOMP**) detecta que el sol ha recargado la batería de verdad ($\ge 3.77\text{ V}$). |
| **Tormentas masivas de balizas (*NodeInfo Storm*)**: En el firmware oficial, cada vez que un nodo arranca emite su NodeInfo solicitando respuesta obligatoria a toda la red (`want_response=true`), forzando a que todos los nodos al alcance contesten a la vez y colapsando la frecuencia. | **Escudo Anti-Tormentas (*Adiós a las tormentas de NodeInfo*)**: El repetidor anuncia su identidad y nombre a la red pero desactiva la petición de respuesta (`want_response=false`), evitando que toda la montaña conteste al unísono y manteniendo el canal LoRa 100% limpio. |
| **Desgaste y muerte de la memoria Flash**: El firmware estándar escribe continuamente en la memoria flash interna cada vez que recibe un paquete o nodo de paso, quemando las celdas de memoria en pocos meses. | **Cero Desgaste de Flash (`NodeDB RAM-Only`) y Diagnósticos Volátiles**: La base de datos de nodos y los registros de auditoría (`stats`, `log`, `mute`, `test_tx`) operan al 100% en memoria RAM sin degradar la memoria flash. |
| **Saturación del canal y retransmisión ciega**: En mallas extensas, los paquetes agotan sus saltos (*hops*) antes de llegar al destino, y configurar listas de nodos favoritos para crear un *bypass* exige desplazarse físicamente a cada repetidor con cable. | **Auto-Favoritos Inteligentes 0-Hop (hasta 32 nodos)**: El repetidor descubre a sus routers vecinos directos y los añade automáticamente a su lista de favoritos con retransmisión a 0 saltos (**Zero-Hop**). Además, el usuario puede añadir, consultar o borrar favoritos a distancia por radio (`/nava fav add/rm/ls`), sin ordenador ni cables. |
| **Mantenimiento obligado con PC o cable**: Para cambiar un canal, rol, potencia o diagnosticar problemas hay que conectarse por Bluetooth al lado del nodo o llevar un portátil con cable USB. | **Administración Remota Total por Radio (`NavaCLI`) + App Sync**: Más de 50 comandos ejecutables a distancia mediante mensajes directos privados (DM) desde mandos autorizados, sincronización transparente desde la App oficial y soporte para **[MeshNavarra Utility](https://github.com/EA2OY/MeshNavarra-Utility)**. |
| **Nodos huérfanos tras un reset**: Si un repetidor sufre un reseteo de configuración, se borran las claves de administración y los canales, quedando inaccesible e inservible en la montaña. | **Blindaje y Persistencia Criptográfica V6 (`NAV6`)**: Las claves de administración, canales secundarios, capa física LoRa, Canal 0 y parámetros de rescate se protegen en `/resilience.bin` y sobreviven a cualquier reset de configuración. |

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

## 📦 Los 12 Entornos y Descargas Oficiales (Releases)

Todos los binarios compilados listos para flashear (**`.uf2` por USB y `.zip` por OTA**) para baterías **LiPo** y **NiMH** están empaquetados y disponibles en la sección oficial de descargas de GitHub:

<div align="center">

[![Descargar Firmware NavaTastic V5](https://img.shields.io/badge/📥%20Descargar%20Binarios%20UF2%20y%20OTA-GitHub%20Releases%20v4.3.4-blue?style=for-the-badge&logo=github)](https://github.com/EA2OY/NavaTastic/releases/tag/v4.3.4)

</div>

### 📋 Matriz de Placas y Radios Soportadas:

| Placa / Hardware | Módulo Radio LoRa | Potencia | Rama 2 (Routers / Repetidores) | Rama 1 (Clientes / Portátiles) |
|---|---|:---:|:---:|:---:|
| **Promicro nRF52 + E22P** | Ebyte E22P (SX1262 TCXO) | 12 dBm (hasta 30 dBm) | `Promicro NRF52+E22P R2IG` | `Promicro NRF52+E22P R1IG` |
| **Promicro / Faketec HT-RA62** | SX1262 Estándar / TCXO | 22 dBm | `Faketec R2IG` (LiPo / NiMH) | `Faketec R1IG` (LiPo / NiMH) |
| **Seeed Solar Node P1** | SX1262 | 22 dBm | `Seed Solar Node P1 R2IG` | `Seed Solar Node P1 R1IG` |
| **Heltec T114** | SX1262 | 22 dBm | `Heltec T114 R2IG` | `Heltec T114 R1IG` |
| **Seeed Xiao nRF52840 Kit** | SX1262 | 22 dBm | `XiaoKitI2c R2IG` (LiPo / NiMH) | `XiaoKitI2c R1IG` (LiPo / NiMH) |
| **Seeed Xiao Kit + E22P** | Ebyte E22P (SX1262 TCXO) | 12 dBm | `XiaoKitI2c+E22P R2IG` | `XiaoKitI2c+E22P R1IG` |

---

### 📚 Manuales y Documentación Técnica (PDF y Lectura Online)

- **[Manual de Comandos y Administración Remota V5 (PDF)](docs/pdf/Manual_NavaTastic.pdf)** ([Leer en GitHub](docs/Manual_NavaTastic.md)) — Guía completa de los 50+ comandos `/nava`, sintaxis y ejemplos.
- **[Manual de Uso del Firmware y Montaje V5 (PDF)](docs/pdf/Manual_uso_NavaTastic.pdf)** ([Leer en GitHub](docs/Manual_uso_NavaTastic.md)) — Montaje, divisor 1M+1M, químicas de batería, coexistencia con la App y Botón del Pánico.
- **[Informe Técnico de Auditoría Ultra-Exhaustiva V4 (PDF)](docs/pdf/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.pdf)** ([Leer en GitHub](docs/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.md)) — Certificación 100% PASS (56/56 pruebas en hardware real).
- **[Documento Maestro de Arquitectura y Diferencias vs Upstream](docs/DIFERENCIAS_VS_UPSTREAM.md)** — Inventario anatómico de todas las modificaciones respecto a Meshtastic 2.7.26 oficial.
- **[Guía de Compilación Propia y Personalizada](docs/Compilar_NavaTastic.md)** — Instrucciones paso a paso para compilar con PlatformIO.

## 🔒 Seguridad y Claves de Administración

- El canal **Navadmin** usa la clave pública por defecto de Meshtastic (`AQ==`, Slot 1): **solo admite consultas de lectura**. Los comandos de cambio exigen **Mensaje Directo Privado (DM)** firmado con tu clave de Administrador.
- El firmware incluye una clave pública de fábrica para rescate. Puedes añadir tus propias claves de administrador desde la App (*Radio config $\rightarrow$ Security $\rightarrow$ Admin key*). Tus claves quedan blindadas en `/resilience.bin` y **sobreviven a cualquier reseteo de fábrica**.


## Licencia

- **Firmware (código de este repositorio)**: **GPL v3** — heredada de
  [meshtastic/firmware](https://github.com/meshtastic/firmware), del que NavaTastic es un fork.
  Ver [LICENSE](LICENSE). Las modificaciones de NavaTastic se publican bajo la misma licencia.
- **Cumplimiento GPL**: los binarios distribuidos en [`distribucion/`](distribucion/) tienen su
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

Firmware **NavaTastic** — an optimized and hardened [Meshtastic](https://meshtastic.org) v2.7.26 fork for **solar-powered infrastructure repeaters** on the **SFNarrow** LoRa preset (EU_868). A single repository produces **12 different firmwares** (6 boards/radios × 2 branches: Routers / Clients).

<div align="center">

[![Audit V4](https://img.shields.io/badge/Audit%20V4-100%25%20PASS%20(Hardware%20Bench)-brightgreen?logo=checkmarx&logoColor=white)](docs/pdf/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.pdf)
[![Manual PDF](https://img.shields.io/badge/User%20Manual-PDF%20Download-blue?logo=adobeacrobatreader&logoColor=white)](https://github.com/EA2OY/NavaTastic/releases/download/v4.3.4/Manual_NavaTastic.pdf)

</div>

> 🚀 **NavaTastic V5 Generation (Release 4.3.4)**:  
> Features **Transparent Bidirectional Official App Sync**, **Hop-Aware Adaptive Timing**, **Asynchronous Traceroute Decoupling**, **LoRa PHY & Primary Channel 0 Persistence (`NAV6`)**, **Panic Button Mesh Evacuation**, and **32 Auto-Favorites**.  
> 📜 **[Read Historical V4 README](docs/README_V4.md)** · 🛡️ **[V4 Technical Audit Report (PDF)](docs/pdf/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.pdf)**  
> 📄 **[Command Manual V5 (PDF)](docs/pdf/Manual_NavaTastic.pdf)** ([Read on GitHub](docs/Manual_NavaTastic.md)) · 📘 **[Usage Guide V5 (PDF)](docs/pdf/Manual_uso_NavaTastic.pdf)** ([Read on GitHub](docs/Manual_uso_NavaTastic.md))

---

## ⚡ Quick Start Installation Guide (5 Steps)

> ⚠️ **IMPORTANT! The #1 cause of issues is skipping the Factory Reset.** If migrating from standard firmware or an older build, existing flash preferences will block the deployment of `Navadmin` and NavaTastic features until you perform **Step 4**.

```mermaid
graph LR
    A[1. Check Hardware] --> B[2. Flash UF2 / OTA]
    B --> C[3. Backup Private Key]
    C --> D[4. Factory Reset MANDATORY]
    D --> E[5. Add Navadmin PSK AQ==]
    E --> F[🎉 Enjoy!]
    style D fill:#ff5555,stroke:#333,stroke-width:2px,color:#fff
    style F fill:#2ecc71,stroke:#333,stroke-width:2px,color:#fff
```

### 1️⃣ Step 1: Ensure Hardware Compatibility
* **Microcontroller**: Compatible with **Nordic nRF52840** boards (Promicro DIY, Faketec, Seeed Xiao, Heltec T114).
* **Battery Divider**: DIY boards require a **1 MΩ + 1 MΩ (2.0 factor)** voltage divider for accurate ADC voltage telemetry and LPCOMP solar wake-up comparator.

### 2️⃣ Step 2: Flash NavaTastic Firmware
* **Via USB (.UF2)**: Connect to PC, **double-tap the RESET button** to enter DFU bootloader mode, and drag & drop the appropriate `.uf2` file.
* **Via OTA (.zip)**: If already connected over Bluetooth, use the Meshtastic App OTA update feature with the corresponding `.zip` file.
* **Bluetooth Pairing**: Default connection PIN is **`654321`** (`FIXED_PIN` mode).

### 3️⃣ Step 3: Backup Private Key *(Optional)*
* If you want to **keep the same node identity**, copy your private key before resetting. If setting up a fresh node, skip this step.

### 4️⃣ Step 4: 🔴 FACTORY RESET (Mandatory Step)
* Open the Meshtastic App $\rightarrow$ *Settings* $\rightarrow$ *Device Config* $\rightarrow$ Tap **Factory Reset**.
* On reboot, NavaTastic will automatically initialize `/resilience.bin` V6 (`NAV6`), lock cryptographic admin keys, and deploy the **Navadmin rescue channel on Slot 1**.

### 5️⃣ Step 5: Add the `Navadmin` Channel on Your Admin Device
* Create a secondary channel on your mobile/controller app with:
  * **Channel Name**: `Navadmin` (case-sensitive).
  * **Pre-shared Key (PSK)**: `AQ==` (standard Meshtastic default key `{ 0x01 }`).
* 🔄 **Bidirectional App Sync**: In NavaTastic V5, settings configured in the official Meshtastic App (role, channels, fixed location, telemetry, LoRa preset, or PIN) **automatically synchronize to `/resilience.bin`** and persist cleanly across reboots!

---

## 📦 The 12 Builds & Official Downloads (Releases)

All pre-compiled flashable binaries (**`.uf2` for USB and `.zip` for OTA**) for **LiPo** and **NiMH** batteries are packaged and available in the official GitHub Releases section:

<div align="center">

[![Download NavaTastic V5 Firmware](https://img.shields.io/badge/📥%20Download%20UF2%20&%20OTA%20Binaries-GitHub%20Releases%20v4.3.4-blue?style=for-the-badge&logo=github)](https://github.com/EA2OY/NavaTastic/releases/tag/v4.3.4)

</div>

### 📋 Supported Boards and Radio Matrix:

| Board / Hardware | LoRa Radio Module | Power | Branch 2 (Routers / Repeaters) | Branch 1 (Clients / Handhelds) |
|---|---|:---:|:---:|:---:|
| **Promicro nRF52 + E22P** | Ebyte E22P (SX1262 TCXO) | 12 dBm (up to 30 dBm) | `Promicro NRF52+E22P R2IG` | `Promicro NRF52+E22P R1IG` |
| **Promicro / Faketec HT-RA62** | SX1262 Standard / TCXO | 22 dBm | `Faketec R2IG` (LiPo / NiMH) | `Faketec R1IG` (LiPo / NiMH) |
| **Seeed Solar Node P1** | SX1262 | 22 dBm | `Seed Solar Node P1 R2IG` | `Seed Solar Node P1 R1IG` |
| **Heltec T114** | SX1262 | 22 dBm | `Heltec T114 R2IG` | `Heltec T114 R1IG` |
| **Seeed Xiao nRF52840 Kit** | SX1262 | 22 dBm | `XiaoKitI2c R2IG` (LiPo / NiMH) | `XiaoKitI2c R1IG` (LiPo / NiMH) |
| **Seeed Xiao Kit + E22P** | Ebyte E22P (SX1262 TCXO) | 12 dBm | `XiaoKitI2c+E22P R2IG` | `XiaoKitI2c+E22P R1IG` |

---

### 📚 Manuals & Technical Documentation (PDF & Online)

- **[Remote Administration Manual V5 (PDF)](docs/pdf/Manual_NavaTastic.pdf)** ([Read on GitHub](docs/Manual_NavaTastic.md)) — Complete 50+ `/nava` command guide, syntax, and real examples.
- **[Firmware User Manual V5 (PDF)](docs/pdf/Manual_uso_NavaTastic.pdf)** ([Read on GitHub](docs/Manual_uso_NavaTastic.md)) — Assembly, 1M+1M divider, battery chemistry, App coexistence, and Panic Button.
- **[Technical Audit Report V4 (PDF)](docs/pdf/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.pdf)** ([Read on GitHub](docs/INFORME_AUDITORIA_ULTRA_EXHAUSTIVA_NAVATASTIC_V4.md)) — 100% PASS Certification (56/56 hardware bench tests).
- **[Master Architecture & Upstream Differences Document](docs/DIFERENCIAS_VS_UPSTREAM.md)** — Comprehensive anatomical breakdown of all modifications over Meshtastic 2.7.26.
- **[Custom Compilation Guide](docs/Compilar_NavaTastic.md)** — Step-by-step instructions to build firmware with PlatformIO.

## 🔒 Security & Admin Keys

- The **Navadmin** channel uses Meshtastic's default public key (`AQ==`, Slot 1): **read-only queries only**. Administrative state changes require an **Encrypted Direct Message (DM)** signed by an authorized Admin public key.
- Factory firmware ships with a recovery key. You can add your own admin keys in the App (*Radio config $\rightarrow$ Security $\rightarrow$ Admin key*). Your keys are locked in `/resilience.bin` and **survive factory resets**.


## Acknowledgments

This project was born thanks to **JBAU92** and his [firmware_solar_fix](https://github.com/JBAU92/firmware_solar_fix). Thanks also to all friends across the **Navarra mesh** and neighboring mesh networks, and the **Meshtastic España** community.

## ☕ Voluntary Project Support

This project is and will always remain **100% free, open-source, and developed entirely altruistically** for the community.

If you wish to support development voluntarily:

<div align="center">

[![Support on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/ea2oy)

</div>