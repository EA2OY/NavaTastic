# 🛠️ Guía Técnica de Corrección: Fixes Críticos de Desincronización V4 para NavaTastic V5

> **DOCUMENTO DE TRASPASO DIRECTO PARA EL AGENTE DE DESARROLLO**
> Este documento detalla la corrección exacta de los 4 desacoples internos localizados entre la memoria RAM, los ficheros Protobuf de Flash, la base de datos `NodeDB` y los paquetes de difusión `NodeInfo` / `Position` hacia pasarelas MQTT y MeshMap.

---

## 📌 1. BUG CRÍTICO 1: Desincronización del Rol en MQTT y Mapas (`owner.role` vs `config.device.role`)

### 🔍 Síntoma
Un nodo al que se le cambia el rol (por ejemplo de `ROUTER` a `CLIENT_MUTE` y luego de vuelta a `ROUTER`) se comporta como router en la radio y en la consola `/nava`, pero en los mapas y servidores MQTT de las pasarelas **sigue apareciendo congelado permanentemente como `CLIENT_MUTE`**.

### 🧩 Causa Raíz
`config.device.role` (enrutamiento de malla) se actualizaba correctamente, pero `owner.role` (`meshtastic_User`, la identidad pública emitida en `NODEINFO_APP`) **nunca se sincronizaba**. `NodeInfoModule` emite `owner` y la base de datos local `nodeDatabase.nodes[0].user.role` conservaba el rol antiguo de Flash (`devicestate.owner.role`).

### 🛠️ Corrección Obligatoria en Código

1. **En `src/modules/AdminModule.cpp`** (dentro de `handleSetConfig`, caso `device`):
```cpp
// Sincronizar la identidad de usuario con el rol configurado:
owner.role = config.device.role;
nodeDB->updateUser(nodeDB->getNodeNum(), owner);
changes |= SEGMENT_DEVICESTATE | SEGMENT_NODEDATABASE;
```

2. **En `src/modules/NavaCLIModule.cpp`** (dentro de `/nava set_role` y en `syncDeviceRoleFromConfig`):
```cpp
owner.role = config.device.role;
nodeDB->updateUser(nodeDB->getNodeNum(), owner);
nodeDB->saveToDisk(SEGMENT_DEVICESTATE | SEGMENT_NODEDATABASE);
if (service) {
    service->reloadOwner(true); // Emite NodeInfo actualizado de inmediato a la malla y MQTT
}
```

3. **En `src/modules/NavaCLIModule.cpp`** (dentro de `loadResiliencePrefs` al boot):
```cpp
if (prefs.role <= meshtastic_Config_DeviceConfig_Role_ROUTER) {
    config.device.role = (meshtastic_Config_DeviceConfig_Role)prefs.role;
    owner.role = config.device.role;
    nodeDB->updateUser(nodeDB->getNodeNum(), owner);
    nodeDB->installRoleDefaults(config.device.role);
}
```

---

## 📌 2. BUG 2: Persistencia Incompleta y Refresco en `/nava set_name`

### 🔍 Síntoma
Al cambiar el nombre con `/nava set_name "Largo" "Corto"`, el nombre cambiaba en RAM, pero el nodo local en la lista de nodos (`nodeDatabase.nodes[0]`) no se actualizaba de inmediato en la App ni se forzaba la emisión del nuevo `NodeInfo` a la red.

### 🧩 Causa Raíz
En `NavaCLIModule.cpp` (línea ~2847), `set_name` ejecutaba únicamente `nodeDB->saveToDisk(SEGMENT_NODEDATABASE)`. Sin embargo, `owner` vive en `devicestate` (`SEGMENT_DEVICESTATE`, `/prefs/device.proto`), por lo que la identidad en Flash quedaba desfasada y `nodeDB->updateUser` no era invocado.

### 🛠️ Corrección Obligatoria en `src/modules/NavaCLIModule.cpp` (en `set_name`):
```cpp
strncpy(owner.long_name, longN.c_str(), sizeof(owner.long_name) - 1);
strncpy(owner.short_name, shortN.c_str(), sizeof(owner.short_name) - 1);
owner.long_name[sizeof(owner.long_name) - 1] = '\0';
owner.short_name[sizeof(owner.short_name) - 1] = '\0';

// Sincronizar en NodeDB local y persistir ambos segmentos:
nodeDB->updateUser(nodeDB->getNodeNum(), owner);
nodeDB->saveToDisk(SEGMENT_DEVICESTATE | SEGMENT_NODEDATABASE);

// Forzar emisión inmediata del nuevo NodeInfo a la red:
if (service) {
    service->reloadOwner(true);
}
```

---

## 📌 3. BUG 3: Difusión Inmediata tras `/nava set_pos` y `pos_clear`

### 🔍 Síntoma
Al fijar coordenadas con `/nava set_pos <lat> <lon> [alt]`, las coordenadas se guardaban en la configuración, pero las pasarelas MQTT y los mapas tardaban horas en reflejar la nueva posición fija.

### 🧩 Causa Raíz
El comando guardaba en `config` y `resilience.bin`, pero no notificaba al módulo de posición para emitir la baliza de actualización inmediata (*instant position broadcast*).

### 🛠️ Corrección Obligatoria en `src/modules/NavaCLIModule.cpp` (en `set_pos` y `pos_clear`):
```cpp
// Tras setLocalPosition(pos):
nodeDB->setLocalPosition(pos);
nodeDB->saveToDisk(SEGMENT_CONFIG | SEGMENT_NODEDATABASE);
saveResiliencePrefs();

// Disparar envío inmediato de posición a la malla:
if (positionModule) {
    positionModule->sendOurPosition(NODENUM_BROADCAST, false);
}
```

---

## 📌 4. BUG 4: Coherencia de Mensajería Directa (`owner.is_unmessagable`)

### 🔍 Síntoma
Al cambiar a rol `ROUTER`, la plantilla de fábrica marca `owner.is_unmessagable = true`. En repetidores de infraestructura NavaTastic, la mensajería administrativa por DM debe estar siempre operativa.

### 🛠️ Corrección Obligatoria:
Asegurar que en `loadResiliencePrefs()` y tras `installRoleDefaults(role)` se mantenga:
```cpp
owner.is_unmessagable = false;
owner.has_is_unmessagable = true;
nodeDB->updateUser(nodeDB->getNodeNum(), owner);
```

---

### ✅ Verificación Post-Implementación
1. Compilar con `pio run -e navarrico_promicro_e22p_r2ig`.
2. Verificar que al alternar roles (`/nava set_role router` / `client`), el paquete `NodeInfo` transmitido por radio contenga `user.role == ROUTER` y coincida al 100% con `config.device.role`.
3. Verificar que `/nava set_name` propague el nuevo nombre inmediatamente en `nodeDatabase.nodes[0]`.
