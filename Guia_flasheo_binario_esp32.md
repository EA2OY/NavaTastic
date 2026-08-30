# Guía de flasheo de binarios en placas Heltec V3 / V4 (ESP32-S3)

> Documento público. Complementa a la [guía de compilación](Compilar_NavaTastic.md): aquí se
> flashean los **binarios ya compilados** que se descargan de [Releases](https://github.com/EA2OY/NavaTastic/releases).

## Qué es cada fichero

En la descarga de cada placa y rama hay **dos binarios**:

| Fichero | Qué contiene | Cuándo usarlo |
|---|---|---|
| `...FACTORY.bin` | Todo en uno: bootloader + tabla de particiones + firmware | **Recomendado** — funciona siempre |
| `...APP.bin` | Solo el firmware | Solo si la placa ya tiene bootloader y particiones (p. ej. ya flasheada antes) |

Sufijos: **R2IG** = Router repetidor fijo · **R1IG** = Cliente convertible a Router.
Los binarios son específicos por placa: **no intercambiar V3 y V4**.

## Requisitos

- La placa Heltec **V3** o **V4** (ESP32-S3) y un cable USB de datos.
- Windows (método A) o Chrome/Edge con Web Serial (método C).
- Saber el puerto COM de la placa (Administrador de dispositivos → Puertos, o `esptool chip_id`).

## Paso 0 — Modo programación (obligatorio)

1. **Mantén pulsado el botón PRG/BOOT** de la placa.
2. Con el botón pulsado, **conecta el cable USB**.
3. Suelta el botón: la placa queda en modo programación (aparece el COM).

> ⚠️ El auto-reset por USB-JTAG no siempre funciona: si tras flashear la placa no arranca,
> **pulsa RESET a mano** (o desconecta y reconecta sin pulsar PRG).

## Método A — Espressif Flash Download Tool (Windows, verificado)

1. Descarga **Flash Download Tools** desde la página oficial de Espressif
   (https://www.espressif.com/en/support/download/other-tools).
2. Abre `ESP32FlashDownloadTool.exe` → selecciona el chip **ESP32-S3**.
3. Añade los ficheros con sus **direcciones**:
   - `FACTORY.bin` → dirección **`0x0`**
   - (o, si usas solo el `APP.bin`: dirección **`0x10000`**)
   - ⚠️ **NUNCA uses `0x1000`** — deja la imagen rota.
4. Selecciona el puerto COM de la placa, baud 115200 (o 921600) y pulsa **START**.
5. Al terminar: pulsa **RESET** en la placa.

## Método B — esptool (Python, cualquier sistema)

```bash
pip install esptool
esptool --port COMx write_flash 0x0 HeltecV4.NavTastic.2.7.26.V5.4.3.4.r2ig.FACTORY.bin
```

Cambia el puerto (`COMx` en Windows, `/dev/ttyACM0` en Linux/macOS) y el nombre del binario.

## Método C — Web (Chrome/Edge con Web Serial)

1. Abre la herramienta web de Espressif: **https://esptool.github.io/esptool-js/**.
2. **Connect** → elige el puerto de la placa.
3. Añade el binario con su offset: `FACTORY.bin` → **0x0** (o `APP.bin` → 0x10000).
4. **Program** y espera. Al terminar: pulsa **RESET** en la placa.

## Verificación

- La pantalla OLED debe arrancar (logo → interfaz) y el nodo emitir su NodeInfo a los pocos
  minutos (visible en la app de Meshtastic).
- Si la placa se queda en el splash o reinicia en bucle, **repite el flasheo con el modo PRG
  bien aplicado** (es el síntoma típico de un binario viejo que el auto-reset no llegó a cargar).

---

# Flashing guide for Heltec V3 / V4 boards (ESP32-S3) (English)

> Public document. Companion to the [build guide](Compilar_NavaTastic.md): here you flash the
> **pre-built binaries** downloaded from [Releases](https://github.com/EA2OY/NavaTastic/releases).

## What each file is

Each board/branch download has **two binaries**:

| File | Contents | When to use |
|---|---|---|
| `...FACTORY.bin` | All-in-one: bootloader + partition table + firmware | **Recommended** — always works |
| `...APP.bin` | Firmware only | Only if the board already has bootloader + partitions (e.g. flashed before) |

Suffixes: **R2IG** = fixed repeater router · **R1IG** = client convertible to router.
Binaries are board-specific: **do not swap V3 and V4**.

## Requirements

- A Heltec **V3** or **V4** board (ESP32-S3) and a data USB cable.
- Windows (method A) or Chrome/Edge with Web Serial (method C).
- The board's COM port (Device Manager → Ports, or `esptool chip_id`).

## Step 0 — Programming mode (required)

1. **Hold the PRG/BOOT button** on the board.
2. While holding it, **plug in the USB cable**.
3. Release the button: the board is now in programming mode (the COM port appears).

> ⚠️ Auto-reset via USB-JTAG does not always work: if the board does not start after
> flashing, **press RESET manually** (or unplug/replug without holding PRG).

## Method A — Espressif Flash Download Tool (Windows, verified)

1. Download **Flash Download Tools** from Espressif's official site
   (https://www.espressif.com/en/support/download/other-tools).
2. Open `ESP32FlashDownloadTool.exe` → select the **ESP32-S3** chip.
3. Add the files with their **offsets**:
   - `FACTORY.bin` → offset **`0x0`**
   - (or if using only `APP.bin`: offset **`0x10000`**)
   - ⚠️ **NEVER use `0x1000`** — it corrupts the image.
4. Select the board's COM port, baud 115200 (or 921600) and press **START**.
5. When done: press **RESET** on the board.

## Method B — esptool (Python, any OS)

```bash
pip install esptool
esptool --port COMx write_flash 0x0 HeltecV4.NavTastic.2.7.26.V5.4.3.4.r2ig.FACTORY.bin
```

Change the port (`COMx` on Windows, `/dev/ttyACM0` on Linux/macOS) and the binary name.

## Method C — Web (Chrome/Edge with Web Serial)

1. Open Espressif's web tool: **https://esptool.github.io/esptool-js/**.
2. **Connect** → choose the board's port.
3. Add the binary with its offset: `FACTORY.bin` → **0x0** (or `APP.bin` → 0x10000).
4. **Program** and wait. When done: press **RESET** on the board.

## Verification

- The OLED screen should boot (logo → UI) and the node should emit its NodeInfo within a few
  minutes (visible in the Meshtastic app).
- If the board stays on the splash or reboots in a loop, **repeat the flash with PRG mode
  properly applied** (typical symptom of an old binary the auto-reset failed to load).
