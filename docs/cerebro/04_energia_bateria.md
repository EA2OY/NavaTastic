# 04 — Energía y Batería

> **ESTADO 14/08/2026 — REPO UNIFICADO**: contenido **VIGENTE** (la receta de resiliencia
> no cambió). En el repo único: `src/main.cpp` (pre-check ~533), `src/platform/nrf52/
> main-nrf52.cpp` (`getActiveLpcompThreshold()` ~610 con los `#ifdef` por placa; storm
> ~663-730), `src/Power.cpp` (OCV/químicas ~1983). Los `#ifdef` de placa (SEEED_SOLAR_NODE,
> SEEED_XIAO_NRF52840_KIT, HELTEC_T114) los define el env. Las químicas/divisores de la
> tabla **siguen vigentes**.

## COMPORTAMIENTO ACTUAL DEL CICLO SUEÑO/DESPERTAR (V3 / 17/08/2026)

> Esta sección manda sobre el ciclo completo de resiliencia solar.

**Los 5 avisos automáticos** (canal Navadmin slot 1, gate `/nava sleepmsg`, contenido =
`ADC X mV | CPU X.X C` — solo sensores internos, nada de I2C):

| Aviso | Banda / Cuándo | Comportamiento |
|---|---|---|
| **`[Listo]`** | Despertar con $V \ge \text{corte}$ (LPCOMP o reset) | Despertar limpio por recuperación solar $\rightarrow$ Opera con normalidad continua. |
| **`[Vivo]`** | Reset con $V \in [\text{corte}−100, \text{corte})$ (Nivel 1: $3.30\text{V}-3.40\text{V}$) | Avisa y **OPERA 160s (8 lecturas)** — si no sube, se duerme con `[Sueño]`. |
| **`[Critico]`** | Reset con $V < \text{corte}−100$ (Nivel 2: $< 3.30\text{V}$) | Avisa con texto `bateria en capacidad critica, operando 160s` y **OPERA 160s (8 lecturas)** — permite seguir la rampa solar y se duerme con `[Sueño]` de forma limpia. |
| **`[Sueño]`** | Monitor runtime confirma $V < \text{corte}$ (8 lecturas, ~160s) | Avisa + duerme **TODO** (`doDeepSleep`: radio SX1262 a sleep por SPI + GPS + LED off + System OFF) $\rightarrow$ **0.4 mA** (Faketec) / **1.5 mA** (E22P+Booster). |
| **`[Boot]`** | Cualquier arranque NO venido del ciclo de sueño, a los **2 min de uptime** | Aviso con causa del reset (`RESETREAS`: WDT/RESETPIN/SOFT/LOCKUP/LPCOMP/VBUS) + etiqueta `NAVA V3`; el retraso de 2 min es el anti-bucle. |

**Bandas del pre-check al despertar de sueño** (gate = corte OCV, no LPCOMP):
* $V \ge \text{corte}$ $\rightarrow$ Boot normal $\rightarrow$ **`[Listo]`**. (El comparador hardware LPCOMP a ~3.71V teórico / ~3.75-3.80V real siempre despierta en esta banda).
* $V \in [\text{corte}−100, \text{corte})$ $\rightarrow$ Boot $\rightarrow$ **`[Vivo]`** (Nivel 1) $\rightarrow$ 8 lecturas $\rightarrow$ `[Sueño]`.
* $V < \text{corte}−100$ $\rightarrow$ Boot $\rightarrow$ **`[Critico]`** (Nivel 2) $\rightarrow$ 8 lecturas $\rightarrow$ `[Sueño]`.

**Consumos reales medidos en banco de laboratorio**:
* **Faketec SX1262 / Promicro DIY**: **`0.4 mA`** en Deep Sleep / System OFF.
* **NRF52840 + E22P + Booster 5V**: **`1.5 mA`** en Deep Sleep / System OFF (con el booster de 5V alimentando el transceptor).

**Dormir TODO**: el sueño diferido de NavaCLI ejecuta `doDeepSleep(portMAX_DELAY, false,
true)` (la misma puerta que Eclipse V1), asegurando que la radio SX1262 reciba la orden `RadioInterface::sleep()`
por SPI y que el LED de estado se apague antes de entrar en `sd_power_system_off()`.

## REFERENCIA HISTÓRICA — Eclipse V1 (12/08) vs V2

- **Eclipse V1 (lo que funcionaba)**: sin mensajes de aviso. Batería baja → PowerFSM →
  `doDeepSleep` → apagado completo (~1 mA). El pre-check llamaba `readPowerStatus()` sin
  force, que SÍ pre-cargaba el contador (quirk); el dormir era el correcto, el ritmo no
  (dormía a los ~20s tras un arranque con batería baja, en silencio).
- **V2.4 intermedio (descartado)**: [Vivo] con re-sueño inmediato (~8s) y `cpuDeepSleep`
  directo → dormía "enseguida", LED enclavado, y las SX1262 no apagaban la radio.
- **V2.6 (actual)**: [Vivo] opera → monitor ~100s → [Sueño] → `doDeepSleep` completo +
  LED off → ~1 mA. = comportamiento de Eclipse + los avisos encima.
- **V3 (15/08, F18)**: igual que V2.6 con el contador unificado a **8 lecturas (~160s)**
  para las 6 placas (umbral del perfil `USERPREFS_LOW_BATTERY_READINGS_COUNT`; Power.cpp
  ya no usa `#ifdef` asimétrico `>4`/`>10`).

## PORQUÉ DE TODA LA RESILIENCIA ENERGÉTICA (brownout de ascenso solar)
**Problema documentado (Nordic nRF52 y también ESP32)**: si la batería se sobredescarga el nodo se apaga; al día siguiente el regulador solar empieza a ascender su tensión y se genera un estado **inestable en el MCU que lo deja bloqueado**: no responde ni al botón de reset, solo lo saca un corte limpio de corriente. Es el escenario que motiva todas las medidas de NavTastic:
- `waitUntilPowerLevelSafe()` (main.cpp:285) + `powerHAL_isPowerLevelSafe()` (main-nrf52.cpp:100): **espera en bucle** hasta que VDD ≥ umbral + histéresis (default 2.7 V + 0.2 V; T114 sin override usa el default) **antes** de inicializar radio/flash/softdevice → evita bootear en el rango inestable.
- `powerHAL_platformInit()` POFCON a 2.2 V (último recurso).
- Pre-check de batería (abajo) evita encender radio con batería baja (pico de corriente → brownout de arranque).
- LPCOMP `3_8`/`2_8` (despertar por tensión con histéresis) evita arrancar a tensión inestable y volver a bloquearse.

## Pre-check de batería (main.cpp)
Mide 8 lecturas espaciadas 200ms (V3/F18: `USERPREFS_LOW_BATTERY_READINGS_COUNT=8`); si están por debajo de `USERPREFS_LOW_BATTERY_SLEEP_THRESHOLD_MV` → `cpuDeepSleep(portMAX_DELAY)` antes de inicializar radio/flash (evita brownout de arranque).

## LPCOMP (main-nrf52.cpp, `cpuDeepSleep()`)
- Histéresis `NRF_LPCOMP_HYST_ENABLED` (50mV), divisor mantenido (`ADC_CTRL`), umbral dinámico `getActiveLpcompThreshold()`.
- **NUNCA** `sd_power_system_off()` para storm (no despierta por timer). Storm usa RTC2 + LOWPWR + `sd_app_evt_wait()`.
- **`delay(3000)` antes de armar el LPCOMP — propósito confirmado por el operador (2026-08-11)**: dar tiempo a **leer el voltaje de la batería** y **programar el LPCOMP con éxito**. Tras apagar la radio, la tensión se recupera del pico de consumo; armar el comparador durante el transitorio causaría re-despertar inmediato con batería baja → bucle despertar/dormir (p. ej. con reset externo ATtiny). **NO eliminar.** Origen: Rama 1 (4.2). También en `transfer_context.md` §4.A.
- **Divisor Promicro/Faketec**: estándar actual = **2×1M (0.5)**. La referencia antigua 2×10MΩ (`Descartables\report_fix.md`) está **deprecated**.

## Químicas de batería (`/nava set_chem`)
| Química | Corte | vwake | Despertar real |
|---|---|---|---|
| lipo | 3500 | 3 | ~3.71V |
| nimh | 3400 | 3 | ~3.71V |
| sodium | 2600 | 3 | ~3.71V |
| lifepo4 | 2800 | **5** | **~3.30V** |

## Niveles `set_vwake` (divisor 0.5, VDD 3.3V)
1=`5_16`~2.06V, 2=`3_8`~2.48V, 3=`9_16`~3.71V (default LiPo/NiMH/Sodio), 4=`11_16`~4.54V, 5=`4_8`~3.30V (LiFePO4).

> ⚠️ Estos niveles SOLO aplican a variantes con divisor 0.5 (Promicro/Faketec). Seed/Xiao/T114 usan umbral fijo de fábrica (ver fixes abajo). Tabla completa de divisores reales en `transfer_context.md` sección 6.

## Crítico
- `Power::OCV[11]` debe inicializarse con `OCV_ARRAY` (bug corregido afectaba a las 6).
- Aislamiento SPI: `SPI.end()`/`SPI1.end()` antes de dormir. E22P fuerza `RADIO_POWER_ENABLE_PIN` LOW.
- `main-nrf52.cpp` es COMÚN a las 6; manejo de radio resuelto con `#ifdef RADIO_POWER_ENABLE_PIN`. No eliminar esos bloques.

## ⚠️ Fix Seed Solar Node P1 (2026-08-11) — divisor de placa ≠ 0.5
Los niveles `set_vwake` 1-5 de `getActiveLpcompThreshold()` están calibrados al divisor **0.5 (1M/1M) del Promicro**. El Seed Solar Node P1 usa divisor de placa distinto (`ADC_MULTIPLIER 3.3` → ~0.303), por lo que el default `9_16` pedía **~5.5V** → el nodo **no despertaría nunca por solar** tras un System OFF.
**Fix**: en `getActiveLpcompThreshold()` (main-nrf52.cpp), bajo `#ifdef SEEED_SOLAR_NODE` se devuelve `BATTERY_LPCOMP_THRESHOLD` (`3_8` fijo, ~3.67V real) — el umbral de la referencia funcional (port 24/07). El resto de variantes conserva la lógica dinámica. Aplicado SOLO en la carpeta Seed R2IP y distribuido (UF2/OTA Propia, 11/08).
**Ojo**: en el Seed, `set_vwake`/`set_chem` no cambian el umbral real de despertar (siempre 3_8); LiFePO4 despertaría a ~3.67V (tarde, pero despierta). Para calibrar niveles reales del Seed haría falta medir P0.31 a tensión conocida.

**Medición de laboratorio (operador, añadido 15/08 — ronda auditoría externa)**: el Seed despertaba a **~3,8 V** con el firmware 4.2 probado en banco (mismo `3_8` + `HYST_NOHYST`). → El divisor efectivo del pin LPCOMP es **~0,326**, NO ~0,303 como deducía la teoría desde `ADC_MULTIPLIER 3.3` (L27: la medición manda sobre la teoría; el multiplicador incluye calibración/tolerancias). El repo actual usa el mismo `3_8` con `HYST_ENABLED` (50 mV) → despierta ~3,85 V. El `3670` del aviso [Sueño] (`navaGetLpcompWakeMv`) es informativo y conservador; **NO aplicar la propuesta de auditoría de cambiarlo a 4084** (sería falso: ~300 mV por encima del real). Pendiente opcional: re-medir en banco y fijar ~3800.

## ⚠️ Fix Heltec T114 (2026-08-11) — divisor de fábrica 100/490
Mismo bug: el `getActiveLpcompThreshold()` está calibrado al divisor 0.5 del Promicro, pero el T114 usa **divisor 100/490 (~0.204)** (de fábrica, `ADC_MULTIPLIER 4.916`). El default `9_16` pedía **~9.1V** → no despertaría por solar.
**Fix**: bajo `#ifdef HELTEC_T114` se devuelve `BATTERY_LPCOMP_THRESHOLD` (`2_8`, ~4.04V real) — valor de fábrica Meshtastic (verificado en `C:\Users\Jesus\Desktop\firmware` y repo oficial). Aplicado en la carpeta T114 R2IP y distribuido (UF2/OTA Propia, 11/08).
**⚠️ Nota**: Meshtastic deja `BATTERY_LPCOMP_INPUT` **desactivado** en el T114 de fábrica por **fuga de 2.9mA en System OFF** (issue #8801). El fork lo activa a propósito (despertar solar). La fuga solo ocurre DORMIDO (el divisor+LPCOMP se arman en `cpuDeepSleep`); despierto no cuesta. Coste aceptado por diseño de resiliencia (evitar brownout de ascenso).

## ⚠️ Fix Xiao Kit i2c y Xiao E22P (2026-08-11) — divisor de fábrica 1M/510k
Mismo bug que el Seed: el `getActiveLpcompThreshold()` está calibrado al divisor 0.5 del Promicro, pero los Xiao usan **divisor de fábrica 1M/510k (0.3377)** (esquemático oficial Seeed: R16=1M, R17=510k; `ADC_MULTIPLIER 3`). El default `9_16` pedía **~5.5V** → no despertarían por solar.
**Fix**: bajo `#ifdef SEEED_XIAO_NRF52840_KIT` (macro de ambos envs Xiao) se devuelve `BATTERY_LPCOMP_THRESHOLD` (`3_8`, ~3.67V real) — el valor de fábrica de Meshtastic (verificado en `C:\Users\Jesus\Desktop\firmware`, original 2.7.26: `c.reference = BATTERY_LPCOMP_THRESHOLD` + `HYST_NOHYST`). Aplicado en `XiaoKitI2c` y `XiaoKitI2c+E22P` de Propia y distribuido (UF2/OTA, 11/08). Los dos Xiao comparten `variant.h`/env → el fix cubre ambos.

**Verificado en campo (11/08/2026, operador)**: Xiao Kit i2c despierta a **~3.8V real** (`3_8` teórico ~3.67V; encaja por tolerancias resistivas 1M/510k + histéresis LPCOMP 50mV + sag en el instante de despertar). Xiao+E22P igual de bien. **NO tocar los Xiao (ni divisor ni umbral).** T114 (`2_8` ~4.04V) y Seed (divisor stock ADC 3.3 vs 3.0 del Xiao; el pin LPCOMP puede no seguir el multiplicador ADC) sin verificar en campo: mismo test de banco (fuente regulable + anotar V de despertar) para confirmarlos.

**Verificado en banco (15/08, operador)**: ciclo V2.6 completo OK en **Xiao Kit i2c (SX1262)** y **Xiao Kit i2c + E22P** ([Sueño]/dormir/despertar/avisos). El "fallo" inicial del Xiao+E22P era el pico de consumo del E22P en TX (L29): en banco con fuente justa usar TX bajo; con SX1262 no hace falta (decisión del operador: 22 dBm siempre).
