# 08 — Instrumento de Diagnóstico LAB

> **ESTADO 14/08/2026 — REPO UNIFICADO**: **DESCARTADO y NO migrado** al repo único
> (no existe `src/NavarricoLog.*` en `C:\NavaTastic Codigo completo`). Se conserva solo
> como referencia histórica; los tests de banco se hacen con los envs reales
> (`navarrico_*`). Las **pruebas por USB con el CLI** (comandos validados, comportamiento
> config vs full reset) **siguen vigentes**.

> **⛔ DESCARTADO (2026-08-12, decisión del operador)**: instrumento antiguo creado para un problema puntual (bucle sleep del Promicro con fuente 3.8V). **NO usar** para test ni reproducción (además no incluye el rate-limit 30 s del canal Navadmin ni el guard de lifepo4 del build de producción). Test de banco se hace con variantes reales de Propia/General. Se conserva solo como referencia histórica.

Build de laboratorio para depurar la resiliencia (dormir/despertar) en el banco. **NUNCA desplegar en campo.**

## Ubicación
- Carpeta: `C:\Firmware Navarrico 4.3\LAB\Promicro NRF52+E22P NavTastic 2.7.26 LAB`
- Fork del Promicro R2IP (fuera de las ramas → `distribuir_binarios.ps1` no lo toca). Sin `.pio/.git/Compilados`.
- Binario: `C:\Firmware Navarrico 4.3\LAB\UF2LAB\Promicro NavTastic 2.7.26 LAB.uf2`

## Qué añade (vs el Promicro canónico)
1. **`src/NavarricoLog.h/.cpp`**:
   - Ring buffer en RAM (96 líneas × 160 B) → **0 desgaste flash** durante bucles.
   - Salida `[NAVA] ...` por USB en vivo con `Serial.flush()` cuando hay terminal.
   - Volcado a `/navalog.txt` en LittleFS (máx 8 KB, rotación) en eventos puntuales.
2. **Hooks** (todos con `#include "NavarricoLog.h"`):
   - `main.cpp` pre-check (`isLowNow` + System OFF): hasBat/hasUSB/mv + motivo.
   - `main-nrf52.cpp` `nrf52Setup()`: reset_reason + wake_level al arrancar.
   - `main-nrf52.cpp` `cpuDeepSleep()`: wake_level + lpcomp_ref + ms antes de `sd_power_system_off()`.
   - `Power.cpp` `readPowerStatus()`: low_voltage_counter + mv + cutoff.
   - `Router.cpp` `send()`: duty cycle (tx%, duty, silent mins).
   - `NavaCLIModule`: comando `/nava log` (DM PKI) → volcado del buffer.

## Cómo usarlo
1. Compilar: `pio run -e nrf52_promicro_diy_tcxo` **desde la carpeta LAB**.
2. Flashear `LAB\UF2LAB\Promicro NavTastic 2.7.26 LAB.uf2`.
3. USB conectado: ver `[NAVA]` en tiempo real antes de cada sueño y `[NAVA] BOOT reset_reason=...` en cada despertar.
4. Sin USB: el buffer queda en `/navalog.txt`; leer con `pio device monitor` o `/nava log`.

## Cómo leer `reset_reason` (NRF_POWER->RESETREAS)
`0x01`=PIN, `0x02`=NFC, `0x04`=DOG, `0x08`=DEBUG, `0x10`=OFF, `0x20`=LPCOMP, `0x40`=DIF, `0x80`=NFC(otro). Un `LPCOMP (0x20)` repetido indica despertar por batería (bucle).

## Investigación en curso (2026-08-11) — Promicro se duerme con fuente 3.8V
**Síntoma**: con fuente de laboratorio a 3.8 V el Promicro se duerme a poco de arrancar (System OFF) y no despierta; además recibió `Duty cycle limit exceeded` (EU_868 = 10%, >360 s TX/hora → sospecha de bucle dormir→despertar→reboot).
**Estado**: instrumento LAB creado y compilado SUCCESS. Pendiente:
- Flashear LAB y capturar `/navalog.txt` / `[NAVA]` en el bucle.
- Discriminar: pre-check (`Battery below... System OFF`) vs contador (`Low voltage detected, trigger deep sleep`) vs bucle LPCOMP.
- Verificar divisor real del Promicro (usuario: 2×1M → ADC 2.0) y si `AREF_VOLTAGE=3.6` (architecture.h nrf52) debe ser 3.0 para `AR_INTERNAL_3_0`.
- Referencia sana: Faketec R2IP (mismo env, funciona) y Estella desplegada (`.uf2` 07/08, pre-Secuencia 2).

## Referencia funcional del Seed (aportada 2026-08-11)
`C:\Firmware Navarrico 4.2\Rama 1 General\codigosolarnodep1\Seed Studio Node P1 Codigo\xiao kit i2c 2.7.26 sleep + sfn hardcode`
Fork funcional del que se portó la Seed (pre-NavaCLI): `c.reference = BATTERY_LPCOMP_THRESHOLD` fijo (`3_8`), `HYST_NOHYST`. Sirvió para el fix del Seed (ver `04_energia_bateria.md`).

## 🧪 Pruebas por USB con el CLI (procedimiento validado 2026-08-11)

Cualquier variante de Propia se puede conectar por USB y probar con el CLI de Meshtastic (`meshtastic.exe`, instalado en `C:\Users\Jesus\AppData\Local\Programs\Python\Python314\Scripts\`). El puerto se detecta con `Get-CimInstance Win32_SerialPort` (habitual `COM9`).

### Comandos validados (probados en Faketec R2IP)

```powershell
meshtastic --port COM9 --info                                  # estado: owner, firmware, rol, nodos
meshtastic --port COM9 --export-config                          # config YAML completa (canales, region, tiempos)
meshtastic --port COM9 --factory-reset                         # config reset (conserva vínculos BLE)
meshtastic --port COM9 --factory-reset-device                   # full reset (borra bonds BLE → pide vincular de nuevo)
meshtastic --port COM9 --reboot                                 # soft reset (los cambios del usuario PERSISTEN)
meshtastic --port COM9 --set lora.modem_preset MEDIUM_FAST      # cambiar preset
meshtastic --port COM9 --set device.node_info_broadcast_secs 300
meshtastic --port COM9 --set position.position_broadcast_secs 600
meshtastic --port COM9 --set-owner "Nombre" --set-owner-short "NOM"
meshtastic --port COM9 --ch-set name "NombreCanal" --ch-index 0  # cambiar nombre del canal
```

### Hechos verificados en hardware (11/08/2026)

1. **`--factory-reset` (config reset)**: conserva los vínculos BLE (no pide vincular de nuevo).
2. **`--factory-reset-device` (full reset)**: borra bonds BLE → la app pide vincular de nuevo → correcto. Tras él, el nodo vuelve a fábrica Navarrico (nombre default, SFNarrow EU_868, canal 4/869.618 MHz, claves K0/K1 de fábrica re-inyectadas).
3. **`--reboot` (soft reset)**: los cambios del usuario (nombre, preset, canal, tiempos) **persisten** — confirma la regla "solo factory reset re-inyecta defaults, los reinicios normales respetan lo guardado".
4. **Claves admin**: el CLI **NO expone** `security.admin_key` (ni set ni get — el export YAML muestra `adminKey:` vacío por diseño). Para verificar claves: usar `/nava admin_ls` desde un mando acreditado, o `/nava` por DM PKI.
5. **Auto-recuperación de claves**: si la clave del slot 0 queda vacía, en **cada boot** se re-inyectan K0/K1 de fábrica (`NodeDB.cpp`, `local_sum==0` → `USERPREFS_USE_ADMIN_KEY_0/1`). Ver `02_claves_admin.md`.

### Notas

- Si el nodo está conectado por BLE a la app, no conectar simultáneamente por USB para pruebas de escritura (evitar conflictos).
- `--factory-reset-device` es el que reprodujo el bug del reset de fábrica (issue #10851); ya corregido con el fix #10873 (disableBluetooth después del reset) — ver `cerebro.md` log.
