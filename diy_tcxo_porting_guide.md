# Porting & Customization Guide: nRF52840 Pro Micro DIY TCXO

This document details the complete 4-block implementation plan used to adapt the Meshtastic firmware (v2.7.26) for the custom **nRF52840 Pro Micro DIY TCXO** variant. This guide is structured to allow developers, other AI agents, or yourself to easily port this configuration to similar custom hardware layouts.

---

## Block 1: Build-Time Preferences & Admin PKI Security

To avoid modifying core firmware files with hardcoded default values, compile-time configurations are defined in the project's configuration file.

### 1. Configuration File Layout
Create or update [userPrefs.jsonc](file:///c:/Users/Jesus/Desktop/firmware/userPrefs.jsonc) at the root of the project:

```json
{
  // Enforces the legal EU_868 band directly, allowing instant RF transmission
  "USERPREFS_CONFIG_LORA_REGION": "meshtastic_Config_LoRaConfig_RegionCode_EU_868",
  
  // Modem preset configuration
  "USERPREFS_LORACONFIG_MODEM_PRESET": "meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST",
  
  // Enforce startup transmission power to a safe 8 dBm limit
  "USERPREFS_LORACONFIG_TX_POWER": "8",
  
  // Inject the authorized public keys into config.security.admin_key for remote PKI administration
  "USERPREFS_USE_ADMIN_KEY_0": "{ 0xc7, 0xdc, 0x0d, 0xe9, 0x6d, 0x13, 0xba, 0x86, 0x3f, 0x82, 0xd5, 0x4a, 0x33, 0xff, 0xa5, 0xca, 0xcc, 0x7c, 0x45, 0xec, 0x1b, 0xe4, 0x20, 0x1d, 0x67, 0xd6, 0x1f, 0xcc, 0x85, 0x51, 0x00, 0x55 }",
  
  // Custom RTTTL Ringtone
  "USERPREFS_RINGTONE_RTTTL": "24:d=32,o=5,b=565:f6,p,f6,4p,p,f6,p,f6,2p,p,b6,p,b6,p,b6,p,b6,p,b,p,b,p,b,p,b,p,b,p,b,p,b,p,b,1p.,2p.,p",
  
  "USERPREFS_TZ_STRING": "tzplaceholder                                         "
}
```

> [!TIP]
> Pre-programming the administration public key (`USERPREFS_USE_ADMIN_KEY_0`) enables remote administration over the air immediately after a factory wipe, even before the peer database is rebuilt.

---

## Block 2: Hardware Isolation, Pin Mapping & Early Boot Stabilization

This block configures physical hardware mappings, voltage dividers, and early power rails stabilization to safeguard the radio module and prevent bus conflicts.

### 1. Board Definitions File
Modify the variant header [variant.h](file:///c:/Users/Jesus/Desktop/firmware/variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h):

* **Enable Radio Power Rail:** Map `RADIO_POWER_ENABLE_PIN` to Pin 17 (P0.17).
* **Isolate Antenna Switch Conflict:** Set `RF95_RXEN` and `SX126X_RXEN` to `RADIOLIB_NC`. This prevents the radio libraries from toggling the main power rail (Pin 17) during RX/TX mode switches.
* **Define Divider Factor:** Set `VBAT_DIVIDER_COMP` to `2.0` to match the symmetrical 1:1 physical resistor network.
* **Add LPCOMP Pins:** Define `BATTERY_LPCOMP_INPUT` and `BATTERY_LPCOMP_THRESHOLD` for wake-up.

```diff
 // Pin 13 enables 3.3V periphery. If the Lora module is on this pin, then it should stay enabled at all times.
 #define PIN_3V3_EN (0 + 13) // P0.13
+#define RADIO_POWER_ENABLE_PIN (0 + 17) // P0.17 Radio Power Enable
...
 // Voltage divider value => 1.5M + 1M voltage divider on VBAT = (1.5M / (1M + 1.5M))
 #define VBAT_DIVIDER (0.6F)
 // Compensation factor for the VBAT divider
-#define VBAT_DIVIDER_COMP (1.73)
+#define VBAT_DIVIDER_COMP 2.0
...
 // RX/TX for RFM95/SX127x
-#define RF95_RXEN (0 + 17)    // P0.17
+#define RF95_RXEN RADIOLIB_NC
...
-#define SX126X_RXEN (0 + 17)     // P0.17
+#define SX126X_RXEN RADIOLIB_NC
...
 #define PIN_EINK_BUSY (32 + 6)
 
+// LPCOMP Configuration
+#define BATTERY_LPCOMP_INPUT NRF_LPCOMP_INPUT_7
+#define BATTERY_LPCOMP_THRESHOLD NRF_LPCOMP_REF_SUPPLY_9_16
```

### 2. Early Boot Hardware Neutralization
Add LPCOMP register resetting and early radio power rail activation to the top of `nrf52Setup()` in [main-nrf52.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/platform/nrf52/main-nrf52.cpp):

```cpp
void nrf52Setup()
{
#ifdef BATTERY_LPCOMP_INPUT
    // Neutralize comparator to prevent soft-reboot lockups
    NRF_LPCOMP->TASKS_STOP = 1;
    NRF_LPCOMP->ENABLE = LPCOMP_ENABLE_ENABLE_Disabled;
    NRF_LPCOMP->INTENCLR = 0xFFFFFFFF; // Clear all interrupt flags
    NRF_LPCOMP->EVENTS_READY = 0;
    NRF_LPCOMP->EVENTS_DOWN = 0;
    NRF_LPCOMP->EVENTS_UP = 0;
    NRF_LPCOMP->EVENTS_CROSS = 0;
#endif

#ifdef RADIO_POWER_ENABLE_PIN
    // Drive power rail HIGH immediately to stabilize the SPI bus and prevent parasitic loading
    pinMode(RADIO_POWER_ENABLE_PIN, OUTPUT);
    digitalWrite(RADIO_POWER_ENABLE_PIN, HIGH);
#endif
```

---

## Block 3: Deep Sleep Optimization, Debounce Filters & Coordinated Reboot

Optimizes deep energy states, clamps hardware thresholds, and handles soft-reset execution paths.

### 1. Battery Clamping & Hardware TX Limit
Define cell protection bounds and transmission ceiling macros in [variant.h](file:///c:/Users/Jesus/Desktop/firmware/variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h):

```cpp
// Set maximum allowed power for SX126x
#define SX126X_MAX_POWER 12
#define HARDWARE_TX_POWER_LIMIT 12

// Clamps the discharge curve table (OCV_ARRAY) to 3.5V (3500mV) to protect cells
#define OCV_ARRAY 4190, 4050, 3990, 3890, 3800, 3720, 3630, 3530, 3500, 3500, 3500
```

### 2. Absolute TX Power Clamping
Enforce the hardware TX ceiling at the entry point of `RadioInterface::limitPower()` in [RadioInterface.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/mesh/RadioInterface.cpp):

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
```

### 3. Low-Voltage Debounce Filter
Accelerate low-voltage shutdown checks in [Power.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/Power.cpp) by reducing the counter from 10 to 4 cycles for rapid battery protection:

```cpp
#if defined(PROMICRO_DIY_TCXO) || defined(ARDUINO_NRF52_PROMICRO_DIY_TCXO)
            LOG_DEBUG("Low voltage counter: %d/4", low_voltage_counter);
            if (low_voltage_counter > 4) {
#else
            LOG_DEBUG("Low voltage counter: %d/10", low_voltage_counter);
            if (low_voltage_counter > 10) {
#endif
```

### 4. Deep Sleep Isolation & Register-Level Re-arm
Isolate the radio module and configure register-level LPCOMP events before entering sleep in `cpuDeepSleep()` in [main-nrf52.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/platform/nrf52/main-nrf52.cpp) to prevent infinite sleep/wake loops:

```cpp
#ifdef RADIO_POWER_ENABLE_PIN
        // Completely isolate the radio module power rail
        pinMode(RADIO_POWER_ENABLE_PIN, OUTPUT);
        digitalWrite(RADIO_POWER_ENABLE_PIN, LOW);
#endif

        delay(3000); // Allow bus lines to settle

#ifdef BATTERY_LPCOMP_INPUT
        // Clean register re-arm of NRF_LPCOMP
        NRF_LPCOMP->ENABLE = LPCOMP_ENABLE_ENABLE_Disabled;
        nrf_lpcomp_input_select(NRF_LPCOMP, BATTERY_LPCOMP_INPUT);

        nrf_lpcomp_config_t c;
        c.reference = BATTERY_LPCOMP_THRESHOLD;
        c.detection = NRF_LPCOMP_DETECT_UP; // Trigger wakeup on rising edge (charge applied)
        c.hyst = NRF_LPCOMP_HYST_NOHYST;
        nrf_lpcomp_configure(NRF_LPCOMP, &c);

        // Explicitly clear all event latches
        NRF_LPCOMP->EVENTS_READY = 0;
        NRF_LPCOMP->EVENTS_DOWN = 0;
        NRF_LPCOMP->EVENTS_UP = 0;
        NRF_LPCOMP->EVENTS_CROSS = 0;

        // Enable and start
        NRF_LPCOMP->ENABLE = LPCOMP_ENABLE_ENABLE_Enabled;
        NRF_LPCOMP->TASKS_START = 1;

        while (NRF_LPCOMP->EVENTS_READY == 0)
            ;

        // Clear transient events once more
        NRF_LPCOMP->EVENTS_READY = 0;
        NRF_LPCOMP->EVENTS_UP = 0;
        delay(10);
#endif
```

---

## Block 4: Factory Reset Fix & Custom Default Override

Resolves the native v2.7.26 SoftDevice reboot freeze regression, handles thread coordination during database wipes, and sets correct default values after factory resets.

### 1. Safety Macro Initialization
Define the compilation guard macro in [variant.h](file:///c:/Users/Jesus/Desktop/firmware/variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h):

```cpp
// Fix for native regression boot/reset hang
#define FIX_NATIVE_CORE_RESET
```

### 2. Reboot Fallback to Hardware Reset
In v2.7.26, calling `sd_nvic_SystemReset()` when the SoftDevice is partially/fully shut down by `disableBluetooth()` crashes the MCU. Update `Power::reboot()` in [Power.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/Power.cpp) to fallback to the stable hardware reset:

```cpp
#elif defined(ARCH_NRF52)
#ifdef FIX_NATIVE_CORE_RESET
    // Use stable hardware reset to bypass SoftDevice/DFU bootloader traps
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

### 3. Immediate Coordinated Safety Reset
To prevent background threads (like display/telemetry) from running with empty/unset database structures after a reset, trigger `NVIC_SystemReset()` immediately inside `NodeDB::factoryReset` in [NodeDB.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/mesh/NodeDB.cpp):

```cpp
#ifdef FIX_NATIVE_CORE_RESET
    LOG_INFO("FIX_NATIVE_CORE_RESET: Immediate safety reset!");
    NVIC_SystemReset();
#endif
    return true;
}
```

### 4. Post-Reset Defaults and Safe Limit Config
When a factory reset is performed, the system clears the flash partition and recreates default channels. During this initialization, the firmware calls `Channels::initDefaultLoraConfig()` which overrides any previously assigned transmission power back to its default value (`0`). 

To ensure the default power is cleanly set to **8 dBm** after a factory reset (while still allowing the operator to scale it manually up to the absolute hardware maximum of **12 dBm**), modify `Channels::initDefaultLoraConfig()` in [Channels.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/mesh/Channels.cpp) under the safety macro:

```cpp
void Channels::initDefaultLoraConfig()
{
    meshtastic_Config_LoRaConfig &loraConfig = config.lora;

    loraConfig.modem_preset = meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST; // Default to Long Range & Fast
    loraConfig.use_preset = true;
#ifdef FIX_NATIVE_CORE_RESET
    loraConfig.tx_power = 8; // Default safety limit for HT-RA62 / DIY Pro Micro TCXO
#else
    loraConfig.tx_power = 0; // default
#endif
    loraConfig.channel_num = 0;
```
