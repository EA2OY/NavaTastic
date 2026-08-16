# Guía de Portabilidad e Integración — Firmware Navarrico v4.2.1 Fused

> **ADENDA 14/08/2026 — REPO UNIFICADO**: el código canónico ya no vive en las 24
> carpetas de 4.3 (**obsoletas**): vive en **`C:\NavaTastic Codigo completo\src\`** y
> `variants\` (un solo repo, 12 envs). Esta guía sigue siendo la referencia técnica de
> los bloques; para PORTAR a un fork nuevo usar la guía maestra
> **`docs/PORTING_NUEVO_FORK.md`** (inventario con anclas, catálogo de
> bloques con dependencias, procedimiento y checklist de trampas). `C:\Firmware
> Navarrico 4.3` es SOLO LECTURA (archivo histórico).

Documento **2 de 3** del proyecto Navarrico. Esta guía detalla el **paso a paso técnico** para tomar un repositorio original de Meshtastic (v2.7.26 o similar) sin modificar e implementarle todas las mejoras de **Rama 1 (Resiliencia Física y Energía)**, **Rama 2 (Protección de Flash e Infraestructura)** y la **Secuencia de Comandos Remota 2**, actualizadas a la ronda de auditoría **v4.2.1**.

> El código canónico vive en los repos de `Rama 2 Infraestructura\Infraestructura Propia\` (Promicro fix y Faketec PROPIA). Este documento mantiene bloques de código actualizados para auditar o portar sin necesidad de poseer todo el repositorio.

---

## ÍNDICE
1. [CÓDIGO COMÚN (todas las placas y radios)](#1-código-común)
   * [A. Lógica Anticolapso de Batería (main.cpp)](#a-lógica-anticolapso-de-batería-maincpp)
   * [B. Configuración de Sueño y Despertar LPCOMP (main-nrf52.cpp)](#b-configuración-de-sueño-y-despertar-lpcomp-main-nrf52cpp)
   * [C. Filtro de Guardado en Flash de la Base de Nodos (NodeDB.cpp)](#c-filtro-de-guardado-en-flash-de-la-base-de-nodos-nodedbcpp)
   * [D. Lógica de Auto-Favoritos con Estrella (NodeDB.cpp)](#d-lógica-de-auto-favoritos-con-estrella-nodedbcpp)
   * [E. Bypass de Límite de Saltos (Router.cpp)](#e-bypass-de-límite-de-saltos-routercpp)
   * [F. Anulación del Guardado de Historial (TransmitHistory.cpp)](#f-anulación-del-guardado-de-historial-transmithistorycpp)
   * [G. Creación Remota de Favoritos (NodeDB + AdminModule)](#g-creación-remota-de-favoritos-nodedb--adminmodule)
   * [H. Blindaje contra Factory Reset (NodeDB.cpp)](#h-blindaje-contra-factory-reset-nodedbcpp)
   * [I. Claves de Administrador Dinámicas (NodeDB.cpp)](#i-claves-de-administrador-dinámicas-nodedbcpp)
   * [J. Gestión de Alimentación de la Radio E22P (main-nrf52.cpp)](#j-gestión-de-alimentación-de-la-radio-e22p-main-nrf52cpp)
   * [K. Filtro de Escritura Condicional de DeviceState (NodeDB.cpp)](#k-filtro-de-escritura-condicional-de-devicestate-nodedbcpp)
   * [L. Reinicio Universal de nRF52 y Override de SoftDevice (Power.cpp)](#l-reinicio-universal-de-nrf52-y-override-de-softdevice-powercpp)
   * [M. Escudo de Protección NodeInfo y Antitormentas (NodeInfoModule.cpp)](#m-escudo-de-protección-nodeinfo-y-antitormentas-nodeinfomodulecpp)
   * [N. Inmunidad Criptográfica para Administradores](#n-inmunidad-criptográfica-para-administradores)
   * [Ñ. Comandos de Administración Remota (NavaCLIModule)](#ñ-comandos-de-administración-remota-navaclimodule)
   * [O. Secuencia de Comandos Remota 2](#o-secuencia-de-comandos-remota-2)
2. [CÓDIGO ESPECÍFICO DE HARDWARE (Por variante)](#2-código-específico-de-hardware)
   * [A. Faketec / Promicro DIY](#a-faketec--promicro-diy)
   * [B. Seeed Xiao BLE (Xiao Kit y Xiao E22P)](#b-seeed-xiao-ble-xiao-kit-y-xiao-e22p)
   * [C. Heltec T114](#c-heltec-t114)
   * [D. Seed Solar Node P1](#d-seed-solar-node-p1)

---

# 1. CÓDIGO COMÚN

## A. Lógica Anticolapso de Batería (main.cpp)
**Archivo**: `src/main.cpp` — Dentro de `setup()`, inmediatamente después de `power->setup()`.

```cpp
#if defined(USERPREFS_LOW_BATTERY_LOWPOWER_ENABLED) && USERPREFS_LOW_BATTERY_LOWPOWER_ENABLED
    {
        const uint16_t lowBattSleepMv = USERPREFS_LOW_BATTERY_SLEEP_THRESHOLD_MV;
        const uint8_t lowBattReadingsNeeded = USERPREFS_LOW_BATTERY_READINGS_COUNT;
        // V2.6: al venir de sueno (wasInSleep), el gate es el CORTE OCV (no el LPCOMP).
        // Bandas: V < corte-100 -> silencio + re-sueno (brownout);
        //         [corte-100, corte) -> [Vivo] + el nodo SIGUE OPERANDO;
        //         V >= corte -> boot normal ([Listo]).
        bool navaFromSleep = NavaCLIModule::peekWasInSleep();
        auto isLowNow = [&]() -> bool {
            power->readPowerStatus(true); // force: lecturas ADC reales (el contador de baja NO cuenta con force)
            int mv = powerStatus->getBatteryVoltageMv();
            return powerStatus->getHasBattery() && !powerStatus->getHasUSB() && mv > 0 && mv < lowBattSleepMv;
        };
        delay(500); // asentamiento post-reset
        uint8_t consecutiveLow = 0;
        int lastLowMv = 0;
        for (uint8_t i = 0; i < lowBattReadingsNeeded; i++) {
            if (!isLowNow()) break;
            lastLowMv = powerStatus->getBatteryVoltageMv();
            consecutiveLow++;
            delay(200);
        }
        if (consecutiveLow >= lowBattReadingsNeeded) {
            NavaCLIModule::navaSetWasInSleep(true);
            if (NavaCLIModule::peekSleepMsgsEnabled() && navaFromSleep &&
                lastLowMv >= (int)lowBattSleepMv - 100) {
                // banda [corte-100, corte): anunciar [Vivo] y OPERAR (~160s); el
                // monitor runtime decidira dormir si la baja persiste
                NavaCLIModule::navaSetVivoPending();
            } else {
                LOG_WARN("Battery below %u mV ... entering System OFF", lowBattSleepMv);
                cpuDeepSleep(portMAX_DELAY); // re-sueno silencioso (radio nunca encendida)
            }
        }
    }
#endif
```

**V2.6 (claves del comportamiento, verificado en banco)**: el despertar con V ≥ corte opera normal
([Listo]); con V en [corte−100, corte) anuncia [Vivo] y **sigue operando** — el monitor runtime
(`Power.cpp`, contador `low_voltage_counter` SOLO con `!force`, 8 lecturas a ~20s ≈ 160s desde
V3 — F18: umbral desde la macro del perfil `USERPREFS_LOW_BATTERY_READINGS_COUNT` para las 6
placas) decide
dormir ([Sueño] → `doDeepSleep`). `readPowerStatus(force=true)` NO incrementa el contador (si lo
hiciera, el pre-check lo pre-cargaría y el nodo dormiría ~20s tras arrancar con batería baja,
saltándose el filtro anti-falsos-positivos).

## B. Configuración de Sueño y Despertar LPCOMP (main-nrf52.cpp)
**Archivo**: `src/platform/nrf52/main-nrf52.cpp` — En `cpuDeepSleep()`, sustituir el bloque `#ifdef BATTERY_LPCOMP_INPUT`.

```cpp
#ifdef BATTERY_LPCOMP_INPUT
#ifdef ADC_CTRL
        // Mantener activo el divisor de tensión físico durante el sueño profundo
        pinMode(ADC_CTRL, OUTPUT);
        digitalWrite(ADC_CTRL, ADC_CTRL_ENABLED);
#endif
        NRF_LPCOMP->ENABLE = LPCOMP_ENABLE_ENABLE_Disabled;
        nrf_lpcomp_input_select(NRF_LPCOMP, BATTERY_LPCOMP_INPUT);
        nrf_lpcomp_config_t c;
        c.reference = getActiveLpcompThreshold();   // umbral dinámico (set_vwake)
        c.detection = NRF_LPCOMP_DETECT_UP;
        c.hyst = NRF_LPCOMP_HYST_ENABLED;            // 50mV
        nrf_lpcomp_configure(NRF_LPCOMP, &c);
        NRF_LPCOMP->EVENTS_READY = 0;
        NRF_LPCOMP->EVENTS_DOWN = 0;
        NRF_LPCOMP->EVENTS_UP = 0;
        NRF_LPCOMP->EVENTS_CROSS = 0;
        NRF_LPCOMP->ENABLE = LPCOMP_ENABLE_ENABLE_Enabled;
        NRF_LPCOMP->TASKS_START = 1;
        while (NRF_LPCOMP->EVENTS_READY == 0)
            ;
        NRF_LPCOMP->EVENTS_READY = 0;
        NRF_LPCOMP->EVENTS_UP = 0;
        delay(10);
#endif
```

Necesita además (al inicio del archivo o con forward declaration tras los includes):
```cpp
uint32_t rawResetReason = 0;
extern uint8_t currentWakeLevel;
// ...tras #include <hal/nrf_lpcomp.h> y <hal/nrf_rtc.h>:
nrf_lpcomp_ref_t getActiveLpcompThreshold();
```

Y al final del archivo — **versión 2026-08-11 CORREGIDA (desplegada)**. ⚠️ Los niveles 1-5 están calibrados al **divisor físico 0.5 (1M/1M) del Promicro/Faketec**. Las variantes con divisor distinto (Seed, Xiao Kit i2c, Xiao E22P, Heltec T114) usan `#ifdef` para devolver `BATTERY_LPCOMP_THRESHOLD` (valor de fábrica). NO copiar el `switch` a ciegas en placas que no sean divisor 0.5:
```cpp
nrf_lpcomp_ref_t getActiveLpcompThreshold() {
    // Voltajes de despertar calculados con divisor fisico 0.5 (1M/1M) y VDD 3.3V.
    // El LPCOMP compara el pin (bateria x divisor) contra la fraccion de VDD.
#ifdef SEEED_SOLAR_NODE
    // Seed Solar Node P1: divisor ~0.303 (ADC_MULTIPLIER 3.3, de fabrica Meshtastic).
    // Niveles 1-5 calibrados al Promicro serian inalcanzables (9_16 ~5.5V).
    return BATTERY_LPCOMP_THRESHOLD; // NRF_LPCOMP_REF_SUPPLY_3_8 (~3.67V)
#elif defined(SEEED_XIAO_NRF52840_KIT)
    // Xiao Kit i2c / Xiao E22P: divisor 1M/510k (~0.3377, esquematico Seeed R16=1M, R17=510k).
    return BATTERY_LPCOMP_THRESHOLD; // NRF_LPCOMP_REF_SUPPLY_3_8 (~3.67V)
#elif defined(HELTEC_T114)
    // Heltec T114: divisor 100/490 (~0.204, ADC_MULTIPLIER 4.916 de fabrica).
    // Ojo: Meshtastic desactiva BATTERY_LPCOMP_INPUT por fuga 2.9mA en System OFF (issue #8801).
    return BATTERY_LPCOMP_THRESHOLD; // NRF_LPCOMP_REF_SUPPLY_2_8 (~4.04V)
#else
    // Promicro fix / Faketec PROPIA (divisor 0.5): switch dinamico set_vwake.
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
```

## C. Filtro de Guardado en Flash de la Base de Nodos (NodeDB.cpp)
**Archivo**: `src/mesh/NodeDB.cpp` — Sustituir `saveNodeDatabaseToDisk()` por la versión filtrada (solo nodo propio, favoritos, ignorados, routers directos y admins criptográficos). Ver el código canónico en `src/mesh/NodeDB.cpp` del repo Rama 2. Además, el callback `meshtastic_NodeDatabase_callback` se filtra en origen (solo serializa propio + favorito + ignorado + key-manually-verified).

## D. Lógica de Auto-Favoritos con Estrella (NodeDB.cpp)
**Archivo**: `src/mesh/NodeDB.cpp` — Añadir `checkAndRegisterRAMAutoFavorite()` y llamarla en `updateUser()` y `updateFrom()`.

```cpp
void NodeDB::checkAndRegisterRAMAutoFavorite(meshtastic_NodeInfoLite *info)
{
    if (info && info->has_user) {
        if (info->has_hops_away && info->hops_away == 0) {
            if (IS_ONE_OF(info->user.role,
                          meshtastic_Config_DeviceConfig_Role_ROUTER,
                          meshtastic_Config_DeviceConfig_Role_ROUTER_LATE,
                          meshtastic_Config_DeviceConfig_Role_CLIENT_BASE)) {
                if (!info->is_favorite) {
                    LOG_INFO("Auto-Favorite: Marking direct router 0x%08x as favorite", info->num);
                    info->is_favorite = true;
                    sortMeshDB();
                    saveNodeDatabaseToDisk();
                }
                auto &adr = router->activeDirectRouters;
                if (std::find(adr.begin(), adr.end(), info->num) == adr.end()) {
                    adr.push_back(info->num);
                }
            }
        }
    }
}
```

## E. Bypass de Límite de Saltos (Router.cpp)
**Archivos**: `src/mesh/Router.h` (añadir `activeDirectRouters` y `getInterface()`) y `src/mesh/Router.cpp` (`shouldDecrementHopLimit`).

```cpp
// Router.h (sección public):
std::vector<NodeNum> activeDirectRouters;
RadioInterface* getInterface() { return iface.get(); }
```

```cpp
bool Router::shouldDecrementHopLimit(const meshtastic_MeshPacket *p)
{
    bool localIsRouter =
        IS_ONE_OF(config.device.role, meshtastic_Config_DeviceConfig_Role_ROUTER,
                  meshtastic_Config_DeviceConfig_Role_ROUTER_LATE, meshtastic_Config_DeviceConfig_Role_CLIENT_BASE);
    if (!localIsRouter) {
        return true;
    }
    for (size_t i = 0; i < nodeDB->getNumMeshNodes(); i++) {
        meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(i);
        if (!node)
            continue;
        bool isFav = node->is_favorite;
        if (!isFav) {
            isFav = (std::find(activeDirectRouters.begin(), activeDirectRouters.end(), node->num) != activeDirectRouters.end());
        }
        if (!isFav)
            continue;
        if (!node->has_user)
            continue;
        if (!IS_ONE_OF(node->user.role, meshtastic_Config_DeviceConfig_Role_ROUTER,
                       meshtastic_Config_DeviceConfig_Role_ROUTER_LATE, meshtastic_Config_DeviceConfig_Role_CLIENT_BASE)) {
            continue;
        }
        if (nodeDB->getLastByteOfNodeNum(node->num) == p->relay_node) {
            LOG_DEBUG("Identificado relay favorito de infraestructura: 0x%x. No se resta Hop Limit.", node->num);
            return false;
        }
    }
    return true;
}
```

## F. Anulación del Guardado de Historial (TransmitHistory.cpp)
**Archivo**: `src/mesh/TransmitHistory.cpp` — Al inicio de `saveToDisk()`.

```cpp
bool TransmitHistory::saveToDisk()
{
    return true; // Bypass saving duplicate packet history to flash to protect flash memory
    // ... (el resto del código original queda inactivo)
}
```

> Recomendado (más modular): envolver bajo `#if defined(USERPREFS_NODEDB_RAM_ONLY) && USERPREFS_NODEDB_RAM_ONLY`.

## G. Creación Remota de Favoritos (NodeDB + AdminModule)
1. **`NodeDB.h`**: mover `getOrCreateMeshNode(NodeNum n)` de `private:` a `public:`.
2. **`AdminModule.cpp`** (`set_favorite_node_tag`): comprobar `countOrphanFavorites() < 10` y usar `getOrCreateMeshNode()`.
3. **`NodeDB.cpp`** `set_favorite()`: `is_favorite ? getOrCreateMeshNode(nodeId) : getMeshNode(nodeId)`.
4. **`NodeDB.h/.cpp`**: declarar y definir `int countOrphanFavorites()` (cuenta favoritos con `last_heard == 0`).

## H. Blindaje contra Factory Reset (NodeDB.cpp)
**Archivo**: `src/mesh/NodeDB.cpp` — En `installRoleDefaults()`, respetar `USERPREFS_CONFIG_DEVICE_REBROADCAST_MODE` en lugar de forzar `CORE_PORTNUMS_ONLY`.

## I. Claves de Administrador Dinámicas (NodeDB.cpp)
**Archivo**: `src/mesh/NodeDB.cpp` — Helper `loadDefaultAdminKeys(meshtastic_Config_SecurityConfig &security)` que lee `USERPREFS_USE_ADMIN_KEY_0/1/2` (inyectadas desde `userPrefs.jsonc` en compilación) y se llama en `clear()` y `loadFromDisk()` cuando la suma de la key 0 es 0. Ver el código canónico del repo.

## I.2 Claves admin persistidas — F20 (NavaCLIModule + AdminModule, V3)
**Archivos**: `src/modules/NavaCLIModule.h/.cpp` + `src/modules/AdminModule.cpp` — las claves admin
PÚBLICAS del usuario sobreviven a los resets de fábrica guardadas en `/resilience.bin` (campos
`keySlot1/keySlot2/keySlot0Own` en `ResiliencePrefs`, marcador "NAV3" `0x4E415633`):
- **Sincronización**: gancho en `AdminModule::handleSetConfig` caso security → `navaCLIModule->
  syncAdminKeysFromConfig()` (merge: slot entrante no vacío se persiste; vacío nunca borra;
  slot 0 = proyecto limpia la override). Nunca persistir las claves del proyecto como de
  usuario (dedupe con `#ifdef` por `USERPREFS_USE_ADMIN_KEY_0/1/2`).
- **Restauración**: primer tick de `runOnce` (DESPUÉS de `NodeDB::init`): `keySlot0Own` → slot 0
  (regla "slot 0 = estado previo del usuario"); `keySlot1/2` → slots vacíos; recomputa
  `admin_key_count`; `saveToDisk(SEGMENT_CONFIG)` SOLO si cambió.
- **Migración/adopción**: fichero legacy (84 B) → campos de claves a cero + adopción única de
  las claves de usuario ya en la config.
- **Purga**: `/nava keys_clear` (cero SOLO los 3 campos, ACK diferido, sin reboot) o `wipe`.
  `full_reset` usa `navaFullResetKeepKeys()` (conserva SOLO las claves, resto a defaults de
  perfil — NO borrar el fichero entero; L33).

## J. Gestión de Alimentación de la Radio E22P (main-nrf52.cpp)
**Archivo**: `src/platform/nrf52/main-nrf52.cpp` — SOLO en variantes con pin de alimentación (Promicro fix y Xiao E22P). El bloque va envuelto en `#ifdef RADIO_POWER_ENABLE_PIN`:
- En `nrf52Setup()`: `RADIO_POWER_ENABLE_PIN = HIGH`.
- En `cpuDeepSleep()`: `RADIO_POWER_ENABLE_PIN = LOW` antes de `sd_power_system_off()`.
- En `timedSystemSleepSeconds()` (storm): `RADIO_POWER_ENABLE_PIN = LOW` antes del RTC2.

> **⚠️ CRÍTICO**: estos bloques `#ifdef` deben conservarse al portar `main-nrf52.cpp`. Si se copia el fichero desde una variante SX1262 (que no tiene el pin), los E22P perderían el apagado físico de la radio (~40mA en deep sleep y storm).
>
> **En las variantes SX1262 (Faketec, Xiao Kit, Seed P1, T114) NO existe este pin**: la radio se apaga por SPI/driver. No aplicar esta sección. El storm en E22P apaga el pin; en SX1262 apaga por SPI.

## K. Filtro de Escritura Condicional de DeviceState (NodeDB.cpp)
**Archivo**: `src/mesh/NodeDB.cpp` — Sustituir `saveDeviceStateToDisk()` por la versión con caché `memcmp` de identidad (owner, my_node_num, device_id); si no cambian, retorna `true` sin escribir.

## L. Reinicio Universal de nRF52 y Override de SoftDevice (Power.cpp)
**Archivo**: `src/Power.cpp` — En `Power::reboot()`.

```cpp
#elif defined(ARCH_NRF52)
#ifdef FIX_NATIVE_CORE_RESET
    sd_softdevice_disable(); // Desactivar SoftDevice antes de resetear hardware
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

## M. Escudo de Protección NodeInfo y Antitormentas (NodeInfoModule.cpp)
**Archivo**: `src/modules/NodeInfoModule.cpp` — En el constructor:
```cpp
currentGeneration = radioGeneration; // Navarrico NodeInfo storm protection
```

## N. Inmunidad Criptográfica para Administradores
**Archivos**: `NodeDB.h` (máscara `NODEINFO_BITFIELD_IS_CRYPTOGRAPHICALLY_VERIFIED_ADMIN_MASK (0x08)`), `NodeDB.cpp` (`isAdminNode()` lee el bitfield), `AdminModule.cpp` (asigna el bitfield tras validar PKI), `NodeInfoModule.cpp` (inmunidad de supresión/throttle para admins). Ver el código canónico del repo Rama 2.

## Ñ. Rotación de Clave Admin Aceptada en `updateUser` (NodeDB.cpp) — fix 2026-08-10
**Archivo**: `src/mesh/NodeDB.cpp` — función `updateUser(const meshtastic_User &p, bool updateInfo)`. En el bloque de mismatch de clave pública, si la **nueva** clave coincide con una `admin_key` configurada se acepta el cambio y se re-marca favorito (en vez de `return false`). Este fix está aplicado en las 6 variantes de Rama 2.

**Contexto del bug**: un mando de rescate se filtró en la DB del repetidor con una clave no autorizada (previa a cargar la clave de admin correcta). El router descifra los DM PKI con la clave de la DB (Router.cpp: `decryptCurve25519(p->from, nodeDB->getMeshNode(p->from)->user.public_key, ...)`), NO con la del paquete. Por tanto, mientras la DB conserve la clave errónea, el `/nava` por DM jamás descifra.

**Flujo correcto**: el mando reenvía su NodeInfo (broadcast, no va por PKI) con la clave nueva → `updateUser` la acepta (por coincidir con admin_key) y actualiza la DB → el siguiente DM PKI `/nava` se descifra y valida.

**Código** (en el bloque `if (info->user.public_key.size == 32)` de `updateUser`):
```cpp
if (p.public_key.size != 32 || (memcmp(p.public_key.bytes, info->user.public_key.bytes, 32) != 0)) {
    bool newKeyIsAdmin =
        (config.security.admin_key[0].size == 32 && memcmp(p.public_key.bytes, config.security.admin_key[0].bytes, 32) == 0) ||
        (config.security.admin_key[1].size == 32 && memcmp(p.public_key.bytes, config.security.admin_key[1].bytes, 32) == 0) ||
        (config.security.admin_key[2].size == 32 && memcmp(p.public_key.bytes, config.security.admin_key[2].bytes, 32) == 0);
    if (newKeyIsAdmin) {
        LOG_WARN("Public Key mismatch, but NEW key matches an admin key. Accepting and re-favoriting node.");
        info->is_favorite = true; // permitir continuar y sobreescribir info->user (incluida la nueva clave)
    } else {
        LOG_WARN("Public Key mismatch, dropping NodeInfo");
        return false;
    }
}
```

**Nota**: no hay conflicto de seguridad — solo se acepta el cambio si la nueva clave coincide con un admin_key configurado en `userPrefs.jsonc`. Ver `transfer_context.md` sección 5 para el estado de claves unificadas (K0/K1 del Promicro en las 6 variantes).

## Ñ. Comandos de Administración Remota (NavaCLIModule)
Módulo headless `/nava`. El código fuente canónico v4.2.1 (`.h` + `.cpp`) está en `Rama 2 Infraestructura\Infraestructura Propia\Promicro NRF52+E22P NavTastic 2.7.26 R2IP\src\modules\NavaCLIModule.h/.cpp` (idéntico en la Faketec salvo `set_txpower` 0-22). Características:
- Canal Navadmin (Canal 1) con **whitelist de solo lectura** y silencio total para no-admins.
- DM PKI obligatorio para comandos destructivos.
- Rate-limit `std::set<NodeNum> unauthorizedReplied`.
- `help <comando>`, respuestas en español, guards `substr()`, normalización antes del filtro.
- `factory_reset` diferido, `ign add` seguro, `ble` real, `bat` honesto.
- Storm (`storm test1/test2` para pruebas, `storm [1-720]` horas).

> Para auditar sin el repo completo: ver el código fuente en los `.cpp`/`.h` de la variante o consultar el documento 3 (`Manual_NavaTastic.md`) para el manual de comandos. El código histórico (v2/V3) está en `OLD_CONTEXT/`.

## O. Secuencia de Comandos Remota 2
### O.1 Captura de Deriva de Frecuencia (afc)
- `RadioLibInterface.h`: `extern float lastRxFrequencyError;`
- `RadioLibInterface.cpp`: `float lastRxFrequencyError = 0.0f;`
- `SX126xInterface.cpp` y `RF95Interface.cpp` en `addReceiveMetadata()`:
```cpp
mp->rx_snr = lora.getSNR();
mp->rx_rssi = lround(lora.getRSSI());
lastRxFrequencyError = lora.getFrequencyError(); // o lora-> en RF95
```

### O.2 Getter de Interfaz de Radio
- `Router.h`: `RadioInterface* getInterface() { return iface.get(); }` (para `/nava noise`).

### O.3 Modificaciones de Energía Orientadas a Objetos (power.h / Power.cpp)
- `HasBatteryLevel` gana `virtual void updateOcvCurve(uint16_t cutoff) {}` y `virtual void setChemistryProfile(uint8_t chem) {}`.
- `AnalogBatteryLevel` implementa ambos (muta `OCV` en RAM, `chargingVolt`/`noBatVolt` dejados no-const).
- `Power` gana wrappers `updateOcvCurve()` / `setChemistryProfile()`.
- Definir `uint8_t currentWakeLevel = 3;` en `Power.cpp` y `extern uint8_t currentWakeLevel;` en `power.h`.
- **CRÍTICO**: `Power::OCV[11]` debe declararse **con inicializador**: `uint16_t OCV[11] = {OCV_ARRAY};`. Si se deja sin inicializar (como `uint16_t OCV[11];`), `Power::readPowerStatus()` compara contra basura y el deep sleep por batería baja **no funciona** (el nodo no se duerme). Además, los wrappers `updateOcvCurve`/`setChemistryProfile` de `Power` deben sincronizar TAMBIÉN el array de `Power` (no solo el de `AnalogBatteryLevel`).
- Químicas: `chem 0=LIPO, 1=NIMH, 2=SODIUM, 3=LIFEPO4`. LiFePO4 usa curva `{3650..2800}` y corte 2800 mV.
- **`executeTxDropTest()` fue ELIMINADO en v4.2.1** (no media nada real).

### O.4 Registro de Reinicios y Storm (main-nrf52.cpp)
- `rawResetReason` capturado en `nrf52Setup()` desde `NRF_POWER->RESETREAS`.
- Storm: `RTC2_IRQHandler` (`extern "C"`) + `timedSystemSleepSeconds()`:
```cpp
void timedSystemSleepSeconds(uint32_t seconds)
{
    if (seconds == 0) return;
    // 1. Dormir la radio DE VERDAD: notifyDeepSleep dispara RadioInterface::sleep()
    //    (setStandby + lora.sleep en SX1262). DEBE hacerse ANTES de SPI.end().
    notifyDeepSleep.notifyObservers(NULL);
#ifdef HAS_WIRE
    Wire.end();
#endif
    SPI.end();
#ifdef PIN_SERIAL1_RX
    if (Serial1) Serial1.end();
#endif
    setBluetoothEnable(false);   // switch nativo (config.bluetooth.enabled)
    if (screen) screen->doDeepSleep();
#ifdef RADIO_POWER_ENABLE_PIN
    pinMode(RADIO_POWER_ENABLE_PIN, OUTPUT);
    digitalWrite(RADIO_POWER_ENABLE_PIN, LOW);   // E22P
#endif
    nrf_rtc_prescaler_set(NRF_RTC2, 0);
    nrf_rtc_task_trigger(NRF_RTC2, NRF_RTC_TASK_CLEAR);
    nrf_rtc_event_enable(NRF_RTC2, RTC_CHANNEL_INT_MASK(0));
    nrf_rtc_int_enable(NRF_RTC2, RTC_CHANNEL_INT_MASK(0));
    NVIC_ClearPendingIRQ(RTC2_IRQn);
    NVIC_EnableIRQ(RTC2_IRQn);
    uint32_t remaining = seconds;
    while (remaining > 0) {
        uint32_t block = (remaining > 500u) ? 500u : remaining;
        uint32_t target = (nrf_rtc_counter_get(NRF_RTC2) + block * 32768u) & 0xFFFFFFu;
        nrf_rtc_cc_set(NRF_RTC2, 0, target);
        nrf_rtc_task_trigger(NRF_RTC2, NRF_RTC_TASK_CLEAR);
        nrf_rtc_task_trigger(NRF_RTC2, NRF_RTC_TASK_START);
        rtc2StormWake = false;
        while (!rtc2StormWake) {
            sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
            sd_app_evt_wait();   // API correcta con SoftDevice (no __WFE__)
        }
        remaining -= block;
    }
    nrf_rtc_task_trigger(NRF_RTC2, NRF_RTC_TASK_STOP);
    NVIC_DisableIRQ(RTC2_IRQn);
    NVIC_SystemReset();
    while (1) { delay(1000); }
}
```

> **Storm + ACK**: `NavaCLIModule::runOnce()` espera 15s (`stormTime = millis()+15000`) antes de ejecutar el storm, y mientras `stormPending` esté activo retorna 1000 (revisa cada segundo). Así el ACK "MODO TORMENTA ACTIVADO..." se transmite antes de dormir. Requiere `#include "sleep.h"` en `main-nrf52.cpp`.

### O.5 Reclasificación de Visibilidad en Telemetría (EnvironmentTelemetry.h)
Mover `sendTelemetry(...)` de `protected:` a `public:` (necesario para `/nava sendtel`).

### O.6 Fixes V2.3/F15/F16a (15/08 — cierre verificado en banco)
1. **Avisos de sueño SIEMPRE con NODENUM_BROADCAST** (`NavaCLIModule.cpp`): los 6 `enqueueResponse(0, 1, ...)`
   de [Sueño]/[Vivo]/[Listo] (y diags) usaban `to=0`, que NO es broadcast en 2.7
   (`isBroadcast()` = NODENUM_BROADCAST/NODENUM_BROADCAST_NO_LORA) → el paquete se emitía y nadie lo
   entregaba. Regla: **`enqueueResponse(NODENUM_BROADCAST, 1, ...)`** para todo aviso por canal 1.
2. **`/resilience.bin` a prueba de corrupción** (`NavaCLIModule.cpp`): `FSCom.remove("/resilience.bin")`
   antes de CADA escritura (el `FILE_O_WRITE` de Adafruit InternalFS no trunca y el fichero nunca
   encoge); gates de migración `fileSize != sizeof(prefs) || version != 0x4E415653` (marcador al final
   del struct, 84 B) + saneado de campos fuera de rango; migración de ficheros legacy 80 B
   (`sleepMsgs=1`, `role=0xFF`, `wasInSleep=0`). Fichero corrupto → defaults, nunca rol fantasma.
3. **Acreditación admin persistente** (`AdminModule.cpp`, bloque "Automatically favorite the node that
   is using the admin key"): tras `bitfield |= 0x08` y `is_favorite = true`, añadir
   `if (accChanged) nodeDB->saveToDisk(SEGMENT_NODEDATABASE);` — el admin sigue autorizado tras reboot
   sin re-anunciar nodeinfo. (El save es el filtrado: favoritos/admins/direct routers/ignored.)
4. **`sleepmsg` parseo**: `substr(9)` ("sleepmsg" = 8 letras + espacio) — con `substr(8)` el arg quedaba
   " on" y el gate NUNCA se activaba por comando.

---

# 2. CÓDIGO ESPECÍFICO DE HARDWARE

> Los valores de potencia y OCV se definen en `variants/<...>/variant.h` de cada variante. **No tocar los divisores ADC de fábrica** (Promicro `2.0`, el resto de serie). Ejemplos representativos:

## A. Faketec / Promicro DIY (env `nrf52_promicro_diy_tcxo`)
```cpp
#define BATTERY_LPCOMP_INPUT NRF_LPCOMP_INPUT_7
#define BATTERY_LPCOMP_THRESHOLD NRF_LPCOMP_REF_SUPPLY_9_16
// Promicro fix (E22P, 12 dBm, corte 3.5V):
#define SX126X_MAX_POWER 12
#define HARDWARE_TX_POWER_LIMIT 12
#define OCV_ARRAY 4190, 4050, 3990, 3890, 3800, 3720, 3630, 3530, 3500, 3500, 3500
// Faketec PROPIA (HT-RA62, 22 dBm, corte 3.4V):
#define SX126X_MAX_POWER 22
#define HARDWARE_TX_POWER_LIMIT 22
#define OCV_ARRAY 4190, 4050, 3990, 3890, 3800, 3720, 3630, 3530, 3400, 3400, 3400
#define FIX_NATIVE_CORE_RESET
```

## B. Seeed Xiao BLE — Xiao Kit y Xiao E22P (env `seeed_xiao_nrf52840_kit_i2c`)
```cpp
#define BATTERY_LPCOMP_INPUT NRF_LPCOMP_INPUT_7
#define BATTERY_LPCOMP_THRESHOLD NRF_LPCOMP_REF_SUPPLY_3_8  // divisor 1M/510k
// Xiao Kit (SX1262, 22 dBm, corte 3.4V):
#define SX126X_MAX_POWER 22
#define HARDWARE_TX_POWER_LIMIT 22
#define OCV_ARRAY 4190, 4050, 3990, 3890, 3800, 3720, 3630, 3530, 3400, 3400, 3400
// Xiao E22P (E22P, 12 dBm, corte 3.5V, RADIO_POWER_ENABLE_PIN=D5):
#define SX126X_MAX_POWER 12
#define HARDWARE_TX_POWER_LIMIT 12
#define OCV_ARRAY 4190, 4050, 3990, 3890, 3800, 3720, 3630, 3530, 3500, 3500, 3500
```

## C. Heltec T114 (env `heltec-mesh-node-t114`)
```cpp
#define BATTERY_LPCOMP_INPUT NRF_LPCOMP_INPUT_2
#define BATTERY_LPCOMP_THRESHOLD NRF_LPCOMP_REF_SUPPLY_2_8  // ~4.05V
#define SX126X_MAX_POWER 22
#define HARDWARE_TX_POWER_LIMIT 22
#define OCV_ARRAY 4190, 4050, 3990, 3890, 3800, 3720, 3630, 3530, 3400, 3400, 3400
```

## D. Seed Solar Node P1 (env `seeed_solar_node`)
```cpp
#define BATTERY_LPCOMP_INPUT NRF_LPCOMP_INPUT_7
#define BATTERY_LPCOMP_THRESHOLD NRF_LPCOMP_REF_SUPPLY_3_8  // ~3.8V
#define ADC_CTRL BAT_READ        // divisor activable
#define SX126X_MAX_POWER 22
#define HARDWARE_TX_POWER_LIMIT 22
#define OCV_ARRAY 4190, 4050, 3990, 3890, 3800, 3720, 3630, 3530, 3400, 3400, 3400
```

## Regla del canal de rescate (Channels.cpp)
- **E22P → `tx_power = 8`** (límite conservador; el usuario puede subir a 12 con `/nava set_txpower [0-12]`).
- **SX1262 → `tx_power = 22`** (`/nava set_txpower [0-22]`).

---

## ⚠️ Errores Conocidos (importantes al portar/auditar)

### 1. `getActiveLpcompThreshold()` NO es universal (bug corregido 11/08/2026)
La Secuencia 2 calibrada al **divisor 0.5 del Promicro** dejó Seed, Xiao Kit i2c, Xiao E22P y Heltec T114 **sin despertar por solar** (el default `9_16` pedía 5.5V/5.5V/9.1V). Fix: `#ifdef` por variante devolviendo `BATTERY_LPCOMP_THRESHOLD` de fábrica (ver sección B arriba). **Regla**: al portar a una placa DIY, verificar SIEMPRE su divisor real (multímetro en pin ADC o esquemático) antes de asumir los niveles `set_vwake`.

### 2. Heltec T114 — LPCOMP desactivado en el stock de Meshtastic
`BATTERY_LPCOMP_INPUT` está comentado en el original por **fuga de 2.9 mA en System OFF** (issue #8801). El fork lo activa a propósito para despertar por solar; el coste se paga solo DORMIDO (despierto no consume). Decidir por diseño.

### 3. Divisores reales por variante (fuentes primarias)
| Variante | Divisor | Fuente |
|---|---|---|
| Promicro fix / Faketec | 1M+1M = 0.5 | placa del operador |
| Xiao Kit i2c / Xiao E22P | 1M/510k = 0.3377 | esquemático Seeed (R16=1M, R17=510k) |
| Seed Solar P1 | ≈0.303 | ADC_MULTIPLIER 3.3 de fábrica Meshtastic |
| Heltec T114 | 100/490 = 0.204 | variant.h de fábrica Meshtastic |
