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

Código bajo la licencia de Meshtastic (GPL v3) — ver [LICENSE](LICENSE). Proyecto derivado de
[meshtastic/firmware](https://github.com/meshtastic/firmware).
