# NAVARICO - Perfiles de configuración por (rama x placa)

Cada fichero es el `userPrefs.jsonc` original de su build (copiado 1:1, sin tocar).
El env correspondiente lo usa vía `custom_meshtastic_prefs` en `variants/nrf52840/navarrico.ini`.

## Qué contiene cada perfil
- Claves admin (K0/K1) - SOLO claves PÚBLICAS
- Canal 0 SFNarrow + Canal 1 Navadmin (PSK `{0x01}`)
- Región EU_868, canal 4 / 869.618 MHz, preset MEDIUM_FAST, TX base
- Rol (ROUTER en Rama 2, CLIENT en Rama 1)
- BT fijo, pre-check de batería, tiempos de broadcast

## Nomenclatura
`<RAMA>_<Placa>.jsonc` con RAMA = R2IG (Rama 2 General) | R1IG (Rama 1 Clientes General).
**Propia (R2IP/R1IP)**: sin perfiles en disco. Los 12 envs `R2IP_*/R1IP_*` extienden los de
General y `bin/platformio-custom.py` inyecta las claves del operador y el PIN BT desde
variables de entorno (`NAVARICO_PROPIA_KEY_0/1`, `NAVARICO_PROPIA_BT`), que pide
`build_propia.ps1` — **las claves Propia nunca se almacenan** (gitignored por si acaso:
`profiles/*R2IP*`/`*R1IP*`).

## Reglas
- Un perfil = exactamente el build original: no mezclar claves de ramas distintas.
- No editar un perfil para "arreglar" un env: editar el perfil (es la fuente).
- Al añadir un perfil nuevo: copiar el jsonc original 1:1 y crear el env en navarrico.ini.
- NUNCA escribir claves privadas o claves Propia en estos ficheros (GitHub público).
