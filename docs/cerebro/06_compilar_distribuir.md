# 06 — Compilar y Distribuir

> **ESTADO 14/08/2026 — REPO UNIFICADO**: el flujo de abajo quedó **OBSOLETO en mecánica**
> (la estructura 4.3 de carpetas desapareció). En el repo único, desde la raíz:
> `pio run -e navarrico_<placa>_<radio>_<rama>` (12 envs, ver `navarrico.ini`) o
> `.\build.ps1 -EnvName <env> [-Distribuir]` / `.\distribuir.ps1 -Todo` (→ `distribucion\`
> con los 32 ficheros históricos) / `.\verificar_paridad.ps1` (paridad MD5 12/12).
> `default_envs = tbeam` ya no existe. Errores conocidos #13/MAX_PATH → resueltos de raíz
> en el repo (raíz corta + `custom_meshtastic_libdeps_map`).

## Comandos de build (SIEMPRE con `-e`, desde la carpeta de la variante)
```bash
pio run -e nrf52_promicro_diy_tcxo        # Promicro fix, Faketec PROPIA
pio run -e seeed_solar_node               # Seed Studio P1
pio run -e heltec-mesh-node-t114          # Heltec T114
pio run -e seeed_xiao_nrf52840_kit_i2c    # Xiao E22P, Xiao Kit i2c
```
- Ruta por rama: `C:\Firmware Navarrico 4.3\Rama 2 Infraestructura\Infraestructura <General|Propia>\<Carpeta variante>`
- Binarios en `.pio\build\<env>\` (`.uf2`, `.hex`, `.zip` OTA).
- `pio run` a secas usa `default_envs = tbeam` → falla (`CreateProcess: No such file or directory`, toolchain ESP32). **NO cambiar `default_envs`** (decisión 2026-08-10: el fallo visible es preferible a un build silencioso equivocado).
- Compilar cada Xiao en su propia carpeta: XiaoKitI2c (SX1262) y XiaoKitI2c+E22P comparten env pero usan `variant.h` distintos.

## Distribución (estructura 4.3)
Script `HerramientasPropiasIA\distribuir_binarios.ps1`:
- Deduce la rama del sufijo de la carpeta (`R2IG`→General, `R2IP`→Propia).
- Toma `.uf2`/`.zip` del `.pio\build\<env>` (fallback `Compilados\`) y los copia a `UF2\` y `OTA\` de la rama correspondiente.
- Nombre del fichero = nombre de la carpeta de la variante (incluye variante + rama).
```powershell
.\distribuir_binarios.ps1 -Carpeta "Faketec NavTastic 2.7.26 R2IP" -Env nrf52_promicro_diy_tcxo
```

## Verificación de paridad
Al tocar archivos comunes, compilar las 6 de la rama afectada y verificar que los núcleos quedan idénticos contra la Promicro (referencia canónica). **General ACTIVA (12/08)**: verificar paridad de núcleo contra Propia (los `.cpp` deben ser idénticos; solo difieren `userPrefs.jsonc` y el fuzzer, según `09_general_vs_propia.md`).

## Errores conocidos
- `tbeam` build: fallo toolchain ESP32, preexistente, ajeno al código.
- No definir `build_dir` en `[platformio]` (la Faketec lo tenía redirigido a `C:/Users/Jesus/.platformio/build`, ya corregido).
- **MAX_PATH en Rama 1 (error #13, 12/08)**: en `Rama 1 Clientes en Infraestructura\`, las carpetas `Promicro NRF52+E22P...` y `XiaoKitI2c+E22P...` (P y G) superan 260 chars al incluir `SparkFun_MMC5983MA_Arduino_Library_Constants.h` → `fatal error: No such file or directory` aunque el archivo exista. Fix en su `platformio.ini`: `libdeps_dir` y `build_dir` cortos (`C:/Users/Jesus/.platformio/libdeps|build/r1xxx`). Sus binarios salen FUERA del proyecto (`C:/Users/Jesus/.platformio/build/r1xxx/<env>/`). No borrar estas líneas al recompilar.
- **No paralelizar dos builds del MISMO env** (pio comparte la caché de downloads → carreras que corrompen libs; señal: libs parciales o zips de 1 byte en `~/.platformio/.cache/downloads`). Compilar el mismo env secuencialmente; distintos envs pueden ir en paralelo.
- **`.pio` heredado**: al copiar carpetas (p. ej. R2→R1) el `.pio` puede venir incluido y enmascarar errores (builds incrementales con paths viejos). Limpiarlo antes de la primera compilación en la nueva rama.

## Distribución al Desktop (estructura del operador)
Script `HerramientasPropiasIA\distribuir_desktop.ps1` (ASCII puro; invocar con `powershell -NoProfile -ExecutionPolicy Bypass -Command "& '<ruta>' -Confirm:$false"`): copia los UF2/OTA de R1 y R2 a `Desktop\NavaTastic 4.3 120826\Rama 1 Clientes\` y `Rama 2 Routers\`, en `LIPO\` (todas) y `NIMH\` (**solo Faketec y XiaoKitI2c SIN +E22P**, norma del operador). Nombre de fichero = nombre de carpeta de la variante. Params: `-Rama 1|2|all`, `-Confirm`. Detalle en subnota `11_rama1_plan.md` §5.
