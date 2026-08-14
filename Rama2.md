# Report: Navarrico Rama 2 (Infraestructura y Repetidores) Architecture for ProMicro

This document serves as the implementation guide and AI-context file to port the **Navarrico Rama 2 (In-RAM DB & Flash Protection)** modifications onto any clean fork of the Meshtastic firmware for the **Faketec / nRF52 ProMicro DIY (nrf52_promicro_diy_tcxo)**.

---

## 1. Summary of Rama 2 Logic

Rama 2 is designed specifically for solar-powered infrastructure routers. It inherits all hardware resilience changes from Rama 1 (voltage clamping, solar waking comparator, and reset fixes) and adds:
1.  **Zero-Wear Flash Protection**: Bypasses duplicate packet transmission caches from writing to disk, and filters the NodeDB serializer to prevent ephemeral network data (peers, positions, telemetry) from causing flash memory degradation.
2.  **In-RAM Auto-Favorite Registry**: Dynamically discovers neighbor routers (`hops_away == 0`) and treats them as favorites in RAM to bypass hop decrements without modifying the persistent NodeDB files.
3.  **Automated Factory Reset Defaults**: Auto-configures the device as a `ROUTER` with `LOCAL_ONLY` rebroadcasting and 72-hour update intervals upon factory reset.

---

## 2. Code Modifications & Rationale

### A. [Router.h](file:///C:/Firmware Navarrico 4.2/Rama 2 Infraestructura/Promicro fix 2.7.26 Rama 2/src/mesh/Router.h)
*   **Where**: Under the public section of the `Router` class (line 36).
*   **Code**:
    ```cpp
    std::vector<NodeNum> activeDirectRouters;
    ```
*   **Why**: Stores the dynamically discovered direct neighbor router IDs strictly in RAM. This list acts as our volatile favorites registry.

---

### B. [Router.cpp](file:///C:/Firmware Navarrico 4.2/Rama 2 Infraestructura/Promicro fix 2.7.26 Rama 2/src/mesh/Router.cpp)
*   **Where**: Included `<algorithm>` at the top of the file, and updated the favorite check in `Router::shouldDecrementHopLimit` (line 103).
*   **Code**:
    ```cpp
    // Check 1: is_favorite (flash) OR present in RAM auto-favorite registry
    bool isFav = node->is_favorite;
    if (!isFav) {
        isFav = (std::find(activeDirectRouters.begin(), activeDirectRouters.end(), node->num) != activeDirectRouters.end());
    }
    if (!isFav)
        continue;
    ```
*   **Why**: Allows the zero-hop logic to recognize the RAM-only dynamic favorites registry (`activeDirectRouters`) exactly as if they were standard admin-selected favorites, skipping the hop decrement.

---

### C. [NodeDB.h](file:///C:/Firmware Navarrico 4.2/Rama 2 Infraestructura/Promicro fix 2.7.26 Rama 2/src/mesh/NodeDB.h)
*   **Where**: In the private section of the `NodeDB` class (line 316).
*   **Code**:
    ```cpp
    void checkAndRegisterRAMAutoFavorite(meshtastic_NodeInfoLite *info);
    ```
*   **Why**: Declares the helper method responsible for analyzing incoming node info updates and populating the RAM-only registry.

---

### D. [NodeDB.cpp](file:///C:/Firmware Navarrico 4.2/Rama 2 Infraestructura/Promicro fix 2.7.26 Rama 2/src/mesh/NodeDB.cpp)
This file handles the bulk of serialization filtering and default configurations:

1.  **Selective Serialization in `meshtastic_NodeDatabase_callback`** (line 165):
    *   **Code**:
        ```cpp
        // ONLY serialize our own node OR admin-flagged nodes (favorites, ignored, verified)
        if (item.num == nodeDB->getNodeNum() || 
            item.is_favorite || 
            item.is_ignored || 
            (item.bitfield & NODEINFO_BITFIELD_IS_KEY_MANUALLY_VERIFIED_MASK)) {
            
            if (!pb_encode_tag_for_field(ostream, field))
                return false;
            pb_encode_submessage(ostream, meshtastic_NodeInfoLite_fields, &item);
        }
        ```
    *   **Why**: Prevents writing transient discovered nodes to flash during database saves. Only the local node settings and admin actions (remote favorites, ignored nodes, and verified keys) are written to flash.
2.  **RAM Favorite Registration Logic**:
    *   **Code**:
        ```cpp
        void NodeDB::checkAndRegisterRAMAutoFavorite(meshtastic_NodeInfoLite *info)
        {
            if (info && info->has_user) {
                if (info->has_hops_away && info->hops_away == 0) {
                    if (IS_ONE_OF(info->user.role, 
                                  meshtastic_Config_DeviceConfig_Role_ROUTER, 
                                  meshtastic_Config_DeviceConfig_Role_ROUTER_LATE, 
                                  meshtastic_Config_DeviceConfig_Role_CLIENT_BASE)) {
                        auto &adr = router->activeDirectRouters;
                        if (std::find(adr.begin(), adr.end(), info->num) == adr.end()) {
                            LOG_INFO("RAM Auto-Favorite: Registered direct router 0x%08x", info->num);
                            adr.push_back(info->num);
                        }
                    }
                }
            }
        }
        ```
    *   **Why**: Analyzes incoming telemetry. If a node is a router and is directly connected (`hops_away == 0`), it is logged and appended to the RAM active direct list.
    *   **Hook Locations**: Called inside `updateUser()` and `updateFrom()`:
        ```cpp
        checkAndRegisterRAMAutoFavorite(info);
        ```
3.  **Role Defaulting and Interval Changes**:
    *   Set default role to `ROUTER` in `installDefaultConfig()`:
        ```cpp
        #ifdef USERPREFS_CONFIG_DEVICE_ROLE
            config.device.role = USERPREFS_CONFIG_DEVICE_ROLE;
        #else
            config.device.role = meshtastic_Config_DeviceConfig_Role_ROUTER; // Default to router.
        #endif
        ```
    *   Unconditionally call `installRoleDefaults(config.device.role)` on reset.
    *   Set repeater profiles in `installRoleDefaults()` for `ROUTER` role:
        ```cpp
        config.device.rebroadcast_mode = meshtastic_Config_DeviceConfig_RebroadcastMode_LOCAL_ONLY;
        config.device.node_info_broadcast_secs = 72 * 60 * 60;          // 72 hours
        config.position.position_broadcast_secs = 72 * 60 * 60;         // 72 hours
        config.position.position_broadcast_smart_enabled = false;       // Smart Position OFF
        ```

---

### E. [TransmitHistory.cpp](file:///C:/Firmware Navarrico 4.2/Rama 2 Infraestructura/Promicro fix 2.7.26 Rama 2/src/mesh/TransmitHistory.cpp)
*   **Where**: At the start of `TransmitHistory::saveToDisk()` (line 209).
*   **Code**:
    ```cpp
    return true; // Bypass saving duplicate packet history to flash to protect flash memory
    ```
*   **Why**: Prevents writing duplicate packet histories to disk during power management or sleep loops, ensuring zero-wear for packet cache.
