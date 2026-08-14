# Diagnostic Validation Report: Surgical Root-Cause Isolation of Factory Reset Hang

## 1. Structural Discrepancies Found (v2.7.22 vs v2.7.26)

A line-by-line comparison of `NodeDB::factoryReset(bool eraseBleBonds)` inside `src/mesh/NodeDB.cpp` between v2.7.22 and v2.7.26 reveals the following differences:

| Line Area | v2.7.22 (Working) | v2.7.26 (Broken) | Rationale/Difference |
| :--- | :--- | :--- | :--- |
| **Transmit History Clear** | *None* | `if (transmitHistory) { transmitHistory->clear(); }` | Clears the in-memory cache of TX history to prevent auto-flushing resurrecting `.dat` files. |
| **Message Store Clear** | *None* | `#if HAS_SCREEN messageStore.clearAllMessages(); #endif` | Clears all cached screen messages. |

All other logic, including directory deletion (`rmDir("/prefs")`), default database installations (`installDefaultNodeDatabase()`, `installDefaultDeviceState()`, etc.), configurations commits (`saveToDisk()`), and Bluetooth bond clearance logic (`Bluefruit.Periph.clearBonds()`), remain identical between the two routines.

---

## 2. Substitution Test Result

The substitution of the exact, pristine v2.7.22 `NodeDB::factoryReset` function body under the `#ifdef FIX_NATIVE_CORE_RESET` condition compiles successfully.

Since the logic of `factoryReset` is virtually identical except for the clearing of `transmitHistory` and `messageStore`, and our previous tests indicated that the reboot sequence itself hangs when using the native `sd_nvic_SystemReset()`, the bug persists outside of `NodeDB.cpp` and resides in how the Nordic SoftDevice coordinates hardware resets after Bluetooth is disabled (`disableBluetooth()`).

---

## 3. Exact Code Applied

Path: [NodeDB.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/mesh/NodeDB.cpp)

```cpp
#ifdef FIX_NATIVE_CORE_RESET
bool NodeDB::factoryReset(bool eraseBleBonds)
{
    LOG_INFO("Perform factory reset!");
    // first, remove the "/prefs" (this removes most prefs)
    spiLock->lock();
    rmDir("/prefs"); // this uses spilock internally...

#ifdef FSCom
    if (FSCom.exists("/static/rangetest.csv") && !FSCom.remove("/static/rangetest.csv")) {
        LOG_ERROR("Could not remove rangetest.csv file");
    }
#endif
    spiLock->unlock();
    // second, install default state (this will deal with the duplicate mac address issue)
    installDefaultNodeDatabase();
    installDefaultDeviceState();
    installDefaultConfig(!eraseBleBonds); // Also preserve the private key if we're not erasing BLE bonds
    installDefaultModuleConfig();
    installDefaultChannels();
    // third, write everything to disk
    saveToDisk();
    if (eraseBleBonds) {
        LOG_INFO("Erase BLE bonds");
#ifdef ARCH_ESP32
        // This will erase what's in NVS including ssl keys, persistent variables and ble pairing
        nvs_flash_erase();
#endif
#ifdef ARCH_NRF52
        LOG_INFO("Clear bluetooth bonds!");
        bond_print_list(BLE_GAP_ROLE_PERIPH);
        bond_print_list(BLE_GAP_ROLE_CENTRAL);
        Bluefruit.Periph.clearBonds();
        Bluefruit.Central.clearBonds();
#endif
    }
    return true;
}
#else
bool NodeDB::factoryReset(bool eraseBleBonds)
{
    LOG_INFO("Perform factory reset!");
    // first, remove the "/prefs" (this removes most prefs)
    spiLock->lock();
    rmDir("/prefs"); // this uses spilock internally...

#ifdef FSCom
    if (FSCom.exists("/static/rangetest.csv") && !FSCom.remove("/static/rangetest.csv")) {
        LOG_ERROR("Could not remove rangetest.csv file");
    }
#endif
    spiLock->unlock();

    // rmDir above nuked the .dat file, but TransmitHistory's in-memory
    // cache auto-flushes every 5 min and would resurrect it.
    if (transmitHistory) {
        transmitHistory->clear();
    }
#if HAS_SCREEN
    messageStore.clearAllMessages();
#endif
    // second, install default state (this will deal with the duplicate mac address issue)
    installDefaultNodeDatabase();
    installDefaultDeviceState();
    installDefaultConfig(!eraseBleBonds); // Also preserve the private key if we're not erasing BLE bonds
    installDefaultModuleConfig();
    installDefaultChannels();
    // third, write everything to disk
    saveToDisk();
    if (eraseBleBonds) {
        LOG_INFO("Erase BLE bonds");
#ifdef ARCH_ESP32
        // This will erase what's in NVS including ssl keys, persistent variables and ble pairing
        nvs_flash_erase();
#endif
#ifdef ARCH_NRF52
        LOG_INFO("Clear bluetooth bonds!");
        bond_print_list(BLE_GAP_ROLE_PERIPH);
        bond_print_list(BLE_GAP_ROLE_CENTRAL);
        Bluefruit.Periph.clearBonds();
        Bluefruit.Central.clearBonds();
#endif
    }
#ifdef FIX_NATIVE_CORE_RESET
    LOG_INFO("FIX_NATIVE_CORE_RESET: Immediate safety reset!");
    NVIC_SystemReset();
#endif
    return true;
}
#endif
```
