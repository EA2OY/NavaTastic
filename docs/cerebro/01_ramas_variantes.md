# 01 — Ramas y Variantes

> **ESTADO 14/08/2026 — REPO UNIFICADO**: la estructura de 24 carpetas de 4.3 quedó
> **OBSOLETA**. En el repo único (`C:\NavaTastic Codigo completo`) las ramas/placas son
> **envs** `navarrico_<placa>_<radio>_<rama>` en `variants/nrf52840/navarrico.ini`
> (6 placas/radios × R2IG/R1IG) y los valores (claves, canal Navadmin, rol, BT) viven en
> `profiles/<RAMA>_<Placa>.jsonc`. La tabla de variantes de abajo (potencias, cortes,
> LPCOMP, envs por hardware) **sigue vigente** como referencia de valores por placa.
> El workaround MAX_PATH de R1 (r1promic/r1xiaoki) quedó absorbido en
> `custom_meshtastic_libdeps_map` (paridad 12/12, ver BITACORA F4/F5).

## Separación
- **Rama 1 Clientes** (`Rama 1 Clientes en Infraestructura\`): nodos de infraestructura que NO son routers (rol CLIENT). **Creada 12/08 desde copia del operador de Rama 2** (renombrada R1). Sufijos `R1IG`/`R1IP`, 6 variantes por rama + `UF2\`/`OTA\`. Normas vs R2 en la subnota `11_rama1_plan.md` (rol CLIENT + rol semi-permanente en resilience.bin; el resto idéntico a R2). ⚠️ MAX_PATH: Promicro×2 y E22P×2 llevan `libdeps_dir`/`build_dir` cortos en su `platformio.ini` (ver subnota 11 §4 y error #13).
- **Rama 1** (`Rama 1 General\` de 4.2, resiliencia física original): **NO migrada a 4.3** — el plan original de recreación evolucionó a "Rama 1 Clientes" (esta). Histórico en `C:\Firmware Navarrico 4.2\Rama 1 General`.
- **Rama 2** (`Rama 2 Infraestructura\`): hereda Rama 1 + protección absoluta de Flash. `USERPREFS_NODEDB_RAM_ONLY`, auto-favoritos routers directos en RAM, bypass límite hops, desalojo híbrido. Se subdivide en **dos ramas de infraestructura**:
  - `Infraestructura General\` → 6 variantes con sufijo `R2IG`. **ACTIVA desde 2026-08-12**: el operador copió las 6 carpetas desde Propia y se aplicaron las normas de `09_general_vs_propia.md` (K0=Master Node, 1 sola clave, BT 654321, canal Navadmin homogeneizado). La copia de carpetas la hace SIEMPRE el operador.
  - `Infraestructura Propia\` → 6 variantes con sufijo `R2IP`. Rama de referencia (con fixes LPCOMP aplicados).

## Variantes activas Rama 2 (6, idénticas entre General y Propia salvo sufijo de carpeta)
| Variante | Carpeta | Env | Hardware | Pot. máx | Corte | LPCOMP |
|---|---|---|---|---|---|---|
| Promicro fix | `Promicro NRF52+E22P NavTastic 2.7.26 R2I[G/P]` | `nrf52_promicro_diy_tcxo` | nRF52840 + E22P/TCXO | 12 | 3.5V | `9_16` |
| Faketec PROPIA | `Faketec NavTastic 2.7.26 R2I[G/P]` | `nrf52_promicro_diy_tcxo` | nRF52840 + HT-RA62 (SX1262) | 22 | 3.4V | `9_16` |
| Seed Studio P1 | `Seed Solar Node P1 NavTastic 2.7.26 R2I[G/P]` | `seeed_solar_node` | Seeed Solar Node (SX1262) | 22 | 3.4V | `3_8` |
| Heltec T114 | `Heltec T114 NavTastic 2.7.26 R2I[G/P]` | `heltec-mesh-node-t114` | Heltec T114 (SX1262) | 22 | 3.4V | `2_8` |
| Xiao E22P | `XiaoKitI2c+E22P NavTastic 2.7.26 R2I[G/P]` | `seeed_xiao_nrf52840_kit_i2c` | Xiao + E22P | 12 | 3.5V | `3_8` |
| Xiao Kit i2c | `XiaoKitI2c NavTastic 2.7.26 R2I[G/P]` | `seeed_xiao_nrf52840_kit_i2c` | Xiao + OLED I2C (SX1262) | 22 | 3.4V | `3_8` |

## Diferencias permitidas entre variantes
`set_txpower` (0-12 E22P / 0-22 SX1262), pin de radio (`RADIO_POWER_ENABLE_PIN` en E22P; SX1262 por SPI/driver), valores de variante. **No tocar ADCs de fábrica.**

**⚠️ XiaoKitI2c y XiaoKitI2c+E22P comparten env** (`seeed_xiao_nrf52840_kit_i2c`); la diferencia E22P/SX1262 vive en el `variant.h` de la carpeta `seeed_xiao_nrf52840_kit` de **cada repo**. Compilar siempre dentro de la carpeta de la variante; no mezclar binarios entre ambas carpetas.

## Rutas clave (estructura 4.3)
- Código + binarios: `Rama 2 Infraestructura\Infraestructura <General|Propia>\<Carpeta variante>\`
- UF2/OTA por rama: `Infraestructura <General|Propia>\UF2\` y `OTA\`
- Compilados por variante: `<Carpeta variante>\Compilados\` (o `compilados\`)
- Scripts IA: `HerramientasPropiasIA\` (`distribuir_binarios.ps1`, `generar_pdf.ps1`)
- Archivado histórico: proyecto `C:\Firmware Navarrico 4.2\` (antiguo) — no afecta activos. `old\` (variantes deprecated) queda en `C:\Firmware Navarrico 4.2\Rama 2 Infraestructura\Codigo Rama 2\old\`.
- `default_envs = tbeam` en los 6 `platformio.ini` → falla (toolchain ESP32). **Compilar siempre con `-e`**.
