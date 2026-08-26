#include "NavaCLIModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "Router.h"
#include "main.h"
#include "memGet.h"
#include "FSCommon.h"
#include "SPILock.h"
#include <ErriezCRC32.h>
#include "power.h"
#include "sleep.h"
#include "modules/TraceRouteModule.h"
#include "modules/PositionModule.h"
#include "modules/NodeInfoModule.h"
#include "mesh/RadioLibInterface.h"
#include "buzz/buzz.h"
#include "Channels.h"
#include "RTC.h"
#include "../mesh/generated/meshtastic/apponly.pb.h"
#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR
#include "modules/Telemetry/EnvironmentTelemetry.h"
#endif

// APIs del SoftDevice de Nordic para temperatura interna de la CPU
#ifdef NRF52840_XXAA
#include "nrf_soc.h"
#endif

// Protobuf de telemetría para decodificación en caliente
#include "../mesh/generated/meshtastic/telemetry.pb.h"

// Variables y clases externas declaradas en otros ficheros
extern float lastRxFrequencyError;
extern uint32_t rawResetReason;
extern void timedSystemSleepSeconds(uint32_t seconds);
extern void setBleForceDisabled(bool on);
extern uint16_t navaGetLpcompWakeMv(); // V2: tension teorica de despertar por LPCOMP (mV) segun placa/nivel

NavaCLIModule *navaCLIModule = nullptr;

bool navaAutoFavoriteEnabled = true; // Auto-favoriteo de routers directos 0-hop (default ON)

// V2: flag estatico intermedio: lo pone el pre-check de main.cpp ANTES de que el modulo exista
static bool navaVivoPendingGlobal = false;
static bool navaReservaPendingGlobal = false;

NavaCLIModule::NavaCLIModule()
    : SinglePortModule("nava_cli", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("NavaCLI")
{
    isPromiscuous = true;
    
    // Forzamos que la mensajería privada siempre esté activa para los administradores
    owner.is_unmessagable = false;
    owner.has_is_unmessagable = true;
    
    // Cargar los parámetros de resiliencia persistentes
    loadResiliencePrefs();

    // V2: flags de sueño (los puso el pre-check de main.cpp antes de construir el modulo)
    wokeFromSleep = (prefs.wasInSleep != 0);
    vivoPending = navaVivoPendingGlobal;
    reservaPending = navaReservaPendingGlobal;
}

void NavaCLIModule::loadResiliencePrefs() {
    memset(&prefs, 0, sizeof(prefs));
    bool validExisting = false;

    {
        concurrency::LockGuard g(spiLock);
        if (FSCom.exists("/resilience.bin")) {
            File f = FSCom.open("/resilience.bin", FILE_O_READ);
            if (f) {
                size_t fileSize = f.size();
                if (fileSize == sizeof(ResiliencePrefs)) {
                    size_t bytesRead = f.read((uint8_t*)&prefs, sizeof(prefs));
                    if (bytesRead == sizeof(ResiliencePrefs) && prefs.magic == 0x52455349 && prefs.version == NAVS_RESILIENCE_VERSION) {
                        uint32_t calcCrc = crc32Buffer(&prefs, offsetof(ResiliencePrefs, crc32));
                        if (calcCrc == prefs.crc32) {
                            bool fieldsSane = (prefs.chemistry <= 3 &&
                                prefs.vbat_cutoff >= 2400 && prefs.vbat_cutoff <= 3600 &&
                                prefs.vwake_level >= 1 && prefs.vwake_level <= 5 &&
                                prefs.tx_disabled <= 1 && prefs.ble_disabled <= 1 && prefs.auto_fav <= 1 &&
                                (prefs.role <= meshtastic_Config_DeviceConfig_Role_ROUTER || prefs.role == 0xFF) &&
                                prefs.autoFavCount <= 32 && prefs.sleepMsgs <= 1 && prefs.wasInSleep <= 1 &&
                                prefs.cliChannelSlot >= 1 && prefs.cliChannelSlot <= 7 && prefs.navadminMuted <= 1 &&
                                prefs.ignoredCount <= 8);

                            if (fieldsSane) {
                                validExisting = true;
                            }
                        }
                    }
                }
                f.close();
            }
        }
    }

    if (!validExisting) {
        {
            concurrency::LockGuard g(spiLock);
            if (FSCom.exists("/resilience.bin")) {
                LOG_WARN("NavaCLI: /resilience.bin no conforme o corrupto detectado. Purgando a limpio (Clean Slate)...");
                FSCom.remove("/resilience.bin");
            }
            if (FSCom.exists("/resilience.tmp")) {
                FSCom.remove("/resilience.tmp");
            }
        }
        installSurvivalBaseline();
        return;
    }

    // SANITIZACIÓN UNIVERSAL DE CLAVES ADMIN (purga de 0x01+31 ceros y claves corruptas)
    if (!navaKeyIsValid(prefs.keySlot0Own)) memset(prefs.keySlot0Own, 0, sizeof(prefs.keySlot0Own));
    if (!navaKeyIsValid(prefs.keySlot1)) memset(prefs.keySlot1, 0, sizeof(prefs.keySlot1));
    if (!navaKeyIsValid(prefs.keySlot2)) memset(prefs.keySlot2, 0, sizeof(prefs.keySlot2));

    // SANITIZACIÓN UNIVERSAL DE ESTADOS DE PÁNICO / ACCIONES DIFERIDAS AL BOOT
    prefs.panic_active = 0;
    prefs.panic_target_time_ms = 0;
    prefs.panic_last_pulse_ms = 0;
    if (prefs.panic_trial_active != 1) {
        prefs.panic_trial_active = 0;
        prefs.panic_trial_deadline_ms = 0;
    }

    saveResiliencePrefs();

    navaAutoFavoriteEnabled = (prefs.auto_fav != 0);
    // Aplicar parámetros cargados a RAM
    power->setChemistryProfile(prefs.chemistry);
    power->updateOcvCurve(prefs.vbat_cutoff);
    config.lora.tx_enabled = (prefs.tx_disabled == 0);
    currentWakeLevel = prefs.vwake_level;
    if (prefs.ble_disabled == 1) {
        config.bluetooth.enabled = false;
        setBleForceDisabled(true);
    } else {
        config.bluetooth.enabled = true;
        setBleForceDisabled(false);
    }
    // V2.1 Rama 1 y Rama 2: rol semi-permanente
    if (prefs.role <= meshtastic_Config_DeviceConfig_Role_ROUTER) {
        config.device.role = (meshtastic_Config_DeviceConfig_Role)prefs.role;
        owner.role = config.device.role;
        nodeDB->installRoleDefaults(config.device.role);
        owner.is_unmessagable = false;
        owner.has_is_unmessagable = true;
        nodeDB->updateUser(nodeDB->getNodeNum(), owner);
    }
    if (prefs.fixed_pin > 0) {
        config.bluetooth.fixed_pin = prefs.fixed_pin;
    }
    if (prefs.ok_to_mqtt == 1) {
        config.lora.config_ok_to_mqtt = true;
    } else if (prefs.ok_to_mqtt == 2) {
        config.lora.config_ok_to_mqtt = false;
    }
    if (prefs.fixed_pos_enabled == 1) {
        config.position.fixed_position = true;
        meshtastic_Position pos = meshtastic_Position_init_zero;
        pos.latitude_i = prefs.fixed_pos_lat;
        pos.longitude_i = prefs.fixed_pos_lon;
        pos.altitude = prefs.fixed_pos_alt;
        pos.time = getValidTime(RTCQualityFromNet);
        nodeDB->setLocalPosition(pos);
    }
    if (prefs.beacon_interval_secs > 0) {
        config.device.node_info_broadcast_secs = prefs.beacon_interval_secs;
        config.position.position_broadcast_secs = prefs.beacon_interval_secs;
    }
    if (prefs.pos_tx_secs > 0) {
        config.position.position_broadcast_secs = prefs.pos_tx_secs;
    }
    if (prefs.nodeinfo_tx_secs > 0) {
        config.device.node_info_broadcast_secs = prefs.nodeinfo_tx_secs;
    }
    if (prefs.telem_tx_secs > 0) {
        moduleConfig.telemetry.device_update_interval = prefs.telem_tx_secs;
        moduleConfig.telemetry.environment_update_interval = prefs.telem_tx_secs;
        moduleConfig.telemetry.power_update_interval = prefs.telem_tx_secs;
        moduleConfig.telemetry.air_quality_interval = prefs.telem_tx_secs;
        moduleConfig.telemetry.health_update_interval = prefs.telem_tx_secs;
    }
    // V5: Restaurar nombre personalizado persistido si existe y es válido
    if (prefs.custom_long_name[0] != '\0') {
        bool isValidCustomName = true;
        size_t len = strnlen(prefs.custom_long_name, sizeof(prefs.custom_long_name));
        if (len == 0 || len >= sizeof(prefs.custom_long_name)) {
            isValidCustomName = false;
        } else {
            for (size_t i = 0; i < len; i++) {
                unsigned char c = (unsigned char)prefs.custom_long_name[i];
                if (c < 0x20 || c == 0x7F) {
                    isValidCustomName = false;
                    break;
                }
            }
        }
        if (!isValidCustomName) {
            memset(prefs.custom_long_name, 0, sizeof(prefs.custom_long_name));
            memset(prefs.custom_short_name, 0, sizeof(prefs.custom_short_name));
            saveResiliencePrefs();
        } else {
            strncpy(owner.long_name, prefs.custom_long_name, sizeof(owner.long_name) - 1);
            owner.long_name[sizeof(owner.long_name) - 1] = '\0';
            sanitizeUtf8(owner.long_name, sizeof(owner.long_name));
            if (prefs.custom_short_name[0] != '\0') {
                strncpy(owner.short_name, prefs.custom_short_name, sizeof(owner.short_name) - 1);
                owner.short_name[sizeof(owner.short_name) - 1] = '\0';
                sanitizeUtf8(owner.short_name, sizeof(owner.short_name));
            }
            nodeDB->updateUser(nodeDB->getNodeNum(), owner);
        }
    }
}

void NavaCLIModule::installSurvivalBaseline()
{
    LOG_INFO("NavaCLI: Instalando Linea de Base de Supervivencia NavaTastic...");
    memset(&prefs, 0, sizeof(prefs));
    prefs.magic = 0x52455349;
    prefs.version = NAVS_RESILIENCE_VERSION;
#if defined(USERPREFS_BATTERY_CHEMISTRY_SODIUM)
    prefs.chemistry = 2; // SODIUM
    prefs.vbat_cutoff = 2600;
    prefs.vwake_level = 1;
#else
    prefs.chemistry = 0; // LIPO
    prefs.vbat_cutoff = 3500;
    prefs.vwake_level = 3;
#endif
    prefs.tx_disabled = 0;
    prefs.ble_disabled = 0;
    prefs.auto_fav = 1;
    prefs.role = 0xFF; // sin rol fijado (default: el del perfil del env)
    prefs.autoFavCount = 0;
    memset(prefs.autoFavIds, 0, sizeof(prefs.autoFavIds));
    memset(prefs.extraAutoFavIds, 0, sizeof(prefs.extraAutoFavIds));
    prefs.sleepMsgs = 1;
    prefs.wasInSleep = 0;
    prefs.reserved = 0;
    memset(prefs.keySlot1, 0, sizeof(prefs.keySlot1));
    memset(prefs.keySlot2, 0, sizeof(prefs.keySlot2));
    memset(prefs.keySlot0Own, 0, sizeof(prefs.keySlot0Own));

    prefs.cliChannelSlot = 1;
    prefs.navadminMuted = 0;
    memset(prefs.customChannels, 0, sizeof(prefs.customChannels));
    prefs.ok_to_mqtt = 0;
    prefs.fixed_pin = 0;
    prefs.fixed_pos_lat = 0;
    prefs.fixed_pos_lon = 0;
    prefs.fixed_pos_alt = 0;
    prefs.fixed_pos_enabled = 0;
    prefs.beacon_interval_secs = 0;
    prefs.pos_tx_secs = 259200;
    prefs.nodeinfo_tx_secs = 259200;
    prefs.telem_tx_secs = 43200; // Default V5: 12 horas (43200s)
    prefs.ignoredCount = 0;
    memset(prefs.ignoredNodes, 0, sizeof(prefs.ignoredNodes));
    prefs.lora_use_preset = 0;
    prefs.lora_modem_preset = 0;
    prefs.lora_bandwidth = 0;
    prefs.lora_spread_factor = 0;
    prefs.lora_coding_rate = 0;
    prefs.lora_channel_num = 0;
    prefs.lora_override_frequency = 0.0f;
    prefs.lora_tx_power = 0;
    prefs.lora_configured = 0;
    memset(prefs.ch0_name, 0, sizeof(prefs.ch0_name));
    memset(prefs.ch0_psk, 0, sizeof(prefs.ch0_psk));
    prefs.ch0_psk_len = 0;
    prefs.ch0_configured = 0;
    prefs.panic_active = 0;
    prefs.panic_target_preset = 0;
    prefs.panic_target_sf = 0;
    prefs.panic_target_cr = 0;
    prefs.panic_target_bw = 0;
    prefs.panic_target_slot = 0;
    prefs.panic_target_freq = 0.0f;
    prefs.panic_rollback_mins = 0;
    prefs.panic_target_time_ms = 0;
    prefs.panic_last_pulse_ms = 0;
    prefs.panic_trial_active = 0;
    prefs.panic_trial_deadline_ms = 0;
    memset(prefs.custom_long_name, 0, sizeof(prefs.custom_long_name));
    memset(prefs.custom_short_name, 0, sizeof(prefs.custom_short_name));
    navaAutoFavoriteEnabled = true;
    setBleForceDisabled(false);

    saveResiliencePrefs();
}

void NavaCLIModule::ensureNavadminChannel()
{
    meshtastic_Channel &ch1 = channels.getByIndex(1);
    
    // Si el Slot 1 ya es Navadmin, no hay nada que hacer
    if (ch1.has_settings && ch1.role == meshtastic_Channel_Role_SECONDARY && strcmp(ch1.settings.name, "Navadmin") == 0) {
        return;
    }

    // Si el Slot 1 tiene un canal previo configurado del usuario (que NO es Navadmin)
    if (ch1.has_settings && ch1.role != meshtastic_Channel_Role_DISABLED && ch1.settings.name[0] != '\0') {
        int freeSlot = -1;
        for (int i = 2; i < MAX_NUM_CHANNELS; i++) {
            const meshtastic_Channel &cand = channels.getByIndex(i);
            if (!cand.has_settings || cand.role == meshtastic_Channel_Role_DISABLED) {
                freeSlot = i;
                break;
            }
        }
        if (freeSlot >= 2) {
            LOG_INFO("NavaCLI: Reubicando canal previo de slot 1 hacia slot %d para dar paso a Navadmin", freeSlot);
            meshtastic_Channel movedCh = ch1;
            movedCh.index = freeSlot;
            channels.setChannel(movedCh);
            syncCustomChannelFromConfig(freeSlot);
        } else {
            LOG_WARN("NavaCLI: Todos los slots ocupados (0..7). Sustituyendo slot 1 por Navadmin prioritario.");
        }
    }

    // Aprovisionar Navadmin en Slot 1
    meshtastic_Channel navadminCh = meshtastic_Channel_init_zero;
    navadminCh.index = 1;
    navadminCh.role = meshtastic_Channel_Role_SECONDARY;
    navadminCh.has_settings = true;
    strcpy(navadminCh.settings.name, "Navadmin");
    navadminCh.settings.psk.size = 1;
    navadminCh.settings.psk.bytes[0] = 0x01;
    navadminCh.settings.module_settings.position_precision = 0;
    navadminCh.settings.uplink_enabled = false;
    navadminCh.settings.downlink_enabled = false;
    navadminCh.settings.has_module_settings = true;
    channels.setChannel(navadminCh);

    channels.onConfigChanged();
    nodeDB->saveToDisk(SEGMENT_CHANNELS);
    LOG_INFO("NavaCLI: Canal 1 Navadmin auto-aprovisionado en Flash con exito.");
}

void NavaCLIModule::adoptExistingOperationalConfig()
{
    bool changed = false;

    // 1. Claves de administración del dueño (respetando soberanía: no inyectar MasterNode si el dueño ya tiene clave)
    const meshtastic_Config_SecurityConfig &sec = config.security;
    bool hasOwnerKey = false;
    for (pb_size_t i = 0; i < sec.admin_key_count && i < 3; i++) {
        const uint8_t *k = sec.admin_key[i].bytes;
        size_t sz = sec.admin_key[i].size;
        if (sz == 32 && navaKeyIsValid(k) && !navaKeyIsProjectKey(k)) {
            hasOwnerKey = true;
            if (i == 0 && navaKeyIsEmpty(prefs.keySlot0Own)) {
                memcpy(prefs.keySlot0Own, k, 32);
                changed = true;
                LOG_INFO("NavaCLI: Respaldo pasivo - Clave admin de dueno slot 0 absorbida hacia /resilience.bin");
            } else if (i == 1 && navaKeyIsEmpty(prefs.keySlot1)) {
                memcpy(prefs.keySlot1, k, 32);
                changed = true;
                LOG_INFO("NavaCLI: Respaldo pasivo - Clave admin slot 1 absorbida hacia /resilience.bin");
            } else if (i == 2 && navaKeyIsEmpty(prefs.keySlot2)) {
                memcpy(prefs.keySlot2, k, 32);
                changed = true;
                LOG_INFO("NavaCLI: Respaldo pasivo - Clave admin slot 2 absorbida hacia /resilience.bin");
            }
        }
    }

    // Si el nodo NO tenía ninguna clave admin configurada (nodo virgen o sin administrador configurado):
    // Se asegura de que la clave oficial de MasterNode de fábrica esté inyectada en admin_key[0]
    if (!hasOwnerKey && sec.admin_key_count == 0) {
#ifdef USERPREFS_USE_ADMIN_KEY_0
        static const uint8_t projK[] = USERPREFS_USE_ADMIN_KEY_0;
        if (sizeof(projK) == 32) {
            memcpy(config.security.admin_key[0].bytes, projK, 32);
            config.security.admin_key[0].size = 32;
            config.security.admin_key_count = 1;
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            LOG_INFO("NavaCLI: Nodo virgen sin administrador previo - Clave MasterNode de fabrica inyectada");
        }
#endif
    }

    // 2. Canales secundarios (Slots 2..7): absorber canales preexistentes si no estaban guardados en resilience.bin
    for (uint8_t i = 2; i < MAX_NUM_CHANNELS; i++) {
        uint8_t idx = i - 2;
        const meshtastic_Channel &ch = channels.getByIndex(i);
        ResilientChannel &rc = prefs.customChannels[idx];
        if (rc.is_active == 0 && ch.has_settings && ch.role != meshtastic_Channel_Role_DISABLED && ch.settings.name[0] != '\0') {
            rc.is_active = 1;
            strncpy(rc.name, ch.settings.name, sizeof(rc.name) - 1);
            rc.name[sizeof(rc.name) - 1] = '\0';
            if (ch.settings.psk.size > 0 && ch.settings.psk.size <= 32) {
                memcpy(rc.psk, ch.settings.psk.bytes, ch.settings.psk.size);
                rc.psk_len = ch.settings.psk.size;
            } else {
                rc.psk[0] = 0x01;
                rc.psk_len = 1;
            }
            rc.uplink_enabled = ch.settings.uplink_enabled ? 1 : 0;
            rc.downlink_enabled = ch.settings.downlink_enabled ? 1 : 0;
            changed = true;
            LOG_INFO("NavaCLI: Respaldo pasivo - Canal secundario slot %d ('%s') absorbido hacia /resilience.bin", i, rc.name);
        }
    }

    // 3. Canal 0 Primario
    if (prefs.ch0_configured == 0) {
        const meshtastic_Channel &ch0 = channels.getByIndex(0);
        if (ch0.has_settings && ch0.settings.name[0] != '\0') {
            strncpy(prefs.ch0_name, ch0.settings.name, sizeof(prefs.ch0_name) - 1);
            prefs.ch0_name[sizeof(prefs.ch0_name) - 1] = '\0';
            if (ch0.settings.psk.size > 0 && ch0.settings.psk.size <= 32) {
                memcpy(prefs.ch0_psk, ch0.settings.psk.bytes, ch0.settings.psk.size);
                prefs.ch0_psk_len = ch0.settings.psk.size;
            } else {
                prefs.ch0_psk[0] = 0x01;
                prefs.ch0_psk_len = 1;
            }
            prefs.ch0_configured = 1;
            changed = true;
            LOG_INFO("NavaCLI: Respaldo pasivo - Canal 0 ('%s') absorbido hacia /resilience.bin", prefs.ch0_name);
        }
    }

    // 4. Capa Física LoRa
    if (prefs.lora_configured == 0) {
        const meshtastic_Config_LoRaConfig &lora = config.lora;
        prefs.lora_use_preset = lora.use_preset ? 1 : 0;
        prefs.lora_modem_preset = (uint8_t)lora.modem_preset;
        prefs.lora_bandwidth = lora.bandwidth;
        prefs.lora_spread_factor = lora.spread_factor;
        prefs.lora_coding_rate = lora.coding_rate;
        prefs.lora_channel_num = lora.channel_num;
        prefs.lora_override_frequency = lora.override_frequency;
        prefs.lora_tx_power = lora.tx_power;
        prefs.lora_configured = 1;
        changed = true;
        LOG_INFO("NavaCLI: Respaldo pasivo - Capa Fisica LoRa absorbida hacia /resilience.bin");
    }

    // 5. Nombre del repetidor / nodo
    if (prefs.custom_long_name[0] == '\0' && owner.long_name[0] != '\0') {
        strncpy(prefs.custom_long_name, owner.long_name, sizeof(prefs.custom_long_name) - 1);
        prefs.custom_long_name[sizeof(prefs.custom_long_name) - 1] = '\0';
        if (owner.short_name[0] != '\0') {
            strncpy(prefs.custom_short_name, owner.short_name, sizeof(prefs.custom_short_name) - 1);
            prefs.custom_short_name[sizeof(prefs.custom_short_name) - 1] = '\0';
        }
        changed = true;
        LOG_INFO("NavaCLI: Respaldo pasivo - Nombre ('%s') absorbido hacia /resilience.bin", prefs.custom_long_name);
    }

    // 6. Rol del dispositivo
    if (prefs.role == 0xFF && config.device.role <= meshtastic_Config_DeviceConfig_Role_ROUTER) {
        prefs.role = (uint8_t)config.device.role;
        changed = true;
        LOG_INFO("NavaCLI: Respaldo pasivo - Rol (%d) absorbido hacia /resilience.bin", prefs.role);
    }

    // 7. Guardado si hubo cambios
    if (changed) {
        saveResiliencePrefs();
        LOG_INFO("NavaCLI: Respaldo pasivo completado y guardado en /resilience.bin con exito.");
    }
}

// --- V2: acceso estatico a los flags de sueño (leidos desde main.cpp pre-check) ---
static bool navaResiliencePeek(uint8_t &sleepMsgsOut, uint8_t &wasInSleepOut)
{
    sleepMsgsOut = 1;
    wasInSleepOut = 0;
    if (FSCom.exists("/resilience.bin")) {
        File f = FSCom.open("/resilience.bin", FILE_O_READ);
        if (f) {
            ResiliencePrefs tmp;
            memset(&tmp, 0, sizeof(tmp));
            size_t fileSize = f.size();
            if (fileSize > 0 && fileSize <= sizeof(tmp)) {
                f.read((uint8_t *)&tmp, fileSize);
            }
            f.close();
            if (tmp.magic == 0x52455349) {
                if (fileSize != sizeof(tmp) || tmp.version != NAVS_RESILIENCE_VERSION) {
                    sleepMsgsOut = 1;
                    wasInSleepOut = 0;
                    return true;
                }
                sleepMsgsOut = tmp.sleepMsgs;
                wasInSleepOut = tmp.wasInSleep;
                return true;
            }
        }
    }
    return false;
}

bool NavaCLIModule::peekSleepMsgsEnabled()
{
    uint8_t sm, ws;
    navaResiliencePeek(sm, ws);
    return sm != 0;
}

bool NavaCLIModule::peekWasInSleep()
{
    uint8_t sm, ws;
    navaResiliencePeek(sm, ws);
    return ws != 0;
}

void NavaCLIModule::navaSetWasInSleep(bool on)
{
    ResiliencePrefs tmp;
    memset(&tmp, 0, sizeof(tmp));
    bool exists = false;
    if (FSCom.exists("/resilience.bin")) {
        File f = FSCom.open("/resilience.bin", FILE_O_READ);
        if (f) {
            size_t fileSize = f.size();
            if (fileSize > 0 && fileSize <= sizeof(tmp)) {
                f.read((uint8_t *)&tmp, fileSize);
                if (tmp.magic == 0x52455349) {
                    exists = true;
                    if (fileSize != sizeof(tmp) || tmp.version != NAVS_RESILIENCE_VERSION) {
                        tmp.autoFavCount = 0;
                        memset(tmp.autoFavIds, 0, sizeof(tmp.autoFavIds));
                        tmp.sleepMsgs = 1;
                        tmp.reserved = 0;
                        tmp.role = 0xFF;
                        tmp.cliChannelSlot = 1;
                        tmp.navadminMuted = 0;
                        memset(tmp.customChannels, 0, sizeof(tmp.customChannels));
                        tmp.ok_to_mqtt = 0;
                        tmp.fixed_pin = 0;
                        tmp.fixed_pos_lat = 0;
                        tmp.fixed_pos_lon = 0;
                        tmp.fixed_pos_alt = 0;
                        tmp.fixed_pos_enabled = 0;
                        tmp.beacon_interval_secs = 0;
                        tmp.pos_tx_secs = 259200;
                        tmp.nodeinfo_tx_secs = 259200;
                        tmp.telem_tx_secs = 43200;
                        tmp.ignoredCount = 0;
                        memset(tmp.ignoredNodes, 0, sizeof(tmp.ignoredNodes));
                        tmp.version = NAVS_RESILIENCE_VERSION;
                        if (tmp.chemistry > 3) tmp.chemistry = 0;
                        if (tmp.vbat_cutoff < 2400 || tmp.vbat_cutoff > 3600) tmp.vbat_cutoff = 3500;
                        if (tmp.vwake_level < 1 || tmp.vwake_level > 5) tmp.vwake_level = 3;
                        if (tmp.tx_disabled > 1) tmp.tx_disabled = 0;
                        if (tmp.ble_disabled > 1) tmp.ble_disabled = 0;
                    }
                }
            }
            f.close();
        }
    }
    if (!exists) {
        tmp.magic = 0x52455349;
        tmp.sleepMsgs = 1;
        tmp.auto_fav = 1;
        tmp.role = 0xFF;
        #if defined(USERPREFS_BATTERY_CHEMISTRY_SODIUM)
            tmp.chemistry = 2; // SODIUM
            tmp.vbat_cutoff = 2600;
            tmp.vwake_level = 1;
        #else
            tmp.chemistry = 0; // LIPO
            tmp.vbat_cutoff = 3500;
            tmp.vwake_level = 3;
        #endif
        tmp.tx_disabled = 0;
        tmp.ble_disabled = 0;
        tmp.autoFavCount = 0;
        memset(tmp.autoFavIds, 0, sizeof(tmp.autoFavIds));
        tmp.wasInSleep = 0;
        tmp.reserved = 0;
        tmp.cliChannelSlot = 1;
        tmp.navadminMuted = 0;
        memset(tmp.customChannels, 0, sizeof(tmp.customChannels));
        tmp.ok_to_mqtt = 0;
        tmp.fixed_pin = 0;
        tmp.fixed_pos_lat = 0;
        tmp.fixed_pos_lon = 0;
        tmp.fixed_pos_alt = 0;
        tmp.fixed_pos_enabled = 0;
        tmp.beacon_interval_secs = 0;
        tmp.pos_tx_secs = 259200;
        tmp.nodeinfo_tx_secs = 259200;
        tmp.telem_tx_secs = 43200;
        tmp.ignoredCount = 0;
        memset(tmp.ignoredNodes, 0, sizeof(tmp.ignoredNodes));
        tmp.version = NAVS_RESILIENCE_VERSION;
    }
    tmp.wasInSleep = on ? 1 : 0;
    FSCom.remove("/resilience.bin");
    File f = FSCom.open("/resilience.bin", FILE_O_WRITE);
    if (f) {
        f.write((uint8_t *)&tmp, sizeof(tmp));
        f.close();
    }
}

void NavaCLIModule::navaSetVivoPending()
{
    navaVivoPendingGlobal = true;
}

bool NavaCLIModule::navaGetVivoPending()
{
    return navaVivoPendingGlobal;
}

void NavaCLIModule::navaSetReservaPending()
{
    navaReservaPendingGlobal = true;
}

bool NavaCLIModule::navaGetReservaPending()
{
    return navaReservaPendingGlobal;
}

bool NavaCLIModule::navaIsMuteActive()
{
    if (navaCLIModule && navaCLIModule->muteUntilMs > 0) {
        if ((int32_t)(millis() - navaCLIModule->muteUntilMs) < 0) {
            return true;
        } else {
            navaCLIModule->muteUntilMs = 0;
            return false;
        }
    }
    return false;
}

void NavaCLIModule::recordRoutedPacket()
{
    if (navaCLIModule) {
        navaCLIModule->statsRoutedPackets++;
    }
}

void NavaCLIModule::logRamEvent(const char *msg)
{
    if (navaCLIModule) {
        navaCLIModule->logEvent("%s", msg);
    }
}

void NavaCLIModule::logEvent(const char *fmt, ...)
{
    char buf[48];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ramLogs[ramLogHead].uptime = millis() / 1000;
    strncpy(ramLogs[ramLogHead].msg, buf, sizeof(ramLogs[0].msg) - 1);
    ramLogs[ramLogHead].msg[sizeof(ramLogs[0].msg) - 1] = '\0';
    ramLogHead = (ramLogHead + 1) % 16;
    if (ramLogCount < 16) ramLogCount++;
}

bool NavaCLIModule::handleLowBatteryEvent()
{
    if (sleepPending) {
        return true;
    }
    sleepPending = true;
    sleepTime = millis() + 5000; // fallback por si no hay aviso o falla encolar
    prefs.wasInSleep = 1;
    saveResiliencePrefs();

    logEvent("LOWBAT sleep diferido");

    if (prefs.sleepMsgs) {
        char buf[220];
        snprintf(buf, sizeof(buf), "[Sueno] %s id%08x | %s | sueno profundo, despertara >= %u mV",
                 owner.long_name, (unsigned int)nodeDB->getNodeNum(), buildEnergyLine().c_str(),
                 (unsigned int)navaGetLpcompWakeMv());
        uint8_t targetChan = prefs.cliChannelSlot;
        if (targetChan < 1 || targetChan > 7) targetChan = 1;
        enqueueResponse(NODENUM_BROADCAST, targetChan, buf, true, true);
    }
    return true;
}

void NavaCLIModule::saveResiliencePrefs() {
    prefs.magic = 0x52455349;
    prefs.version = NAVS_RESILIENCE_VERSION;
    prefs.crc32 = crc32Buffer(&prefs, offsetof(ResiliencePrefs, crc32));

    concurrency::LockGuard g(spiLock);
    File f = FSCom.open("/resilience.tmp", FILE_O_WRITE);
    if (f) {
        size_t written = f.write((const uint8_t*)&prefs, sizeof(prefs));
        f.close();
        if (written == sizeof(prefs)) {
            FSCom.remove("/resilience.bin");
            FSCom.rename("/resilience.tmp", "/resilience.bin");
        } else {
            FSCom.remove("/resilience.tmp");
        }
    }
}

// Helper: motivo de reset de Nordic nRF52 decodificado a texto corto
static const char *navaricoResetReasonName(uint32_t reas)
{
    if (reas == 0) return "POWER_ON";
    if (reas & 0x01) return "RESETPIN";
    if (reas & 0x02) return "DOG";
    if (reas & 0x04) return "SREQ";
    if (reas & 0x08) return "LOCKUP";
    if (reas & 0x10) return "OFF_RESET";
    if (reas & 0x20) return "LPCOMP";
    if (reas & 0x40) return "DIF";
    if (reas & 0x80) return "NFC";
    if (reas & 0x10000) return "VBUS";
    return "UNKNOWN";
}

// V2: construye la linea de energia: ADC mV + INA si presente
std::string NavaCLIModule::buildEnergyLine()
{
    char buf[128];
    uint16_t adcV = powerStatus->getBatteryVoltageMv();
#if HAS_TELEMETRY && !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && __has_include(<Adafruit_INA219.h>)
    uint16_t inaMv = (ina219Sensor.hasSensor()) ? ina219Sensor.getBusVoltageMv() : 0;
    if (inaMv > 0) {
        int16_t inamA = ina219Sensor.getCurrentMa();
        float inaV = inaMv / 1000.0f;
        const char *estado = (inamA > 1) ? "CARGANDO" : (inamA < -1) ? "DESCARGANDO" : "STANDBY";
        snprintf(buf, sizeof(buf), "ADC %u mV | INA %.2f V %+d mA %s",
                 (unsigned int)adcV, inaV, (int)inamA, estado);
        return std::string(buf);
    }
#endif
    snprintf(buf, sizeof(buf), "ADC %u mV", (unsigned int)adcV);
    return std::string(buf);
}

// NAVARICO F20: helpers de deteccion de clave vacia y de clave del proyecto
bool NavaCLIModule::navaKeyIsEmpty(const uint8_t *key)
{
    if (!key) return true;
    for (size_t i = 0; i < 32; i++) {
        if (key[i] != 0) return false;
    }
    return true;
}

bool NavaCLIModule::navaKeyIsProjectKey(const uint8_t *key)
{
    if (navaKeyIsEmpty(key)) return false;
#ifdef USERPREFS_USE_ADMIN_KEY_0
    static const uint8_t pk0[] = USERPREFS_USE_ADMIN_KEY_0;
    if (sizeof(pk0) == 32 && memcmp(key, pk0, 32) == 0) return true;
#endif
#ifdef USERPREFS_USE_ADMIN_KEY_1
    static const uint8_t pk1[] = USERPREFS_USE_ADMIN_KEY_1;
    if (sizeof(pk1) == 32 && memcmp(key, pk1, 32) == 0) return true;
#endif
#ifdef USERPREFS_USE_ADMIN_KEY_2
    static const uint8_t pk2[] = USERPREFS_USE_ADMIN_KEY_2;
    if (sizeof(pk2) == 32 && memcmp(key, pk2, 32) == 0) return true;
#endif
    return false;
}

bool NavaCLIModule::navaKeyIsValid(const uint8_t *key)
{
    if (!key || navaKeyIsEmpty(key)) return false;

    // Rechazar claves corruptas o residuales de shift con más de 10 ceros
    size_t zeroCount = 0;
    for (size_t i = 0; i < 32; i++) {
        if (key[i] == 0) zeroCount++;
    }
    if (zeroCount > 10) return false;

    // Si bytes 1..31 son todos cero, o si todos los bytes son iguales, es un valor corrupto/inválido.
    bool allZerosAfterFirst = true;
    for (size_t i = 1; i < 32; i++) {
        if (key[i] != 0) {
            allZerosAfterFirst = false;
            break;
        }
    }
    if (allZerosAfterFirst) return false;

    bool allIdentical = true;
    for (size_t i = 1; i < 32; i++) {
        if (key[i] != key[0]) {
            allIdentical = false;
            break;
        }
    }
    if (allIdentical) return false;

    return true;
}

uint32_t NavaCLIModule::getAutoFavId(size_t index) const
{
    if (index < 16) return prefs.autoFavIds[index];
    if (index < 32) return prefs.extraAutoFavIds[index - 16];
    return 0;
}

void NavaCLIModule::setAutoFavId(size_t index, uint32_t id)
{
    if (index < 16) prefs.autoFavIds[index] = id;
    else if (index < 32) prefs.extraAutoFavIds[index - 16] = id;
}

void NavaCLIModule::adoptPersistedAdminKeys()
{
    meshtastic_Config_SecurityConfig &sec = config.security;
    for (pb_size_t i = 0; i < sec.admin_key_count && i < 3; i++) {
        const uint8_t *k = sec.admin_key[i].bytes;
        size_t sz = sec.admin_key[i].size;
        if (sz != 32 || !navaKeyIsValid(k) || navaKeyIsProjectKey(k)) {
            continue;
        }
        if (i == 0) {
            memcpy(prefs.keySlot0Own, k, 32);
        } else if (i == 1) {
            memcpy(prefs.keySlot1, k, 32);
        } else if (i == 2) {
            memcpy(prefs.keySlot2, k, 32);
        }
    }
}

void NavaCLIModule::applyPersistedAdminKeys()
{
    meshtastic_Config_SecurityConfig &sec = config.security;
    bool changed = false;

    if (navaKeyIsValid(prefs.keySlot0Own)) {
        if (sec.admin_key[0].size != 32 || memcmp(sec.admin_key[0].bytes, prefs.keySlot0Own, 32) != 0) {
            memcpy(sec.admin_key[0].bytes, prefs.keySlot0Own, 32);
            sec.admin_key[0].size = 32;
            changed = true;
        }
    }

    if (navaKeyIsValid(prefs.keySlot1)) {
        if (sec.admin_key_count < 2 || sec.admin_key[1].size != 32 ||
            memcmp(sec.admin_key[1].bytes, prefs.keySlot1, 32) != 0) {
            memcpy(sec.admin_key[1].bytes, prefs.keySlot1, 32);
            sec.admin_key[1].size = 32;
            changed = true;
        }
    } else {
        if (sec.admin_key[1].size > 0 || !navaKeyIsEmpty(sec.admin_key[1].bytes)) {
            memset(sec.admin_key[1].bytes, 0, sizeof(sec.admin_key[1].bytes));
            sec.admin_key[1].size = 0;
            changed = true;
        }
    }

    if (navaKeyIsValid(prefs.keySlot2)) {
        if (sec.admin_key_count < 3 || sec.admin_key[2].size != 32 ||
            memcmp(sec.admin_key[2].bytes, prefs.keySlot2, 32) != 0) {
            memcpy(sec.admin_key[2].bytes, prefs.keySlot2, 32);
            sec.admin_key[2].size = 32;
            changed = true;
        }
    } else {
        if (sec.admin_key[2].size > 0 || !navaKeyIsEmpty(sec.admin_key[2].bytes)) {
            memset(sec.admin_key[2].bytes, 0, sizeof(sec.admin_key[2].bytes));
            sec.admin_key[2].size = 0;
            changed = true;
        }
    }

    pb_size_t count = 0;
    for (pb_size_t i = 0; i < 3; i++) {
        if (sec.admin_key[i].size == 32 && navaKeyIsValid(sec.admin_key[i].bytes)) {
            count = i + 1;
        } else if (sec.admin_key[i].size != 32 || !navaKeyIsValid(sec.admin_key[i].bytes)) {
            if (!navaKeyIsEmpty(sec.admin_key[i].bytes)) {
                memset(sec.admin_key[i].bytes, 0, sizeof(sec.admin_key[i].bytes));
                sec.admin_key[i].size = 0;
                changed = true;
            }
        }
    }
    if (sec.admin_key_count != count) {
        sec.admin_key_count = count;
        changed = true;
    }

    if (changed) {
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        LOG_INFO("F20: claves admin saneadas y restauradas desde /resilience.bin");
    }
}

void NavaCLIModule::syncAdminKeysFromConfig()
{
    meshtastic_Config_SecurityConfig &sec = config.security;
    bool changed = false;

    // Slot 0: Clave propia del dueño vs MasterNode
    if (sec.admin_key_count > 0 && sec.admin_key[0].size == 32) {
        const uint8_t *k0 = sec.admin_key[0].bytes;
        if (navaKeyIsProjectKey(k0)) {
            if (!navaKeyIsEmpty(prefs.keySlot0Own)) {
                memset(prefs.keySlot0Own, 0, sizeof(prefs.keySlot0Own));
                changed = true;
            }
        } else if (navaKeyIsValid(k0)) {
            if (memcmp(prefs.keySlot0Own, k0, 32) != 0) {
                memcpy(prefs.keySlot0Own, k0, 32);
                changed = true;
            }
        }
    } else {
        if (!navaKeyIsEmpty(prefs.keySlot0Own)) {
            memset(prefs.keySlot0Own, 0, sizeof(prefs.keySlot0Own));
            changed = true;
        }
    }

    // Slot 1: Si existe y es válida, guardar; si el usuario la borró (admin_key_count < 2), BORRAR de resilience.bin
    if (sec.admin_key_count > 1 && sec.admin_key[1].size == 32 && navaKeyIsValid(sec.admin_key[1].bytes)) {
        const uint8_t *k1 = sec.admin_key[1].bytes;
        if (memcmp(prefs.keySlot1, k1, 32) != 0) {
            memcpy(prefs.keySlot1, k1, 32);
            changed = true;
        }
    } else {
        if (!navaKeyIsEmpty(prefs.keySlot1)) {
            memset(prefs.keySlot1, 0, sizeof(prefs.keySlot1));
            changed = true;
            LOG_INFO("NavaCLI: Clave admin slot 1 eliminada por usuario - purgada de /resilience.bin");
        }
    }

    // Slot 2: Si existe y es válida, guardar; si el usuario la borró (admin_key_count < 3), BORRAR de resilience.bin
    if (sec.admin_key_count > 2 && sec.admin_key[2].size == 32 && navaKeyIsValid(sec.admin_key[2].bytes)) {
        const uint8_t *k2 = sec.admin_key[2].bytes;
        if (memcmp(prefs.keySlot2, k2, 32) != 0) {
            memcpy(prefs.keySlot2, k2, 32);
            changed = true;
        }
    } else {
        if (!navaKeyIsEmpty(prefs.keySlot2)) {
            memset(prefs.keySlot2, 0, sizeof(prefs.keySlot2));
            changed = true;
            LOG_INFO("NavaCLI: Clave admin slot 2 eliminada por usuario - purgada de /resilience.bin");
        }
    }

    if (changed) {
        saveResiliencePrefs();
        LOG_INFO("F20: claves admin sincronizadas y saneadas hacia /resilience.bin");
    }
}

void NavaCLIModule::applyPersistedChannels()
{
    bool changed = false;
    for (uint8_t i = 2; i < MAX_NUM_CHANNELS; i++) {
        const ResilientChannel &rc = prefs.customChannels[i - 2];
        if (rc.is_active) {
            meshtastic_Channel &current = channels.getByIndex(i);
            if (!current.has_settings || current.role == meshtastic_Channel_Role_DISABLED ||
                current.settings.psk.size != rc.psk_len ||
                memcmp(current.settings.psk.bytes, rc.psk, rc.psk_len) != 0 ||
                strncmp(current.settings.name, rc.name, sizeof(current.settings.name)) != 0) {
                
                meshtastic_Channel ch = meshtastic_Channel_init_zero;
                ch.index = i;
                ch.role = meshtastic_Channel_Role_SECONDARY;
                ch.has_settings = true;
                strncpy(ch.settings.name, rc.name, sizeof(ch.settings.name) - 1);
                ch.settings.psk.size = rc.psk_len;
                memcpy(ch.settings.psk.bytes, rc.psk, rc.psk_len);
                ch.settings.uplink_enabled = (rc.uplink_enabled != 0);
                ch.settings.downlink_enabled = (rc.downlink_enabled != 0);
                ch.settings.has_module_settings = true;
                channels.setChannel(ch);
                changed = true;
            }
        }
    }
    if (changed) {
        channels.onConfigChanged();
        nodeDB->saveToDisk(SEGMENT_CHANNELS);
        LOG_INFO("F21: Canales secundarios restaurados desde /resilience.bin");
    }
}

// NAVARICO V5: Persistencia y Restauración de Capa Física LoRa
void NavaCLIModule::applyPersistedLoraConfig()
{
    if (prefs.lora_configured != 1) return;
    bool changed = false;
    meshtastic_Config_LoRaConfig &lora = config.lora;
    if (prefs.lora_use_preset == 1) {
        if (!lora.use_preset || lora.modem_preset != prefs.lora_modem_preset) {
            lora.use_preset = true;
            lora.modem_preset = (meshtastic_Config_LoRaConfig_ModemPreset)prefs.lora_modem_preset;
            lora.override_frequency = 0.0f;
            changed = true;
        }
    } else if (prefs.lora_use_preset == 0 && prefs.lora_bandwidth > 0 && prefs.lora_spread_factor >= 5) {
        if (lora.use_preset || lora.bandwidth != prefs.lora_bandwidth || lora.spread_factor != prefs.lora_spread_factor ||
            lora.coding_rate != prefs.lora_coding_rate || lora.override_frequency != prefs.lora_override_frequency ||
            lora.channel_num != prefs.lora_channel_num) {
            lora.use_preset = false;
            lora.bandwidth = prefs.lora_bandwidth;
            lora.spread_factor = prefs.lora_spread_factor;
            lora.coding_rate = prefs.lora_coding_rate;
            lora.override_frequency = prefs.lora_override_frequency;
            lora.channel_num = prefs.lora_channel_num;
            changed = true;
        }
    }
    if (prefs.lora_tx_power > 0 && lora.tx_power != prefs.lora_tx_power) {
        lora.tx_power = prefs.lora_tx_power;
        changed = true;
    }
    if (changed) {
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        LOG_INFO("NavaCLI: Capa Fisica LoRa restaurada desde /resilience.bin");
    }
}

void NavaCLIModule::adoptPersistedLoraConfig()
{
    if (prefs.lora_configured == 1) return;
    const meshtastic_Config_LoRaConfig &lora = config.lora;
    prefs.lora_use_preset = lora.use_preset ? 1 : 0;
    prefs.lora_modem_preset = (uint8_t)lora.modem_preset;
    prefs.lora_bandwidth = lora.bandwidth;
    prefs.lora_spread_factor = lora.spread_factor;
    prefs.lora_coding_rate = lora.coding_rate;
    prefs.lora_channel_num = lora.channel_num;
    prefs.lora_override_frequency = lora.override_frequency;
    prefs.lora_tx_power = lora.tx_power;
    prefs.lora_configured = 1;
    saveResiliencePrefs();
}

void NavaCLIModule::syncLoraConfigFromConfig()
{
    const meshtastic_Config_LoRaConfig &lora = config.lora;
    bool changed = false;
    uint8_t use_pre = lora.use_preset ? 1 : 0;
    if (prefs.lora_use_preset != use_pre) { prefs.lora_use_preset = use_pre; changed = true; }
    if (prefs.lora_modem_preset != (uint8_t)lora.modem_preset) { prefs.lora_modem_preset = (uint8_t)lora.modem_preset; changed = true; }
    if (prefs.lora_bandwidth != lora.bandwidth) { prefs.lora_bandwidth = lora.bandwidth; changed = true; }
    if (prefs.lora_spread_factor != lora.spread_factor) { prefs.lora_spread_factor = lora.spread_factor; changed = true; }
    if (prefs.lora_coding_rate != lora.coding_rate) { prefs.lora_coding_rate = lora.coding_rate; changed = true; }
    if (prefs.lora_channel_num != lora.channel_num) { prefs.lora_channel_num = lora.channel_num; changed = true; }
    if (prefs.lora_override_frequency != lora.override_frequency) { prefs.lora_override_frequency = lora.override_frequency; changed = true; }
    if (prefs.lora_tx_power != lora.tx_power) { prefs.lora_tx_power = lora.tx_power; changed = true; }
    if (prefs.lora_configured != 1) { prefs.lora_configured = 1; changed = true; }

    if (changed) {
        saveResiliencePrefs();
        LOG_INFO("NavaCLI: Capa Fisica LoRa sincronizada hacia /resilience.bin");
    }
}

// NAVARICO V5: Persistencia y Restauración de Canal 0 Primario
void NavaCLIModule::applyPersistedChannel0()
{
    if (prefs.ch0_configured != 1) return;
    meshtastic_Channel &ch0 = channels.getByIndex(0);
    bool changed = false;
    if (prefs.ch0_name[0] != '\0' && strncmp(ch0.settings.name, prefs.ch0_name, sizeof(ch0.settings.name)) != 0) {
        strncpy(ch0.settings.name, prefs.ch0_name, sizeof(ch0.settings.name) - 1);
        ch0.settings.name[sizeof(ch0.settings.name) - 1] = '\0';
        changed = true;
    }
    if (prefs.ch0_psk_len > 0) {
        if (ch0.settings.psk.size != prefs.ch0_psk_len || memcmp(ch0.settings.psk.bytes, prefs.ch0_psk, prefs.ch0_psk_len) != 0) {
            memcpy(ch0.settings.psk.bytes, prefs.ch0_psk, prefs.ch0_psk_len);
            ch0.settings.psk.size = prefs.ch0_psk_len;
            changed = true;
        }
    }
    if (changed) {
        ch0.has_settings = true;
        ch0.role = meshtastic_Channel_Role_PRIMARY;
        channels.setChannel(ch0);
        channels.onConfigChanged();
        nodeDB->saveToDisk(SEGMENT_CHANNELS);
        LOG_INFO("NavaCLI: Canal 0 Primario restaurado desde /resilience.bin");
    }
}

void NavaCLIModule::adoptPersistedChannel0()
{
    if (prefs.ch0_configured == 1) return;
    const meshtastic_Channel &ch0 = channels.getByIndex(0);
    if (ch0.has_settings) {
        strncpy(prefs.ch0_name, ch0.settings.name, sizeof(prefs.ch0_name) - 1);
        prefs.ch0_name[sizeof(prefs.ch0_name) - 1] = '\0';
        if (ch0.settings.psk.size > 0 && ch0.settings.psk.size <= 32) {
            memcpy(prefs.ch0_psk, ch0.settings.psk.bytes, ch0.settings.psk.size);
            prefs.ch0_psk_len = ch0.settings.psk.size;
        } else {
            prefs.ch0_psk[0] = 0x01;
            prefs.ch0_psk_len = 1;
        }
        prefs.ch0_configured = 1;
        saveResiliencePrefs();
    }
}

void NavaCLIModule::syncChannel0FromConfig()
{
    const meshtastic_Channel &ch0 = channels.getByIndex(0);
    if (!ch0.has_settings) return;
    bool changed = false;
    if (strncmp(prefs.ch0_name, ch0.settings.name, sizeof(prefs.ch0_name)) != 0) {
        strncpy(prefs.ch0_name, ch0.settings.name, sizeof(prefs.ch0_name) - 1);
        prefs.ch0_name[sizeof(prefs.ch0_name) - 1] = '\0';
        changed = true;
    }
    if (ch0.settings.psk.size > 0 && ch0.settings.psk.size <= 32) {
        if (prefs.ch0_psk_len != ch0.settings.psk.size || memcmp(prefs.ch0_psk, ch0.settings.psk.bytes, ch0.settings.psk.size) != 0) {
            memcpy(prefs.ch0_psk, ch0.settings.psk.bytes, ch0.settings.psk.size);
            prefs.ch0_psk_len = ch0.settings.psk.size;
            changed = true;
        }
    }
    if (prefs.ch0_configured != 1) {
        prefs.ch0_configured = 1;
        changed = true;
    }
    if (changed) {
        saveResiliencePrefs();
        LOG_INFO("NavaCLI: Canal 0 Primario sincronizado hacia /resilience.bin");
    }
}

// NAVARICO V5: Sincronizaciones desde App Oficial
void NavaCLIModule::syncDeviceRoleFromConfig()
{
    if (prefs.role != (uint8_t)config.device.role) {
        prefs.role = (uint8_t)config.device.role;
        saveResiliencePrefs();
        LOG_INFO("NavaCLI: Rol de dispositivo sincronizado hacia /resilience.bin: %d", prefs.role);
    }
    owner.role = config.device.role;
    owner.is_unmessagable = false;
    owner.has_is_unmessagable = true;
    nodeDB->updateUser(nodeDB->getNodeNum(), owner);
    nodeDB->saveToDisk(SEGMENT_DEVICESTATE | SEGMENT_NODEDATABASE);
    if (service) {
        service->reloadOwner(true);
    }
}

void NavaCLIModule::syncOkToMqttFromConfig()
{
    uint8_t val = config.lora.config_ok_to_mqtt ? 1 : 2;
    if (prefs.ok_to_mqtt != val) {
        prefs.ok_to_mqtt = val;
        saveResiliencePrefs();
        LOG_INFO("NavaCLI: OK to MQTT sincronizado hacia /resilience.bin: %d", val);
    }
}

void NavaCLIModule::syncTelemetryIntervalFromConfig()
{
    uint32_t val = moduleConfig.telemetry.device_update_interval;
    if (val == 0 && moduleConfig.telemetry.environment_update_interval > 0) {
        val = moduleConfig.telemetry.environment_update_interval;
    }
    if (prefs.telem_tx_secs != val) {
        prefs.telem_tx_secs = val;
        saveResiliencePrefs();
        LOG_INFO("NavaCLI: Telemetry interval sincronizado hacia /resilience.bin: %u", prefs.telem_tx_secs);
    }
}

void NavaCLIModule::syncNodeInfoIntervalFromConfig()
{
    if (prefs.nodeinfo_tx_secs != config.device.node_info_broadcast_secs) {
        prefs.nodeinfo_tx_secs = config.device.node_info_broadcast_secs;
        saveResiliencePrefs();
        LOG_INFO("NavaCLI: NodeInfo interval sincronizado hacia /resilience.bin: %u", prefs.nodeinfo_tx_secs);
    }
}

void NavaCLIModule::syncPositionIntervalFromConfig()
{
    if (prefs.pos_tx_secs != config.position.position_broadcast_secs) {
        prefs.pos_tx_secs = config.position.position_broadcast_secs;
        saveResiliencePrefs();
        LOG_INFO("NavaCLI: Position interval sincronizado hacia /resilience.bin: %u", prefs.pos_tx_secs);
    }
}

void NavaCLIModule::syncFixedPositionFromConfig()
{
    bool changed = false;
    uint8_t en = config.position.fixed_position ? 1 : 0;
    if (prefs.fixed_pos_enabled != en) { prefs.fixed_pos_enabled = en; changed = true; }
    if (prefs.fixed_pos_lat != (int32_t)(localPosition.latitude_i)) {
        prefs.fixed_pos_lat = (int32_t)(localPosition.latitude_i);
        changed = true;
    }
    if (prefs.fixed_pos_lon != (int32_t)(localPosition.longitude_i)) {
        prefs.fixed_pos_lon = (int32_t)(localPosition.longitude_i);
        changed = true;
    }
    if (prefs.fixed_pos_alt != (int32_t)(localPosition.altitude)) {
        prefs.fixed_pos_alt = (int32_t)(localPosition.altitude);
        changed = true;
    }
    if (changed) {
        saveResiliencePrefs();
        LOG_INFO("NavaCLI: Posicion fija sincronizada hacia /resilience.bin");
    }
}

void NavaCLIModule::syncBluetoothPinFromConfig()
{
    if (prefs.fixed_pin != config.bluetooth.fixed_pin) {
        prefs.fixed_pin = config.bluetooth.fixed_pin;
        saveResiliencePrefs();
        LOG_INFO("NavaCLI: PIN Bluetooth fijo sincronizado hacia /resilience.bin: %u", prefs.fixed_pin);
    }
}

void NavaCLIModule::syncCustomChannelFromConfig(uint8_t slot)
{
    if (slot < 2 || slot > 7) return;
    uint8_t idx = slot - 2;
    const meshtastic_Channel &ch = channels.getByIndex(slot);
    bool changed = false;
    ResilientChannel &rc = prefs.customChannels[idx];
    if (ch.role == meshtastic_Channel_Role_DISABLED || !ch.has_settings) {
        if (rc.is_active != 0) {
            memset(&rc, 0, sizeof(rc));
            changed = true;
        }
    } else {
        if (rc.is_active != 1) { rc.is_active = 1; changed = true; }
        if (strncmp(rc.name, ch.settings.name, sizeof(rc.name)) != 0) {
            strncpy(rc.name, ch.settings.name, sizeof(rc.name) - 1);
            rc.name[sizeof(rc.name) - 1] = '\0';
            changed = true;
        }
        if (rc.psk_len != ch.settings.psk.size || memcmp(rc.psk, ch.settings.psk.bytes, ch.settings.psk.size) != 0) {
            memcpy(rc.psk, ch.settings.psk.bytes, ch.settings.psk.size);
            rc.psk_len = ch.settings.psk.size;
            changed = true;
        }
        uint8_t up = ch.settings.uplink_enabled ? 1 : 0;
        uint8_t dn = ch.settings.downlink_enabled ? 1 : 0;
        if (rc.uplink_enabled != up) { rc.uplink_enabled = up; changed = true; }
        if (rc.downlink_enabled != dn) { rc.downlink_enabled = dn; changed = true; }
    }
    if (changed) {
        saveResiliencePrefs();
        LOG_INFO("NavaCLI: Canal secundario slot %d sincronizado hacia /resilience.bin", slot);
    }
}

// NAVARICO V5: Protocolo "Botón del Pánico"
bool NavaCLIModule::navaIsPanicActive()
{
    return (navaCLIModule && navaCLIModule->prefs.panic_active != 0);
}

bool NavaCLIModule::navaIsPanicTunnelMode()
{
    return (navaCLIModule && (navaCLIModule->prefs.panic_active != 0 || navaCLIModule->prefs.panic_trial_active != 0));
}

void NavaCLIModule::startPanic(const NavaPanicPulse &pulse)
{
    // Si ya estamos en pánico activo para la misma sesión, anclamos el tiempo y no movemos el reloj
    if (prefs.panic_active == 1) {
        if (currentPanicSessionId != 0 && pulse.session_id == currentPanicSessionId) {
            return;
        }
    }

    currentPanicSessionId = (pulse.session_id != 0) ? pulse.session_id : ((uint32_t)rand() ^ (uint32_t)millis());
    prefs.panic_active = 1;
    prefs.panic_target_preset = pulse.modem_preset;
    prefs.panic_target_sf = pulse.sf;
    prefs.panic_target_cr = pulse.cr;
    prefs.panic_target_bw = pulse.bw_code;
    prefs.panic_target_slot = pulse.channel_slot;
    prefs.panic_target_freq = pulse.freq_mhz;
    prefs.panic_rollback_mins = pulse.rollback_minutes;
    // Anclaje Monotónico de Sesión
    prefs.panic_target_time_ms = millis() + ((uint32_t)pulse.remaining_seconds * 1000);
    prefs.panic_last_pulse_ms = millis();
    saveResiliencePrefs();

    config.lora.override_duty_cycle = true;

    // Emisión de aviso textual claro por difusión en el canal CLI (Navadmin o asignado)
    uint8_t targetChan = prefs.cliChannelSlot;
    if (targetChan < 1 || targetChan > 7) targetChan = 1;

    char textBuf[160];
    if (pulse.use_preset) {
        const char *pname = "DESCONOCIDO";
        switch (pulse.modem_preset) {
            case meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST: pname = "LONG_FAST"; break;
            case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST: pname = "MEDIUM_FAST"; break;
            case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST: pname = "SHORT_FAST"; break;
            case meshtastic_Config_LoRaConfig_ModemPreset_LONG_SLOW: pname = "LONG_SLOW"; break;
            case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_SLOW: pname = "SHORT_SLOW"; break;
            case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_SLOW: pname = "MEDIUM_SLOW"; break;
            case meshtastic_Config_LoRaConfig_ModemPreset_LONG_MODERATE: pname = "LONG_MODERATE"; break;
            case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_TURBO: pname = "SHORT_TURBO"; break;
            default: pname = "PRESET"; break;
        }
        snprintf(textBuf, sizeof(textBuf), "[Panico] EVACUACION a %s en %u min. Rollback: %u min. Silencio a T-60s.",
                 pname, (pulse.remaining_seconds + 59) / 60, (unsigned int)pulse.rollback_minutes);
    } else {
        snprintf(textBuf, sizeof(textBuf), "[Panico] EVACUACION a BW%u SF%u CR4/%u Freq:%.4f Slot:%u en %u min. Rollback: %u min.",
                 pulse.bw_code, pulse.sf, pulse.cr, pulse.freq_mhz, pulse.channel_slot,
                 (pulse.remaining_seconds + 59) / 60, (unsigned int)pulse.rollback_minutes);
    }
    enqueueResponse(NODENUM_BROADCAST, targetChan, textBuf, true, true);

    emitPanicPulse();
}

void NavaCLIModule::emitPanicPulse()
{
    if (prefs.panic_active != 1) return;
    int32_t remSecs = (int32_t)(prefs.panic_target_time_ms - millis()) / 1000;
    if (remSecs <= 60) {
        return; // Ventana de silencio en los últimos 60 segundos
    }

    uint8_t targetChan = prefs.cliChannelSlot;
    if (targetChan < 1 || targetChan > 7) targetChan = 1;

    NavaPanicPulse pulse;
    memset(&pulse, 0, sizeof(pulse));
    memcpy(pulse.magic, "PANC", 4);
    pulse.session_id = currentPanicSessionId;
    pulse.use_preset = (prefs.panic_target_preset != 0) ? 1 : 0;
    pulse.modem_preset = prefs.panic_target_preset;
    pulse.sf = prefs.panic_target_sf;
    pulse.cr = prefs.panic_target_cr;
    pulse.bw_code = (uint16_t)prefs.panic_target_bw;
    pulse.channel_slot = (uint16_t)prefs.panic_target_slot;
    pulse.freq_mhz = prefs.panic_target_freq;
    pulse.remaining_seconds = (uint16_t)remSecs;
    pulse.rollback_minutes = (uint16_t)prefs.panic_rollback_mins;
    pulse.sender_nodenum = nodeDB->getNodeNum();

    meshtastic_MeshPacket *p = allocDataPacket();
    if (p) {
        p->to = NODENUM_BROADCAST;
        p->channel = targetChan;
        p->hop_limit = 1; // Pulso directo local para avanzar de valle en valle en cascada sin rebotes innecesarios
        p->priority = meshtastic_MeshPacket_Priority_ALERT;
        p->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
        p->decoded.payload.size = sizeof(pulse);
        memcpy(p->decoded.payload.bytes, &pulse, sizeof(pulse));
        service->sendToMesh(p, RX_SRC_LOCAL, false);
    }
    nextPulseIntervalMs = 25000 + (rand() % 20000);
    prefs.panic_last_pulse_ms = millis();
    LOG_INFO("NavaCLI: Pulso de Panico emitido (sesion 0x%08x). Quedan %d segundos para evacuacion", currentPanicSessionId, remSecs);
}

void NavaCLIModule::emitPanicOkPulse()
{
    uint8_t targetChan = prefs.cliChannelSlot;
    if (targetChan < 1 || targetChan > 7) targetChan = 1;

    NavaPanicPulse pulse;
    memset(&pulse, 0, sizeof(pulse));
    memcpy(pulse.magic, "POK!", 4);
    pulse.session_id = currentPanicSessionId;
    pulse.sender_nodenum = nodeDB->getNodeNum();

    meshtastic_MeshPacket *p = allocDataPacket();
    if (p) {
        p->to = NODENUM_BROADCAST;
        p->channel = targetChan;
        p->hop_limit = Default::getConfiguredOrDefaultHopLimit(config.lora.hop_limit);
        p->priority = meshtastic_MeshPacket_Priority_ALERT;
        p->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
        p->decoded.payload.size = sizeof(pulse);
        memcpy(p->decoded.payload.bytes, &pulse, sizeof(pulse));
        service->sendToMesh(p, RX_SRC_LOCAL, false);
    }

    char textBuf[120];
    snprintf(textBuf, sizeof(textBuf), "[Panico] SALTO CONSOLIDADO. Rollback cancelado en toda la red.");
    enqueueResponse(NODENUM_BROADCAST, targetChan, textBuf, true, true);
    LOG_INFO("NavaCLI: Pulso POK emitido para consolidar la red completa");
}

void NavaCLIModule::cancelPanicRollback()
{
    if (prefs.panic_trial_active || prefs.panic_rollback_mins > 0) {
        prefs.panic_trial_active = 0;
        prefs.panic_rollback_mins = 0;
        prefs.panic_trial_deadline_ms = 0;
        saveResiliencePrefs();
        LOG_INFO("NavaCLI: Rollback de Panico cancelado. Salto consolidado permanentemente.");
    }
}

void NavaCLIModule::navaFullResetKeepKeys()
{
    uint8_t k1[32], k2[32], k0[32];
    memcpy(k1, prefs.keySlot1, 32);
    memcpy(k2, prefs.keySlot2, 32);
    memcpy(k0, prefs.keySlot0Own, 32);

    memset(&prefs, 0, sizeof(prefs));
    prefs.magic = 0x52455349;
    prefs.version = NAVS_RESILIENCE_VERSION;
    #if defined(USERPREFS_BATTERY_CHEMISTRY_SODIUM)
        prefs.chemistry = 2;
        prefs.vbat_cutoff = 2600;
        prefs.vwake_level = 1;
    #else
        prefs.chemistry = 0;
        prefs.vbat_cutoff = 3500;
        prefs.vwake_level = 3;
    #endif
    prefs.tx_disabled = 0;
    prefs.ble_disabled = 0;
    prefs.auto_fav = 1;
    prefs.role = 0xFF;
    prefs.autoFavCount = 0;
    prefs.sleepMsgs = 1;
    prefs.wasInSleep = 0;
    prefs.reserved = 0;
    prefs.cliChannelSlot = 1;
    prefs.navadminMuted = 0;
    memset(prefs.customChannels, 0, sizeof(prefs.customChannels));
    prefs.ok_to_mqtt = 0;
    prefs.fixed_pin = 0;
    prefs.fixed_pos_lat = 0;
    prefs.fixed_pos_lon = 0;
    prefs.fixed_pos_alt = 0;
    prefs.fixed_pos_enabled = 0;
    prefs.beacon_interval_secs = 0;
    prefs.pos_tx_secs = 259200;
    prefs.nodeinfo_tx_secs = 259200;
    prefs.telem_tx_secs = 43200;
    prefs.ignoredCount = 0;
    memset(prefs.ignoredNodes, 0, sizeof(prefs.ignoredNodes));
    prefs.lora_use_preset = 0;
    prefs.lora_modem_preset = 0;
    prefs.lora_bandwidth = 0;
    prefs.lora_spread_factor = 0;
    prefs.lora_coding_rate = 0;
    prefs.lora_channel_num = 0;
    prefs.lora_override_frequency = 0.0f;
    prefs.lora_tx_power = 0;
    prefs.lora_configured = 0;
    memset(prefs.ch0_name, 0, sizeof(prefs.ch0_name));
    memset(prefs.ch0_psk, 0, sizeof(prefs.ch0_psk));
    prefs.ch0_psk_len = 0;
    prefs.ch0_configured = 0;
    prefs.panic_active = 0;
    prefs.panic_target_preset = 0;
    prefs.panic_target_sf = 0;
    prefs.panic_target_cr = 0;
    prefs.panic_target_bw = 0;
    prefs.panic_target_slot = 0;
    prefs.panic_target_freq = 0.0f;
    prefs.panic_rollback_mins = 0;
    prefs.panic_target_time_ms = 0;
    prefs.panic_last_pulse_ms = 0;
    prefs.panic_trial_active = 0;
    prefs.panic_trial_deadline_ms = 0;
    memset(prefs.custom_long_name, 0, sizeof(prefs.custom_long_name));
    memset(prefs.custom_short_name, 0, sizeof(prefs.custom_short_name));
    memset(prefs.extraAutoFavIds, 0, sizeof(prefs.extraAutoFavIds));

    memcpy(prefs.keySlot1, k1, 32);
    memcpy(prefs.keySlot2, k2, 32);
    memcpy(prefs.keySlot0Own, k0, 32);

    saveResiliencePrefs();
}

bool NavaCLIModule::isAutoFav(uint32_t nodeNum) const
{
    for (uint8_t i = 0; i < prefs.autoFavCount && i < 32; i++) {
        if (getAutoFavId(i) == nodeNum) return true;
    }
    return false;
}

bool NavaCLIModule::addAutoFav(uint32_t nodeNum)
{
    if (isAutoFav(nodeNum)) return false;
    if (prefs.autoFavCount < 32) {
        setAutoFavId(prefs.autoFavCount++, nodeNum);
        saveResiliencePrefs();
        return true;
    }
    return false;
}

bool NavaCLIModule::removeAutoFav(uint32_t nodeNum)
{
    for (uint8_t i = 0; i < prefs.autoFavCount && i < 32; i++) {
        if (getAutoFavId(i) == nodeNum) {
            for (uint8_t j = i; j + 1 < prefs.autoFavCount; j++) {
                setAutoFavId(j, getAutoFavId(j + 1));
            }
            prefs.autoFavCount--;
            setAutoFavId(prefs.autoFavCount, 0);
            saveResiliencePrefs();
            return true;
        }
    }
    return false;
}

void NavaCLIModule::reconcileAutoFavs()
{
    if (!navaAutoFavoriteEnabled || !router) return;
    static uint32_t lastReconcile = 0;
    if (millis() - lastReconcile < 60000) return;
    lastReconcile = millis();

    bool changed = false;
    for (size_t i = 0; i < router->activeDirectRouters.size() && i < 32; i++) {
        uint32_t id = router->activeDirectRouters[i];
        if (id != 0 && !isAutoFav(id)) {
            if (prefs.autoFavCount < 32) {
                setAutoFavId(prefs.autoFavCount++, id);
                changed = true;
            }
        }
    }
    if (changed) {
        saveResiliencePrefs();
        nodeDB->saveToDisk(SEGMENT_NODEDATABASE);
    }
}

bool NavaCLIModule::isNodeIgnored(NodeNum node)
{
    if (!navaCLIModule) return false;
    for (uint8_t i = 0; i < navaCLIModule->prefs.ignoredCount && i < 8; i++) {
        if (navaCLIModule->prefs.ignoredNodes[i] == node) return true;
    }
    return false;
}

bool NavaCLIModule::addIgnoredNode(uint32_t nodeNum)
{
    for (uint8_t i = 0; i < prefs.ignoredCount && i < 8; i++) {
        if (prefs.ignoredNodes[i] == nodeNum) return false;
    }
    if (prefs.ignoredCount < 8) {
        prefs.ignoredNodes[prefs.ignoredCount++] = nodeNum;
        saveResiliencePrefs();
        return true;
    }
    return false;
}

bool NavaCLIModule::removeIgnoredNode(uint32_t nodeNum)
{
    for (uint8_t i = 0; i < prefs.ignoredCount && i < 8; i++) {
        if (prefs.ignoredNodes[i] == nodeNum) {
            for (uint8_t j = i; j + 1 < prefs.ignoredCount; j++) {
                prefs.ignoredNodes[j] = prefs.ignoredNodes[j + 1];
            }
            prefs.ignoredCount--;
            prefs.ignoredNodes[prefs.ignoredCount] = 0;
            saveResiliencePrefs();
            return true;
        }
    }
    return false;
}

void NavaCLIModule::clearIgnoredNodes()
{
    prefs.ignoredCount = 0;
    memset(prefs.ignoredNodes, 0, sizeof(prefs.ignoredNodes));
    saveResiliencePrefs();
}

bool NavaCLIModule::base64Decode(const std::string &in, uint8_t *out, size_t &outLen, size_t maxLen)
{
    static const int8_t b64inv[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,62,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    size_t len = in.length();
    while (len > 0 && (in[len - 1] == '=' || in[len - 1] == ' ' || in[len - 1] == '\r' || in[len - 1] == '\n')) {
        len--;
    }
    size_t w = 0;
    uint32_t buf = 0;
    int bits = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)in[i];
        int8_t val = b64inv[c];
        if (val < 0) continue;
        buf = (buf << 6) | (uint8_t)val;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (w < maxLen) {
                out[w++] = (uint8_t)(buf >> bits);
            }
        }
    }
    outLen = w;
    return (w > 0);
}

std::string NavaCLIModule::generateChannelUrl(uint8_t channelIndex)
{
    if (channelIndex >= MAX_NUM_CHANNELS) return "ERR: SLOT INVALIDO";
    const meshtastic_Channel &targetCh = channels.getByIndex(channelIndex);
    if (!targetCh.has_settings || targetCh.role == meshtastic_Channel_Role_DISABLED) {
        return "ERR: CANAL DESHABILITADO";
    }

    meshtastic_ChannelSet cs = meshtastic_ChannelSet_init_zero;
    cs.settings_count = 1;
    cs.settings[0] = targetCh.settings;
    cs.has_lora_config = true;
    cs.lora_config = config.lora;

    uint8_t buffer[MESHTASTIC_MESHTASTIC_APPONLY_PB_H_MAX_SIZE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, &meshtastic_ChannelSet_msg, &cs)) {
        return "ERR: FALLO ENCODING PROTOBUF";
    }

    static const char b64url[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string b64;
    size_t len = stream.bytes_written;
    b64.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= len) {
        uint32_t n = (buffer[i] << 16) | (buffer[i + 1] << 8) | buffer[i + 2];
        b64 += b64url[(n >> 18) & 63];
        b64 += b64url[(n >> 12) & 63];
        b64 += b64url[(n >> 6) & 63];
        b64 += b64url[n & 63];
        i += 3;
    }
    if (i + 1 == len) {
        uint32_t n = buffer[i] << 16;
        b64 += b64url[(n >> 18) & 63];
        b64 += b64url[(n >> 12) & 63];
    } else if (i + 2 == len) {
        uint32_t n = (buffer[i] << 16) | (buffer[i + 1] << 8);
        b64 += b64url[(n >> 18) & 63];
        b64 += b64url[(n >> 12) & 63];
        b64 += b64url[(n >> 6) & 63];
    }

    return "https://meshtastic.org/e/#" + b64;
}

bool NavaCLIModule::wantPacket(const meshtastic_MeshPacket *p)
{
    if (p != nullptr) {
        statsRxPackets++;
        // --- INYECCIÓN NAVARRICO: REGISTRO PROMISCUO RXLOG ---
        RxLogEntry &entry = rxLog[rxLogIndex];
        entry.from = p->from;
        entry.portnum = p->decoded.portnum;
        entry.snr = p->rx_snr;
        entry.rssi = p->rx_rssi;
        entry.timestamp = millis() / 1000;
        
        rxLogIndex = (rxLogIndex + 1) % 5;
        if (rxLogCount < 5) rxLogCount++;
    }

    if (p != nullptr && (p->decoded.portnum == meshtastic_PortNum_PRIVATE_APP || p->decoded.portnum == ourPortNum) && p->decoded.payload.size >= 24) {
        if (memcmp(p->decoded.payload.bytes, "PANC", 4) == 0 || memcmp(p->decoded.payload.bytes, "POK!", 4) == 0) {
            return true;
        }
    }

    if (p != nullptr && p->decoded.portnum == ourPortNum && p->decoded.payload.size >= 5) {
        bool isDM = !isBroadcast(p->to) && (p->to == nodeDB->getNodeNum());
        uint8_t cliSlot = prefs.cliChannelSlot;
        if (cliSlot < 1 || cliSlot > 7) cliSlot = 1;

        bool isCliChan = (p->channel == cliSlot);
        bool isNavadmin = (p->channel == 1);
        if (isNavadmin && prefs.navadminMuted && cliSlot != 1) {
            isNavadmin = false;
        }

        if (isDM || isCliChan || isNavadmin) {
            return (memcmp(p->decoded.payload.bytes, "/nava", 5) == 0);
        }
    }
    
    // Olfateamos telemetría local pasiva para actualizar la caché redundante
    if (p != nullptr && p->decoded.portnum == meshtastic_PortNum_TELEMETRY_APP && p->from == nodeDB->getNodeNum()) {
        return true;
    }
    
    return false;
}

ProcessMessage NavaCLIModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.portnum == meshtastic_PortNum_TELEMETRY_APP) {
        meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_zero;
        if (pb_decode_from_bytes(mp.decoded.payload.bytes, mp.decoded.payload.size, &meshtastic_Telemetry_msg, &telemetry)) {
            if (telemetry.which_variant == meshtastic_Telemetry_environment_metrics_tag) {
                auto &m = telemetry.variant.environment_metrics;
                if (m.has_temperature) {
                    latestTemp = m.temperature;
                    latestHum = m.relative_humidity;
                    hasTelemetryCache = true;
                }
            }
        }
        return ProcessMessage::CONTINUE;
    }

    // Comprobar si es un pulso binario de pánico o consolidación
    if ((mp.decoded.portnum == meshtastic_PortNum_PRIVATE_APP || mp.decoded.portnum == ourPortNum) && mp.decoded.payload.size >= 24) {
        if (memcmp(mp.decoded.payload.bytes, "POK!", 4) == 0) {
            LOG_INFO("NavaCLI: Recibido pulso POK de consolidacion de red desde 0x%08x", (unsigned int)mp.from);
            cancelPanicRollback();
            return ProcessMessage::STOP;
        }
        if (memcmp(mp.decoded.payload.bytes, "PANC", 4) == 0) {
            uint8_t cliSlot = prefs.cliChannelSlot;
            if (cliSlot < 1 || cliSlot > 7) cliSlot = 1;
            if (mp.channel == cliSlot || mp.channel == 1) {
                NavaPanicPulse pulse;
                memset(&pulse, 0, sizeof(pulse));
                memcpy(&pulse, mp.decoded.payload.bytes, std::min<size_t>(sizeof(pulse), mp.decoded.payload.size));
                startPanic(pulse);
            }
            return ProcessMessage::STOP;
        }
    }

    std::string text((char *)mp.decoded.payload.bytes, mp.decoded.payload.size);
    std::string cmd = (text.length() > 6) ? text.substr(6) : "";

    uint8_t cliSlot = prefs.cliChannelSlot;
    if (cliSlot < 1 || cliSlot > 7) cliSlot = 1;

    // Determinar canal y destinatario de la respuesta
    uint8_t replyChannel = 0;
    NodeNum replyDest = mp.from;
    if (mp.channel == cliSlot || (mp.channel == 1 && !prefs.navadminMuted)) {
        replyChannel = mp.channel;
        replyDest = NODENUM_BROADCAST;
    }

    // Cálculo dinámico de saltos recorridos (Hop-Aware Timing)
    uint8_t hops = (mp.hop_start >= mp.hop_limit) ? (mp.hop_start - mp.hop_limit) : 0;

    // --- AUTENTICACIÓN ---
    if (replyChannel == 0) {
        // Mensaje Directo: DEBE estar cifrado por PKI (evita suplantar la ID del admin).
        if (!mp.pki_encrypted) {
            LOG_WARN("Rechazado: Comando /nava por DM no cifrado PKI desde 0x%08x", mp.from);
            return ProcessMessage::STOP;
        }

        const meshtastic_NodeInfoLite *senderNode = nodeDB->getMeshNode(mp.from);
        if (!senderNode) {
            if (unauthorizedReplied.insert(mp.from).second) {
                LOG_WARN("Rechazado: DM PKI de nodo sin registrar 0x%08x", mp.from);
                enqueueResponse(mp.from, 0, "NODO NO REGISTRADO EN NODEDB", true, false, hops);
            }
            return ProcessMessage::STOP;
        }
        if (!nodeDB->isAdminNode(*senderNode)) {
            if (unauthorizedReplied.insert(mp.from).second) {
                LOG_WARN("Rechazado: nodo 0x%08x no es admin verificado", mp.from);
                enqueueResponse(mp.from, 0, "NO AUTORIZADO COMO ADMINISTRADOR", true, false, hops);
            }
            return ProcessMessage::STOP;
        }
        // NAVARICO: Blindar al administrador verificado como favorito en NodeDB (en RAM)
        if (senderNode && !senderNode->is_favorite) {
            meshtastic_NodeInfoLite *lite = nodeDB->getMeshNode(senderNode->num);
            if (lite) {
                lite->is_favorite = true;
            }
        }
    } else {
        // Canal de difusión: solo responden los admins verificados
        const meshtastic_NodeInfoLite *senderNode = nodeDB->getMeshNode(mp.from);
        if (!senderNode || !nodeDB->isAdminNode(*senderNode)) {
            LOG_WARN("Rechazado: Comando /nava en canal sin firma PKI desde 0x%08x", mp.from);
            return ProcessMessage::STOP;
        }
        // NAVARICO: Blindar al administrador verificado como favorito en NodeDB (en RAM)
        if (senderNode && !senderNode->is_favorite) {
            meshtastic_NodeInfoLite *lite = nodeDB->getMeshNode(senderNode->num);
            if (lite) {
                lite->is_favorite = true;
            }
        }
        // Rate-limit genérico del canal de difusión: max 1 comando cada 30s por nodo emisor (excepto urgentes)
        bool isUrgentCmd = (cmd == "ping" || cmd == "status" || cmd == "reboot");
        static std::map<NodeNum, uint32_t> lastBroadcastCmd;
        auto it = lastBroadcastCmd.find(mp.from);
        if (!isUrgentCmd && it != lastBroadcastCmd.end() && (int32_t)(millis() - it->second) < 30000) {
            return ProcessMessage::STOP;
        }
        lastBroadcastCmd[mp.from] = millis();
    }

    executeCommand(mp.from, cmd, replyChannel, replyDest, mp.rx_snr, hops);
    return ProcessMessage::STOP;
}

void NavaCLIModule::enqueueResponse(NodeNum toNode, uint8_t channel, const std::string &msg, bool isFirstFragment, bool quick, uint8_t hops)
{
    size_t pos = 0;
    while (pos < msg.length() && responseQueue.size() < 10) {
        NavaResponse resp;
        resp.dest = toNode;
        resp.channel = channel;
        resp.hops = hops;
        size_t len = std::min<size_t>(190, msg.length() - pos);
        if (pos + len < msg.length()) {
            size_t cut = msg.find_last_of('\n', pos + len - 1);
            if (cut >= pos && cut <= pos + len - 1) {
                len = cut - pos + 1;
            } else {
                cut = msg.find_last_of(' ', pos + len - 1);
                if (cut > pos) {
                    len = cut - pos;
                }
            }
        }
        resp.text = msg.substr(pos, len);
        responseQueue.push(resp);
        pos += len;
        if (pos < msg.length() && msg[pos] == ' ') {
            pos++;
        }
    }
    if (pos < msg.length()) {
        NavaResponse resp;
        resp.dest = toNode;
        resp.channel = channel;
        resp.hops = hops;
        resp.text = "... [TRUNCADO POR LIMITES DE MTU]";
        responseQueue.push(resp);
    }

    if (isFirstFragment) {
        if (channel == 0) {
            // DM Privado Cifrado: Hop-Aware Timing adaptativo
            uint32_t delayMs;
            if (hops == 0) {
                delayMs = 300 + (rand() % 300);       // 300 - 600 ms (directo / lab)
            } else if (hops == 1) {
                delayMs = 1500 + (rand() % 1000);     // 1.5 - 2.5 s (1 repetidor intermedio)
            } else {
                delayMs = 3500 + (rand() % 1500);     // 3.5 - 5.0 s (malla profunda / valles)
            }
            setIntervalFromNow(delayMs);
        } else {
            // Canal Navadmin / Difusión: True Random Jitter anti-colisiones
            uint32_t jitter;
            if (quick) {
                jitter = 300 + (rand() % 2000);       // 300 ms - 2.3 s para avisos rápidos
            } else {
                jitter = 5000 + (rand() % 8000);      // 5.0 s - 13.0 s para comandos generales
            }
            setIntervalFromNow(jitter);
        }
    }
}

void NavaCLIModule::executeCommand(NodeNum fromNode, std::string cmd, uint8_t replyChannel, NodeNum replyDest, float rxSnr, uint8_t hops)
{
    while (!cmd.empty() && (cmd.front() == ' ' || cmd.front() == '\'' || cmd.front() == '"' || cmd.front() == '\t')) {
        cmd.erase(0, 1);
    }
    while (!cmd.empty() && (cmd.back() == ' ' || cmd.back() == '\'' || cmd.back() == '"' || cmd.back() == '\r' || cmd.back() == '\n' || cmd.back() == '\t')) {
        cmd.pop_back();
    }

    for (size_t i = 0; i < cmd.length() && i < 15; i++) {
        if (cmd[i] == '"' || cmd[i] == '\'') break;
        cmd[i] = tolower(cmd[i]);
    }
    
    bool isDirected = false;

    // 1. Filtrado dinámico individual (!ID)
    if (cmd.rfind("!", 0) == 0) {
        size_t spacePos = cmd.find(" ");
        if (spacePos != std::string::npos) {
            std::string targetIdStr = cmd.substr(1, spacePos - 1);
            uint32_t targetId = strtoul(targetIdStr.c_str(), NULL, 16);
            if (targetId != nodeDB->getNodeNum()) {
                return;
            }
            isDirected = true;
            cmd = cmd.substr(spacePos + 1);
            while (!cmd.empty() && (cmd.front() == ' ' || cmd.front() == '\'' || cmd.front() == '"' || cmd.front() == '\t')) {
                cmd.erase(0, 1);
            }
        }
    }
    // 2. Filtrado dinámico por grupo (@r, @c, @a, @name:)
    else if (cmd.rfind("@", 0) == 0) {
        size_t spacePos = cmd.find(" ");
        if (spacePos != std::string::npos) {
            std::string group = cmd.substr(1, spacePos - 1);
            bool matchesGroup = false;
            
            if (group == "router" || group == "r") {
                matchesGroup = (config.device.role == meshtastic_Config_DeviceConfig_Role_ROUTER ||
                                config.device.role == meshtastic_Config_DeviceConfig_Role_ROUTER_LATE);
            } else if (group == "client" || group == "c") {
                matchesGroup = (config.device.role == meshtastic_Config_DeviceConfig_Role_CLIENT ||
                                config.device.role == meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE);
            } else if (group == "all" || group == "a") {
                matchesGroup = true;
            } else if (group.rfind("name:", 0) == 0) {
                std::string prefix = group.substr(5);
                matchesGroup = (strncasecmp(owner.long_name, prefix.c_str(), prefix.length()) == 0) ||
                               (strncasecmp(owner.short_name, prefix.c_str(), prefix.length()) == 0);
            }
            
            if (!matchesGroup) {
                return;
            }
            isDirected = true;
            cmd = cmd.substr(spacePos + 1);
            while (!cmd.empty() && (cmd.front() == ' ' || cmd.front() == '\'' || cmd.front() == '"' || cmd.front() == '\t')) {
                cmd.erase(0, 1);
            }
        }
    }

    // 3. Filtro de canal híbrido y gestión de flota
    if (replyChannel != 0) {
        bool isPrivateAdminChan = (replyChannel == prefs.cliChannelSlot && prefs.cliChannelSlot >= 2);
        
        if (isPrivateAdminChan) {
            // Canal Privado de Flota (Slot 2..7):
            // Comandos que por topología o seguridad nuclear exigen isDirected o DM
            bool individualOnly = (cmd.rfind("fav", 0) == 0 ||
                                  cmd.rfind("set_pos ", 0) == 0 ||
                                  cmd.rfind("set_name", 0) == 0 ||
                                  cmd.rfind("set_pin", 0) == 0 ||
                                  cmd == "pos_clear" ||
                                  cmd.rfind("ch_set", 0) == 0 ||
                                  cmd.rfind("ch_del", 0) == 0 ||
                                  cmd.rfind("set_cli_chan", 0) == 0 ||
                                  cmd == "ch_reset" ||
                                  cmd == "reboot" ||
                                  cmd == "factory_reset" ||
                                  cmd == "full_reset" ||
                                  cmd == "wipe" ||
                                  cmd == "keys_clear" ||
                                  cmd.rfind("set_preset", 0) == 0 ||
                                  cmd.rfind("set_lora", 0) == 0 ||
                                  cmd.rfind("set_freq", 0) == 0);
            if (individualOnly && !isDirected) {
                enqueueResponse(replyDest, replyChannel, "ERR: COMANDO INDIVIDUAL (USA !ID O DM)", true, false, hops);
                return;
            }
        } else {
            // Canal Público Navadmin (Slot 1 o canal abierto no-privado):
            if (!isDirected) {
                // Broadcast no dirigido: comandos ligeros de sondeo
                bool ligeroPermitido = (cmd == "ping" || cmd == "status" || cmd == "bat" ||
                                       cmd == "power" || cmd == "env" || cmd == "channel" ||
                                       cmd == "noise" || cmd.rfind("panic_ok", 0) == 0);
                if (!ligeroPermitido) {
                    // Silencio intencionado para evitar tormentas de radio masivas
                    return;
                }
            } else {
                // Broadcast dirigido con !ID o @grupo: permite diagnósticos y lecturas
                bool dirigidoPermitido = (cmd == "help" || cmd.rfind("help ", 0) == 0 ||
                                         cmd == "ping" || cmd == "status" || cmd == "bat" ||
                                         cmd == "power" || cmd == "env" || cmd == "channel" ||
                                         cmd == "noise" || cmd == "stats" || cmd.rfind("log", 0) == 0 ||
                                         cmd == "ch_ls" || cmd == "peers" || cmd == "rxlog" ||
                                         cmd == "afc" || cmd == "reset_reason" ||
                                         cmd.rfind("route", 0) == 0 || cmd.rfind("trace", 0) == 0 ||
                                         cmd.rfind("set_preset", 0) == 0 || cmd.rfind("set_lora", 0) == 0 ||
                                         cmd.rfind("set_freq", 0) == 0 || cmd.rfind("panic_ok", 0) == 0);
                if (!dirigidoPermitido) {
                    enqueueResponse(replyDest, replyChannel, "ERR: SOLO DM SEGURO", true, false, hops);
                    return;
                }
            }
        }
    }

    // Interrogacion generica: "/nava <cmd> ?" o "/nava <cmd> help"
    if (!(cmd.rfind("msg", 0) == 0)) {
        size_t sp = cmd.find_last_of(' ');
        if (sp != std::string::npos && sp + 1 < cmd.length()) {
            std::string last = cmd.substr(sp + 1);
            if (last == "?" || last == "help") {
                std::string base = cmd.substr(0, sp);
                size_t sp2 = base.find(' ');
                if (sp2 != std::string::npos) base = base.substr(0, sp2);
                enqueueResponse(replyDest, replyChannel, usageAndState(base), true);
                return;
            }
        }
    }

    // --- CONDICIONALES DE COMANDOS ---
    if (cmd == "help" || cmd.rfind("help ", 0) == 0) {
        std::string topic = (cmd.length() > 5) ? cmd.substr(5) : "";
        while (!topic.empty() && (topic.back() == ' ' || topic.back() == '\r' || topic.back() == '\n')) topic.pop_back();
        if (!topic.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState(topic), true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel,
                "CMDS:\n[Q] ping / status / env / channel / peers / bat / power\n[Q] rxlog / afc / reset_reason / noise / stats / log\n[E] ch_ls / ch_set / ch_del / ch_url / set_cli_chan / navadmin_mute / ch_reset\n[E] ch_mqtt / set_ok_to_mqtt / set_pos / set_pos_tx / set_nodeinfo_tx / set_telem_tx / pos_clear\n[E] set_preset / set_lora / set_freq / panic / panic_ok\n[E] set_beacon / mute / set_pin / test_tx / set_chem / set_vbat / set_vwake / storm / txoff / txon / ble\n[E] msg / bell / pos / nodeinfo / sendtel / fav / ign / db_purge / db_clear\n[E] set_name / set_role / set_mqtt / set_tz / set_hops / set_txpower\n[E] sleepmsg / reboot / factory_reset / full_reset / wipe / admin_ls / keys_ls / keys_clear\n\nAYUDA: /nava help <comando>\nDIR: ![ID] / @[r/c/a] / @name:[pref]", true, false, hops);
        }
    }
    else if (cmd == "ping") {
        auto it = lastPingTime.find(fromNode);
        if (it != lastPingTime.end() && (int32_t)(millis() - it->second) < 10000) {
            return;
        }
        lastPingTime[fromNode] = millis();

        char buf[160];
        uint32_t upSecs = millis() / 1000;
        uint32_t upD = upSecs / 86400;
        uint32_t upH = (upSecs % 86400) / 3600;
        int noiseFloor = 0;
        bool hasNoise = false;
        if (router && router->getInterface()) {
            RadioLibInterface* rLib = static_cast<RadioLibInterface*>(router->getInterface());
            if (rLib) {
                noiseFloor = rLib->getNoiseFloor();
                hasNoise = true;
            }
        }
        if (hasNoise) {
            snprintf(buf, sizeof(buf), "PONG: %s | SNR: %.1f dB | Bat: %d mV | UP: %lud %luh | RUIDO: %d dBm",
                     owner.short_name, rxSnr, powerStatus->getBatteryVoltageMv(),
                     (unsigned long)upD, (unsigned long)upH, noiseFloor);
        } else {
            snprintf(buf, sizeof(buf), "PONG: %s | SNR: %.1f dB | Bat: %d mV | UP: %lud %luh",
                     owner.short_name, rxSnr, powerStatus->getBatteryVoltageMv(),
                     (unsigned long)upD, (unsigned long)upH);
        }
        enqueueResponse(replyDest, replyChannel, buf, true, false, hops);
    }
    else if (cmd == "ch_ls") {
        std::string out = "CANALES (0-7):\n";
        for (uint8_t i = 0; i < MAX_NUM_CHANNELS; i++) {
            const meshtastic_Channel &ch = channels.getByIndex(i);
            char buf[64];
            const char *roleStr = (ch.role == meshtastic_Channel_Role_PRIMARY) ? "PRI" :
                                  (ch.role == meshtastic_Channel_Role_SECONDARY) ? "SEC" : "DIS";
            const char *activeMark = (i == prefs.cliChannelSlot) ? "*" : " ";
            
            std::string pskStr = "-";
            if (ch.has_settings && ch.role != meshtastic_Channel_Role_DISABLED) {
                if (ch.settings.psk.size == 0) {
                    pskStr = (ch.role == meshtastic_Channel_Role_SECONDARY) ? "DEF_PRI" : "NO_KEY";
                } else if (ch.settings.psk.size == 1) {
                    pskStr = "#" + std::to_string((int)ch.settings.psk.bytes[0]);
                } else if (ch.settings.psk.size == 16) {
                    pskStr = "AES128";
                } else if (ch.settings.psk.size == 32) {
                    pskStr = "AES256";
                } else {
                    pskStr = std::to_string(ch.settings.psk.size) + "B";
                }
            }
            const char *name = (ch.has_settings && ch.role != meshtastic_Channel_Role_DISABLED) ? channels.getName(i) : "-";
            char mqttStr[8] = "-";
            if (ch.has_settings && ch.role != meshtastic_Channel_Role_DISABLED) {
                if (ch.settings.uplink_enabled && ch.settings.downlink_enabled) strcpy(mqttStr, "U/D");
                else if (ch.settings.uplink_enabled) strcpy(mqttStr, "U");
                else if (ch.settings.downlink_enabled) strcpy(mqttStr, "D");
            }
            snprintf(buf, sizeof(buf), "[%d]%s%s %s (%s) %s\n", i, activeMark, roleStr, name, pskStr.c_str(), mqttStr);
            out += buf;
        }
        char tail[80];
        snprintf(tail, sizeof(tail), "CLI: Slot %d | Navadmin: %s", prefs.cliChannelSlot, prefs.navadminMuted ? "MUTED" : "ACTIVO");
        out += tail;
        enqueueResponse(replyDest, replyChannel, out, true, false, hops);
    }
    else if (cmd.rfind("ch_set", 0) == 0) {
        std::string arg = (cmd.length() > 6) ? cmd.substr(6) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("ch_set"), true, false, hops);
            return;
        }
        size_t sp1 = arg.find(' ');
        if (sp1 == std::string::npos) {
            enqueueResponse(replyDest, replyChannel, "ERR: USO: ch_set <slot 0|2-7> <nombre> <psk_base64>", true, false, hops);
            return;
        }
        int slot = atoi(arg.substr(0, sp1).c_str());
        if (slot != 0 && (slot < 2 || slot > 7)) {
            enqueueResponse(replyDest, replyChannel, "ERR: SLOT INVALIDO (0=Primario, 2-7=Secundarios; 1 Navadmin protegido)", true, false, hops);
            return;
        }
        std::string rest = arg.substr(sp1 + 1);
        while (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
        size_t sp2 = rest.find(' ');
        if (sp2 == std::string::npos) {
            enqueueResponse(replyDest, replyChannel, "ERR: FALTA CLAVE PSK. USO: ch_set <slot> <nombre> <psk_base64>", true, false, hops);
            return;
        }
        std::string chName = rest.substr(0, sp2);
        if (chName.length() > 11) {
            enqueueResponse(replyDest, replyChannel, "ERR: NOMBRE MAX 11 CARACTERES", true, false, hops);
            return;
        }
        std::string pskB64 = rest.substr(sp2 + 1);
        while (!pskB64.empty() && pskB64.front() == ' ') pskB64.erase(0, 1);
        while (!pskB64.empty() && (pskB64.back() == ' ' || pskB64.back() == '\r' || pskB64.back() == '\n')) pskB64.pop_back();

        uint8_t pskBytes[32];
        size_t pskLen = 0;
        if (!base64Decode(pskB64, pskBytes, pskLen, sizeof(pskBytes)) || (pskLen != 1 && pskLen != 16 && pskLen != 32)) {
            enqueueResponse(replyDest, replyChannel, "ERR: CLAVE BASE64 INVALIDA (debe ser 1, 16 o 32 bytes)", true, false, hops);
            return;
        }

        if (slot == 0) {
            meshtastic_Channel ch0 = channels.getByIndex(0);
            ch0.role = meshtastic_Channel_Role_PRIMARY;
            ch0.has_settings = true;
            strncpy(ch0.settings.name, chName.c_str(), sizeof(ch0.settings.name) - 1);
            ch0.settings.name[sizeof(ch0.settings.name) - 1] = '\0';
            ch0.settings.psk.size = pskLen;
            memcpy(ch0.settings.psk.bytes, pskBytes, pskLen);
            channels.setChannel(ch0);
            channels.onConfigChanged();
            nodeDB->saveToDisk(SEGMENT_CHANNELS);

            strncpy(prefs.ch0_name, chName.c_str(), sizeof(prefs.ch0_name) - 1);
            prefs.ch0_name[sizeof(prefs.ch0_name) - 1] = '\0';
            memcpy(prefs.ch0_psk, pskBytes, pskLen);
            prefs.ch0_psk_len = pskLen;
            prefs.ch0_configured = 1;
            saveResiliencePrefs();

            logEvent("CH_SET slot 0 %s", chName.c_str());
            const char *tStr = (pskLen == 1) ? "#1" : (pskLen == 16) ? "AES128" : "AES256";
            char respBuf[100];
            snprintf(respBuf, sizeof(respBuf), "OK: CANAL 0 \"%s\" ACTUALIZADO (%s)", chName.c_str(), tStr);
            enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);
            return;
        }

        meshtastic_Channel ch = meshtastic_Channel_init_zero;
        ch.index = slot;
        ch.role = meshtastic_Channel_Role_SECONDARY;
        ch.has_settings = true;
        strncpy(ch.settings.name, chName.c_str(), sizeof(ch.settings.name) - 1);
        ch.settings.psk.size = pskLen;
        memcpy(ch.settings.psk.bytes, pskBytes, pskLen);
        ch.settings.uplink_enabled = true;
        ch.settings.downlink_enabled = true;
        ch.settings.has_module_settings = true;
        channels.setChannel(ch);
        channels.onConfigChanged();
        nodeDB->saveToDisk(SEGMENT_CHANNELS);

        ResilientChannel &rc = prefs.customChannels[slot - 2];
        strncpy(rc.name, chName.c_str(), 11);
        rc.name[11] = '\0';
        rc.psk_len = pskLen;
        memcpy(rc.psk, pskBytes, pskLen);
        rc.uplink_enabled = 1;
        rc.downlink_enabled = 1;
        rc.is_active = 1;
        saveResiliencePrefs();

        logEvent("CH_SET slot %d %s", slot, chName.c_str());
        const char *tStr = (pskLen == 1) ? "#1" : (pskLen == 16) ? "AES128" : "AES256";
        char respBuf[100];
        snprintf(respBuf, sizeof(respBuf), "OK: CANAL %d \"%s\" CREADO (%s)", slot, chName.c_str(), tStr);
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);
    }
    else if (cmd.rfind("ch_del", 0) == 0) {
        std::string arg = (cmd.length() > 6) ? cmd.substr(6) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("ch_del"), true, false, hops);
            return;
        }
        int slot = atoi(arg.c_str());
        if (slot < 2 || slot > 7) {
            enqueueResponse(replyDest, replyChannel, "ERR: SLOT INVALIDO (SOLO 2-7)", true, false, hops);
            return;
        }
        if (prefs.cliChannelSlot == slot) {
            prefs.cliChannelSlot = 1;
        }
        meshtastic_Channel ch = meshtastic_Channel_init_zero;
        ch.index = slot;
        ch.role = meshtastic_Channel_Role_DISABLED;
        channels.setChannel(ch);
        channels.onConfigChanged();
        nodeDB->saveToDisk(SEGMENT_CHANNELS);

        memset(&prefs.customChannels[slot - 2], 0, sizeof(ResilientChannel));
        saveResiliencePrefs();

        logEvent("CH_DEL slot %d", slot);
        char respBuf[60];
        snprintf(respBuf, sizeof(respBuf), "OK: CANAL %d DESHABILITADO", slot);
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);
    }
    else if (cmd.rfind("ch_url", 0) == 0) {
        std::string arg = (cmd.length() > 6) ? cmd.substr(6) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        int slot = 0;
        if (!arg.empty()) {
            slot = atoi(arg.c_str());
            if (slot < 0 || slot > 7) {
                enqueueResponse(replyDest, replyChannel, "ERR: SLOT INVALIDO (0-7)", true, false, hops);
                return;
            }
        }
        std::string url = generateChannelUrl(slot);
        enqueueResponse(replyDest, replyChannel, url, true, false, hops);
    }
    else if (cmd.rfind("set_cli_chan", 0) == 0) {
        std::string arg = (cmd.length() > 12) ? cmd.substr(12) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_cli_chan"), true, false, hops);
            return;
        }
        int slot = atoi(arg.c_str());
        if (slot < 1 || slot > 7) {
            enqueueResponse(replyDest, replyChannel, "ERR: SLOT INVALIDO (1-7)", true, false, hops);
            return;
        }
        if (slot > 1) {
            const meshtastic_Channel &ch = channels.getByIndex(slot);
            if (!ch.has_settings || ch.role == meshtastic_Channel_Role_DISABLED) {
                enqueueResponse(replyDest, replyChannel, "ERR: EL CANAL INDICADO NO ESTA ACTIVO", true, false, hops);
                return;
            }
        }
        prefs.cliChannelSlot = slot;
        saveResiliencePrefs();
        logEvent("CLI_CHAN -> slot %d", slot);
        char respBuf[80];
        snprintf(respBuf, sizeof(respBuf), "OK: NAVACLI ASIGNADO AL SLOT %d (%s)", slot, channels.getName(slot));
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);
    }
    else if (cmd.rfind("navadmin_mute", 0) == 0) {
        std::string arg = (cmd.length() > 13) ? cmd.substr(13) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("navadmin_mute"), true, false, hops);
            return;
        }
        if (arg == "on" || arg == "1") {
            prefs.navadminMuted = 1;
            saveResiliencePrefs();
            logEvent("NAVADMIN MUTE ON");
            enqueueResponse(replyDest, replyChannel, "OK: NAVADMIN (CANAL 1) SILENCIADO", true, false, hops);
        } else if (arg == "off" || arg == "0") {
            prefs.navadminMuted = 0;
            saveResiliencePrefs();
            logEvent("NAVADMIN MUTE OFF");
            enqueueResponse(replyDest, replyChannel, "OK: NAVADMIN (CANAL 1) ACTIVO", true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: USO: navadmin_mute [on|off]", true, false, hops);
        }
    }
    else if (cmd == "ch_reset") {
        for (uint8_t i = 2; i < MAX_NUM_CHANNELS; i++) {
            meshtastic_Channel ch = meshtastic_Channel_init_zero;
            ch.index = i;
            ch.role = meshtastic_Channel_Role_DISABLED;
            channels.setChannel(ch);
        }
        meshtastic_Channel ch1 = channels.getByIndex(1);
        ch1.role = meshtastic_Channel_Role_SECONDARY;
        ch1.has_settings = true;
        strcpy(ch1.settings.name, "Navadmin");
        ch1.settings.psk.size = 1;
        ch1.settings.psk.bytes[0] = 0x01;
        ch1.settings.uplink_enabled = true;
        ch1.settings.downlink_enabled = true;
        ch1.settings.has_module_settings = true;
        channels.setChannel(ch1);
        channels.onConfigChanged();
        nodeDB->saveToDisk(SEGMENT_CHANNELS);

        prefs.cliChannelSlot = 1;
        prefs.navadminMuted = 0;
        memset(prefs.customChannels, 0, sizeof(prefs.customChannels));
        saveResiliencePrefs();

        logEvent("CH_RESET de fabrica");
        enqueueResponse(replyDest, replyChannel, "OK: CANALES RESTAURADOS A FABRICA (Navadmin Slot 1)", true, false, hops);
    }
    else if (cmd.rfind("ch_mqtt", 0) == 0) {
        std::string arg = (cmd.length() > 7) ? cmd.substr(7) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("ch_mqtt"), true, false, hops);
            return;
        }
        size_t sp = arg.find(' ');
        if (sp == std::string::npos) {
            int slot = atoi(arg.c_str());
            if (slot < 0 || slot > 7) {
                enqueueResponse(replyDest, replyChannel, "ERR: SLOT INVALIDO (0-7)", true, false, hops);
                return;
            }
            const meshtastic_Channel &ch = channels.getByIndex(slot);
            char buf[80];
            snprintf(buf, sizeof(buf), "MQTT CANAL %d: UP=%d DOWN=%d", slot, ch.settings.uplink_enabled, ch.settings.downlink_enabled);
            enqueueResponse(replyDest, replyChannel, buf, true, false, hops);
            return;
        }
        int slot = atoi(arg.substr(0, sp).c_str());
        if (slot < 0 || slot > 7) {
            enqueueResponse(replyDest, replyChannel, "ERR: SLOT INVALIDO (0-7)", true, false, hops);
            return;
        }
        std::string mode = arg.substr(sp + 1);
        while (!mode.empty() && mode.front() == ' ') mode.erase(0, 1);
        while (!mode.empty() && (mode.back() == ' ' || mode.back() == '\r' || mode.back() == '\n')) mode.pop_back();

        meshtastic_Channel ch = channels.getByIndex(slot);
        if (!ch.has_settings || ch.role == meshtastic_Channel_Role_DISABLED) {
            enqueueResponse(replyDest, replyChannel, "ERR: EL CANAL NO ESTA ACTIVO", true, false, hops);
            return;
        }
        if (mode == "up") {
            ch.settings.uplink_enabled = true;
            ch.settings.downlink_enabled = false;
        } else if (mode == "down") {
            ch.settings.uplink_enabled = false;
            ch.settings.downlink_enabled = true;
        } else if (mode == "both") {
            ch.settings.uplink_enabled = true;
            ch.settings.downlink_enabled = true;
        } else if (mode == "off") {
            ch.settings.uplink_enabled = false;
            ch.settings.downlink_enabled = false;
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: MODO INVALIDO (up|down|both|off)", true, false, hops);
            return;
        }
        channels.setChannel(ch);
        channels.onConfigChanged();
        nodeDB->saveToDisk(SEGMENT_CHANNELS);

        if (slot >= 2) {
            prefs.customChannels[slot - 2].uplink_enabled = ch.settings.uplink_enabled ? 1 : 0;
            prefs.customChannels[slot - 2].downlink_enabled = ch.settings.downlink_enabled ? 1 : 0;
            saveResiliencePrefs();
        }
        char respBuf[60];
        snprintf(respBuf, sizeof(respBuf), "OK: MQTT CANAL %d -> %s", slot, mode.c_str());
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);
    }
    else if (cmd.rfind("set_ok_to_mqtt", 0) == 0) {
        std::string arg = (cmd.length() > 14) ? cmd.substr(14) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_ok_to_mqtt"), true, false, hops);
            return;
        }
        if (arg == "on" || arg == "1") {
            config.lora.config_ok_to_mqtt = true;
            prefs.ok_to_mqtt = 1;
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: OK_TO_MQTT ON (Persiste)", true, false, hops);
        } else if (arg == "off" || arg == "0") {
            config.lora.config_ok_to_mqtt = false;
            prefs.ok_to_mqtt = 2;
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: OK_TO_MQTT OFF (Persiste)", true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: USO: set_ok_to_mqtt [on|off]", true, false, hops);
        }
    }
    else if (cmd.rfind("set_pos", 0) == 0) {
        std::string arg = (cmd.length() > 7) ? cmd.substr(7) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_pos"), true, false, hops);
            return;
        }
        float lat = 0.0f, lon = 0.0f;
        int alt = 0;
        if (sscanf(arg.c_str(), "%f %f %d", &lat, &lon, &alt) < 2) {
            enqueueResponse(replyDest, replyChannel, "ERR: USO: set_pos <lat> <lon> [alt]", true, false, hops);
            return;
        }
        config.position.fixed_position = true;
        meshtastic_Position pos = meshtastic_Position_init_zero;
        pos.latitude_i = (int32_t)(lat * 1e7f);
        pos.longitude_i = (int32_t)(lon * 1e7f);
        pos.altitude = alt;
        pos.time = getValidTime(RTCQualityFromNet);
        nodeDB->setLocalPosition(pos);

        prefs.fixed_pos_lat = pos.latitude_i;
        prefs.fixed_pos_lon = pos.longitude_i;
        prefs.fixed_pos_alt = alt;
        nodeDB->saveToDisk(SEGMENT_CONFIG | SEGMENT_NODEDATABASE);
        saveResiliencePrefs();

        if (positionModule) {
            positionModule->sendOurPosition(NODENUM_BROADCAST, false);
        }

        logEvent("SET_POS Lat:%.4f Lon:%.4f", lat, lon);
        char respBuf[100];
        snprintf(respBuf, sizeof(respBuf), "OK: POSICION FIJADA (Lat: %.5f, Lon: %.5f, Alt: %dm)", lat, lon, alt);
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);
    }
    else if (cmd == "pos_clear") {
        prefs.fixed_pos_enabled = 0;
        prefs.fixed_pos_lat = 0;
        prefs.fixed_pos_lon = 0;
        prefs.fixed_pos_alt = 0;
        config.position.fixed_position = false;
        nodeDB->clearLocalPosition();
        nodeDB->saveToDisk(SEGMENT_CONFIG | SEGMENT_NODEDATABASE);
        saveResiliencePrefs();
        logEvent("POS_CLEAR ejecutado");
        enqueueResponse(replyDest, replyChannel, "OK: POSICION FIJA BORRADA", true, false, hops);
    }
    else if (cmd.rfind("set_pos_tx", 0) == 0) {
        std::string arg = (cmd.length() > 10) ? cmd.substr(10) : "";
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) arg.erase(0, 1);
        if (arg == "off" || arg == "0") {
            prefs.pos_tx_secs = 0;
            config.position.position_broadcast_secs = 0;
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: DIFUSION DE POSICION DESACTIVADA (OFF)", true, false, hops);
        } else if (arg == "on" || arg == "1") {
            prefs.pos_tx_secs = 259200;
            config.position.position_broadcast_secs = 259200;
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: DIFUSION DE POSICION ACTIVADA (cada 72h / 259200s)", true, false, hops);
        } else if (!arg.empty()) {
            uint32_t mins = strtoul(arg.c_str(), NULL, 10);
            if (mins >= 1 && mins <= 10080) {
                prefs.pos_tx_secs = mins * 60;
                config.position.position_broadcast_secs = prefs.pos_tx_secs;
                nodeDB->saveToDisk(SEGMENT_CONFIG);
                saveResiliencePrefs();
                char bBuf[100];
                snprintf(bBuf, sizeof(bBuf), "OK: DIFUSION DE POSICION CADA %u min (Persiste)", (unsigned int)mins);
                enqueueResponse(replyDest, replyChannel, bBuf, true, false, hops);
            } else {
                enqueueResponse(replyDest, replyChannel, usageAndState("set_pos_tx"), true, false, hops);
            }
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_pos_tx"), true, false, hops);
        }
    }
    else if (cmd.rfind("set_nodeinfo_tx", 0) == 0) {
        std::string arg = (cmd.length() > 15) ? cmd.substr(15) : "";
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) arg.erase(0, 1);
        if (arg == "off" || arg == "0") {
            prefs.nodeinfo_tx_secs = 0;
            config.device.node_info_broadcast_secs = 0;
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: DIFUSION DE NODEINFO DESACTIVADA (OFF)", true, false, hops);
        } else if (arg == "on" || arg == "1") {
            prefs.nodeinfo_tx_secs = 259200;
            config.device.node_info_broadcast_secs = 259200;
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: DIFUSION DE NODEINFO ACTIVADA (cada 72h / 259200s)", true, false, hops);
        } else if (!arg.empty()) {
            uint32_t mins = strtoul(arg.c_str(), NULL, 10);
            if (mins >= 1 && mins <= 10080) {
                prefs.nodeinfo_tx_secs = mins * 60;
                config.device.node_info_broadcast_secs = prefs.nodeinfo_tx_secs;
                nodeDB->saveToDisk(SEGMENT_CONFIG);
                saveResiliencePrefs();
                char bBuf[100];
                snprintf(bBuf, sizeof(bBuf), "OK: DIFUSION DE NODEINFO CADA %u min (Persiste)", (unsigned int)mins);
                enqueueResponse(replyDest, replyChannel, bBuf, true, false, hops);
            } else {
                enqueueResponse(replyDest, replyChannel, usageAndState("set_nodeinfo_tx"), true, false, hops);
            }
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_nodeinfo_tx"), true, false, hops);
        }
    }
    else if (cmd.rfind("set_telem_tx", 0) == 0) {
        std::string arg = (cmd.length() > 12) ? cmd.substr(12) : "";
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) arg.erase(0, 1);
        if (arg == "off" || arg == "0") {
            prefs.telem_tx_secs = 0;
            moduleConfig.telemetry.device_update_interval = 0;
            moduleConfig.telemetry.environment_update_interval = 0;
            moduleConfig.telemetry.power_update_interval = 0;
            moduleConfig.telemetry.air_quality_interval = 0;
            moduleConfig.telemetry.health_update_interval = 0;
            nodeDB->saveToDisk(SEGMENT_MODULECONFIG);
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: REPORTE DE TELEMETRIA DESACTIVADO (OFF)", true, false, hops);
        } else if (arg == "on" || arg == "1") {
            prefs.telem_tx_secs = 43200; // Default V5: 12 horas (43200s)
            moduleConfig.telemetry.device_update_interval = 43200;
            moduleConfig.telemetry.environment_update_interval = 43200;
            moduleConfig.telemetry.power_update_interval = 43200;
            moduleConfig.telemetry.air_quality_interval = 43200;
            moduleConfig.telemetry.health_update_interval = 43200;
            nodeDB->saveToDisk(SEGMENT_MODULECONFIG);
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: REPORTE DE TELEMETRIA ACTIVADO (cada 12h)", true, false, hops);
        } else if (!arg.empty()) {
            uint32_t mins = strtoul(arg.c_str(), NULL, 10);
            if (mins >= 1 && mins <= 1440) {
                prefs.telem_tx_secs = mins * 60;
                moduleConfig.telemetry.device_update_interval = prefs.telem_tx_secs;
                moduleConfig.telemetry.environment_update_interval = prefs.telem_tx_secs;
                moduleConfig.telemetry.power_update_interval = prefs.telem_tx_secs;
                moduleConfig.telemetry.air_quality_interval = prefs.telem_tx_secs;
                moduleConfig.telemetry.health_update_interval = prefs.telem_tx_secs;
                nodeDB->saveToDisk(SEGMENT_MODULECONFIG);
                saveResiliencePrefs();
                char bBuf[100];
                snprintf(bBuf, sizeof(bBuf), "OK: REPORTE DE TELEMETRIA CADA %u min (Persiste)", (unsigned int)mins);
                enqueueResponse(replyDest, replyChannel, bBuf, true, false, hops);
            } else {
                enqueueResponse(replyDest, replyChannel, usageAndState("set_telem_tx"), true, false, hops);
            }
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_telem_tx"), true, false, hops);
        }
    }
    else if (cmd.rfind("set_preset", 0) == 0) {
        std::string arg = (cmd.length() > 10) ? cmd.substr(10) : "";
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_preset"), true, false, hops);
            return;
        }
        meshtastic_Config_LoRaConfig_ModemPreset preset;
        if (arg == "long_fast" || arg == "lf") preset = meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST;
        else if (arg == "long_slow" || arg == "ls") preset = meshtastic_Config_LoRaConfig_ModemPreset_LONG_SLOW;
        else if (arg == "medium_fast" || arg == "mf") preset = meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST;
        else if (arg == "medium_slow" || arg == "ms") preset = meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_SLOW;
        else if (arg == "short_fast" || arg == "sf") preset = meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST;
        else if (arg == "short_slow" || arg == "ss") preset = meshtastic_Config_LoRaConfig_ModemPreset_SHORT_SLOW;
        else if (arg == "long_moderate" || arg == "lm") preset = meshtastic_Config_LoRaConfig_ModemPreset_LONG_MODERATE;
        else if (arg == "short_turbo" || arg == "st") preset = meshtastic_Config_LoRaConfig_ModemPreset_SHORT_TURBO;
        else {
            enqueueResponse(replyDest, replyChannel, "ERR: PRESET INVALIDO (long_fast, medium_fast, short_fast, etc.)", true, false, hops);
            return;
        }

        config.lora.use_preset = true;
        config.lora.modem_preset = preset;
        config.lora.override_frequency = 0.0f;
        nodeDB->saveToDisk(SEGMENT_CONFIG);

        prefs.lora_use_preset = 1;
        prefs.lora_modem_preset = (uint8_t)preset;
        prefs.lora_override_frequency = 0.0f;
        prefs.lora_configured = 1;
        saveResiliencePrefs();

        logEvent("SET_PRESET %s", arg.c_str());
        char respBuf[100];
        snprintf(respBuf, sizeof(respBuf), "OK: PRESET %s APLICADO (Reinicio diferido)", arg.c_str());
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);

        deferredAction = NAVA_DEFERRED_LORA_CHANGE;
        preRebootArmed = false;
    }
    else if (cmd.rfind("set_lora", 0) == 0) {
        std::string arg = (cmd.length() > 8) ? cmd.substr(8) : "";
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_lora"), true, false, hops);
            return;
        }
        uint32_t bw = 0, sf = 0, cr = 0, slot = 0, pwr = 0;
        float freq = 0.0f;
        int n = sscanf(arg.c_str(), "%u %u %u %f %u %u", &bw, &sf, &cr, &freq, &slot, &pwr);
        if (n < 5) {
            enqueueResponse(replyDest, replyChannel, "ERR: USO: set_lora <bw 31-500> <sf 5-12> <cr 4-8> <freq_mhz> <slot> [txpower]", true, false, hops);
            return;
        }
        if (sf < 5 || sf > 12 || cr < 4 || cr > 8 || freq < 400.0f || freq > 950.0f) {
            enqueueResponse(replyDest, replyChannel, "ERR: PARAMETROS LORA FUERA DE RANGO", true, false, hops);
            return;
        }
        config.lora.use_preset = false;
        config.lora.bandwidth = bw;
        config.lora.spread_factor = sf;
        config.lora.coding_rate = cr;
        config.lora.override_frequency = freq;
        config.lora.channel_num = slot;
        if (n >= 6 && pwr > 0) {
            config.lora.tx_power = pwr;
            prefs.lora_tx_power = pwr;
        }
        nodeDB->saveToDisk(SEGMENT_CONFIG);

        prefs.lora_use_preset = 0;
        prefs.lora_bandwidth = bw;
        prefs.lora_spread_factor = sf;
        prefs.lora_coding_rate = cr;
        prefs.lora_override_frequency = freq;
        prefs.lora_channel_num = slot;
        prefs.lora_configured = 1;
        saveResiliencePrefs();

        logEvent("SET_LORA SF%u BW%u", sf, bw);
        char respBuf[120];
        snprintf(respBuf, sizeof(respBuf), "OK: CAPA LORA ACTUALIZADA (BW:%u SF:%u CR:%u Freq:%.4f Slot:%u). Reinicio diferido", bw, sf, cr, freq, slot);
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);

        deferredAction = NAVA_DEFERRED_LORA_CHANGE;
        preRebootArmed = false;
    }
    else if (cmd.rfind("set_freq", 0) == 0) {
        std::string arg = (cmd.length() > 8) ? cmd.substr(8) : "";
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_freq"), true, false, hops);
            return;
        }
        float freq = 0.0f;
        uint32_t slot = config.lora.channel_num;
        int n = sscanf(arg.c_str(), "%f %u", &freq, &slot);
        if (n < 1 || freq < 400.0f || freq > 950.0f) {
            enqueueResponse(replyDest, replyChannel, "ERR: FRECUENCIA INVALIDA (400.0 - 950.0 MHz). USO: set_freq <freq_mhz> [slot]", true, false, hops);
            return;
        }
        config.lora.override_frequency = freq;
        config.lora.channel_num = slot;
        nodeDB->saveToDisk(SEGMENT_CONFIG);

        prefs.lora_override_frequency = freq;
        prefs.lora_channel_num = slot;
        prefs.lora_configured = 1;
        saveResiliencePrefs();

        logEvent("SET_FREQ %.4f MHz", freq);
        char respBuf[100];
        snprintf(respBuf, sizeof(respBuf), "OK: FRECUENCIA APLICADA (%.4f MHz Slot %u). Reinicio diferido", freq, slot);
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);

        deferredAction = NAVA_DEFERRED_LORA_CHANGE;
        preRebootArmed = false;
    }
    else if (cmd.rfind("panic_ok", 0) == 0) {
        cancelPanicRollback();
        emitPanicOkPulse();
        enqueueResponse(replyDest, replyChannel, "OK: SALTO DE PANICO CONSOLIDADO. ROLLBACK CANCELADO EN LA RED.", true, false, hops);
    }
    else if (cmd.rfind("panic", 0) == 0) {
        std::string arg = (cmd.length() > 5) ? cmd.substr(5) : "";
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("panic"), true, false, hops);
            return;
        }
        char targetStr[40] = {0};
        uint32_t mins = 10;
        uint32_t rollbackMins = 0;
        int n = sscanf(arg.c_str(), "%39s %u %u", targetStr, &mins, &rollbackMins);
        if (n < 1) {
            enqueueResponse(replyDest, replyChannel, "ERR: USO: panic <preset|params> [minutos=10] [rollback_mins=0]", true, false, hops);
            return;
        }
        if (mins < 2 || mins > 120) mins = 10;

        NavaPanicPulse pulse;
        memset(&pulse, 0, sizeof(pulse));
        memcpy(pulse.magic, "PANC", 4);
        pulse.session_id = ((uint32_t)rand() << 16) ^ (uint32_t)millis() ^ nodeDB->getNodeNum();
        pulse.remaining_seconds = mins * 60;
        pulse.rollback_minutes = rollbackMins;
        pulse.sender_nodenum = nodeDB->getNodeNum();

        std::string tName(targetStr);
        if (tName == "sfnarrow") {
            pulse.use_preset = 0;
            pulse.modem_preset = 0;
            pulse.sf = 10;
            pulse.cr = 8;
            pulse.bw_code = 62;
            pulse.channel_slot = 0;
            pulse.freq_mhz = 869.525f;
        } else if (tName == "long_fast" || tName == "lf") {
            pulse.use_preset = 1;
            pulse.modem_preset = meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST;
            pulse.sf = 0; pulse.cr = 0; pulse.bw_code = 0; pulse.channel_slot = 0; pulse.freq_mhz = 0.0f;
        } else if (tName == "medium_fast" || tName == "mf") {
            pulse.use_preset = 1;
            pulse.modem_preset = meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST;
            pulse.sf = 0; pulse.cr = 0; pulse.bw_code = 0; pulse.channel_slot = 0; pulse.freq_mhz = 0.0f;
        } else if (tName == "short_fast" || tName == "sf") {
            pulse.use_preset = 1;
            pulse.modem_preset = meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST;
            pulse.sf = 0; pulse.cr = 0; pulse.bw_code = 0; pulse.channel_slot = 0; pulse.freq_mhz = 0.0f;
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: PRESET DESTINO DE PANICO INVALIDO", true, false, hops);
            return;
        }

        startPanic(pulse);
        char respBuf[120];
        snprintf(respBuf, sizeof(respBuf), "OK: PROTOCOLO DE PANICO INICIADO. EVACUACION EN %u MINUTOS...", (unsigned int)mins);
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);
    }
    else if (cmd.rfind("set_beacon", 0) == 0) {
        std::string arg = (cmd.length() > 10) ? cmd.substr(10) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_beacon"), true, false, hops);
            return;
        }
        uint32_t mins = strtoul(arg.c_str(), NULL, 10);
        if (mins < 1 || mins > 1440) {
            enqueueResponse(replyDest, replyChannel, "ERR: MINUTOS INVALIDOS (1-1440)", true, false, hops);
            return;
        }
        config.device.node_info_broadcast_secs = mins * 60;
        config.position.position_broadcast_secs = mins * 60;
        prefs.beacon_interval_secs = mins * 60;
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        saveResiliencePrefs();

        char respBuf[80];
        snprintf(respBuf, sizeof(respBuf), "OK: BALIZA CONFIGURADA CADA %lu MINUTOS", (unsigned long)mins);
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);
    }
    else if (cmd.rfind("mute", 0) == 0) {
        std::string arg = (cmd.length() > 4) ? cmd.substr(4) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty() || arg == "off" || arg == "0") {
            muteUntilMs = 0;
            logEvent("MUTE OFF");
            enqueueResponse(replyDest, replyChannel, "OK: MUTE DESACTIVADO (Servicio Normal)", true, false, hops);
            return;
        }
        uint32_t mins = strtoul(arg.c_str(), NULL, 10);
        if (mins < 1 || mins > 720) {
            enqueueResponse(replyDest, replyChannel, "ERR: MINUTOS INVALIDOS (1-720)", true, false, hops);
            return;
        }
        muteUntilMs = millis() + (mins * 60000);
        logEvent("MUTE ON %lu min", (unsigned long)mins);
        char respBuf[80];
        snprintf(respBuf, sizeof(respBuf), "OK: REPETIDOR EN MUTE TEMPORAL POR %lu MINUTOS (RAM)", (unsigned long)mins);
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);
    }
    else if (cmd.rfind("set_pin", 0) == 0) {
        std::string arg = (cmd.length() > 7) ? cmd.substr(7) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_pin"), true, false, hops);
            return;
        }
        uint32_t pin = strtoul(arg.c_str(), NULL, 10);
        if (pin < 100000 || pin > 999999) {
            enqueueResponse(replyDest, replyChannel, "ERR: EL PIN DEBE TENER 6 DIGITOS (100000-999999)", true, false, hops);
            return;
        }
        config.bluetooth.fixed_pin = pin;
        prefs.fixed_pin = pin;
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        saveResiliencePrefs();

        logEvent("SET_PIN cambiado");
        char respBuf[80];
        snprintf(respBuf, sizeof(respBuf), "OK: PIN BT CAMBIADO A %lu (Persiste)", (unsigned long)pin);
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);
    }
    else if (cmd == "stats") {
        char buf[220];
        float curTemp = 0.0f;
        #ifdef NRF52840_XXAA
        int32_t tRaw = 0;
        if (sd_temp_get(&tRaw) == NRF_SUCCESS) curTemp = tRaw / 4.0f;
        #endif
        uint16_t curBat = powerStatus->getBatteryVoltageMv();
        float minT = (statsMinTemp < 500.0f) ? statsMinTemp : curTemp;
        float maxT = (statsMaxTemp > -500.0f) ? statsMaxTemp : curTemp;
        uint16_t minB = (statsMinBattMv < 60000) ? statsMinBattMv : curBat;

        snprintf(buf, sizeof(buf),
            "STATS (RAM):\nCPU: min %.1fC / max %.1fC (act %.1fC)\nBat Min: %dmV (act %dmV)\nPkts: RX %lu / TX %lu / Rout %lu\nAutoFav: %d activos",
            minT, maxT, curTemp, minB, curBat,
            (unsigned long)statsRxPackets, (unsigned long)statsTxPackets, (unsigned long)statsRoutedPackets,
            prefs.autoFavCount);
        enqueueResponse(replyDest, replyChannel, buf, true, false, hops);
    }
    else if (cmd.rfind("test_tx", 0) == 0) {
        std::string arg = (cmd.length() > 7) ? cmd.substr(7) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        uint8_t secs = 10;
        if (!arg.empty()) {
            secs = (uint8_t)atoi(arg.c_str());
            if (secs < 5) secs = 5;
            if (secs > 30) secs = 30;
        }
        testTxCountRemaining = secs;
        testTxNextMs = millis();
        logEvent("TEST_TX %ds", (int)secs);
        char respBuf[60];
        snprintf(respBuf, sizeof(respBuf), "OK: TEST TX INICIADO (%ds a 1 pkt/s)", (int)secs);
        enqueueResponse(replyDest, replyChannel, respBuf, true, false, hops);
    }
    else if (cmd == "log" || cmd.rfind("log ", 0) == 0) {
        std::string arg = (cmd.length() > 3) ? cmd.substr(3) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        uint8_t lines = 10;
        if (!arg.empty()) {
            lines = (uint8_t)atoi(arg.c_str());
            if (lines < 1) lines = 1;
            if (lines > 15) lines = 15;
        }
        if (ramLogCount == 0) {
            enqueueResponse(replyDest, replyChannel, "LOG RAM: VACIO", true, false, hops);
            return;
        }
        if (lines > ramLogCount) lines = ramLogCount;
        std::string out = "LOG EVENTOS RAM:\n";
        for (uint8_t i = 0; i < lines; i++) {
            uint8_t idx = (ramLogHead + 16 - lines + i) % 16;
            const NavaLogEntry &le = ramLogs[idx];
            uint32_t h = (le.uptime % 86400) / 3600;
            uint32_t m = (le.uptime % 3600) / 60;
            uint32_t s = le.uptime % 60;
            char lbuf[64];
            snprintf(lbuf, sizeof(lbuf), "[%02lu:%02lu:%02lu] %s\n", (unsigned long)h, (unsigned long)m, (unsigned long)s, le.msg);
            out += lbuf;
        }
        enqueueResponse(replyDest, replyChannel, out, true, false, hops);
    }
    else if (cmd == "power") {
        char buf[200];
        uint16_t adcV = powerStatus->getBatteryVoltageMv();
#if HAS_TELEMETRY && !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && __has_include(<Adafruit_INA219.h>)
        uint16_t inaMv = (ina219Sensor.hasSensor()) ? ina219Sensor.getBusVoltageMv() : 0;
        if (inaMv > 0) {
            int16_t inamA = ina219Sensor.getCurrentMa();
            float inaV = inaMv / 1000.0f;
            float inamW = inaV * inamA;
            const char *estado = (inamA > 1) ? "CARGANDO" : (inamA < -1) ? "DESCARGANDO" : "STANDBY";
            snprintf(buf, sizeof(buf), "POWER: ADC %u mV | INA219: %.2f V | %+d mA | %s | %.0f mW",
                     (unsigned int)adcV, inaV, (int)inamA, estado, inamW);
            enqueueResponse(replyDest, replyChannel, buf, true, false, hops);
            return;
        }
#endif
        snprintf(buf, sizeof(buf), "POWER: ADC %u mV | INA: NO DETECTADO (solo ADC)", (unsigned int)adcV);
        enqueueResponse(replyDest, replyChannel, buf, true, false, hops);
    }
    else if (cmd == "bat") {
        char buf[140];
        const char *qca = (prefs.chemistry == 1) ? "NIMH" : (prefs.chemistry == 2) ? "SODIUM" : (prefs.chemistry == 3) ? "LIFEPO4" : "LIPO";
        snprintf(buf, sizeof(buf), "QUIMICA: %s | Bat: %d mV | OCV: %d%% | TX: %s",
                 qca, powerStatus->getBatteryVoltageMv(), powerStatus->getBatteryChargePercent(),
                 config.lora.tx_enabled ? "ON" : "OFF");
        enqueueResponse(replyDest, replyChannel, buf, true, false, hops);
    }
    else if (cmd.rfind("fav", 0) == 0) {
        std::string sub = (cmd.length() > 3) ? cmd.substr(3) : "";
        while (!sub.empty() && sub.front() == ' ') sub.erase(0, 1);
        if (sub.rfind("auto", 0) == 0) {
            std::string autoArg = (sub.length() > 4) ? sub.substr(4) : "";
            while (!autoArg.empty() && autoArg.front() == ' ') autoArg.erase(0, 1);
            if (autoArg == "on" || autoArg == "1") {
                navaAutoFavoriteEnabled = true;
                prefs.auto_fav = 1;
                saveResiliencePrefs();
                enqueueResponse(replyDest, replyChannel, "OK: AUTO-FAV ACTIVADO", true, false, hops);
            } else if (autoArg == "off" || autoArg == "0") {
                navaAutoFavoriteEnabled = false;
                prefs.auto_fav = 0;
                saveResiliencePrefs();
                enqueueResponse(replyDest, replyChannel, "OK: AUTO-FAV DESACTIVADO", true, false, hops);
            } else {
                char buf[80];
                snprintf(buf, sizeof(buf), "AUTO-FAV: %s | auto-favs: %d", navaAutoFavoriteEnabled ? "ON" : "OFF", prefs.autoFavCount);
                enqueueResponse(replyDest, replyChannel, buf, true, false, hops);
            }
        }
        else if (sub.rfind("add", 0) == 0) {
            std::string targetStr = sub.substr(3);
            while (!targetStr.empty() && (targetStr.front() == ' ' || targetStr.front() == '!')) targetStr.erase(0, 1);
            if (targetStr.empty()) {
                enqueueResponse(replyDest, replyChannel, usageAndState("fav"), true, false, hops);
                return;
            }
            uint32_t targetId = strtoul(targetStr.c_str(), NULL, 16);
            meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(targetId);
            if (node) {
                removeAutoFav(targetId);
                node->is_favorite = true;
                nodeDB->saveToDisk(SEGMENT_NODEDATABASE);
                enqueueResponse(replyDest, replyChannel, "OK: FAVORITO MANUAL GUARDADO", true, false, hops);
            } else {
                enqueueResponse(replyDest, replyChannel, "NODO NO EXISTE EN TABLA RAM", true, false, hops);
            }
        }
        else if (sub.rfind("rm", 0) == 0) {
            std::string targetStr = sub.substr(2);
            while (!targetStr.empty() && (targetStr.front() == ' ' || targetStr.front() == '!')) targetStr.erase(0, 1);
            if (targetStr.empty()) {
                enqueueResponse(replyDest, replyChannel, usageAndState("fav"), true, false, hops);
                return;
            }
            uint32_t targetId = strtoul(targetStr.c_str(), NULL, 16);
            meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(targetId);
            if (node) {
                node->is_favorite = false;
                removeAutoFav(targetId);
                nodeDB->saveToDisk(SEGMENT_NODEDATABASE);
                enqueueResponse(replyDest, replyChannel, "OK: FAVORITO ELIMINADO", true, false, hops);
            } else {
                enqueueResponse(replyDest, replyChannel, "NODO NO EXISTE EN TABLA RAM", true, false, hops);
            }
        }
        else if (sub == "ls") {
            std::string favList = "FAVORITOS:\n";
            uint32_t totalNodos = nodeDB->getNumMeshNodes();
            bool found = false;
            for (size_t i = 0; i < totalNodos; i++) {
                const meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(i);
                if (node && node->is_favorite) {
                    found = true;
                    char fBuf[80];
                    const char *tag = isAutoFav(node->num) ? "[AUTO]" : "[MAN]";
                    snprintf(fBuf, sizeof(fBuf), "!%08x %s (S:%d)\n", (unsigned int)node->num, tag, node->hops_away);
                    favList += fBuf;
                }
            }
            if (!found) favList += "NINGUNO";
            enqueueResponse(replyDest, replyChannel, favList, true, false, hops);
        }
        else {
            enqueueResponse(replyDest, replyChannel, usageAndState("fav"), true, false, hops);
        }
    }
    else if (cmd.rfind("ign", 0) == 0) {
        std::string sub = (cmd.length() > 3) ? cmd.substr(3) : "";
        while (!sub.empty() && sub.front() == ' ') sub.erase(0, 1);
        if (sub.rfind("add", 0) == 0) {
            std::string targetStr = sub.substr(3);
            while (!targetStr.empty() && (targetStr.front() == ' ' || targetStr.front() == '!')) targetStr.erase(0, 1);
            if (targetStr.empty()) {
                enqueueResponse(replyDest, replyChannel, usageAndState("ign"), true, false, hops);
                return;
            }
            uint32_t targetId = strtoul(targetStr.c_str(), NULL, 16);
            meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(targetId);
            if (node) {
                node->is_ignored = true;
            }
            if (addIgnoredNode(targetId)) {
                enqueueResponse(replyDest, replyChannel, "OK: NODO IGNORADO (Persiste)", true, false, hops);
            } else {
                enqueueResponse(replyDest, replyChannel, "OK: NODO YA EN LISTA NEGRA", true, false, hops);
            }
        }
        else if (sub.rfind("rm", 0) == 0 || sub.rfind("del", 0) == 0) {
            std::string targetStr = sub.substr((sub.rfind("del", 0) == 0) ? 3 : 2);
            while (!targetStr.empty() && (targetStr.front() == ' ' || targetStr.front() == '!')) targetStr.erase(0, 1);
            if (targetStr.empty()) {
                enqueueResponse(replyDest, replyChannel, usageAndState("ign"), true, false, hops);
                return;
            }
            uint32_t targetId = strtoul(targetStr.c_str(), NULL, 16);
            meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(targetId);
            if (node) {
                node->is_ignored = false;
            }
            if (removeIgnoredNode(targetId)) {
                enqueueResponse(replyDest, replyChannel, "OK: NODO DESBLOQUEADO (Persiste)", true, false, hops);
            } else {
                enqueueResponse(replyDest, replyChannel, "NODO NO ESTABA EN LISTA NEGRA", true, false, hops);
            }
        }
        else if (sub == "clear") {
            clearIgnoredNodes();
            enqueueResponse(replyDest, replyChannel, "OK: LISTA NEGRA BORRADA POR COMPLETO", true, false, hops);
        }
        else if (sub == "ls") {
            std::string ignList = "IGNORADOS (Persistentes):\n";
            if (prefs.ignoredCount > 0) {
                for (uint8_t i = 0; i < prefs.ignoredCount && i < 8; i++) {
                    char iBuf[60];
                    snprintf(iBuf, sizeof(iBuf), "[%u] !%08x\n", (unsigned int)i, (unsigned int)prefs.ignoredNodes[i]);
                    ignList += iBuf;
                }
            } else {
                ignList += "NINGUNO";
            }
            enqueueResponse(replyDest, replyChannel, ignList, true, false, hops);
        }
        else {
            enqueueResponse(replyDest, replyChannel, usageAndState("ign"), true, false, hops);
        }
    }
    else if (cmd.rfind("set_chem", 0) == 0) {
        std::string arg = (cmd.length() > 8) ? cmd.substr(8) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_chem"), true, false, hops);
            return;
        }
        if (arg == "lipo") {
            prefs.chemistry = 0;
            prefs.vbat_cutoff = 3500;
            prefs.vwake_level = 3;
        } else if (arg == "nimh") {
            prefs.chemistry = 1;
            prefs.vbat_cutoff = 3400;
            prefs.vwake_level = 3;
        } else if (arg == "sodium") {
            prefs.chemistry = 2;
            prefs.vbat_cutoff = 2600;
            prefs.vwake_level = 1;
        } else if (arg == "lifepo4") {
#if defined(SEEED_SOLAR_NODE) || defined(SEEED_XIAO_NRF52840_KIT) || defined(HELTEC_T114)
            enqueueResponse(replyDest, replyChannel, "ERR: LIFEPO4 NO COMPATIBLE, UMBRAL LPCOMP FIJO", true, false, hops);
            return;
#else
            prefs.chemistry = 3;
            prefs.vbat_cutoff = 2800;
            prefs.vwake_level = 5;
#endif
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: QUIMICA INVALIDA (lipo/nimh/sodium/lifepo4)", true, false, hops);
            return;
        }
        saveResiliencePrefs();
        power->setChemistryProfile(prefs.chemistry);
        power->updateOcvCurve(prefs.vbat_cutoff);
        currentWakeLevel = prefs.vwake_level;
        enqueueResponse(replyDest, replyChannel, "OK: QUIMICA APLICADA (Persiste. ROLLBACK SOLO: nrf erase)", true, false, hops);
    }
    else if (cmd.rfind("set_vbat", 0) == 0) {
        std::string arg = (cmd.length() > 8) ? cmd.substr(8) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_vbat"), true, false, hops);
            return;
        }
        uint16_t val = atoi(arg.c_str());
        if (val < 2400 || val > 3600) {
            enqueueResponse(replyDest, replyChannel, "ERR: RANGO INVALIDO (2400-3600 mV)", true, false, hops);
            return;
        }
        prefs.vbat_cutoff = val;
        saveResiliencePrefs();
        power->updateOcvCurve(prefs.vbat_cutoff);
        enqueueResponse(replyDest, replyChannel, "OK: CORTE VBAT APLICADO (Persiste. ROLLBACK SOLO: nrf erase)", true, false, hops);
    }
    else if (cmd.rfind("set_vwake", 0) == 0) {
        std::string arg = (cmd.length() > 9) ? cmd.substr(9) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_vwake"), true, false, hops);
            return;
        }
        uint8_t lvl = atoi(arg.c_str());
        if (lvl < 1 || lvl > 5) {
            enqueueResponse(replyDest, replyChannel, "ERR: NIVEL INVALIDO (1-5)", true, false, hops);
            return;
        }
        uint16_t wakeMv = 0;
        switch (lvl) {
            case 1: wakeMv = 2100; break;
            case 2: wakeMv = 2500; break;
            case 3: wakeMv = 3700; break;
            case 4: wakeMv = 4500; break;
            case 5: wakeMv = 3300; break;
        }
        if (wakeMv <= prefs.vbat_cutoff) {
            char err[100];
            snprintf(err, sizeof(err), "ERR: VWAKE (%umV) DEBE SUPERAR VBAT_CUTOFF (%umV)", wakeMv, (unsigned int)prefs.vbat_cutoff);
            enqueueResponse(replyDest, replyChannel, err, true, false, hops);
            return;
        }
        prefs.vwake_level = lvl;
        saveResiliencePrefs();
        currentWakeLevel = lvl;
        enqueueResponse(replyDest, replyChannel, "OK: NIVEL VWAKE APLICADO (Persiste. ROLLBACK SOLO: nrf erase)", true, false, hops);
    }
    else if (cmd.rfind("storm", 0) == 0) {
        std::string arg = (cmd.length() > 5) ? cmd.substr(5) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg == "test1") {
            stormSeconds = 60;
            enqueueResponse(replyDest, replyChannel, "OK: HIBERNACION TEST 1 MIN (tras vaciar cola)", true, false, hops);
            deferredAction = NAVA_DEFERRED_STORM;
            preRebootArmed = false;
        } else if (arg == "test2") {
            stormSeconds = 120;
            enqueueResponse(replyDest, replyChannel, "OK: HIBERNACION TEST 2 MIN (tras vaciar cola)", true, false, hops);
            deferredAction = NAVA_DEFERRED_STORM;
            preRebootArmed = false;
        } else if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("storm"), true, false, hops);
        } else {
            uint32_t hours = atoi(arg.c_str());
            if (hours >= 1 && hours <= 720) {
                stormSeconds = hours * 3600;
                char sBuf[80];
                snprintf(sBuf, sizeof(sBuf), "OK: MODO TORMENTA %lu HORAS (tras vaciar cola)", (unsigned long)hours);
                enqueueResponse(replyDest, replyChannel, sBuf, true, false, hops);
                deferredAction = NAVA_DEFERRED_STORM;
                preRebootArmed = false;
            } else {
                enqueueResponse(replyDest, replyChannel, "ERR: HORAS INVALIDAS (1-720)", true, false, hops);
            }
        }
    }
    else if (cmd == "txoff") {
        enqueueResponse(replyDest, replyChannel, "OK: TX APAGADO (tras vaciar cola. Persiste. ROLLBACK SOLO: nrf erase)", true, false, hops);
        deferredAction = NAVA_DEFERRED_TXOFF;
        preRebootArmed = false;
    }
    else if (cmd == "txon") {
        config.lora.tx_enabled = true;
        prefs.tx_disabled = 0;
        saveResiliencePrefs();
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        enqueueResponse(replyDest, replyChannel, "OK: TX LORA REACTIVADO", true, false, hops);
    }
    else if (cmd.rfind("ble", 0) == 0) {
        std::string arg = (cmd.length() > 3) ? cmd.substr(3) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg == "on") {
            prefs.ble_disabled = 0;
            saveResiliencePrefs();
            config.bluetooth.enabled = true;
            setBleForceDisabled(false);
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            enqueueResponse(replyDest, replyChannel, "OK: BLE ACTIVADO (REQUIERE REINICIO)", true, false, hops);
        } else if (arg == "off") {
            prefs.ble_disabled = 1;
            saveResiliencePrefs();
            config.bluetooth.enabled = false;
            setBleForceDisabled(true);
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            enqueueResponse(replyDest, replyChannel, "OK: BLE APAGADO (Persiste. ROLLBACK SOLO: nrf erase)", true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("ble"), true, false, hops);
        }
    }
    else if (cmd.rfind("msg", 0) == 0) {
        std::string msgStr = (cmd.length() > 3) ? cmd.substr(3) : "";
        while (!msgStr.empty() && (msgStr.front() == ' ' || msgStr.front() == '"')) msgStr.erase(0, 1);
        while (!msgStr.empty() && (msgStr.back() == ' ' || msgStr.back() == '"')) msgStr.pop_back();
        if (msgStr.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("msg"), true, false, hops);
            return;
        }
        enqueueResponse(NODENUM_BROADCAST, 0, msgStr, true, false, hops);
        enqueueResponse(replyDest, replyChannel, "OK: MENSAJE DIFUNDIDO EN CANAL 0", true, false, hops);
    }
    else if (cmd == "bell") {
        playComboTune();
        enqueueResponse(replyDest, replyChannel, "OK: TONO DE ALARMA EMITIDO", true, false, hops);
    }
    else if (cmd == "pos") {
        if (positionModule) {
            positionModule->sendOurPosition(NODENUM_BROADCAST, true);
            enqueueResponse(replyDest, replyChannel, "OK: POSICION ENVIADA", true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: MODULO POSICION NO ACTIVO", true, false, hops);
        }
    }
    else if (cmd == "nodeinfo") {
        if (nodeInfoModule) {
            nodeInfoModule->sendOurNodeInfo(NODENUM_BROADCAST);
            enqueueResponse(replyDest, replyChannel, "OK: NODEINFO ENVIADO", true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: MODULO NODEINFO NO ACTIVO", true, false, hops);
        }
    }
    else if (cmd == "sendtel") {
#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR
        if (environmentTelemetryModule) {
            environmentTelemetryModule->sendTelemetry(NODENUM_BROADCAST);
            enqueueResponse(replyDest, replyChannel, "OK: TELEMETRIA AMBIENTAL ENVIADA", true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: TELEMETRIA NO ACTIVA", true, false, hops);
        }
#else
        enqueueResponse(replyDest, replyChannel, "ERR: TELEMETRIA EXCLUIDA EN FIRMWARE", true, false, hops);
#endif
    }
    else if (cmd.rfind("set_name", 0) == 0) {
        std::string arg = (cmd.length() > 8) ? cmd.substr(8) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_name"), true, false, hops);
            return;
        }

        // Subcomando flush / clear / reset para volver al comportamiento natural de la app
        if (strcasecmp(arg.c_str(), "flush") == 0 || strcasecmp(arg.c_str(), "clear") == 0 || strcasecmp(arg.c_str(), "reset") == 0) {
            memset(prefs.custom_long_name, 0, sizeof(prefs.custom_long_name));
            memset(prefs.custom_short_name, 0, sizeof(prefs.custom_short_name));
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: NOMBRE PERSISTENTE BORRADO (MODO NATURAL)", true, false, hops);
            return;
        }

        size_t q1 = arg.find('"');
        size_t q2 = (q1 != std::string::npos) ? arg.find('"', q1 + 1) : std::string::npos;
        size_t q3 = (q2 != std::string::npos) ? arg.find('"', q2 + 1) : std::string::npos;
        size_t q4 = (q3 != std::string::npos) ? arg.find('"', q3 + 1) : std::string::npos;
        if (q1 != std::string::npos && q2 != std::string::npos && q3 != std::string::npos && q4 != std::string::npos) {
            std::string longN = arg.substr(q1 + 1, q2 - q1 - 1);
            std::string shortN = arg.substr(q3 + 1, q4 - q3 - 1);
            strncpy(owner.long_name, longN.c_str(), sizeof(owner.long_name) - 1);
            strncpy(owner.short_name, shortN.c_str(), sizeof(owner.short_name) - 1);
            owner.long_name[sizeof(owner.long_name) - 1] = '\0';
            owner.short_name[sizeof(owner.short_name) - 1] = '\0';

            // Guardar en resilience.bin como hardcodeo persistente:
            strncpy(prefs.custom_long_name, owner.long_name, sizeof(prefs.custom_long_name) - 1);
            strncpy(prefs.custom_short_name, owner.short_name, sizeof(prefs.custom_short_name) - 1);
            prefs.custom_long_name[sizeof(prefs.custom_long_name) - 1] = '\0';
            prefs.custom_short_name[sizeof(prefs.custom_short_name) - 1] = '\0';
            saveResiliencePrefs();

            // Sincronizar en NodeDB local y persistir ambos segmentos:
            nodeDB->updateUser(nodeDB->getNodeNum(), owner);
            nodeDB->saveToDisk(SEGMENT_DEVICESTATE | SEGMENT_NODEDATABASE);

            // Forzar emisión inmediata del nuevo NodeInfo a la red:
            if (service) {
                service->reloadOwner(true);
            }
            enqueueResponse(replyDest, replyChannel, "OK: NOMBRE HARDCODEADO EN RESILIENCIA", true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: FORMATO set_name \"Largo\" \"Corto\" (o flush)", true, false, hops);
        }
    }
    else if (cmd.rfind("set_role", 0) == 0) {
        std::string arg = (cmd.length() > 8) ? cmd.substr(8) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_role"), true, false, hops);
            return;
        }
        if (arg == "client") {
            config.device.role = meshtastic_Config_DeviceConfig_Role_CLIENT;
            prefs.role = meshtastic_Config_DeviceConfig_Role_CLIENT;
        } else if (arg == "mute") {
            config.device.role = meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE;
            prefs.role = meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE;
        } else if (arg == "router") {
            config.device.role = meshtastic_Config_DeviceConfig_Role_ROUTER;
            prefs.role = meshtastic_Config_DeviceConfig_Role_ROUTER;
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: ROL INVALIDO (client/mute/router)", true, false, hops);
            return;
        }
        nodeDB->installRoleDefaults(config.device.role);
        owner.role = config.device.role;
        owner.is_unmessagable = false;
        owner.has_is_unmessagable = true;
        nodeDB->updateUser(nodeDB->getNodeNum(), owner);
        nodeDB->saveToDisk(SEGMENT_CONFIG | SEGMENT_DEVICESTATE | SEGMENT_NODEDATABASE);
        saveResiliencePrefs();
        if (service) {
            service->reloadOwner(true);
        }
        enqueueResponse(replyDest, replyChannel, "OK: ROL CAMBIADO (persiste a factory reset)", true, false, hops);
    }
    else if (cmd.rfind("set_mqtt", 0) == 0) {
        std::string arg = (cmd.length() > 8) ? cmd.substr(8) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg == "on") {
            moduleConfig.mqtt.enabled = true;
            nodeDB->saveToDisk(SEGMENT_MODULECONFIG);
            enqueueResponse(replyDest, replyChannel, "OK: MQTT ON", true, false, hops);
        } else if (arg == "off") {
            moduleConfig.mqtt.enabled = false;
            nodeDB->saveToDisk(SEGMENT_MODULECONFIG);
            enqueueResponse(replyDest, replyChannel, "OK: MQTT OFF", true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_mqtt"), true, false, hops);
        }
    }
    else if (cmd.rfind("set_tz", 0) == 0) {
        std::string arg = (cmd.length() > 6) ? cmd.substr(6) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_tz"), true, false, hops);
            return;
        }
        strncpy(config.device.tzdef, arg.c_str(), sizeof(config.device.tzdef) - 1);
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        enqueueResponse(replyDest, replyChannel, "OK: ZONA HORARIA APLICADA", true, false, hops);
    }
    else if (cmd.rfind("set_hops", 0) == 0) {
        std::string arg = (cmd.length() > 8) ? cmd.substr(8) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_hops"), true, false, hops);
            return;
        }
        uint8_t h = atoi(arg.c_str());
        if (h >= 1 && h <= 7) {
            config.lora.hop_limit = h;
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            enqueueResponse(replyDest, replyChannel, "OK: LIMITE DE SALTOS APLICADO", true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: SALTOS INVALIDOS (1-7)", true, false, hops);
        }
    }
    else if (cmd.rfind("set_txpower", 0) == 0) {
        std::string arg = (cmd.length() > 11) ? cmd.substr(11) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_txpower"), true, false, hops);
            return;
        }
        int p = atoi(arg.c_str());
#ifdef NAVARICO_RADIO_E22P
        if (p >= 0 && p <= 12) {
            config.lora.tx_power = p;
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            enqueueResponse(replyDest, replyChannel, "OK: POTENCIA TX E22P APLICADA", true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: POTENCIA INVALIDA E22P (0-12 dBm)", true, false, hops);
        }
#else
        if (p >= 0 && p <= 22) {
            config.lora.tx_power = p;
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            enqueueResponse(replyDest, replyChannel, "OK: POTENCIA TX SX1262 APLICADA", true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: POTENCIA INVALIDA SX1262 (0-22 dBm)", true, false, hops);
        }
#endif
    }
    else if (cmd.rfind("sleepmsg", 0) == 0) {
        std::string arg = (cmd.length() > 8) ? cmd.substr(8) : "";
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
        if (arg == "on" || arg == "1") {
            prefs.sleepMsgs = 1;
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: AVISOS DE SUENO ACTIVADOS (ON)", true, false, hops);
        } else if (arg == "off" || arg == "0") {
            prefs.sleepMsgs = 0;
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: AVISOS DE SUENO DESACTIVADOS (OFF)", true, false, hops);
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("sleepmsg"), true, false, hops);
        }
    }
    else if (cmd == "db_purge") {
        uint32_t total = nodeDB->getNumMeshNodes();
        uint32_t purged = 0;
        for (int i = (int)total - 1; i >= 0; i--) {
            const meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(i);
            if (node && !node->is_favorite && !nodeDB->isAdminNode(*node) && node->num != nodeDB->getNodeNum()) {
                nodeDB->removeNodeByNum(node->num);
                purged++;
            }
        }
        char bBuf[80];
        snprintf(bBuf, sizeof(bBuf), "OK: %u NODOS EXPULSADOS DE RAM", (unsigned int)purged);
        enqueueResponse(replyDest, replyChannel, bBuf, true, false, hops);
    }
    else if (cmd == "db_clear") {
        nodeDB->resetNodes();
        enqueueResponse(replyDest, replyChannel, "OK: BASE DE DATOS PURGADA POR COMPLETO", true, false, hops);
    }
    else if (cmd == "reboot") {
        enqueueResponse(replyDest, replyChannel, "OK: REINICIANDO (tras vaciar cola...)", true, false, hops);
        deferredAction = NAVA_DEFERRED_REBOOT;
        preRebootArmed = false;
    }
    else if (cmd == "factory_reset") {
        enqueueResponse(replyDest, replyChannel, "OK: RESET DE FABRICA PROGRAMADO (tras vaciar cola...)", true, false, hops);
        deferredAction = NAVA_DEFERRED_FACTORY_RESET;
        preRebootArmed = false;
    }
    else if (cmd.rfind("full_reset", 0) == 0) {
        std::string arg = (cmd.length() > 10) ? cmd.substr(10) : "";
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) arg.erase(0, 1);
        if (arg != "confirm" && arg != "CONFIRM") {
            enqueueResponse(replyDest, replyChannel, "ERR: COMANDO DESTRUCTIVO. Requiere: /nava full_reset CONFIRM", true, false, hops);
            return;
        }
        enqueueResponse(replyDest, replyChannel, "OK: RESET COMPLETO PROGRAMADO (PKI conservado, tras vaciar cola...)", true, false, hops);
        deferredAction = NAVA_DEFERRED_FULL_RESET;
        preRebootArmed = false;
    }
    else if (cmd.rfind("wipe", 0) == 0) {
        std::string arg = (cmd.length() > 4) ? cmd.substr(4) : "";
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) arg.erase(0, 1);
        if (arg != "confirm" && arg != "CONFIRM") {
            enqueueResponse(replyDest, replyChannel, "ERR: COMANDO DESTRUCTIVO. Requiere: /nava wipe CONFIRM", true, false, hops);
            return;
        }
        enqueueResponse(replyDest, replyChannel, "OK: WIPE PROGRAMADO (par PKI nuevo al reiniciar, tras vaciar cola...)", true, false, hops);
        deferredAction = NAVA_DEFERRED_WIPE;
        preRebootArmed = false;
    }
    else if (cmd == "admin_ls") {
        std::string out = "CLAVES ADMIN CONFIG (base64):\n";
        meshtastic_Config_SecurityConfig &sec = config.security;
        for (pb_size_t i = 0; i < 3; i++) {
            char line[100];
            if (i < sec.admin_key_count && sec.admin_key[i].size > 0) {
                std::string b64 = base64Encode(sec.admin_key[i].bytes, sec.admin_key[i].size);
                snprintf(line, sizeof(line), "[%d] %s (%uB)\n", i, b64.c_str(), (unsigned int)sec.admin_key[i].size);
            } else {
                snprintf(line, sizeof(line), "[%d] (vacio)\n", i);
            }
            out += line;
        }
        enqueueResponse(replyDest, replyChannel, out, true, false, hops);
    }
    else if (cmd == "keys_ls") {
        std::string out = "CLAVES ADMIN PERSISTIDAS (base64):\n";
        char line[100];
        if (!navaKeyIsEmpty(prefs.keySlot0Own)) {
            std::string b64 = base64Encode(prefs.keySlot0Own, 32);
            snprintf(line, sizeof(line), "[S0 propia] %s (32B)\n", b64.c_str());
        } else {
            snprintf(line, sizeof(line), "[S0 propia] (sin fijar -> fabrica)\n");
        }
        out += line;
        if (!navaKeyIsEmpty(prefs.keySlot1)) {
            std::string b64 = base64Encode(prefs.keySlot1, 32);
            snprintf(line, sizeof(line), "[Slot 1]    %s (32B)\n", b64.c_str());
        } else {
            snprintf(line, sizeof(line), "[Slot 1]    (vacio)\n");
        }
        out += line;
        if (!navaKeyIsEmpty(prefs.keySlot2)) {
            std::string b64 = base64Encode(prefs.keySlot2, 32);
            snprintf(line, sizeof(line), "[Slot 2]    %s (32B)", b64.c_str());
        } else {
            snprintf(line, sizeof(line), "[Slot 2]    (vacio)");
        }
        out += line;
        enqueueResponse(replyDest, replyChannel, out, true, false, hops);
    }
    else if (cmd == "keys_clear") {
        enqueueResponse(replyDest, replyChannel, "OK: CLAVES PERSISTIDAS BORRADAS (tras vaciar cola...)", true, false, hops);
        deferredAction = NAVA_DEFERRED_KEYS_CLEAR;
        preRebootArmed = false;
    }
    else {
        enqueueResponse(replyDest, replyChannel, "ERR: COMANDO DESCONOCIDO", true, false, hops);
    }
}

int32_t NavaCLIModule::runOnce()
{
    // V2: reconciliar el listado persistente de auto-favoritos con los routers directos
    reconcileAutoFavs();

    // Actualizar métricas de stats en RAM
    uint16_t curBat = powerStatus->getBatteryVoltageMv();
    if (curBat > 1000 && curBat < statsMinBattMv) {
        statsMinBattMv = curBat;
    }
#ifdef NRF52840_XXAA
    int32_t tempRaw = 0;
    if (sd_temp_get(&tempRaw) == NRF_SUCCESS) {
        float curTemp = tempRaw / 4.0f;
        if (curTemp < statsMinTemp) statsMinTemp = curTemp;
        if (curTemp > statsMaxTemp) statsMaxTemp = curTemp;
    }
#endif

    // Ráfaga periódica de prueba RF (test_tx)
    if (testTxCountRemaining > 0 && (int32_t)(millis() - testTxNextMs) >= 0) {
        uint8_t targetChan = prefs.cliChannelSlot;
        if (targetChan < 1 || targetChan > 7) targetChan = 1;
        enqueueResponse(NODENUM_BROADCAST, targetChan, "TEST TX BEACON", true, true);
        testTxCountRemaining--;
        testTxNextMs = millis() + 1000;
    }

    // Primer tick tras el boot
    if (!firstRunDone) {
        firstRunDone = true;
        // NAVARICO V5: Auto-aprovisionar Navadmin en Slot 1 (sin requerir factory reset tras flasheo)
        ensureNavadminChannel();
        // NAVARICO F20: restaurar claves admin persistidas
        applyPersistedAdminKeys();
        // NAVARICO F21: restaurar canales secundarios persistidos
        applyPersistedChannels();
        // NAVARICO V5: restaurar capa física LoRa y Canal 0 Primario persistidos
        applyPersistedLoraConfig();
        applyPersistedChannel0();
        // NAVARICO V5: Respaldo pasivo y adopción no destructiva de la configuración activa del usuario
        adoptExistingOperationalConfig();

        // Si arrancamos en modo prueba post-salto de pánico, rearmar el plazo relativo a este arranque fresco
        if (prefs.panic_trial_active == 1) {
            uint32_t rollMins = (prefs.panic_rollback_mins > 0 && prefs.panic_rollback_mins <= 1440) ? prefs.panic_rollback_mins : 5;
            prefs.panic_trial_deadline_ms = millis() + (rollMins * 60000);
            LOG_INFO("NavaCLI: Nodo operando en periodo de prueba de panico. Rollback en %u min si no se recibe panic_ok", (unsigned int)rollMins);
        }

        logEvent("BOOT causa 0x%08X", (unsigned int)rawResetReason);

        if (wokeFromSleep || vivoPending || reservaPending) {
            uint8_t targetChan = prefs.cliChannelSlot;
            if (targetChan < 1 || targetChan > 7) targetChan = 1;

            if (reservaPending && prefs.sleepMsgs) {
                prefs.wasInSleep = 1;
                saveResiliencePrefs();
                char buf[220];
                snprintf(buf, sizeof(buf), "[Critico] %s id%08x | %s | bateria en capacidad critica, operando 160s",
                         owner.long_name, (unsigned int)nodeDB->getNodeNum(), buildEnergyLine().c_str());
                enqueueResponse(NODENUM_BROADCAST, targetChan, buf, true, true);
                logEvent("ESTADO [Critico]");
            } else if (vivoPending && prefs.sleepMsgs) {
                prefs.wasInSleep = 1;
                saveResiliencePrefs();
                char buf[220];
                snprintf(buf, sizeof(buf), "[Vivo] %s id%08x | %s | sigo vivo, al limite de carga",
                         owner.long_name, (unsigned int)nodeDB->getNodeNum(), buildEnergyLine().c_str());
                enqueueResponse(NODENUM_BROADCAST, targetChan, buf, true, true);
                logEvent("ESTADO [Vivo]");
            } else if (wokeFromSleep && prefs.sleepMsgs) {
                prefs.wasInSleep = 0;
                saveResiliencePrefs();
                char buf[220];
                snprintf(buf, sizeof(buf), "[Listo] %s id%08x | %s | despierto, cargando, listo para trabajar",
                         owner.long_name, (unsigned int)nodeDB->getNodeNum(), buildEnergyLine().c_str());
                enqueueResponse(NODENUM_BROADCAST, targetChan, buf, true, true);
                logEvent("ESTADO [Listo]");
            } else {
                if (wokeFromSleep) {
                    prefs.wasInSleep = 0;
                    saveResiliencePrefs();
                }
            }
        }
    }

    // Aviso de arranque [Boot] DIFERIDO 2 minutos
    {
        static bool bootNoticeSent = false;
        static uint32_t bootNoticeAt = 0;
        if (!bootNoticeSent && !wokeFromSleep && !vivoPending && !reservaPending && prefs.sleepMsgs) {
            if (bootNoticeAt == 0) {
                bootNoticeAt = millis() + 120000;
            }
            if ((int32_t)(millis() - bootNoticeAt) >= 0) {
                bootNoticeSent = true;
                char buf[240];
                snprintf(buf, sizeof(buf), "[Boot] %s id%08x | NAVA %s | %s | causa: 0x%08X (%s)",
                         owner.long_name, (unsigned int)nodeDB->getNodeNum(), NAVATASTIC_BUILD,
                         buildEnergyLine().c_str(), (unsigned int)rawResetReason,
                         navaricoResetReasonName(rawResetReason));
                uint8_t targetChan = prefs.cliChannelSlot;
                if (targetChan < 1 || targetChan > 7) targetChan = 1;
                enqueueResponse(NODENUM_BROADCAST, targetChan, buf, true, true);
            }
        }
    }

    // Emisión de paquetes de la cola de respuestas
    if (!responseQueue.empty()) {
        auto response = responseQueue.front();
        responseQueue.pop();

        meshtastic_MeshPacket *reply = allocDataPacket();
        if (reply) {
            reply->decoded.payload.size = response.text.length();
            memcpy(reply->decoded.payload.bytes, response.text.c_str(), response.text.length());
            reply->to = response.dest;
            reply->channel = response.channel;
            reply->want_ack = false;
            statsTxPackets++;
            service->sendToMesh(reply, RX_SRC_LOCAL, true);
        }

        if (sleepPending && responseQueue.empty()) {
            sleepTime = millis() + 3000;
        }

        if (!responseQueue.empty()) {
            return 12000;
        }
    }

    // NAVARICO V5: Protocolo de Pánico - Cuenta atrás y emisión periódica de pulsos
    if (prefs.panic_active == 1) {
        int32_t remSecs = (int32_t)(prefs.panic_target_time_ms - millis()) / 1000;
        if (remSecs <= 0) {
            LOG_INFO("NavaCLI: Salto de Panico T=0. Ejecutando cambio de preset y sincronizando radio...");
            if (prefs.panic_target_preset != 0) {
                prefs.lora_use_preset = 1;
                prefs.lora_modem_preset = prefs.panic_target_preset;
                prefs.lora_override_frequency = 0.0f;
                config.lora.use_preset = true;
                config.lora.modem_preset = (meshtastic_Config_LoRaConfig_ModemPreset)prefs.panic_target_preset;
                config.lora.override_frequency = 0.0f;
            } else {
                prefs.lora_use_preset = 0;
                prefs.lora_bandwidth = prefs.panic_target_bw;
                prefs.lora_spread_factor = prefs.panic_target_sf;
                prefs.lora_coding_rate = prefs.panic_target_cr;
                prefs.lora_override_frequency = prefs.panic_target_freq;
                prefs.lora_channel_num = prefs.panic_target_slot;
                config.lora.use_preset = false;
                config.lora.bandwidth = prefs.panic_target_bw;
                config.lora.spread_factor = prefs.panic_target_sf;
                config.lora.coding_rate = prefs.panic_target_cr;
                config.lora.override_frequency = prefs.panic_target_freq;
                config.lora.channel_num = prefs.panic_target_slot;
            }
            prefs.lora_configured = 1;
            prefs.panic_active = 0;
            prefs.panic_target_time_ms = 0;
            prefs.panic_last_pulse_ms = 0;
            if (prefs.panic_rollback_mins > 0) {
                prefs.panic_trial_active = 1;
                prefs.panic_trial_deadline_ms = millis() + (prefs.panic_rollback_mins * 60000);
            }
            saveResiliencePrefs();
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            rebootAtMsec = millis() + 25;
            return 1000;
        } else if (remSecs > 60 && (millis() - prefs.panic_last_pulse_ms >= nextPulseIntervalMs)) {
            emitPanicPulse();
        }
    }

    // Comprobación de expiración de periodo de prueba de pánico
    if (prefs.panic_trial_active == 1 && (int32_t)(millis() - prefs.panic_trial_deadline_ms) >= 0) {
        LOG_WARN("NavaCLI: Tiempo de prueba de Panico expirado sin panic_ok. Revirtiendo a valores de fabrica...");
        prefs.panic_trial_active = 0;
        prefs.panic_rollback_mins = 0;
        prefs.lora_configured = 0;
        saveResiliencePrefs();
        nodeDB->factoryReset(false);
        rebootAtMsec = millis() + 25;
        return 1000;
    }

    // NAVARICO V5: Desacople Asíncrono de Traceroute
    if (tracePending && responseQueue.empty() && (int32_t)(millis() - traceExecutionTime) >= 0) {
        tracePending = false;
        if (traceRouteModule) {
            LOG_INFO("NavaCLI: Disparando sonda TraceRoute desacoplada hacia 0x%08x", (unsigned int)traceTarget);
            traceRouteModule->startTraceRoute(traceTarget);
        }
    }

    // NAVARICO V5: Manejador centralizado de Acciones Diferidas con Ventana de Gracia Post-Envío (6s)
    if (deferredAction != NAVA_DEFERRED_NONE && !preRebootArmed && responseQueue.empty()) {
        preRebootArmed = true;
        deferredExecutionTime = millis() + 6000;
        LOG_INFO("NavaCLI: Cola vacia. Ventana de gracia armada (6s) para accion diferida %d", (int)deferredAction);
    }
    if (preRebootArmed && (int32_t)(millis() - deferredExecutionTime) >= 0) {
        NavaDeferredAction act = deferredAction;
        deferredAction = NAVA_DEFERRED_NONE;
        preRebootArmed = false;
        switch (act) {
            case NAVA_DEFERRED_REBOOT:
                LOG_INFO("Ejecutando reinicio diferido...");
                nodeDB->saveToDisk(SEGMENT_NODEDATABASE);
                rebootAtMsec = millis() + 25;
                return 1000;
            case NAVA_DEFERRED_FACTORY_RESET:
                LOG_INFO("Ejecutando factory reset diferido...");
                nodeDB->factoryReset(true);
                rebootAtMsec = millis() + 25;
                return 1000;
            case NAVA_DEFERRED_FULL_RESET:
                LOG_INFO("Ejecutando full reset diferido (PKI conservado)...");
                navaFullResetKeepKeys();
                nodeDB->factoryReset(false);
                rebootAtMsec = millis() + 25;
                return 1000;
            case NAVA_DEFERRED_WIPE:
                LOG_INFO("Ejecutando wipe diferido (nuevo par PKI)...");
                FSCom.remove("/resilience.bin");
                nodeDB->factoryReset(true);
                rebootAtMsec = millis() + 25;
                return 1000;
            case NAVA_DEFERRED_STORM:
                LOG_INFO("Entrando en modo tormenta: %lu segundos", (unsigned long)stormSeconds);
                timedSystemSleepSeconds(stormSeconds);
                break;
            case NAVA_DEFERRED_TXOFF:
                LOG_INFO("Desactivando TX LoRa...");
                config.lora.tx_enabled = false;
                prefs.tx_disabled = 1;
                saveResiliencePrefs();
                nodeDB->saveToDisk(SEGMENT_CONFIG);
                break;
            case NAVA_DEFERRED_KEYS_CLEAR:
                LOG_INFO("Borrando claves admin persistidas...");
                memset(prefs.keySlot1, 0, sizeof(prefs.keySlot1));
                memset(prefs.keySlot2, 0, sizeof(prefs.keySlot2));
                memset(prefs.keySlot0Own, 0, sizeof(prefs.keySlot0Own));
                saveResiliencePrefs();
                break;
            case NAVA_DEFERRED_LORA_CHANGE:
            case NAVA_DEFERRED_PANIC_JUMP:
                LOG_INFO("Aplicando cambio de parametros LoRa / reiniciando...");
                rebootAtMsec = millis() + 25;
                return 1000;
            default:
                break;
        }
    }

    if (sleepPending && responseQueue.empty() && (int32_t)(millis() - sleepTime) >= 0) {
        LOG_INFO("Entering battery sleep after status message");
        sleepPending = false;
        doDeepSleep(portMAX_DELAY, false, true);
        return 1000;
    }
    if (sleepPending) {
        return 1000;
    }

    if (testTxCountRemaining > 0 || tracePending || preRebootArmed || prefs.panic_active != 0) {
        return 1000;
    }

    return 60000;
}

std::string NavaCLIModule::getRoleName(meshtastic_Config_DeviceConfig_Role role)
{
    switch(role) {
        case meshtastic_Config_DeviceConfig_Role_ROUTER: return "ROUTER";
        case meshtastic_Config_DeviceConfig_Role_ROUTER_LATE: return "ROUTER_LATE";
        case meshtastic_Config_DeviceConfig_Role_CLIENT_BASE: return "CLIENT_BASE";
        case meshtastic_Config_DeviceConfig_Role_CLIENT: return "CLIENT";
        case meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE: return "CLIENT_MUTE";
        default: return "OTHER";
    }
}

std::string NavaCLIModule::helpForCommand(const std::string &topic)
{
    if (topic == "ping")
        return "ping: Comprueba la latencia del repetidor. Uso: /nava ping";
    else if (topic == "status")
        return "status: Estado de la base de datos RAM (nodos/80), favoritos y tiempo activo. Uso: /nava status";
    else if (topic == "env")
        return "env: Telemetria del nodo: bateria, heap, temperatura CPU y sensores I2C. Uso: /nava env";
    else if (topic == "channel")
        return "channel: Porcentaje de uso del canal de radio (airtime) y transmision propia. Uso: /nava channel";
    else if (topic == "peers")
        return "peers: Lista de vecinos directos a 0 saltos con rol, SNR y antiguedad. Uso: /nava peers";
    else if (topic == "rxlog")
        return "rxlog: Metadatos (ID, PortNum, SNR, RSSI) de los ultimos 5 paquetes recibidos. Uso: /nava rxlog";
    else if (topic == "afc")
        return "afc: Deriva de frecuencia del TCXO en Hz del ultimo paquete. Uso: /nava afc";
    else if (topic == "reset_reason")
        return "reset_reason: Motivo del ultimo reinicio del chip (registro RESETREAS). Uso: /nava reset_reason";
    else if (topic == "route")
        return "route: Muestra a cuantos saltos y con que SNR escucha al nodo indicado. Uso: /nava route !ID";
    else if (topic == "trace")
        return "trace: Lanza un trazado de ruta nativo hacia el nodo indicado desacoplado en 8s. Uso: /nava trace !ID";
    else if (topic == "noise")
        return "noise: Piso de ruido instantaneo del chip de radio. Uso: /nava noise";
    else if (topic == "power")
        return "power: Metricas de energia: ADC interno + INA219 (V, +-mA, CARGANDO/DESCARGANDO, mW). Uso: /nava power";
    else if (topic == "bat")
        return "bat: Estado de bateria: quimica activa, voltaje, % OCV y estado TX. Uso: /nava bat";
    else if (topic == "ch_ls")
        return "ch_ls: Lista los 8 slots de canales (0-7), rol, nombre, tipo de clave y MQTT. Uso: /nava ch_ls";
    else if (topic == "ch_set")
        return "ch_set: Configura canal primario (slot 0) o secundario (slots 2-7). Uso: /nava ch_set <slot 0|2-7> <nombre> <psk_base64>";
    else if (topic == "ch_del")
        return "ch_del: Deshabilita el canal del slot seleccionado. Uso: /nava ch_del <slot 2-7>";
    else if (topic == "ch_url")
        return "ch_url: Genera la URL oficial para importar el canal en la app del movil. Uso: /nava ch_url [slot 0-7]";
    else if (topic == "set_cli_chan")
        return "set_cli_chan: Redirige escucha de NavaCLI y avisos solares al slot elegido. Uso: /nava set_cli_chan [slot 1-7]";
    else if (topic == "navadmin_mute")
        return "navadmin_mute: Silencia o reactiva el Canal 1 publico Navadmin. Uso: /nava navadmin_mute [on|off]";
    else if (topic == "ch_reset")
        return "ch_reset: Restaura configuracion de fabrica de canales (Navadmin Slot 1). Uso: /nava ch_reset";
    else if (topic == "ch_mqtt")
        return "ch_mqtt: Configura la compuerta MQTT por canal. Uso: /nava ch_mqtt <slot 0-7> [up|down|both|off]";
    else if (topic == "set_ok_to_mqtt")
        return "set_ok_to_mqtt: Autoriza a pasarelas ajenas a subir paquetes del nodo a internet. Uso: /nava set_ok_to_mqtt [on|off]";
    else if (topic == "set_pos")
        return "set_pos: Fija coordenadas GPS estaticas en el repetidor. Uso: /nava set_pos <lat> <lon> [alt]";
    else if (topic == "pos_clear")
        return "pos_clear: Borra las coordenadas fijas guardadas dejando el nodo sin posicion. Uso: /nava pos_clear";
    else if (topic == "set_pos_tx")
        return "set_pos_tx: Controla la difusion periodica de posicion de flota. Uso: /nava set_pos_tx [on|off|minutos]";
    else if (topic == "set_nodeinfo_tx")
        return "set_nodeinfo_tx: Controla la difusion periodica de NodeInfo/nombres de flota. Uso: /nava set_nodeinfo_tx [on|off|minutos]";
    else if (topic == "set_telem_tx")
        return "set_telem_tx: Controla la emision de telemetria ambiental y de bateria (default 12h). Uso: /nava set_telem_tx [on(12h)|off|minutos]";
    else if (topic == "set_preset")
        return "set_preset: Cambia el modem preset LoRa estandar y reinicia. Uso: /nava set_preset [long_fast|medium_fast|short_fast|long_slow|short_slow|medium_slow|long_moderate|short_turbo]";
    else if (topic == "set_lora")
        return "set_lora: Configura la capa fisica LoRa personalizada. Uso: /nava set_lora <bw 31-500> <sf 5-12> <cr 4-8> <freq_mhz> <slot> [txpower]";
    else if (topic == "set_freq")
        return "set_freq: Ajusta la frecuencia fisica LoRa y slot. Uso: /nava set_freq <freq_mhz> [slot]";
    else if (topic == "panic")
        return "panic: Evacuacion coordinada de emergencia de la red mesh. Uso: /nava panic <preset|sfnarrow> [minutos=10] [rollback_mins=0]";
    else if (topic == "panic_ok")
        return "panic_ok: Consolida el salto de evacuacion de panico cancelando el rollback. Uso: /nava panic_ok";
    else if (topic == "set_beacon")
        return "set_beacon: Ajusta cadencia de emision de NodeInfo/Posicion en minutos. Uso: /nava set_beacon [minutos]";
    else if (topic == "mute")
        return "mute: Silencia temporalmente el reenvio de paquetes ajenos (RAM). Uso: /nava mute [minutos|off]";
    else if (topic == "set_pin")
        return "set_pin: Cambia el PIN Bluetooth fijo de 6 digitos. Uso: /nava set_pin <6_digitos>";
    else if (topic == "stats")
        return "stats: Informe de rendimiento y extremos del uptime (100% RAM). Uso: /nava stats";
    else if (topic == "test_tx")
        return "test_tx: Emite una rafaga periodica de prueba (1 pkt/s) para medir senal. Uso: /nava test_tx [segundos 5-30]";
    else if (topic == "log")
        return "log: Muestra las ultimas lineas del buffer circular de eventos en RAM. Uso: /nava log [lineas]";
    else if (topic == "fav")
        return "fav: Gestiona favoritos (nodos con bypass de saltos). Uso: /nava fav add !ID | fav rm !ID | fav ls | fav auto [on|off]";
    else if (topic == "ign")
        return "ign: Bloquea/desbloquea nodos (spam/sabotaje). Uso: /nava ign add !ID | ign rm !ID | ign ls";
    else if (topic == "set_chem")
        return "set_chem: Cambia la quimica y ajusta corte/OCV/LPCOMP. Uso: /nava set_chem [lipo|nimh|sodium|lifepo4]";
    else if (topic == "set_vbat")
        return "set_vbat: Corte de apagado por bateria baja. Uso: /nava set_vbat [2400-3600] mV";
    else if (topic == "set_vwake")
        return "set_vwake: Nivel LPCOMP de reencendido solar. 1=2.1V, 2=2.5V, 3=3.7V, 4=4.5V, 5=3.3V. Uso: /nava set_vwake [1-5]";
    else if (topic == "storm")
        return "storm: Hibernacion con radio apagada. Uso: /nava storm [1-720]h | storm test1 (60s) | storm test2 (120s)";
    else if (topic == "txoff")
        return "txoff: Apaga la transmision LoRa tras vaciar cola (mantiene la escucha RX). Uso: /nava txoff";
    else if (topic == "txon")
        return "txon: Reactiva la transmision LoRa del nodo. Uso: /nava txon";
    else if (topic == "ble")
        return "ble: Apaga/enciende Bluetooth (requiere reinicio). Uso: /nava ble [on|off]";
    else if (topic == "msg")
        return "msg: Difunde un mensaje de texto en el Canal 0 firmado por el repetidor. Uso: /nava msg \"TEXTO\"";
    else if (topic == "bell")
        return "bell: Hace sonar la alarma acustica del nodo para localizarlo. Uso: /nava bell";
    else if (topic == "pos")
        return "pos: Fuerza la emision inmediata de la posicion GPS. Uso: /nava pos";
    else if (topic == "nodeinfo")
        return "nodeinfo: Transmite la baliza de presentacion NodeInfo. Uso: /nava nodeinfo";
    else if (topic == "sendtel")
        return "sendtel: Transmite las telemetrias ambientales de los sensores I2C. Uso: /nava sendtel";
    else if (topic == "set_name")
        return "set_name: Fija el nombre largo/corto persistente a resets en resilience.bin o vuelve al modo natural. Uso: /nava set_name \"Nombre Largo\" \"Corto\" | /nava set_name flush";
    else if (topic == "set_role")
        return "set_role: Cambia el rol del nodo. Uso: /nava set_role [client|mute|router]";
    else if (topic == "set_mqtt")
        return "set_mqtt: Activa/desactiva MQTT. Uso: /nava set_mqtt [on|off]";
    else if (topic == "set_tz")
        return "set_tz: Establece la zona horaria POSIX. Uso: /nava set_tz [tz_POSIX]";
    else if (topic == "set_hops")
        return "set_hops: Limite de saltos LoRa. Uso: /nava set_hops [1-7]";
    else if (topic == "set_txpower")
#ifdef NAVARICO_RADIO_E22P
        return "set_txpower: Potencia de transmision LoRa. Uso: /nava set_txpower [0-12]";
#else
        return "set_txpower: Potencia de transmision LoRa. Uso: /nava set_txpower [0-22]";
#endif
    else if (topic == "db_purge")
        return "db_purge: Expulsa de RAM los nodos que no son favoritos ni admin. Uso: /nava db_purge";
    else if (topic == "db_clear")
        return "db_clear: Borra toda la base de datos de nodos (nuclear). Uso: /nava db_clear";
    else if (topic == "reboot")
        return "reboot: Programa un reinicio limpio del nodo (tras vaciar cola). Uso: /nava reboot";
    else if (topic == "factory_reset")
        return "factory_reset: Formateo remoto de emergencia; restaura valores de rescate. Uso: /nava factory_reset";
    else if (topic == "full_reset")
        return "full_reset: Reset completo (config + semi-persistentes a defaults) conservando claves PKI y bonds BLE. Uso: /nava full_reset CONFIRM";
    else if (topic == "wipe")
        return "wipe: Purga total: regenera el par PKI (los peers fallan DM hasta re-aprender la clave nueva). Uso: /nava wipe CONFIRM";
    else if (topic == "admin_ls")
        return "admin_ls: Muestra las 3 claves criptograficas de admin en base64. Uso: /nava admin_ls";
    else if (topic == "keys_ls")
        return "keys_ls: Muestra las claves admin persistidas (sobreviven a factory/full reset) en base64. Uso: /nava keys_ls";
    else if (topic == "keys_clear")
        return "keys_clear: Borra las claves admin persistidas (no toca la config actual ni reinicia). Uso: /nava keys_clear";
    else if (topic == "sleepmsg")
        return "sleepmsg: Activa/desactiva los avisos de sueno/vivo/listo al canal Navadmin. Uso: /nava sleepmsg [on|off]";
    else if (topic == "help")
        return "help: Muestra la lista de comandos o ayuda de uno concreto. Uso: /nava help [comando]";
    return "Comando no reconocido. Escribe /nava help para ver la lista.";
}

std::string NavaCLIModule::usageAndState(const std::string &topic)
{
    char buf[220];
    if (topic == "ch_set") {
        return "USO: ch_set <slot 0|2-7> <nombre> <psk_base64>\nEj: ch_set 0 LongFast AQ==\nEj: ch_set 2 Privada AQ==\nEj: ch_set 2 MiMalla K8RUGJs...==";
    }
    if (topic == "ch_del") {
        return "USO: ch_del <slot 2-7>\nDeshabilita el canal del slot.";
    }
    if (topic == "ch_url") {
        return "USO: ch_url [slot 0-7]\nGenera la URL meshtastic.org/e/#... para importar el canal.";
    }
    if (topic == "set_cli_chan") {
        snprintf(buf, sizeof(buf), "CLI CHAN ACT: Slot %d (%s). USO: set_cli_chan [slot 1-7]", prefs.cliChannelSlot, channels.getName(prefs.cliChannelSlot));
        return buf;
    }
    if (topic == "navadmin_mute") {
        snprintf(buf, sizeof(buf), "NAVADMIN MUTE: %s. USO: navadmin_mute [on|off]", prefs.navadminMuted ? "ON" : "OFF");
        return buf;
    }
    if (topic == "ch_mqtt") {
        return "USO: ch_mqtt <slot 0-7> [up|down|both|off]\nConfigura la compuerta MQTT para ese slot.";
    }
    if (topic == "set_ok_to_mqtt") {
        snprintf(buf, sizeof(buf), "OK_TO_MQTT ACT: %s. USO: set_ok_to_mqtt [on|off]", config.lora.config_ok_to_mqtt ? "ON" : "OFF");
        return buf;
    }
    if (topic == "set_pos") {
        if (config.position.fixed_position && prefs.fixed_pos_enabled) {
            snprintf(buf, sizeof(buf), "POS ACT: Lat:%.5f Lon:%.5f Alt:%dm. USO: set_pos <lat> <lon> [alt]",
                     prefs.fixed_pos_lat / 1e7f, prefs.fixed_pos_lon / 1e7f, prefs.fixed_pos_alt);
        } else {
            snprintf(buf, sizeof(buf), "POS ACT: SIN FIJAR. USO: set_pos <lat> <lon> [alt]");
        }
        return buf;
    }
    if (topic == "set_beacon") {
        snprintf(buf, sizeof(buf), "BALIZA ACT: %lu min. USO: set_beacon [minutos]", (unsigned long)(config.device.node_info_broadcast_secs / 60));
        return buf;
    }
    if (topic == "mute") {
        if (navaIsMuteActive()) {
            uint32_t remMin = (muteUntilMs - millis()) / 60000;
            snprintf(buf, sizeof(buf), "MUTE ACT: ACTIVO (quedan %lu min). USO: mute [minutos|off]", (unsigned long)remMin);
        } else {
            snprintf(buf, sizeof(buf), "MUTE ACT: DESACTIVADO. USO: mute [minutos|off]");
        }
        return buf;
    }
    if (topic == "set_pin") {
        snprintf(buf, sizeof(buf), "PIN BT ACT: %lu. USO: set_pin <6_digitos>", (unsigned long)config.bluetooth.fixed_pin);
        return buf;
    }
    if (topic == "test_tx") {
        return "USO: test_tx [segundos 5-30]\nEmite rafaga periodica de 1 paquete/s para medir senal.";
    }
    if (topic == "log") {
        return "USO: log [lineas 1-15]\nMuestra las ultimas lineas del buffer de eventos RAM.";
    }
    if (topic == "set_chem") {
        const char *qca = (prefs.chemistry == 1) ? "nimh" : (prefs.chemistry == 2) ? "sodium" : (prefs.chemistry == 3) ? "lifepo4" : "lipo";
#if defined(SEEED_SOLAR_NODE) || defined(SEEED_XIAO_NRF52840_KIT) || defined(HELTEC_T114)
        snprintf(buf, sizeof(buf), "QCA: %s (%dmV,w%d)\nOPC: lipo|nimh|sodium [lifepo4 NO DISP: LPCOMP fijo >3.65V]\nlipo:3500/3.71V nimh:3400/3.71V sodium:2600/3.71V\nAVISO: persiste. ROLLBACK SOLO: nrf erase. CUIDADO", qca, prefs.vbat_cutoff, prefs.vwake_level);
#else
        snprintf(buf, sizeof(buf), "QCA: %s (%dmV,w%d)\nOPC: lipo|nimh|sodium|lifepo4\nlipo:3500/3.71V nimh:3400/3.71V sodium:2600/3.71V lifepo4:2800/3.30V\nAVISO: persiste. ROLLBACK SOLO: nrf erase. CUIDADO", qca, prefs.vbat_cutoff, prefs.vwake_level);
#endif
        return buf;
    }
    if (topic == "sleepmsg") {
        snprintf(buf, sizeof(buf), "SLEEPMSGS ACT: %s. USO: sleepmsg [on|off]", prefs.sleepMsgs ? "ON" : "OFF");
        return buf;
    }
    if (topic == "set_vbat") {
        snprintf(buf, sizeof(buf), "VBAT ACT: %dmV (2400-3600). AVISO: persiste. ROLLBACK SOLO: nrf erase", prefs.vbat_cutoff);
        return buf;
    }
    if (topic == "set_vwake") {
#if defined(SEEED_SOLAR_NODE) || defined(SEEED_XIAO_NRF52840_KIT) || defined(HELTEC_T114)
        snprintf(buf, sizeof(buf), "VWAKE ACT: %d. OJO: umbral FIJO en esta placa (~3.67-4.04V), no cambia despertar. AVISO: persiste. ROLLBACK SOLO: nrf erase", prefs.vwake_level);
#else
        snprintf(buf, sizeof(buf), "VWAKE ACT: %d. Niv: 1=2.1V 2=2.5V 3=3.7V 4=4.5V 5=3.3V. AVISO: persiste. ROLLBACK SOLO: nrf erase", prefs.vwake_level);
#endif
        return buf;
    }
    if (topic == "set_txpower") {
#ifdef NAVARICO_RADIO_E22P
        snprintf(buf, sizeof(buf), "TXPWR ACT: %ddBm (0-12). USO: set_txpower [0-12]", config.lora.tx_power);
#else
        snprintf(buf, sizeof(buf), "TXPWR ACT: %ddBm (0-22). USO: set_txpower [0-22]", config.lora.tx_power);
#endif
        return buf;
    }
    if (topic == "set_hops") {
        snprintf(buf, sizeof(buf), "HOPS ACT: %d (1-7). USO: set_hops [1-7]", config.lora.hop_limit);
        return buf;
    }
    if (topic == "set_role") {
        snprintf(buf, sizeof(buf), "ROL ACT: %s. USO: set_role [client/mute/router]", getRoleName(config.device.role).c_str());
        return buf;
    }
    if (topic == "set_mqtt") {
        snprintf(buf, sizeof(buf), "MQTT ACT: %s. USO: set_mqtt [on/off]", moduleConfig.mqtt.enabled ? "on" : "off");
        return buf;
    }
    if (topic == "set_tz") {
        snprintf(buf, sizeof(buf), "TZ ACT: %s. USO: set_tz [tz_POSIX]", config.device.tzdef);
        return buf;
    }
    if (topic == "set_name") {
        if (prefs.custom_long_name[0] != '\0') {
            snprintf(buf, sizeof(buf), "NOMBRE: \"%s\" \"%s\" (PERSISTENTE). USO: set_name \"Largo\" \"Corto\" | set_name flush", owner.long_name, owner.short_name);
        } else {
            snprintf(buf, sizeof(buf), "NOMBRE: \"%s\" \"%s\" (NATURAL). USO: set_name \"Largo\" \"Corto\" | set_name flush", owner.long_name, owner.short_name);
        }
        return buf;
    }
    if (topic == "ble") {
        snprintf(buf, sizeof(buf), "BLE ACT: %s (persiste; requiere reboot). USO: ble [on/off]. AVISO: ROLLBACK SOLO: nrf erase", (prefs.ble_disabled == 1) ? "off" : "on");
        return buf;
    }
    if (topic == "storm") {
        return "USO: storm [1-720]h | test1(60s) | test2(120s)";
    }
    if (topic == "fav") {
        return "USO: fav add !ID | fav rm !ID | fav ls | fav auto [on|off]";
    }
    if (topic == "ign") {
        return "USO: ign add !ID | ign del !ID | ign ls | ign clear";
    }
    if (topic == "pos_clear") {
        return "USO: pos_clear\nBorra las coordenadas fijas guardadas.";
    }
    if (topic == "set_pos_tx") {
        if (prefs.pos_tx_secs == 0) {
            snprintf(buf, sizeof(buf), "POS_TX ACT: DESACTIVADO (OFF). USO: set_pos_tx [on|off|minutos]");
        } else {
            snprintf(buf, sizeof(buf), "POS_TX ACT: cada %lu min. USO: set_pos_tx [on|off|minutos]", (unsigned long)(prefs.pos_tx_secs / 60));
        }
        return buf;
    }
    if (topic == "set_nodeinfo_tx") {
        if (prefs.nodeinfo_tx_secs == 0) {
            snprintf(buf, sizeof(buf), "NODEINFO_TX ACT: DESACTIVADO (OFF). USO: set_nodeinfo_tx [on|off|minutos]");
        } else {
            snprintf(buf, sizeof(buf), "NODEINFO_TX ACT: cada %lu min. USO: set_nodeinfo_tx [on|off|minutos]", (unsigned long)(prefs.nodeinfo_tx_secs / 60));
        }
        return buf;
    }
    if (topic == "set_telem_tx") {
        if (prefs.telem_tx_secs == 0) {
            snprintf(buf, sizeof(buf), "TELEM_TX ACT: DESACTIVADO (OFF). USO: set_telem_tx [on|off|minutos]");
        } else {
            snprintf(buf, sizeof(buf), "TELEM_TX ACT: cada %lu min. USO: set_telem_tx [on|off|minutos]", (unsigned long)(prefs.telem_tx_secs / 60));
        }
        return buf;
    }
    if (topic == "set_preset") {
        snprintf(buf, sizeof(buf), "PRESET ACT: %s. USO: set_preset [long_fast|medium_fast|short_fast|long_slow|short_slow|medium_slow|long_moderate|short_turbo]", config.lora.use_preset ? "PRESET" : "CUSTOM");
        return buf;
    }
    if (topic == "set_lora") {
        snprintf(buf, sizeof(buf), "LORA ACT: BW%u SF%u CR4/%u Freq:%.4f Slot:%u. USO: set_lora <bw> <sf> <cr> <freq_mhz> <slot> [txpower]", config.lora.bandwidth, config.lora.spread_factor, config.lora.coding_rate, config.lora.override_frequency, config.lora.channel_num);
        return buf;
    }
    if (topic == "set_freq") {
        snprintf(buf, sizeof(buf), "FREQ ACT: %.4f MHz (Slot %u). USO: set_freq <freq_mhz> [slot]", config.lora.override_frequency, config.lora.channel_num);
        return buf;
    }
    if (topic == "panic") {
        snprintf(buf, sizeof(buf), "PANIC: %s. USO: panic <preset|sfnarrow> [minutos=10] [rollback_mins=0]", prefs.panic_active ? "ACTIVO" : "INACTIVO");
        return buf;
    }
    if (topic == "panic_ok") {
        snprintf(buf, sizeof(buf), "PANIC_OK: Consolida permanentemente el salto de panico activo.");
        return buf;
    }
    return helpForCommand(topic);
}

std::string NavaCLIModule::base64Encode(const uint8_t *data, size_t len)
{
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= len) {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += tbl[(n >> 6) & 63];
        out += tbl[n & 63];
        i += 3;
    }
    if (i + 1 == len) {
        uint32_t n = data[i] << 16;
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += "==";
    } else if (i + 2 == len) {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += tbl[(n >> 6) & 63];
        out += '=';
    }
    return out;
}

