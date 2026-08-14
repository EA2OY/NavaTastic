# Diagnostic Validation Report: Native core reset bug (nRF52 Platform)

## 1. The Native Bug Discovered

Triggering a factory reset or other reboot requests in the clean, unmodified Meshtastic v2.7.26 firmware on nRF52 devices causes the device to freeze or lock up.

### Comparative Logic Audit
A comparison of the reboot execution flow in `src/Power.cpp` shows:

* **In Meshtastic v2.7.22 (Working):**
  ```cpp
  void Power::reboot()
  {
      notifyReboot.notifyObservers(NULL);
  #if defined(ARCH_ESP32)
      ESP.restart();
  #elif defined(ARCH_NRF52)
      NVIC_SystemReset();
  ```
  It called `NVIC_SystemReset()` directly which triggered a clean hardware master reset.

* **In Meshtastic v2.7.26 (Broken):**
  ```cpp
  void Power::reboot()
  {
      notifyReboot.notifyObservers(NULL);
  #if defined(ARCH_ESP32)
      ESP.restart();
  #elif defined(ARCH_NRF52)
      extern bool useSoftDevice;
      if (useSoftDevice) {
          sd_nvic_SystemReset();
      } else {
          NVIC_SystemReset();
      }
  ```
  The core developers introduced `sd_nvic_SystemReset()` when `useSoftDevice` is true. However, before the reboot timer expires, commands like `disableBluetooth()` shut down/disconnect Bluetooth stack components. When the reboot executes, the SoftDevice stack is not in a fully operational state for SVC calls, and calling `sd_nvic_SystemReset()` triggers a HardFault/CPU exception or traps the MCU in the bootloader loop, freezing the device.

---

## 2. The Core Solution

To address this cleanly without modifying the core functionality destructively, the changes are guarded under the safety macro condition `#ifdef FIX_NATIVE_CORE_RESET`.

### A. Safety Macro Definition
Path: [variant.h](file:///c:/Users/Jesus/Desktop/firmware/variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h)
```cpp
// Fix for native regression boot/reset hang
#define FIX_NATIVE_CORE_RESET
```

### B. Reboot Fallback in `Power::reboot()`
Path: [Power.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/Power.cpp)
```cpp
#elif defined(ARCH_NRF52)
#ifdef FIX_NATIVE_CORE_RESET
    NVIC_SystemReset();
#else
    extern bool useSoftDevice;
    if (useSoftDevice) {
        sd_nvic_SystemReset();
    } else {
        NVIC_SystemReset();
    }
#endif
```

### C. Immediate Coordinated Safety Reset
Path: [NodeDB.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/mesh/NodeDB.cpp)
Triggering `NVIC_SystemReset()` immediately at the end of `NodeDB::factoryReset` ensures the CPU reboots cleanly before concurrent background threads (like display or telemetry) access cleared database memory configurations and crash.
```cpp
#ifdef FIX_NATIVE_CORE_RESET
    LOG_INFO("FIX_NATIVE_CORE_RESET: Immediate safety reset!");
    NVIC_SystemReset();
#endif
    return true;
}
```

---

## 3. Rollback Safety
Commenting out `#define FIX_NATIVE_CORE_RESET` in `variant.h` completely disables all applied modifications, returning the compilation flow back to the pristine, unmodified Meshtastic v2.7.26 codebase.
