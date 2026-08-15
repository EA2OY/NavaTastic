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
