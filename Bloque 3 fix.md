# Validation Report: Block 3 Execution (Fixed)

## Audit Notes
I audited the reference v2.7.22 setup in `C:\Users\Jesus\Desktop\promicro kacho fix 2.7.22 sleep fix + rf fix adc2.0` regarding deep sleep management, battery level filters, and reboot behaviors:

1. **Reboot Trap**: In the reference code, calling the standard ARM CMSIS `NVIC_SystemReset()` directly while the Bluetooth SoftDevice stack was active caused the device to lock up or freeze in the Adafruit DFU bootloader mode. The SoftDevice controls NVIC interrupts and must be bypassed or gracefully coordinated with the reset task using `sd_nvic_SystemReset()`.
2. **Deep Sleep Register Clears**: In v2.7.22, the comparator events were cleared during system-off sequence, but without register-level clean configuration of all events ready, down, up, and cross latches to zero, which led to immediate fake wakeups.
3. **Low-Voltage Debounce**: The default low-voltage filter ran for 10 cycles, which is too long for custom battery setups and risks dropping below cell safety thresholds under heavy transmission loads. Shortening this loop to 4 cycles under `PROMICRO_DIY_TCXO` ensures rapid recovery protection.

---

## Applied Modifications

### 1. Battery Curve clamping and TX Limit in [variant.h](file:///c:/Users/Jesus/Desktop/firmware/variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h)
* Defined `OCV_ARRAY` clamping at `3.5V` (`3500mV`) for discharge curve protection.
* Defined `HARDWARE_TX_POWER_LIMIT` as `12` and `SX126X_MAX_POWER` as `12`.

```cpp
#define SX126X_MAX_POWER 12
#define HARDWARE_TX_POWER_LIMIT 12
...
// Battery discharge curve clamping at 3.5V (3500mV) for cell protection
#define OCV_ARRAY 4190, 4050, 3990, 3890, 3800, 3720, 3630, 3530, 3500, 3500, 3500
```

### 2. Averted Low-Voltage Debounce & SoftDevice Reset in [Power.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/Power.cpp)
* Added `<nrf_nvic.h>` inclusion under `ARCH_NRF52`.
* Custom `low_voltage_counter` checks to trigger `EVENT_LOW_BATTERY` after 4 cycles instead of 10 for the Pro Micro DIY TCXO.
* Coordinated SoftDevice system reset implementation using `sd_nvic_SystemReset()`.

```cpp
#if defined(ARCH_NRF52)
#include "Nrf52SaadcLock.h"
#include "concurrency/LockGuard.h"
#include <nrf_nvic.h>
#endif
...
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
...
#if defined(PROMICRO_DIY_TCXO) || defined(ARDUINO_NRF52_PROMICRO_DIY_TCXO)
            LOG_DEBUG("Low voltage counter: %d/4", low_voltage_counter);
            if (low_voltage_counter > 4) {
#else
            LOG_DEBUG("Low voltage counter: %d/10", low_voltage_counter);
            if (low_voltage_counter > 10) {
#endif
                LOG_INFO("Low voltage detected, trigger deep sleep");
                powerFSM.trigger(EVENT_LOW_BATTERY);
            }
```

### 3. Absolute Hardware TX Ceiling in [RadioInterface.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/mesh/RadioInterface.cpp)
* Added the `HARDWARE_TX_POWER_LIMIT` clamp at the entry point of `RadioInterface::limitPower()`.

```cpp
void RadioInterface::limitPower(int8_t loraMaxPower)
{
#ifdef HARDWARE_TX_POWER_LIMIT
    if (config.lora.tx_power > HARDWARE_TX_POWER_LIMIT) {
        config.lora.tx_power = HARDWARE_TX_POWER_LIMIT;
    }
    if (loraMaxPower > HARDWARE_TX_POWER_LIMIT) {
        loraMaxPower = HARDWARE_TX_POWER_LIMIT;
    }
#endif

    uint8_t maxPower = 255; // No limit
```

### 4. Deep Sleep Power-Down Sequence in [main-nrf52.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/platform/nrf52/main-nrf52.cpp)
* Added `RADIO_POWER_ENABLE_PIN` low drive, settling delay, register-level LPCOMP clearing, and re-enabling sequence.

```cpp
#ifdef RADIO_POWER_ENABLE_PIN
        pinMode(RADIO_POWER_ENABLE_PIN, OUTPUT);
        digitalWrite(RADIO_POWER_ENABLE_PIN, LOW);
#endif

        delay(3000);

#ifdef BATTERY_LPCOMP_INPUT
        // Clean re-arm of NRF_LPCOMP registers
        NRF_LPCOMP->ENABLE = LPCOMP_ENABLE_ENABLE_Disabled;

        // Select input pin (PSEL) and reference threshold (REFSEL)
        nrf_lpcomp_input_select(NRF_LPCOMP, BATTERY_LPCOMP_INPUT);

        nrf_lpcomp_config_t c;
        c.reference = BATTERY_LPCOMP_THRESHOLD;
        c.detection = NRF_LPCOMP_DETECT_UP; // flank up
        c.hyst = NRF_LPCOMP_HYST_NOHYST;
        nrf_lpcomp_configure(NRF_LPCOMP, &c);

        // Explicitly clear all event latches
        NRF_LPCOMP->EVENTS_READY = 0;
        NRF_LPCOMP->EVENTS_DOWN = 0;
        NRF_LPCOMP->EVENTS_UP = 0;
        NRF_LPCOMP->EVENTS_CROSS = 0;

        // Enable comparator and start task
        NRF_LPCOMP->ENABLE = LPCOMP_ENABLE_ENABLE_Enabled;
        NRF_LPCOMP->TASKS_START = 1;

        // Wait for comparator to stabilize
        while (NRF_LPCOMP->EVENTS_READY == 0)
            ;

        // Clear events once more to prevent warm wakeup triggers
        NRF_LPCOMP->EVENTS_READY = 0;
        NRF_LPCOMP->EVENTS_UP = 0;
        delay(10);
#endif
```

---

## Technical Rationale

1. **Graceful Reboot and DFU Bypassing**:
   The Adafruit bootloader installed on the nRF52840 Pro Micro listens for specific GPREGRET magic values and interrupt configurations to enter the USB DFU mode. When a raw `NVIC_SystemReset()` is executed directly while the SoftDevice is running and owning the vector table and interrupts, the SoftDevice state is torn down abruptly, which leaves registers in a state that gets trapped by the bootloader as a DFU entry instruction, locking the device. Coordinated resets using `sd_nvic_SystemReset()` notify the SoftDevice to tear down the BLE stack gracefully, close active DMA transfers, restore NVIC configurations, and cleanly trigger a hardware master reset, completely bypassing DFU traps.

2. **Suppressing Fake Wakeups via register-level clearing**:
   The low-power comparator (`LPCOMP`) uses latching event registers (`EVENTS_READY`, `EVENTS_DOWN`, `EVENTS_UP`, `EVENTS_CROSS`). When the comparator detect threshold is crossed, the corresponding event register latches to `1`. If these latches are not explicitly cleared to `0` at the hardware register level immediately prior to system off (`sd_power_system_off()`), the hardware still sees the active latch and immediately wakes up the CPU, causing an infinite wake/sleep loop. Setting all `EVENTS_*` registers to `0` and inserting a stabilizing `delay(10)` ensures that no transient events are pending when the MCU powers down.
