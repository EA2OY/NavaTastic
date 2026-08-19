#include "configuration.h"

uint32_t rawResetReason = 0;
extern uint8_t currentWakeLevel;

#include <Adafruit_TinyUSB.h>
#include <Adafruit_nRFCrypto.h>
#include <InternalFileSystem.h>
#include <SPI.h>
#include <Wire.h>

#define APP_WATCHDOG_SECS 90
#define NRFX_WDT_ENABLED 1
#define NRFX_WDT0_ENABLED 1
#define NRFX_WDT_CONFIG_NO_IRQ 1
#include "nrfx_power.h"
#include <assert.h>
#include <ble_gap.h>
#include <memory.h>
#include <nrfx_wdt.c>
#include <nrfx_wdt.h>
#include <stdio.h>
// #include <Adafruit_USBD_Device.h>
#include "HardwareRNG.h"
#include "NodeDB.h"
#include "PowerMon.h"
#include "error.h"
#include "main.h"
#include "meshUtils.h"
#include "power.h"
#include "sleep.h"
#include <power/PowerHAL.h>

#include "Nrf52SaadcLock.h"
#include "concurrency/LockGuard.h"
#include <hal/nrf_lpcomp.h>
#include <hal/nrf_rtc.h>

// Declaración forward: getActiveLpcompThreshold() se usa en cpuDeepSleep()
// antes de su definición al final del archivo.
nrf_lpcomp_ref_t getActiveLpcompThreshold();

#ifdef BQ25703A_ADDR
#include "BQ25713.h"
#endif

// WARNING! THRESHOLD + HYSTERESIS should be less than regulated VDD voltage - which depends on board
// and is 3.0 or 3.3V. Also VDD likes to read values like 2.9999 so make sure you account for that
// otherwise board will not boot at all. Before you modify this part - please triple read NRF52840 power design
// section in datasheet and you understand how REG0 and REG1 regulators work together.
#ifndef SAFE_VDD_VOLTAGE_THRESHOLD
#define SAFE_VDD_VOLTAGE_THRESHOLD 2.7
#endif

// hysteresis value
#ifndef SAFE_VDD_VOLTAGE_THRESHOLD_HYST
#define SAFE_VDD_VOLTAGE_THRESHOLD_HYST 0.2
#endif

uint16_t getVDDVoltage();

// Weak empty variant shutdown prep function.
// May be redefined by variant files.
void variant_shutdown() __attribute__((weak));
void variant_shutdown() {}

// Optional variant hook called each nrf52Loop(); e.g. for low-VDD System OFF.
void variant_nrf52LoopHook(void) __attribute__((weak));
void variant_nrf52LoopHook(void) {}

static nrfx_wdt_t nrfx_wdt = NRFX_WDT_INSTANCE(0);
static nrfx_wdt_channel_id nrfx_wdt_channel_id_nrf52_main;

// This is a public global so that the debugger can set it to false automatically from our gdbinit
// @phaseloop comment: most part of codebase, including filesystem flash driver depend on softdevice
// methods so disabling it may actually crash thing. Proceed with caution.

bool useSoftDevice = true; // Set to false for easier debugging

// Forzado de BLE off por resiliencia (/resilience.bin ble_disabled=1).
// Lo fija NavaCLIModule al boot antes de que PowerFSM encienda BLE.
bool bleForceDisabled = false;
void setBleForceDisabled(bool on)
{
    bleForceDisabled = on;
}

static inline void debugger_break(void)
{
    __asm volatile("bkpt #0x01\n\t"
                   "mov pc, lr\n\t");
}

// PowerHAL NRF52 specific function implementations
bool powerHAL_isVBUSConnected()
{
    return NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk;
}

bool powerHAL_isPowerLevelSafe()
{
    static bool powerLevelSafe = true;

#ifdef SAFE_VDD_VOLTAGE_THRESHOLD_MV
    uint16_t threshold = SAFE_VDD_VOLTAGE_THRESHOLD_MV;
#else
    uint16_t threshold = (uint16_t)(SAFE_VDD_VOLTAGE_THRESHOLD * 1000.0f + 0.5f); // convert V to mV
#endif
#ifdef SAFE_VDD_VOLTAGE_THRESHOLD_HYST_MV
    uint16_t hysteresis = SAFE_VDD_VOLTAGE_THRESHOLD_HYST_MV;
#else
    uint16_t hysteresis = (uint16_t)(SAFE_VDD_VOLTAGE_THRESHOLD_HYST * 1000.0f + 0.5f);
#endif

    if (powerLevelSafe) {
        if (getVDDVoltage() < threshold) {
            powerLevelSafe = false;
        }
    } else {
        // power level is only safe again when it raises above threshold + hysteresis
        if (getVDDVoltage() >= (threshold + hysteresis)) {
            powerLevelSafe = true;
        }
    }

    return powerLevelSafe;
}

void powerHAL_platformInit()
{

    // Enable POF power failure comparator. It will prevent writing to NVMC flash when supply voltage is too low.
    // Set to some low value as last resort - powerHAL_isPowerLevelSafe uses different method and should manage proper node
    // behaviour on its own.

    // POFWARN is pretty useless for node power management because it triggers only once and clearing this event will not
    // re-trigger it again until voltage rises to safe level and drops again. So we will use SAADC routed to VDD to read safely
    // voltage.

    // @phaseloop: I disable POFCON for now because it seems to be unreliable or buggy. Even when set at 2.0V it
    // triggers below 2.8V and corrupts data when pairing bluetooth - because it prevents filesystem writes and
    // adafruit BLE library triggers lfs_assert which reboots node and formats filesystem.
    // I did experiments with bench power supply and no matter what is set to POFCON, it always triggers right below
    // 2.8V. I compared raw registry values with datasheet.

    NRF_POWER->POFCON =
        ((POWER_POFCON_THRESHOLD_V22 << POWER_POFCON_THRESHOLD_Pos) | (POWER_POFCON_POF_Enabled << POWER_POFCON_POF_Pos));

    // remember to always match VBAT_AR_INTERNAL with AREF_VALUE in variant definition file
#ifdef VBAT_AR_INTERNAL
    analogReference(VBAT_AR_INTERNAL);
#else
    analogReference(AR_INTERNAL); // 3.6V
#endif
}

// get VDD voltage (in millivolts)
uint16_t getVDDVoltage()
{
    concurrency::LockGuard guard(concurrency::nrf52SaadcLock);

    // Match battery read resolution; SAADC is shared with AnalogBatteryLevel in Power.cpp.
    analogReadResolution(BATTERY_SENSE_RESOLUTION_BITS);

    // VDD range on NRF52840 is 1.8-3.3V so we need to remap analog reference to 3.6V
    analogReference(AR_INTERNAL);

    uint16_t vddADCRead = analogReadVDD();
    float voltage = ((1000 * 3.6) / pow(2, BATTERY_SENSE_RESOLUTION_BITS)) * vddADCRead;

// restore default battery reading reference
#ifdef VBAT_AR_INTERNAL
    analogReference(VBAT_AR_INTERNAL);
#endif

    return voltage;
}

bool loopCanSleep()
{
    // turn off sleep only while connected via USB
    // return true;
    return !Serial; // the bool operator on the nrf52 serial class returns true if connected to a PC currently
    // return !(TinyUSBDevice.mounted() && !TinyUSBDevice.suspended());
}

// handle standard gcc assert failures
void __attribute__((noreturn)) __assert_func(const char *file, int line, const char *func, const char *failedexpr)
{
    LOG_ERROR("assert failed %s: %d, %s, test=%s", file, line, func, failedexpr);
    // debugger_break(); FIXME doesn't work, possibly not for segger
    // Reboot cpu
    NVIC_SystemReset();
}

void getMacAddr(uint8_t *dmac)
{
    const uint8_t *src = (const uint8_t *)NRF_FICR->DEVICEADDR;
    dmac[5] = src[0];
    dmac[4] = src[1];
    dmac[3] = src[2];
    dmac[2] = src[3];
    dmac[1] = src[4];
    dmac[0] = src[5] | 0xc0; // MSB high two bits get set elsewhere in the bluetooth stack
}

#if !MESHTASTIC_EXCLUDE_BLUETOOTH
void setBluetoothEnable(bool enable)
{
    // For debugging use: don't use bluetooth
    if (!useSoftDevice) {
        if (enable)
            LOG_INFO("Disable NRF52 BLUETOOTH WHILE DEBUGGING");
        return;
    }

    // If user disabled bluetooth: init then disable advertising & reduce power
    // Workaround. Avoid issue where device hangs several days after boot..
    // Allegedly, no significant increase in power consumption
    if (!config.bluetooth.enabled) {
        static bool initialized = false;
        if (!initialized) {
            nrf52Bluetooth = new NRF52Bluetooth();
            nrf52Bluetooth->startDisabled();
            initialized = true;
        }
        return;
    }

    if (enable) {
        powerMon->setState(meshtastic_PowerMon_State_BT_On);

        // If not yet set-up
        if (!nrf52Bluetooth) {
            LOG_DEBUG("Init NRF52 Bluetooth");
            nrf52Bluetooth = new NRF52Bluetooth();
            nrf52Bluetooth->setup();
        }
        // Already setup, apparently
        else
            nrf52Bluetooth->resumeAdvertising();
    }
    // Disable (if previously set-up)
    else if (nrf52Bluetooth) {
        powerMon->clearState(meshtastic_PowerMon_State_BT_On);
        nrf52Bluetooth->shutdown();
    }
}
#else
#warning NRF52 "Bluetooth disable" workaround does not apply to builds with MESHTASTIC_EXCLUDE_BLUETOOTH
void setBluetoothEnable(bool enable) {}
#endif
/**
 * Override printf to use the SEGGER output library (note - this does not effect the printf method on the debug console)
 */
int printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    auto res = SEGGER_RTT_vprintf(0, fmt, &args);
    va_end(args);
    return res;
}

namespace
{
constexpr uint8_t NRF52_MAGIC_LFS_IS_CORRUPT = 0xF5;
constexpr uint32_t MULTIPLE_CORRUPTION_DELAY_MILLIS = 20 * 60 * 1000;
static unsigned long millis_until_formatting_again = 0;

// Report the critical error from loop(), giving a chance for the screen to be initialized first.
inline void reportLittleFSCorruptionOnce()
{
    static bool report_corruption = !!millis_until_formatting_again;
    if (report_corruption) {
        report_corruption = false;
        RECORD_CRITICALERROR(meshtastic_CriticalErrorCode_FLASH_CORRUPTION_UNRECOVERABLE);
    }
}
} // namespace

void preFSBegin()
{
    // The GPREGRET register keeps its value across warm boots. Check that this is a warm boot and, if GPREGRET
    // is set to NRF52_MAGIC_LFS_IS_CORRUPT, format LittleFS.
    if (!(NRF_POWER->RESETREAS == 0 && NRF_POWER->GPREGRET == NRF52_MAGIC_LFS_IS_CORRUPT))
        return;
    NRF_POWER->GPREGRET = 0;
    millis_until_formatting_again = millis() + MULTIPLE_CORRUPTION_DELAY_MILLIS;
    InternalFS.format();
    LOG_INFO("LittleFS format complete; restoring default settings");
}

extern "C" void lfs_assert(const char *reason)
{
    LOG_ERROR("LittleFS corruption detected: %s", reason);
    if (millis_until_formatting_again > millis()) {
        RECORD_CRITICALERROR(meshtastic_CriticalErrorCode_FLASH_CORRUPTION_UNRECOVERABLE);
        const long millis_remain = millis_until_formatting_again - millis();
        LOG_WARN("Pausing %d seconds to avoid wear on flash storage", millis_remain / 1000);
        delay(millis_remain);
    }
    LOG_INFO("Rebooting to format LittleFS");
    delay(500); // Give the serial port a bit of time to output that last message.
    // Try setting GPREGRET with the SoftDevice first. If that fails (perhaps because the SD hasn't been initialize yet) then set
    // NRF_POWER->GPREGRET directly.

    // TODO: this will/can crash CPU if bluetooth stack is not compiled in or bluetooth is not initialized
    // (regardless if enabled or disabled) - as there is no live SoftDevice stack
    // implement "safe" functions detecting softdevice stack state and using proper method to set registers

    // do not set GPREGRET if POFWARN is triggered because it means lfs_assert reports flash undervoltage protection
    // and not data corruption. Reboot is fine as boot procedure will wait until power level is safe again

    if (!NRF_POWER->EVENTS_POFWARN) {
        if (!(sd_power_gpregret_clr(0, 0xFF) == NRF_SUCCESS &&
              sd_power_gpregret_set(0, NRF52_MAGIC_LFS_IS_CORRUPT) == NRF_SUCCESS)) {
            NRF_POWER->GPREGRET = NRF52_MAGIC_LFS_IS_CORRUPT;
        }
    }

    // TODO: this should not be done when SoftDevice is enabled as device will not boot back on soft reset
    // as some data is retained in RAM which will prevent re-enabling bluetooth stack
    // Google what Nordic has to say about NVIC_* + SoftDevice
    NVIC_SystemReset();
}

void checkSDEvents()
{
    if (useSoftDevice) {
        uint32_t evt;
        while (NRF_SUCCESS == sd_evt_get(&evt)) {
            switch (evt) {
            case NRF_EVT_POWER_FAILURE_WARNING:
                RECORD_CRITICALERROR(meshtastic_CriticalErrorCode_BROWNOUT);
                break;

            default:
                LOG_DEBUG("Unexpected SDevt %d", evt);
                break;
            }
        }
    } else {
        if (NRF_POWER->EVENTS_POFWARN)
            RECORD_CRITICALERROR(meshtastic_CriticalErrorCode_BROWNOUT);
    }
}

void nrf52Loop()
{
    {
        static bool watchdog_running = false;
        if (!watchdog_running) {
            nrfx_wdt_enable(&nrfx_wdt);
            watchdog_running = true;
        }
    }
    nrfx_wdt_channel_feed(&nrfx_wdt, nrfx_wdt_channel_id_nrf52_main);

    checkSDEvents();
    reportLittleFSCorruptionOnce();

    variant_nrf52LoopHook(); // Optional variant hook called each nrf52Loop();
}

#ifdef USE_SEMIHOSTING
#include <SemihostingStream.h>
#include <meshUtils.h>

/**
 * Note: this variable is in BSS and therfore false by default.  But the gdbinit
 * file will be installing a temporary breakpoint that changes wantSemihost to true.
 */
bool wantSemihost;

/**
 * Turn on semihosting if the ICE debugger wants it.
 */
void nrf52InitSemiHosting()
{
    if (wantSemihost) {
        static SemihostingStream semiStream;
        // We must dynamically alloc because the constructor does semihost operations which
        // would crash any load not talking to a debugger
        semiStream.open();
        semiStream.println("Semihosting starts!");
        // Redirect our serial output to instead go via the ICE port
        console->setDestination(&semiStream);
    }
}
#endif

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

    uint32_t why = NRF_POWER->RESETREAS;
    rawResetReason = why;
    // per
    // https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.nrf52832.ps.v1.1%2Fpower.html
    LOG_DEBUG("Reset reason: 0x%x", why);

#ifdef USE_SEMIHOSTING
    nrf52InitSemiHosting();
#endif

    // Per
    // https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/monitor-mode-debugging-with-j-link-and-gdbeclipse
    // This is the recommended setting for Monitor Mode Debugging
    NVIC_SetPriority(DebugMonitor_IRQn, 6UL);

#ifdef BQ25703A_ADDR
    auto *bq = new BQ25713();
    if (!bq->setup())
        LOG_ERROR("ERROR! Charge controller init failed");
#endif

    // Init random seed
    uint32_t seed = 0;
    if (!HardwareRNG::seed(seed)) {
        LOG_WARN("Hardware RNG seed unavailable, using PRNG fallback");
        // Use a hardware timer value as a fallback seed for better entropy
        seed = micros();
    }
    LOG_DEBUG("Set random seed %u", seed);
    randomSeed(seed);

    // Set up nrfx watchdog. Do not enable the watchdog yet (we do that
    // the first time through the main loop), so that other threads can
    // allocate their own wdt channel to protect themselves from hangs.
    nrfx_wdt_config_t wdt0_config = {
        .behaviour = NRF_WDT_BEHAVIOUR_PAUSE_SLEEP_HALT, .reload_value = APP_WATCHDOG_SECS * 1000,
        // Note: Not using wdt interrupts.
        // .interrupt_priority = NRFX_WDT_DEFAULT_CONFIG_IRQ_PRIORITY
    };
    nrfx_err_t r = nrfx_wdt_init(&nrfx_wdt, &wdt0_config,
                                 nullptr // Watchdog event handler, not used, we just reset.
    );
    // NAVARICO: assert() embebe __LINE__. El main-nrf52.cpp de los repos SX1262 originales no lleva
    // los bloques RADIO_POWER_ENABLE_PIN, asi que sus asserts viven en 451/454; los E22P en 456/459.
    // #line (expande macros) fija la linea reportada por radio -> paridad byte-a-byte en ambos.
#ifdef NAVARICO_RADIO_E22P
#define NAVARICO_LINE_ASSERT1 456
#else
#define NAVARICO_LINE_ASSERT1 451
#endif
#line NAVARICO_LINE_ASSERT1
    assert(r == NRFX_SUCCESS);

    r = nrfx_wdt_channel_alloc(&nrfx_wdt, &nrfx_wdt_channel_id_nrf52_main);
#ifdef NAVARICO_RADIO_E22P
#define NAVARICO_LINE_ASSERT2 459
#else
#define NAVARICO_LINE_ASSERT2 454
#endif
#line NAVARICO_LINE_ASSERT2
    assert(r == NRFX_SUCCESS);
}

void cpuDeepSleep(uint32_t msecToWake)
{
    // FIXME, configure RTC or button press to wake us
    // FIXME, power down SPI, I2C, RAMs
#if HAS_WIRE
    Wire.end();
#endif
    SPI.end();
#if SPI_INTERFACES_COUNT > 1
    SPI1.end();
#endif
    if (Serial)       // Another check in case of disabled default serial, does nothing bad
        Serial.end(); // This may cause crashes as debug messages continue to flow.

        // This causes troubles with waking up on nrf52 (on pro-micro in particular):
        // we have no Serial1 in use on nrf52, check Serial and GPS modules.
#ifdef PIN_SERIAL1_RX
    if (Serial1) // A straightforward solution to the wake from deepsleep problem
        Serial1.end();
#endif

    setBluetoothEnable(false);

#ifdef RAK4630
#ifdef PIN_3V3_EN
    digitalWrite(PIN_3V3_EN, LOW);
#endif
#ifdef AQ_SET_PIN
    // RAK-12039 set pin for Air quality sensor
    digitalWrite(AQ_SET_PIN, LOW);
#endif
#endif
    // Run shutdown code if specified in variant.cpp
    variant_shutdown();

    // Sleepy trackers or sensors can low power "sleep"
    // Don't enter this if we're sleeping portMAX_DELAY, since that's a shutdown event
    if (msecToWake != portMAX_DELAY &&
        (IS_ONE_OF(config.device.role, meshtastic_Config_DeviceConfig_Role_TRACKER,
                   meshtastic_Config_DeviceConfig_Role_TAK_TRACKER, meshtastic_Config_DeviceConfig_Role_SENSOR) &&
         config.power.is_power_saving == true)) {
        sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
        delay(msecToWake);
        NVIC_SystemReset();
    } else {
        // Resume on user button press
        // https://github.com/lyusupov/SoftRF/blob/81c519ca75693b696752235d559e881f2e0511ee/software/firmware/source/SoftRF/src/platform/nRF52.cpp#L1738
        constexpr uint32_t DFU_MAGIC_SKIP = 0x6d;
        sd_power_gpregret_clr(0, 0xFF);           // Clear the register before setting a new values in it for stability reasons
        sd_power_gpregret_set(0, DFU_MAGIC_SKIP); // Equivalent NRF_POWER->GPREGRET = DFU_MAGIC_SKIP

        // FIXME, use system off mode with ram retention for key state?
        // FIXME, use non-init RAM per
        // https://devzone.nordicsemi.com/f/nordic-q-a/48919/ram-retention-settings-with-softdevice-enabled

#ifdef RADIO_POWER_ENABLE_PIN
        pinMode(RADIO_POWER_ENABLE_PIN, OUTPUT);
        digitalWrite(RADIO_POWER_ENABLE_PIN, LOW);
#endif

        delay(3000);

#ifdef BATTERY_LPCOMP_INPUT
#ifdef ADC_CTRL
        // Keep the battery voltage divider enabled during sleep so LPCOMP reads the divided voltage
        pinMode(ADC_CTRL, OUTPUT);
        digitalWrite(ADC_CTRL, ADC_CTRL_ENABLED);
#endif
        // Clean re-arm of NRF_LPCOMP registers
        NRF_LPCOMP->ENABLE = LPCOMP_ENABLE_ENABLE_Disabled;

        // Select input pin (PSEL) and reference threshold (REFSEL)
        nrf_lpcomp_input_select(NRF_LPCOMP, BATTERY_LPCOMP_INPUT);

        nrf_lpcomp_config_t c;
        c.reference = getActiveLpcompThreshold();
        c.detection = NRF_LPCOMP_DETECT_UP; // flank up
        c.hyst = NRF_LPCOMP_HYST_ENABLED;
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

#ifdef PIN_LED1
        // V2.4: apagar el LED de estado justo antes de System OFF: los GPIO quedan
        // enclavados y un LED encendido consume ~10 mA en sueno profundo (el nodo
        // dormido debe quedar en ~1 mA). Se escribe AL FINAL (tras la estabilizacion)
        // para que el hilo del latido no lo vuelva a encender antes de apagar.
        pinMode(PIN_LED1, OUTPUT);
        digitalWrite(PIN_LED1, LED_STATE_ON ? LOW : HIGH);
#endif

        auto ok = sd_power_system_off();
        if (ok != NRF_SUCCESS) {
            LOG_ERROR("FIXME: Ignoring soft device (EasyDMA pending?) and forcing system-off!");
            NRF_POWER->SYSTEMOFF = 1;
        }
    }

    // The following code should not be run, because we are off
    while (1) {
        delay(5000);
        LOG_DEBUG(".");
    }
}

void clearBonds()
{
    if (!nrf52Bluetooth) {
        nrf52Bluetooth = new NRF52Bluetooth();
        nrf52Bluetooth->setup();
    }
    nrf52Bluetooth->clearBonds();
}

void enterDfuMode()
{
// SDK kit does not have native USB like almost all other NRF52 boards
#ifdef NRF_USE_SERIAL_DFU
    enterSerialDfu();
#else
    enterUf2Dfu();
#endif
}

nrf_lpcomp_ref_t getActiveLpcompThreshold() {
    // Voltajes de despertar calculados con divisor fisico 0.5 (1M/1M) y VDD 3.3V.
    // El LPCOMP compara el pin (bateria x divisor) contra la fraccion de VDD.
    // NAVARICO: bloques por placa incorporados de los repos originales (Seed, Xiao, T114).
    // Cada placa con divisor != 0.5 usa el umbral de fabrica (los niveles 1-5 calibrados
    // al Promicro serian inalcanzables). La macro de placa la define el env de PlatformIO.
#ifdef SEEED_SOLAR_NODE
    // Seed Solar Node P1: divisor de la placa NO es 0.5 (es ~0.303 segun ADC_MULTIPLIER 3.3).
    // Los niveles 1-5 estan calibrados al divisor del Promicro y serian inalcanzables aqui.
    // Se usa el umbral de la referencia funcional (port 24/07): BATTERY_LPCOMP_THRESHOLD = 3_8 (~3.67V real).
    // Mantener la histeresis para evitar rebotes de despertar.
    return BATTERY_LPCOMP_THRESHOLD; // NRF_LPCOMP_REF_SUPPLY_3_8
#elif defined(SEEED_XIAO_NRF52840_KIT)
    // Xiao Kit i2c / Xiao E22P: divisor de fabrica 1M/510k (~0.3377), NO 0.5.
    // Los niveles 1-5 estan calibrados al divisor del Promicro y serian inalcanzables aqui.
    // Se usa el umbral de fabrica Meshtastic: BATTERY_LPCOMP_THRESHOLD = 3_8 (~3.67V real).
    return BATTERY_LPCOMP_THRESHOLD; // NRF_LPCOMP_REF_SUPPLY_3_8
#elif defined(HELTEC_T114)
    // Heltec T114: divisor de fabrica 100/490 (~0.204), NO 0.5. Los niveles 1-5 estan
    // calibrados al divisor del Promicro y serian inalcanzables aqui (9_16 ~9.1V).
    // Se usa el umbral de fabrica Meshtastic: BATTERY_LPCOMP_THRESHOLD = 2_8 (~4.04V).
    // Ojo: Meshtastic lo desactiva por fuga de 2.9mA en System OFF (issue #8801); aqui se activa.
    return BATTERY_LPCOMP_THRESHOLD; // NRF_LPCOMP_REF_SUPPLY_2_8
#else
    switch (currentWakeLevel) {
        case 1: return NRF_LPCOMP_REF_SUPPLY_5_16; //  ~2.06V real
        case 2: return NRF_LPCOMP_REF_SUPPLY_3_8;  //  ~2.48V real
        case 3: return NRF_LPCOMP_REF_SUPPLY_9_16; //  ~3.71V real (Default, verificado ~3.8V)
        case 4: return NRF_LPCOMP_REF_SUPPLY_11_16;//  ~4.54V real (solo con bateria alta)
        case 5: return NRF_LPCOMP_REF_SUPPLY_4_8;  //  ~3.30V real (ideal para LiFePO4)
        default: return (nrf_lpcomp_ref_t)BATTERY_LPCOMP_THRESHOLD;
    }
#endif
}

// V2: tension teorica de despertar por LPCOMP en mV (para los mensajes [Sueño]/[Vivo])
uint16_t navaGetLpcompWakeMv() {
#if defined(SEEED_SOLAR_NODE) || defined(SEEED_XIAO_NRF52840_KIT)
    return 3670; // 3_8 real (~3.67V) en Seed y Xiao x2 (divisor de placa)
#elif defined(HELTEC_T114)
    return 4040; // 2_8 real (~4.04V) en T114 (divisor 100/490)
#else
    switch (currentWakeLevel) {
        case 1: return 2060;
        case 2: return 2480;
        case 3: return 3710; // default LiPo/NiMH/Sodio
        case 4: return 4540;
        case 5: return 3300; // LiFePO4
        default: return 3710;
    }
#endif
}

// ---------------------------------------------------------------------------
// Storm / hibernación temporizada (RTC2 + LOWPWR).
// NO usar sd_power_system_off(): System OFF solo despierta por GPIO/LPCOMP/NFC/reset
// físico, no por temporizador, y con batería por encima del umbral LPCOMP el nodo
// quedaría dormido para siempre. Aquí se usa System ON en bajo consumo + RTC2 COMPARE.
// El contador RTC es de 24 bits a 32768 Hz (~512s máx por compare), así que el
// objetivo se cubre con bloques de max 500s re-armando el COMPARE en cada wake.
// Al cumplirse el tiempo total se ejecuta NVIC_SystemReset(): el boot re-inicializa
// la radio HT-RA62 (RXEN pin 17, apagado por SPI/driver, sin pin de alimentación).
// ---------------------------------------------------------------------------
#define STORM_BLOCK_SECS 500u
#define RTC_FREQ_HZ 32768u
#define RTC_CC_MAX 0xFFFFFFu

static volatile bool rtc2StormWake = false;

// Enlace C obligatorio: la tabla de vectores (gcc_startup_nrf52840.S) referencia
// este símbolo sin mangling C++.
extern "C" void RTC2_IRQHandler(void)
{
    if (nrf_rtc_event_check(NRF_RTC2, NRF_RTC_EVENT_COMPARE_0)) {
        nrf_rtc_event_clear(NRF_RTC2, NRF_RTC_EVENT_COMPARE_0);
        rtc2StormWake = true;
    }
}

void timedSystemSleepSeconds(uint32_t seconds)
{
    if (seconds == 0) {
        return;
    }

    LOG_INFO("Storm: entering RTC2 timed sleep for %lu seconds", (unsigned long)seconds);

    // 1. Dormir la radio DE VERDAD: notifyDeepSleep dispara RadioInterface::sleep()
    //    (setStandby + lora.sleep(keepConfig) en SX1262). Debe hacerse ANTES de
    //    SPI.end(), porque el comando SLEEP viaja por SPI. Sin esto la radio
    //    quedaba en RX consumiendo ~10mA.
    notifyDeepSleep.notifyObservers(NULL);

#ifdef HAS_WIRE
    Wire.end();
#endif
    SPI.end();
#ifdef PIN_SERIAL1_RX
    if (Serial1)
        Serial1.end();
#endif
    setBluetoothEnable(false);

#ifdef RADIO_POWER_ENABLE_PIN
    // Apagar la radio E22P (ahorra ~40mA durante la hibernación)
    pinMode(RADIO_POWER_ENABLE_PIN, OUTPUT);
    digitalWrite(RADIO_POWER_ENABLE_PIN, LOW);
#endif

    // Apagar pantalla si existe (draw ~10uA en deep sleep)
    if (screen) {
        screen->doDeepSleep();
    }

    // Configurar RTC2 (LFCLK 32768Hz, prescaler 0)
    nrf_rtc_prescaler_set(NRF_RTC2, 0);
    nrf_rtc_task_trigger(NRF_RTC2, NRF_RTC_TASK_CLEAR);
    nrf_rtc_event_enable(NRF_RTC2, RTC_CHANNEL_INT_MASK(0));
    nrf_rtc_int_enable(NRF_RTC2, RTC_CHANNEL_INT_MASK(0));
    NVIC_ClearPendingIRQ(RTC2_IRQn);
    NVIC_EnableIRQ(RTC2_IRQn);

    uint32_t remaining = seconds;
    while (remaining > 0) {
        uint32_t block = (remaining > STORM_BLOCK_SECS) ? STORM_BLOCK_SECS : remaining;
        uint32_t target = (nrf_rtc_counter_get(NRF_RTC2) + block * RTC_FREQ_HZ) & RTC_CC_MAX;

        nrf_rtc_cc_set(NRF_RTC2, 0, target);
        nrf_rtc_task_trigger(NRF_RTC2, NRF_RTC_TASK_CLEAR);
        nrf_rtc_task_trigger(NRF_RTC2, NRF_RTC_TASK_START);

        rtc2StormWake = false;

        // Dormir la CPU hasta el COMPARE del RTC2.
        // sd_app_evt_wait() es la API correcta con SoftDevice: deja que el stack
        // BLE/radio gestione el sleep real. __WFE() despertaba la CPU en bucle
        // por eventos internos del SoftDevice, anulando el ahorro de energía.
        while (!rtc2StormWake) {
            sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
            sd_app_evt_wait();
            // Los eventos pendientes pueden despertar sin que el RTC haya disparado;
            // volvemos a dormir hasta que el flag de COMPARE se active.
        }

        remaining -= block;
    }

    LOG_INFO("Storm: timed sleep elapsed, rebooting to restore radio");

    // Apagar RTC2 y reiniciar: el boot restaura la radio y periféricos
    nrf_rtc_task_trigger(NRF_RTC2, NRF_RTC_TASK_STOP);
    NVIC_DisableIRQ(RTC2_IRQn);
    NVIC_SystemReset();
    while (1) {
        delay(1000);
    }
}
