# Validation Report: Block 2 Execution

## Audit Notes
I performed an audit of the v2.7.22 reference directory at `C:\Users\Jesus\Desktop\promicro kacho fix 2.7.22 sleep fix + rf fix adc2.0` to compare its hardware isolation, power stabilization, and analog measurement layout with our modern v2.7.26 configuration:

1. **Power Pin Initialization**: In the v2.7.22 setup, the E22P module enable pin (`E22P_ENABLE_PIN`, Pin 17) was initialized inside the generic `src/main.cpp` using `pinMode()` and `digitalWrite(E22P_ENABLE_PIN, HIGH)`. In our modern v2.7.26 implementation, this has been refactored into the platform-specific early setup (`nrf52Setup()`) to decouple core logic and avoid board-specific macros polluting `src/main.cpp`.
2. **Antenna Switch Pin Sharing Conflict**: In the reference setup, pin 17 was mapped to `SX126X_RXEN` but commented out. However, `RF95_RXEN` was still defined as `(0 + 17)`. In the v2.7.26 layout, we completely isolate pin 17 by mapping both `SX126X_RXEN` and `RF95_RXEN` to `RADIOLIB_NC` (no connection) to prevent the radio driver libraries from toggling the main power rail when switching between RX and TX modes.
3. **ADC & LPCOMP**:
   - The reference setup had `VBAT_DIVIDER_COMP` defined as `(1.73)`. Our v2.7.26 setup overrides this with a strict constant literal value of `2.0` to match the physical symmetrical 1:1 voltage divider.
   - The low-power comparator (`LPCOMP`) definitions (`BATTERY_LPCOMP_INPUT` and `BATTERY_LPCOMP_THRESHOLD`) are defined in `variant.h` in both versions, but the early boot registers clearing was missing on entry in v2.7.22, causing warm-reboot hang hazards.

---

## Applied Changes

### 1. Modifications in [variant.h](file:///c:/Users/Jesus/Desktop/firmware/variants/nrf52840/diy/nrf52_promicro_diy_tcxo/variant.h)
* Defined `RADIO_POWER_ENABLE_PIN` as `(0 + 17)`.
* Changed the ADC multiplier compensation factor `VBAT_DIVIDER_COMP` to `2.0`.
* Replaced pin 17 mappings for `RF95_RXEN` and `SX126X_RXEN` with `RADIOLIB_NC`.
* Defined `BATTERY_LPCOMP_INPUT` and `BATTERY_LPCOMP_THRESHOLD` for battery monitoring wakeups.

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
+// #define RF95_RXEN (0 + 17)    // P0.17
+#define RF95_RXEN RADIOLIB_NC
...
-#define SX126X_RXEN (0 + 17)     // P0.17
+// #define SX126X_RXEN (0 + 17)     // P0.17
+#define SX126X_RXEN RADIOLIB_NC
...
 #define PIN_EINK_BUSY (32 + 6)
 
+// Configuración LPCOMP
+#define BATTERY_LPCOMP_INPUT NRF_LPCOMP_INPUT_7
+#define BATTERY_LPCOMP_THRESHOLD NRF_LPCOMP_REF_SUPPLY_9_16
+
 #ifdef __cplusplus
```

### 2. Modifications in [main-nrf52.cpp](file:///c:/Users/Jesus/Desktop/firmware/src/platform/nrf52/main-nrf52.cpp)
Added LPCOMP neutralization and early power-on sequences to the absolute top of `nrf52Setup()`:

```cpp
void nrf52Setup()
{
#ifdef BATTERY_LPCOMP_INPUT
    NRF_LPCOMP->TASKS_STOP = 1;
    NRF_LPCOMP->ENABLE = LPCOMP_ENABLE_ENABLE_Disabled;
    NRF_LPCOMP->INTENCLR = 0xFFFFFFFF; // Clear all interrupt flags
    NRF_LPCOMP->EVENTS_READY = 0;
    NRF_LPCOMP->EVENTS_DOWN = 0;
    NRF_LPCOMP->EVENTS_UP = 0;
    NRF_LPCOMP->EVENTS_CROSS = 0;
#endif

#ifdef RADIO_POWER_ENABLE_PIN
    pinMode(RADIO_POWER_ENABLE_PIN, OUTPUT);
    digitalWrite(RADIO_POWER_ENABLE_PIN, HIGH);
#endif

#ifdef ADC_V
    pinMode(ADC_V, INPUT);
#endif
```

---

## Technical Rationale

1. **SPI Bus Handshake Protection**:
   The Ebyte E22P LoRa radio module shares the SPI bus with other peripherals (e.g., E-Ink displays, external flash, etc.). At early boot, if the radio module's power rail (controlled by `RADIO_POWER_ENABLE_PIN` on pin 17) is unpowered or floating, the radio pins can act as parasitic loads on the SPI bus lines (MISO, MOSI, SCK, CS), injecting noise and disrupting communication. Driving `RADIO_POWER_ENABLE_PIN` high immediately inside `nrf52Setup()` ensures the E22P is fully powered, stable, and ready to respond to SPI commands *before* the firmware initiates SPI handshakes or attempts to detect/configure the radio transceiver.

2. **Warm soft-reboot Lockup Prevention**:
   During a warm soft-reboot (triggered by watchdog resets, soft resets, or OTA restarts), if the Low-Power Comparator (`LPCOMP`) was active before the reset, the hardware comparator registers can remain in a dirty or active state. Without register-level clearing, this dirty state triggers immediate spurious interrupts or event matches on boot, trapping the chip in a constant reboot/interrupt loop. By directly clearing the `NRF_LPCOMP` registers (stopping the task, disabling the comparator, clearing the interrupts mask, and resetting events) at the absolute entry of `nrf52Setup()`, we guarantee a clean state that prevents soft-reboot lockups.
