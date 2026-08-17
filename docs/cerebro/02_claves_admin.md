# 02 — Claves Admin y Regla del Hardcodeo

> **ESTADO 15/08/2026 — REPO UNIFICADO**: contenido **VIGENTE**. Las claves viven ahora en
> `profiles/*.jsonc` (General = K0 Master Node, 1 clave, BT 654321) y en el
> `userPrefs.jsonc` raíz (perfil por defecto R2IG Promicro). El flujo es idéntico:
> perfil → macro `-DUSERPREFS_USE_ADMIN_KEY_X` → `NodeDB.cpp`. **Propia (K0/K1 del operador,
> BT propio) se compila con claves NO almacenadas**: 12 envs `R2IP_*/R1IP_*` + variables de
> entorno `NAVARICO_PROPIA_KEY_0/1` y `NAVARICO_PROPIA_BT` (script `build_propia.ps1`, que
> las pide y NO las guarda). **Las claves del operador no existen en ningún fichero del
> repo.** Los números de línea citados abajo son orientativos (NodeDB.cpp unificado: ~80-87,
> 725-743, 1460-1520).

## Claves unificadas (fix 2026-08-10)
Las 6 variantes usan exactamente las claves del Promicro (K0/K1 del operador, **valores no publicados** — en el repo unificado se inyectan al compilar los envs Propia, nunca se almacenan).

Estado previo: Xiao E22P/Seed P1 tenían clave de rescate `{0xc7...}`; Heltec T114 ninguna.

## Regla del hardcodeo (sin literales en código)
Flujo: `userPrefs.jsonc` → macro `-DUSERPREFS_USE_ADMIN_KEY_X` (inyectada por `bin/platformio-custom.py`) → `NodeDB.cpp:78-81` define `userprefs_admin_key_X[]` desde la macro. Los bloques "hardcode" de `NodeDB.cpp:751-771` (post-factory) y `1458-1484` (carga si suma 0) leen la macro, NO literales.

**Auditoría 2026-08-10**: sin claves literales en src/variants/ini de las 6 activas. Limpiado `.clusterfuzzlite/router_fuzzer.cpp` (tenía `{0xcd...}`; ahora K0/K1 Promicro). Único resto: `old\` (variantes deprecated) — **no copiado a 4.3**; vive en `C:\Firmware Navarrico 4.2\Rama 2 Infraestructura\Codigo Rama 2\old\`.

## Fix `updateUser` (NodeDB.cpp, 2026-08-10)
**Problema**: router descifra DM PKI con la clave de la DB (`Router.cpp: decryptCurve25519(p->from, nodeDB->getMeshNode(p->from)->user.public_key,...)`), NO con la del paquete. Si la DB tiene clave errónea (mando filtrado con clave no autorizada), el `/nava` por DM jamás descifra.

**Fix**: en `updateUser`, si la NUEVA clave del NodeInfo coincide con una `admin_key` configurada → aceptar cambio y `info->is_favorite = true` (en vez de `return false`).

**Flujo correcto**: NodeInfo (broadcast, sin PKI) con clave nueva → `updateUser` acepta y actualiza DB → siguiente DM PKI descifra → valida.

**Seguridad**: solo se acepta si la nueva clave == admin_key configurada. Sin conflicto.

**Código**: ver `guia_integracion_navarrico.md` sección "Ñ. Rotación de Clave Admin Aceptada".

## 🔐 Auto-recuperación de claves admin (comportamiento, verificado 2026-08-11)

**¿Qué es**: si la clave admin del **slot 0** está vacía, el firmware re-inyecta K0/K1 de fábrica (`userPrefs.jsonc`) en **cada boot** (no solo tras factory reset). El nodo **nunca queda sin admin** aunque un operador borre sus claves a mano por error.

**Código** (`NodeDB.cpp`, `loadFromDisk()`, bajo `FIX_NATIVE_CORE_RESET`):
```cpp
// Ensure default admin keys are loaded if sum is 0
uint16_t local_sum = 0;
for (uint8_t b = 0; b < 32; b++) {
    local_sum += config.security.admin_key[0].bytes[b];
}
if (local_sum == 0) {
    // memcpy(default_admin_key_0, USERPREFS_USE_ADMIN_KEY_0, 32) [+ KEY_1]
    if (numAdminKeys < 1) numAdminKeys = 1;
}
```

**Comportamiento exacto**:
- `loadFromDisk()` corre en **todo arranque** (soft reset y factory reset).
- Si slot 0 suma 0 (vacío) → K0/K1 de fábrica re-inyectadas.
- Si slot 0 tiene cualquier valor no-cero → se respeta la clave del usuario (no se toca).

**Detalle importante**: la condición es sobre **solo el slot 0**. Si el usuario pone su clave en K1 y deja K0 vacío → el boot re-inyecta K0 de fábrica (queda K0=fábrica + K1=usuario). Para tener SOLO su clave, debe ponerla en el **slot 0**.

**Verificado en hardware (Faketec, 2026-08-11)**: tras factory reset, K0/K1 de fábrica correctas. Tras soft reset con claves borradas a mano, se re-inyectan igualmente.

## 🔑 F20 — Claves admin persistidas en `/resilience.bin` (V3, banco 7/7 16/08)

**Qué es**: las claves admin PÚBLICAS del usuario se guardan en `/resilience.bin` (campos
`keySlot1/keySlot2/keySlot0Own`, struct 180 B marcador "NAV3") y se re-aplican al boot tras un
factory/full reset — antes se perdían (L10) y el nodo quedaba solo con la del proyecto.

**Regla final de slot 0 (ENMIENDA del operador, banco verificado)**: "slot 0 = estado previo
del usuario":
- Si el dueño puso SU clave en slot 0 (desautorizando la de fábrica) → se persiste como
  `keySlot0Own` y tras el reset vuelve AL SLOT 0, desplazando a la del proyecto (sin ventana de
  secuestro; el Master Node queda NO AUTORIZADO, verificado en banco).
- Si nunca la desautorizó → slot 0 queda con la del proyecto, como estaba.
- La auto-recuperación (arriba) queda INTACTA y solo cubre configs vacías (wipe/nrf erase):
  ahí sí, canal de rescate garantizado.

**Sincronización (merge)**: cada `set_config` de seguridad (app/CLI) sincroniza: slot entrante
no vacío se persiste (slot 0 = proyecto → limpia `keySlot0Own`); un slot vacío NUNCA borra lo
persistido. **Quitar una clave en la app NO la purga del nodo** — reaparece tras el próximo
reset. Purga real: `/nava keys_clear` (cero solo los 3 campos persistidos; no toca la config
actual ni reinicia) o `/nava wipe`/`nrf erase`.

**Dedupe**: las claves del proyecto (K0/K1 del perfil, macros `USERPREFS_USE_ADMIN_KEY_*`)
NUNCA se persisten como claves de usuario (General solo define K0 → `#ifdef` por clave).

**Comandos**: `/nava keys_ls` (persistidas en base64, mismo formato que `admin_ls`) y
`/nava keys_clear` (ACK diferido, sin reboot). DM-PKI solo (fuera de la whitelist del canal 1).
