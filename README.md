# NavaTastic

Firmware **NavaTastic** — fork de [Meshtastic](https://meshtastic.org) v2.7.26 (base `54e0d8d`)
para **repetidores solares de infraestructura** en malla LoRa **SFNarrow** (preset de uso
nacional en España, EU_868). Un solo repositorio genera **12 firmwares** distintos.

```
6 placas/radios × 2 ramas (Routers / Clientes)
```

---

## Qué aporta NavaTastic sobre Meshtastic

| Bloque | Qué hace |
|---|---|
| **Resiliencia solar** | Anti-brownout de arranque, despertar por LPCOMP con histéresis, hibernación *storm*, químicas de batería (`lipo/nimh/sodium/lifepo4` según placa), corte y despertar configurables |
| **Avisos de ciclo de batería** | `[Sueño]` / `[Vivo]` / `[Listo]` / `[Boot]` por el canal Navadmin: el nodo avisa antes de dormir (~1 mA), al despertar por solar/reset, y de cada reinicio con su **causa** (watchdog, ATtiny, soft, brownout...) |
| **NavaCLI `/nava`** | ~45 comandos de administración remota: telemetría, energía, favoritos, bloqueos, rol semi-permanente, mantenimiento. DM cifrado PKI para acciones críticas; canal Navadmin de solo-lectura |
| **Protección de Flash** | Base de nodos con guardado filtrado, auto-favoritos de routers directos, desalojo híbrido, historial sin escritura |
| **Seguridad** | Acreditación admin por PKI persistente tras reboot, auto-recuperación de claves, límite de favoritos huérfanos |
| **Rol semi-permanente** | `set_role` persiste en `/resilience.bin` y **sobrevive al factory reset** |

## Requisito de hardware: divisor ADC 1M+1M (factor 2.0)

Para que la medición de batería y la protección de bajo voltaje funcionen, las placas
**NRF52 (Promicro/Faketec/Albatastic/Xiaowa)** deben medir la batería con un **divisor de dos
resistencias de 1 MΩ** (factor 2.0).

> **¿Tu placa lleva un divisor distinto?** Puedes ajustarlo antes de compilar en
> `variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h` (macro `ADC_MULTIPLIER`, valor
> `VBAT_DIVIDER_COMP`). **Aviso importante**: ese mismo divisor alimenta el comparador
> **LPCOMP**, que decide el **despertar del modo de resiliencia por batería baja** — los niveles
> de `set_vwake` están calibrados para divisor 2.0; con otro divisor el nodo despertará a una
> tensión distinta (recalibrar `getActiveLpcompThreshold()` en
> `src/platform/nrf52/main-nrf52.cpp`).

## Químicas de batería

Los 12 builds compilados usan **LiPo/Li-Ion por defecto** (corte 3500 mV en E22P / 3400 mV en
SX1262). El firmware soporta **4 químicas** y se cambian **sin recompilar** con el comando
`/nava set_chem` (por DM cifrado, persiste en el nodo):

| Química | Corte | Despertar LPCOMP | Notas |
|---|---|---|---|
| **LiPo / Li-Ion** | 3500 mV | ~3.7 V | Default de los builds |
| **NiMH (3 celdas)** | 3400 mV | ~3.7 V | |
| **Sodio (Na-Ion)** | 2600 mV | ~3.7 V | Carga máx ~4.0 V |
| **LiFePO4** | 2800 mV | ~3.3 V | **Solo Promicro y Faketec** (rechazada en Seed/Xiao/T114: su LPCOMP es fijo y no despertarían por solar) |

Para **compilar con otra química como default**: añade al perfil del env
(`profiles/<RAMA>_<Placa>.jsonc`) la macro `"USERPREFS_BATTERY_CHEMISTRY_SODIUM": "true"`
(default sodio) y/o ajusta el corte con `"USERPREFS_LOW_BATTERY_SLEEP_THRESHOLD_MV"` (p. ej.
NiMH = 3400, LiFePO4 = 2800) antes de `pio run`. El resto (umbral de despertar, curvas OCV) se
adapta con los comandos `set_chem` / `set_vbat` / `set_vwake` ya en el nodo.

## Los 12 builds

| Placa | Radio | Rama 2 (Routers) | Rama 1 (Clientes) |
|---|---|---|---|
| Promicro nRF52840 + E22P | E22P (12 dBm) | `navarrico_promicro_e22p_r2ig` | `navarrico_promicro_e22p_r1ig` |
| Promicro/Faketec + HT-RA62 | SX1262 (22 dBm) | `navarrico_faketec_sx1262_r2ig` | `navarrico_faketec_sx1262_r1ig` |
| Seeed Solar Node P1 | SX1262 (22 dBm) | `navarrico_seed_sx1262_r2ig` | `navarrico_seed_sx1262_r1ig` |
| Heltec T114 | SX1262 (22 dBm) | `navarrico_t114_sx1262_r2ig` | `navarrico_t114_sx1262_r1ig` |
| Xiao nRF52840 Kit | SX1262 (22 dBm) | `navarrico_xiao_kit_sx1262_r2ig` | `navarrico_xiao_kit_sx1262_r1ig` |
| Xiao nRF52840 Kit + E22P | E22P (12 dBm) | `navarrico_xiao_e22p_r2ig` | `navarrico_xiao_e22p_r1ig` |

Diferencias declaradas por env/perfil (nunca editando código): potencia TX, curvas OCV y
LPCOMP por placa, rol por rama, claves admin y Bluetooth por perfil (`profiles/*.jsonc`).

### Estado de pruebas (banco)

| Placa | Estado |
|---|---|
| **Promicro + E22P (R2IG)** | **Verificado completo** (15/08): ciclo dormir/despertar, avisos, consumo ~1 mA, despertar LPCOMP, [Boot] |
| **Seeed Solar Node P1** | **Semi-testeada** — pendiente de ciclo completo de resiliencia en la placa |
| **Heltec T114** | **PENDIENTE**: testear el ciclo de dormir/despertar de resiliencia del firmware |
| Faketec HT-RA62, Xiao Kit, Xiao + E22P | Pendientes de verificación en banco |

## Descargas (firmware compilado)

Última versión: **V2.6 (15/08/2026)** — ciclo sueño/despertar definitivo verificado en banco.

Estructura en [`distribucion/`](distribucion/): `Rama 2 Routers` / `Rama 1 Clientes` ×
`LIPO` (todas) / `NIMH` (solo Faketec y XiaoKitI2c) × `UF2` / `OTA`.

| | Rama 2 Routers | Rama 1 Clientes |
|---|---|---|
| **UF2 (LIPO)** | [descargar](distribucion/Rama%202%20Routers/LIPO/UF2/) | [descargar](distribucion/Rama%201%20Clientes/LIPO/UF2/) |
| **OTA (LIPO)** | [descargar](distribucion/Rama%202%20Routers/LIPO/OTA/) | [descargar](distribucion/Rama%201%20Clientes/LIPO/OTA/) |
| **UF2 (NIMH)** | [descargar](distribucion/Rama%202%20Routers/NIMH/UF2/) | [descargar](distribucion/Rama%201%20Clientes/NIMH/UF2/) |
| **OTA (NIMH)** | [descargar](distribucion/Rama%202%20Routers/NIMH/OTA/) | [descargar](distribucion/Rama%201%20Clientes/NIMH/OTA/) |

### Manuales (PDF)

- [Manual de administración remota `/nava` (PDF)](docs/pdf/Manual_NavaTastic.pdf) — comandos completos
- [Manual de uso del firmware (PDF)](docs/pdf/Manual_uso_NavaTastic_4.2.pdf) — montaje, requisitos de hardware, protocolo de rescate

## Flashear

- **nRF52 (todas las placas)**: con el nodo en modo DFU (doble clic en reset) aparece una
  unidad **NICENANO** → copiar el `.uf2`. Alternativa: `pio run -e <env> -t upload
  --upload-port COMx`.
- **Tras flashear un nodo nuevo de fábrica**: hacer **un factory reset** para materializar
  el canal Navadmin (los avisos y la consulta por canal abierto dependen de él).
- **Copia de seguridad de claves**: el flasheo por sí solo **conserva** los `/prefs` del nodo
  (claves, canales, nombre). Si vas a hacer un **factory reset** (o un `nrf erase`), **exporta
  antes la configuración y las claves del nodo desde la app de Meshtastic** si quieres poder
  restaurarlas: el reset **borra las claves del nodo**, y tras un `nrf erase` se regeneran
  (los demás nodos tendrán que reaprender tu clave nueva para el DM cifrado).
- **Pruebas en banco**: el E22P es inestable en TX con USB (picos de corriente) — usar
  **TX 1 dBm**; la detección de batería baja exige alimentar **sin USB**.

## Compilar

Requisitos: PlatformIO (`pio`). Desde la raíz del repo:

```bash
pio run -e navarrico_promicro_e22p_r2ig   # un env
pio run                                    # los 12 (default_envs)
```

Ver también: `Guia_para_agente_sobre_NavaTastic.md` (mecánica completa),
`docs/transfer_context.md` (memoria técnica) y `docs/cerebro/` (documentación de diseño).

## Rama propia (claves privadas)

Los builds **Propia** (`r2ip`/`r1ip`) usan claves admin y PIN Bluetooth propios que **no se
almacenan en este repositorio**: se piden al compilar.

```powershell
.\build_propia.ps1 -EnvName navarrico_promicro_e22p_r2ip
```

## Seguridad

- El canal Navadmin usa la **PSK pública** de Meshtastic (`{0x01}`, slot 1): cualquiera puede
  escuchar. Solo admite consultas de lectura; los comandos críticos van por **DM cifrado PKI**.
- Las claves admin que lleva el firmware son **públicas** (el binario nunca contiene privadas).

### Gestión de claves admin (altamente recomendable)

El firmware sale con una clave admin **de fábrica** (la del proyecto). Para tu red:

1. **Añade DOS claves de gestión remota propias** (de tus dispositivos de mando) desde la app
   de Meshtastic → *Radio config → Security → Admin key* (el firmware admite 3 slots).
2. **Comprueba que funcionan**: con cada mando, envía un comando `/nava` por DM — debe
   responder (el repetidor acredita a ese mando como admin y lo guarda en disco).
3. **Desautoriza la clave de fábrica** una vez verificadas las tuyas: pon **una de tus claves
   en el slot 0** (sustituyendo a la de fábrica). Ojo: si el slot 0 queda **vacío**, el
   firmware **re-inyecta la clave de fábrica en cada arranque** (auto-recuperación anti-bloqueo),
   así que dejarlo vacío NO la desautoriza — hay que sobreescribirlo con la tuya.

## Licencia

- **Firmware (código de este repositorio)**: **GPL v3** — heredada de
  [meshtastic/firmware](https://github.com/meshtastic/firmware), del que NavaTastic es un fork.
  Ver [LICENSE](LICENSE). Las modificaciones de NavaTastic se publican bajo la misma licencia.
- **Cumplimiento GPL**: los binarios distribuidos en [`distribucion/`](distribucion/) tienen su
  código fuente completo en **este mismo repositorio, en el mismo commit** — cualquier persona
  que descargue un binario puede obtener su fuente aquí, como exige la GPL v3. Se conservan los
  avisos de copyright de Meshtastic en todas las fuentes.
- **Hardware**: los diseños de placas (cuando se publiquen) se licenciarán aparte
  (p. ej. **CERN-OHL**, Open Hardware License). Este repositorio solo contiene firmware y
  documentación.

## Descargo de responsabilidad

- Este firmware se distribuye **SIN GARANTÍA DE NINGÚN TIPO** (ni implícita de comerciabilidad
  ni de aptitud para un fin concreto), bajo los términos de la GPL v3. Úsalo **bajo tu propia
  responsabilidad**.
- Un repetidor solar es un dispositivo que se instala en altura, con baterías y alimentación
  solar: el montaje, el dimensionado de batería/panel y el mantenimiento son responsabilidad
  del instalador. Respeta las advertencias del manual de uso.
- **Uso del espectro radioeléctrico**: los builds están configurados para la banda EU_868 con
  el preset LoRa SFNarrow. Verifica que el uso de la frecuencia y la potencia cumplen la
  normativa de tu país antes de transmitir.
- La configuración por defecto (canales, PSK pública del canal Navadmin, claves admin
  públicas) está pensada para la malla de este proyecto: revísala antes de desplegar tus
  propios nodos.
- Los nodos conectados a una malla pública pueden ser visibles para terceros: no envíes por
  radio información sensible.

---

# NavaTastic (English)

Firmware **NavaTastic** � a [Meshtastic](https://meshtastic.org) v2.7.26 fork (base `54e0d8d`)
for **solar-powered infrastructure repeaters** on the **SFNarrow** LoRa preset (EU_868,
national preset used in Spain). A single repository produces **12 different firmwares**
(6 boards/radios � 2 branches: Routers / Clients).

## What NavaTastic adds on top of Meshtastic

| Area | What it does |
|---|---|
| **Solar resilience** | Boot brownout protection, LPCOMP wake-up with hysteresis, storm hibernation, battery chemistries (`lipo/nimh/sodium/lifepo4` per board), configurable cutoff and wake voltage |
| **Battery cycle notices** | `[Sueno]` / `[Vivo]` / `[Listo]` / `[Boot]` messages on the Navadmin channel: the node announces before sleeping (~1 mA), when waking up on solar/external reset, and every reboot **with its cause** (watchdog, reset pin, soft, brownout...) |
| **NavaCLI `/nava`** | ~45 remote administration commands: telemetry, energy, favorites, blocklist, semi-permanent role, maintenance. Critical actions require PKI-encrypted DM; the Navadmin channel is read-only |
| **Flash protection** | Filtered node database writes, auto-favoriting of direct routers, hybrid eviction, no transmit-history writes |
| **Security** | Persistent admin accreditation across reboots, admin-key auto-recovery, orphan-favorite limit |
| **Semi-permanent role** | `set_role` persists in `/resilience.bin` and **survives factory reset** |

## The 12 builds

| Board | Radio | Branch 2 (Routers) | Branch 1 (Clients) |
|---|---|---|---|
| Promicro nRF52840 + E22P | E22P (12 dBm) | `navarrico_promicro_e22p_r2ig` | `navarrico_promicro_e22p_r1ig` |
| Promicro/Faketec + HT-RA62 | SX1262 (22 dBm) | `navarrico_faketec_sx1262_r2ig` | `navarrico_faketec_sx1262_r1ig` |
| Seeed Solar Node P1 | SX1262 (22 dBm) | `navarrico_seed_sx1262_r2ig` | `navarrico_seed_sx1262_r1ig` |
| Heltec T114 | SX1262 (22 dBm) | `navarrico_t114_sx1262_r2ig` | `navarrico_t114_sx1262_r1ig` |
| Xiao nRF52840 Kit | SX1262 (22 dBm) | `navarrico_xiao_kit_sx1262_r2ig` | `navarrico_xiao_kit_sx1262_r1ig` |
| Xiao nRF52840 Kit + E22P | E22P (12 dBm) | `navarrico_xiao_e22p_r2ig` | `navarrico_xiao_e22p_r1ig` |

Differences are declared per env/profile (never by editing code): TX power, OCV curves and
LPCOMP per board, role per branch, admin keys and Bluetooth per profile (`profiles/*.jsonc`).

### Hardware requirement: ADC divider 1M+1M (ratio 2.0)

For battery measurement and low-voltage protection to work, **NRF52 boards
(Promicro/Faketec/Albatastic/Xiaowa)** must measure the battery through a divider made of
**two 1 MO resistors** (ratio 2.0).

> **Different divider on your board?** Adjust it before compiling in
> `variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h` (macro `ADC_MULTIPLIER`, value
> `VBAT_DIVIDER_COMP`). **Important**: the same divider feeds the **LPCOMP** comparator, which
> decides the **low-battery resilience wake-up** � the `set_vwake` levels are calibrated for a
> 2.0 divider; with a different divider the node will wake at a different voltage (recalibrate
> `getActiveLpcompThreshold()` in `src/platform/nrf52/main-nrf52.cpp`).

### Battery chemistries

The 12 compiled builds default to **LiPo/Li-Ion** (cutoff 3500 mV on E22P / 3400 mV on SX1262).
The firmware supports **4 chemistries**, switchable **without recompiling** via `/nava set_chem`
(encrypted DM, persisted on the node):

| Chemistry | Cutoff | LPCOMP wake | Notes |
|---|---|---|---|
| **LiPo / Li-Ion** | 3500 mV | ~3.7 V | Default in the builds |
| **NiMH (3 cells)** | 3400 mV | ~3.7 V | |
| **Sodium (Na-Ion)** | 2600 mV | ~3.7 V | Max charge ~4.0 V |
| **LiFePO4** | 2800 mV | ~3.3 V | **Promicro and Faketec only** (rejected on Seed/Xiao/T114: their LPCOMP is fixed and they would never wake on solar) |

To **compile with a different default chemistry**: add
`"USERPREFS_BATTERY_CHEMISTRY_SODIUM": "true"` to the env profile
(`profiles/<BRANCH>_<Board>.jsonc`) and/or set the cutoff with
`"USERPREFS_LOW_BATTERY_SLEEP_THRESHOLD_MV"` (e.g. NiMH = 3400, LiFePO4 = 2800) before
`pio run`. Wake threshold and OCV curves are adjusted on the node with
`set_chem` / `set_vbat` / `set_vwake`.

### Bench test status

| Board | Status |
|---|---|
| **Promicro + E22P (R2IG)** | **Fully verified** (15/08): sleep/wake cycle, notices, ~1 mA sleep, LPCOMP wake, [Boot] |
| **Seeed Solar Node P1** | **Semi-tested** � full resilience cycle on the board still pending |
| **Heltec T114** | **PENDING**: test the firmware sleep/wake resilience cycle |
| Faketec HT-RA62, Xiao Kit, Xiao + E22P | Pending bench verification |

## Downloads (prebuilt firmware)

Latest release: **V2.6 (15/08/2026)** � definitive sleep/wake cycle, bench verified.

Layout in [`distribucion/`](distribucion/): `Rama 2 Routers` / `Rama 1 Clientes` �
`LIPO` (all) / `NIMH` (Faketec and XiaoKitI2c only) � `UF2` / `OTA`.

| | Rama 2 Routers | Rama 1 Clientes |
|---|---|---|
| **UF2 (LIPO)** | [download](distribucion/Rama%202%20Routers/LIPO/UF2/) | [download](distribucion/Rama%201%20Clientes/LIPO/UF2/) |
| **OTA (LIPO)** | [download](distribucion/Rama%202%20Routers/LIPO/OTA/) | [download](distribucion/Rama%201%20Clientes/LIPO/OTA/) |
| **UF2 (NIMH)** | [download](distribucion/Rama%202%20Routers/NIMH/UF2/) | [download](distribucion/Rama%201%20Clientes/NIMH/UF2/) |
| **OTA (NIMH)** | [download](distribucion/Rama%202%20Routers/NIMH/OTA/) | [download](distribucion/Rama%201%20Clientes/NIMH/OTA/) |

### Manuals (PDF, Spanish)

- [Remote administration manual `/nava` (PDF)](docs/pdf/Manual_NavaTastic.pdf)
- [Firmware user manual (PDF)](docs/pdf/Manual_uso_NavaTastic_4.2.pdf)

## Flashing

- **nRF52 (all boards)**: put the node in DFU mode (double-press reset) � a **NICENANO** drive
  appears � copy the `.uf2` into it. Alternative: `pio run -e <env> -t upload
  --upload-port COMx`.
- **After flashing a factory-new node**: perform **one factory reset** to materialize the
  Navadmin channel (notices and open-channel queries depend on it).
- **Key backup**: flashing by itself **keeps** the node `/prefs` (keys, channels, name). If you
  are going to **factory reset** (or `nrf erase`), **export the node configuration and keys
  from the Meshtastic app first** if you want to restore them: the reset **erases the node
  keys**, and an `nrf erase` regenerates them (other nodes will need to re-learn your new key
  for encrypted DM).
- **Bench testing**: the E22P TX is unstable over USB (current spikes) � use **1 dBm TX**; the
  low-battery detection requires powering **without USB**.

## Building

Requires PlatformIO (`pio`). From the repo root:

```bash
pio run -e navarrico_promicro_e22p_r2ig   # one env
pio run                                    # all 12 (default_envs)
```

See also `Guia_para_agente_sobre_NavaTastic.md` (full mechanics),
`docs/transfer_context.md` (technical memory) and `docs/cerebro/` (design docs) � Spanish.

## Private branch (own keys)

The **Propia** builds (`r2ip`/`r1ip`) use your own admin keys and Bluetooth PIN, which are
**not stored in this repository** � they are asked at build time:

```powershell
.\build_propia.ps1 -EnvName navarrico_promicro_e22p_r2ip
```

## Security

- The Navadmin channel uses Meshtastic's **public PSK** (`{0x01}`, slot 1): anyone can listen.
  It only accepts read-only queries; critical commands require **PKI-encrypted DM**.
- Admin keys shipped in the firmware are **public** (the binary never contains private keys).

### Admin key management (highly recommended)

The firmware ships with a **factory admin key** (the project's key). For your own network:

1. **Add TWO of your own remote-management keys** (from your control devices) via the
   Meshtastic app ? *Radio config ? Security ? Admin key* (3 slots available).
2. **Verify they work**: from each control device send a `/nava` command over DM � it must
   respond (the repeater accredits that device as admin and saves it to disk).
3. **De-authorize the factory key** once yours are verified: put **one of your keys in slot 0**
   (replacing the factory key). Note: if slot 0 is left **empty**, the firmware **re-injects
   the factory key on every boot** (anti-lockout auto-recovery) � leaving it empty does NOT
   de-authorize it; you must overwrite it with your own.

## License

- **Firmware (code in this repository)**: **GPL v3** � inherited from
  [meshtastic/firmware](https://github.com/meshtastic/firmware), of which NavaTastic is a fork.
  See [LICENSE](LICENSE). NavaTastic changes are published under the same license.
- **GPL compliance**: the binaries distributed under [`distribucion/`](distribucion/) have
  their full source code in **this same repository, at the same commit** � anyone downloading
  a binary can get its source here, as GPL v3 requires. Meshtastic copyright notices are kept
  in all sources.
- **Hardware**: board designs (when published) will be licensed separately (e.g. **CERN-OHL**,
  Open Hardware License). This repository contains firmware and documentation only.

## Disclaimer

- This firmware is distributed **WITHOUT WARRANTY OF ANY KIND** (not even implied
  merchantability or fitness for a particular purpose), under the terms of GPL v3. Use it
  **at your own risk**.
- A solar repeater is a device installed at height, with batteries and solar power: mounting,
  battery/panel sizing and maintenance are the installer's responsibility. Follow the user
  manual warnings.
- **Radio spectrum use**: the builds are configured for the EU_868 band with the SFNarrow
  LoRa preset. Check that the frequency and power comply with your country's regulations
  before transmitting.
- The default configuration (channels, public Navadmin PSK, public admin keys) is meant for
  this project's mesh: review it before deploying your own nodes.
- Nodes connected to a public mesh may be visible to third parties: do not send sensitive
  information over the air.