# 05 — NodeDB y Protección de Flash

> **ESTADO 14/08/2026 — REPO UNIFICADO**: contenido **VIGENTE**. En el repo único:
> `src/mesh/NodeDB.cpp` (`checkAndRegisterRAMAutoFavorite` ~196, filtro de guardado
> ~1717, eviction ~2380, H3 `newKeyIsAdmin` ~2104) + `Router::activeDirectRouters`.
> El gate `/nava fav auto` (default ON) se persiste en `/resilience.bin`.

## Estrategia Rama 2
Base de datos selectiva en RAM (`USERPREFS_NODEDB_RAM_ONLY`) + escrituras mínimas a Flash.

## Mecanismos
- **Desalojo híbrido** (`NodeDB::getOrCreateMeshNode()`): con 80 nodos llenos, desaloja favorito no-admin más antiguo; si todos admins/ignorados → `NULL` (evita crash índice 81).
- **Filtro de guardado selectivo** (`saveNodeDatabaseToDisk()`): solo nodo propio, favoritos, ignorados, admins criptográficos.
- **Filtro DeviceState condicional** (`saveDeviceStateToDisk()`): `memcmp` de identidad (owner, my_node_num, device_id); si solo cambian datos volátiles, omite escritura.
- **Auto-favoritos con estrella** (`checkAndRegisterRAMAutoFavorite()`): routers directos (0 hops) → `is_favorite` + `activeDirectRouters`. **Gate `/nava fav auto`** (default ON, 12/08): OFF bloquea el auto-favoriteo completo; los ya existentes se conservan.
- **Anulación de historial** (`TransmitHistory::saveToDisk()`): `return true;` simple (Rama 2).
- **Límite de huérfanos** (`AdminModule.cpp`): máx. 10 favoritos remotos sobre nodos nunca oídos por RF.

## Admin criptográfico (inmunidad)
Bitfield `NODEINFO_BITFIELD_IS_CRYPTOGRAPHICALLY_VERIFIED_ADMIN_MASK (0x08)` asignado solo tras validar PKI; `isAdminNode()` lo lee. Nunca confiar en NodeInfo en claro.

## Escrituras de F20 (V3, 16/08)
F20 añade DOS escrituras, ambas condicionales (misma filosofía anti-desgaste):
- **Restauración de claves al boot**: `applyPersistedAdminKeys()` escribe `SEGMENT_CONFIG`
  SOLO si cambió algo (boot normal = no-op; tras factory/full reset = 1 escritura).
- **Sincronización**: `syncAdminKeysFromConfig()` reescribe `/resilience.bin` SOLO si cambió
  (L7: remove antes de escribir). `/nava keys_clear` = 1 escritura. Nunca hay escrituras
  periódicas de claves.

## Flujo fix 2026-08-10 (updateUser)
Ver nota `02_claves_admin.md`: NodeInfo con clave nueva == admin_key → aceptar + re-favoritear. Sin esto, la DB conserva clave errónea y el DM PKI no descifra.
