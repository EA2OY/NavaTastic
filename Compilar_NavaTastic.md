# Compilar NavaTastic (firmware)

Guía de compilación del firmware desde el código fuente. NavaTastic usa **PlatformIO**,
un entorno de desarrollo que permite compilar para varias placas desde un solo código
fuente y con herramientas centralizadas.

> Documento público; los enlaces del README y de los manuales apuntan aquí.

---

## Requisitos

- **Git** (en Windows: [Git for Windows](https://git-scm.com/download/win)).
- **PlatformIO**: como extensión de **Visual Studio Code** (recomendado) o en línea de
  comandos (`pip install platformio`).
- **Windows — rutas cortas**: la raíz del proyecto debe ser corta (evita el error
  "Filename too long" al descargar las librerías; el límite de Windows es 260 caracteres).
  Ejemplo: `C:\nava\NavaTastic`.

## Clonar el repositorio

```bash
git clone https://github.com/EA2OY/NavaTastic.git
cd NavaTastic
git submodule update --init
```

> `git submodule update --init` es **obligatorio**: descarga el submódulo de protobufs de
> Meshtastic, necesario para compilar.

> **Nota sobre la rama `main` pública**: contiene el árbol completo de la última release
> (un solo commit). Si compilas desde ahí obtendrás exactamente el firmware publicado en
> Releases. El desarrollo interno del proyecto usa el historial completo del repositorio.

## Actualizar

```bash
git pull --recurse-submodules
```

## Compilar

Abre la carpeta en **Visual Studio Code** (la primera vez PlatformIO descargará los
toolchains y las librerías; puede tardar bastante). Después:

1. `Ctrl+Shift+P` (Windows) / `command+Shift+P` (Mac) → **PlatformIO: Pick Project
   Environment** → elige uno de los **12 entornos** `navarrico_<placa>_<radio>_<rama>`
   (6 placas/radios × 2 ramas: Rama 2 Routers / Rama 1 Clientes — tabla completa en el
   [README](../README.md#los-12-builds)).
2. **PlatformIO: Build** → genera el firmware en `.pio/build/<env>/` (`.uf2` y `.zip` OTA).

O en línea de comandos, desde la raíz del repo:

```bash
pio run -e navarrico_promicro_e22p_r2ig   # un entorno
pio run                                    # los 12 (default_envs)
```

### Placas Heltec V3 / V4 (ESP32-S3)

Además de los 12 entornos nRF52, NavaTastic soporta **Heltec V3 y Heltec V4** (ESP32-S3) con
4 entornos más: `navarrico_heltec_v3_sx1262_r2ig` / `navarrico_heltec_v3_sx1262_r1ig` y
`navarrico_heltec_v4_sx1262_r2ig` / `navarrico_heltec_v4_sx1262_r1ig` (Router/Cliente).
Se compilan **solo con `-e` explícito** (no están en `default_envs`):

```bash
pio run -e navarrico_heltec_v4_sx1262_r2ig   # Heltec V4 Router
pio run -e navarrico_heltec_v3_sx1262_r1ig   # Heltec V3 Cliente
```

La salida son `.bin` (APP) y `.factory.bin` (bootloader + particiones + APP) en
`.pio/build/<env>/`. **Flasheo**: poner la placa en **modo programación** (mantener el botón
PRG al conectar por USB) y subir con PlatformIO; en ESP32 el auto-reset por USB-JTAG no
siempre funciona. Nota: la gestión de batería profunda (LPCOMP, químicas, storm) es
específica de las placas nRF52; en Heltec esos comandos quedan desactivados o con
comportamiento de plataforma.

## Flashear

Con el nodo conectado por USB: **PlatformIO: Upload** (con el entorno correspondiente).
Alternativa: copiar el `.uf2` a la unidad UF2 del bootloader si tu placa la monta en modo
DFU. Detalles en la sección [Flashear del README](../README.md#flashear).

## Builds Propia (claves propias)

Los envs `r2ip`/`r1ip` (12) usan **tus propias claves admin y PIN Bluetooth**, que **no se
almacenan en ningún fichero** del repositorio: se piden al compilar.

```powershell
.\build_propia.ps1 -EnvName navarrico_promicro_e22p_r2ip
```

O con las variables de entorno `NAVARICO_PROPIA_KEY_0`, `NAVARICO_PROPIA_KEY_1` y
`NAVARICO_PROPIA_BT` (el build aborta con instrucciones si faltan).

## Ajustes de hardware y otras placas

NavaTastic está pensado para las **6 placas soportadas** (tabla del
[README](../README.md#los-12-builds)). Los valores físicos (divisor ADC, potencia, LPCOMP)
viven en el `variant.h` de cada placa en `variants/nrf52840/...`; la selección por entorno
usa las macros `NAVARICO_RADIO_E22P`/`NAVARICO_RADIO_SX1262` y el perfil
`profiles/<RAMA>_<Placa>.jsonc`. **Aviso del divisor ADC** (factor 2.0 en las placas
Promicro/Faketec): sección [Requisito de hardware del README](../README.md#requisito-de-hardware-divisor-adc-1m1m-factor-20).

## Más documentación

- [Guia_para_agente_sobre_NavaTastic.md](Guia_para_agente_sobre_NavaTastic.md) — mecánica completa del repo (orientada a agentes).
- [transfer_context.md](transfer_context.md) — memoria técnica de comportamiento.
- [cerebro/](cerebro/) — documentación de diseño (en español).

---

# Building NavaTastic firmware (English)

How to build the firmware from source. NavaTastic uses **PlatformIO**, a development
environment that enables easy multi-platform development and centralized tooling.

> Public document; links from the README and manuals point here.

## Requirements

- **Git** (on Windows: [Git for Windows](https://git-scm.com/download/win)).
- **PlatformIO**: as a **Visual Studio Code** extension (recommended) or from the command
  line (`pip install platformio`).
- **Windows — short paths**: the repo root must be short (avoid the "Filename too long"
  error when downloading libraries; the Windows limit is 260 characters). Example:
  `C:\nava\NavaTastic`.

## Clone the repository

```bash
git clone https://github.com/EA2OY/NavaTastic.git
cd NavaTastic
git submodule update --init
```

> `git submodule update --init` is **required**: it fetches Meshtastic's protobufs
> submodule, needed to build.

> **Note about the public `main` branch**: it contains the full tree of the latest release
> (a single commit). Building from it produces exactly the firmware published in Releases.

## Update

```bash
git pull --recurse-submodules
```

## Build

Open the folder in **Visual Studio Code** (on first launch PlatformIO downloads toolchains
and libraries; this can take a while). Then:

1. `Ctrl+Shift+P` (Windows) / `command+Shift+P` (Mac) → **PlatformIO: Pick Project
   Environment** → choose one of the **12 environments** `navarrico_<board>_<radio>_<branch>`
   (6 boards/radios × 2 branches: Branch 2 Routers / Branch 1 Clients — full table in the
   [README](../README.md#the-12-builds)).
2. **PlatformIO: Build** → generates the firmware in `.pio/build/<env>/` (`.uf2` and OTA
   `.zip`).

Or from the command line, from the repo root:

```bash
pio run -e navarrico_promicro_e22p_r2ig   # one environment
pio run                                    # all 12 (default_envs)
```

### Heltec V3 / V4 boards (ESP32-S3)

Besides the 12 nRF52 environments, NavaTastic supports **Heltec V3 and Heltec V4** (ESP32-S3)
with 4 more environments: `navarrico_heltec_v3_sx1262_r2ig` / `navarrico_heltec_v3_sx1262_r1ig`
and `navarrico_heltec_v4_sx1262_r2ig` / `navarrico_heltec_v4_sx1262_r1ig` (Router/Client).
They build **only with an explicit `-e`** (they are not in `default_envs`):

```bash
pio run -e navarrico_heltec_v4_sx1262_r2ig   # Heltec V4 Router
pio run -e navarrico_heltec_v3_sx1262_r1ig   # Heltec V3 Client
```

Output is `.bin` (APP) and `.factory.bin` (bootloader + partitions + APP) under
`.pio/build/<env>/`. **Flashing**: put the board in **programming mode** (hold the PRG
button while connecting via USB) and upload with PlatformIO; on ESP32 the auto-reset via
USB-JTAG does not always work. Note: deep battery management (LPCOMP, chemistries, storm)
is nRF52-specific; on Heltec those commands are disabled or use platform behavior.

## Flash

With the node connected over USB: **PlatformIO: Upload** (with the matching environment).
Alternative: copy the `.uf2` to the UF2 bootloader drive if your board mounts one in DFU
mode. Details in the [Flashing section of the README](../README.md#flashing).

## Propia builds (own keys)

The `r2ip`/`r1ip` environments (12) use **your own admin keys and Bluetooth PIN**, which are
**not stored in any file** of the repository: they are asked at build time.

```powershell
.\build_propia.ps1 -EnvName navarrico_promicro_e22p_r2ip
```

Or with the environment variables `NAVARICO_PROPIA_KEY_0`, `NAVARICO_PROPIA_KEY_1` and
`NAVARICO_PROPIA_BT` (the build aborts with instructions if they are missing).

## Hardware adjustments and other boards

NavaTastic is designed for the **6 supported boards** ([README table](../README.md#the-12-builds)).
Physical values (ADC divider, power, LPCOMP) live in each board's `variant.h` under
`variants/nrf52840/...`; per-environment selection uses the `NAVARICO_RADIO_E22P`/
`NAVARICO_RADIO_SX1262` macros and the `profiles/<BRANCH>_<Board>.jsonc` profile.
**ADC divider notice** (2.0 factor on Promicro/Faketec boards):
[Hardware requirement section of the README](../README.md#hardware-requirement-adc-divider-1m1m-ratio-20).

## More documentation

- [Guia_para_agente_sobre_NavaTastic.md](Guia_para_agente_sobre_NavaTastic.md) — full repo mechanics (agent-oriented).
- [transfer_context.md](transfer_context.md) — technical behavior memory.
- [cerebro/](cerebro/) — design documentation (Spanish).
