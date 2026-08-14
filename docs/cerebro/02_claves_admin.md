# 02 — Claves Admin y Regla del Hardcodeo

> **ESTADO 14/08/2026 — REPO UNIFICADO**: contenido **VIGENTE**. Las claves viven ahora en
> `profiles/*.jsonc` (General = K0 Master Node, 1 clave, BT 654321) y en el
> `userPrefs.jsonc` raíz (perfil por defecto R2IG Promicro). El flujo es idéntico:
> perfil → macro `-DUSERPREFS_USE_ADMIN_KEY_X` → `NodeDB.cpp`. Propia (K0/K1 Promicro,
> BT 123457) se migrará a perfiles gitignored sin tocar código. Los números de línea
> citados abajo son orientativos (NodeDB.cpp unificado: ~80-87, 725-743, 1460-1520).

## Claves unificadas (fix 2026-08-10)
Las 6 variantes usan exactamente las claves del Promicro:
- **K0** = `{0x12, 0x48, 0xc4, 0xec, ... 0xaa, 0x68}`
- **K1** = `{0x3f, 0x38, 0x04, 0x5a, ... 0x73, 0x38}`

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
