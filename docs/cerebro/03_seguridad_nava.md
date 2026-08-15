# 03 — Seguridad NavaCLI (`/nava`)

> **ESTADO 14/08/2026 — REPO UNIFICADO**: contenido **VIGENTE**. En el repo único el
> módulo es `src/modules/NavaCLIModule.h/.cpp` (una sola copia, no 12), registrado en
> `src/modules/Modules.cpp`. El canal Navadmin (slot 1, PSK `{0x01}`) está homogeneizado
> en los 12 perfiles. Comandos, whitelist, rate-limit 30 s y DM PKI: igual que abajo.

## NavaCLIModule v4.2.1
Módulo headless de admin remota. Código canónico: `Infraestructura Propia\Promicro NRF52+E22P NavTastic 2.7.26 R2IP\src\modules\NavaCLIModule.h/.cpp` (idéntico en Faketec salvo `set_txpower` 0-22).

## Canal Navadmin (Canal 1, PSK pública `{0x01}`)
- Whitelist SOLO LECTURA. No-admins NO reciben respuesta (silencio total).
- Permite: `help`, `ping`, `status`, `env`, `channel`, `peers`, `rxlog`, `afc`, `reset_reason`, `noise`, `bat`, `route !ID`, `trace !ID`.
- Resto responde `ERR: SOLO DM SEGURO`.
- Se identifica por slot (índice 1), NO por nombre. No reordenar canales.

## DM PKI obligatorio
Comandos destructivos/configuración/energía SOLO por DM cifrado (`mp.pki_encrypted`). Autorización: `config.security.admin_key[0..2]`.

## Protecciones
- **Suplantación de `from`**: PSK `{0x01}` pública → cualquiera fabrica paquetes con `from` de admin. Mitigación: whitelist solo-lectura en canal; destructivos solo DM PKI.
- **Rate-limit**: `std::set<NodeNum> unauthorizedReplied` responde UNA vez `NO AUTORIZADO COMO ADMINISTRADOR` por nodo.
- `factory_reset` diferido, `ign add` seguro, normalización antes de filtro, guards `substr()`.

## Comandos clave
`set_chem`, `set_vbat`, `set_vwake`, `bat`, `storm [h]`/`storm test1|test2`, `txoff`, `txon`, `ble`, `rxlog`, `afc`, `reset_reason`, `trace !ID`, `route !ID`, `msg "TXT"`, `bell`, `pos`, `nodeinfo`, `sendtel`, `admin_ls`, `power`, `noise`, `fav auto [on|off]` (12/08). Interrogación universal: `/nava <cmd> ?` / `<cmd> help` (excepto `msg`); comandos sin argumento responden estado + opciones. AVISO en persistentes (set_chem/set_vbat/set_vwake/txoff/ble): rollback SOLO con `nrf erase`. **`set_role` (12/08, Rama 1)**: además de `/prefs` guarda el rol en `/resilience.bin` (semi-permanente, sobrevive a factory reset; ver `11_rama1_plan.md` §2).
**V3 (15/08)**: `/nava full_reset` (config + semi-persistentes a defaults, conserva par PKI/bonds/claves admin) · `/nava wipe` (purga total, par PKI nuevo) · **F20**: `/nava keys_ls` (claves admin persistidas en base64) y `/nava keys_clear` (cero solo los 3 campos persistidos; ACK diferido, sin reboot). Todos DM-PKI (fuera de la whitelist del canal 1). Regla merge: quitar una clave en la app NO purga lo persistido → purgar con `keys_clear`/`wipe`.

## Nota fix 2026-08-10
La rotación de clave en `updateUser` permite re-acreditar un mando tras `db_clear`/`ign rm`: reenvía su NodeInfo con clave correcta y el siguiente DM ya descifra. Manual: `Manual_NavaTastic.md`.
