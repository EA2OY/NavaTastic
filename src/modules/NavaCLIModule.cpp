#include "NavaCLIModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "Router.h"
#include "main.h"
#include "memGet.h"
#include "FSCommon.h"
#include "power.h"
#include "sleep.h"
#include "modules/TraceRouteModule.h"
#include "modules/PositionModule.h"
#include "modules/NodeInfoModule.h"
#include "mesh/RadioLibInterface.h"
#include "buzz/buzz.h"
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
}

void NavaCLIModule::loadResiliencePrefs() {
    if (FSCom.exists("/resilience.bin")) {
        File f = FSCom.open("/resilience.bin", FILE_O_READ);
        if (f) {
            size_t fileSize = f.size();
            f.read((uint8_t*)&prefs, sizeof(prefs));
            f.close();
            if (prefs.magic == 0x52455349) {
                // Migrar ficheros de versiones previas, sin marcador V2, de tamano
                // distinto al esperado (FS corrupto: FILE_O_WRITE no trunca) o con
                // campos fuera de rango. Los campos legacy VALIDOS se preservan.
                if (fileSize != sizeof(prefs) || prefs.version != 0x4E415653 || prefs.chemistry > 3 ||
                    prefs.vbat_cutoff < 2400 || prefs.vbat_cutoff > 3600 || prefs.vwake_level < 1 || prefs.vwake_level > 5 ||
                    prefs.tx_disabled > 1 || prefs.ble_disabled > 1 || prefs.auto_fav > 1 ||
                    (prefs.role > meshtastic_Config_DeviceConfig_Role_ROUTER && prefs.role != 0xFF) ||
                    prefs.autoFavCount > 16 || prefs.sleepMsgs > 1 || prefs.wasInSleep > 1) {
                    prefs.auto_fav = 1;
                    prefs.role = 0xFF; // sin rol fijado (fichero de version previa o corrupto)
                    prefs.autoFavCount = 0;
                    memset(prefs.autoFavIds, 0, sizeof(prefs.autoFavIds));
                    prefs.sleepMsgs = 1;
                    prefs.wasInSleep = 0;
                    prefs.reserved = 0;
                    if (prefs.chemistry > 3) {
                        #if defined(USERPREFS_BATTERY_CHEMISTRY_SODIUM)
                            prefs.chemistry = 2; // SODIUM
                        #else
                            prefs.chemistry = 0; // LIPO
                        #endif
                    }
                    if (prefs.vbat_cutoff < 2400 || prefs.vbat_cutoff > 3600) {
                        #if defined(USERPREFS_BATTERY_CHEMISTRY_SODIUM)
                            prefs.vbat_cutoff = 2600;
                        #else
                            prefs.vbat_cutoff = 3500;
                        #endif
                    }
                    if (prefs.vwake_level < 1 || prefs.vwake_level > 5) {
                        #if defined(USERPREFS_BATTERY_CHEMISTRY_SODIUM)
                            prefs.vwake_level = 1;
                        #else
                            prefs.vwake_level = 3;
                        #endif
                    }
                    if (prefs.tx_disabled > 1) prefs.tx_disabled = 0;
                    if (prefs.ble_disabled > 1) prefs.ble_disabled = 0;
                    prefs.version = 0x4E415653; // NAVARICO: F15 - marcador de formato
                    saveResiliencePrefs();
                }
                navaAutoFavoriteEnabled = (prefs.auto_fav != 0);
                // Aplicar parámetros cargados a RAM
                power->setChemistryProfile(prefs.chemistry);
                power->updateOcvCurve(prefs.vbat_cutoff);
                config.lora.tx_enabled = (prefs.tx_disabled == 0);
                currentWakeLevel = prefs.vwake_level;
                if (prefs.ble_disabled == 1) {
                    // BLE off: usar el switch nativo de Meshtastic (startDisabled:
                    // advertising parado + tx power -40). Se fija en config y se
                    // deja que setBluetoothEnable tome esa rama en el arranque.
                    config.bluetooth.enabled = false;
                    setBleForceDisabled(true);
                } else {
                    config.bluetooth.enabled = true;
                    setBleForceDisabled(false);
                }
                // V2.1 Rama 1 y Rama 2: rol semi-permanente (sobrevive a factory reset).
                // 0xFF = sin fijar. Valores validos: 0=CLIENT, 1=CLIENT_MUTE, 2=ROUTER.
                // Con CLIENT/CLIENT_MUTE installRoleDefaults no cambia nada (solo aplica
                // defaults a roles de infraestructura).
                if (prefs.role <= meshtastic_Config_DeviceConfig_Role_ROUTER) {
                    config.device.role = (meshtastic_Config_DeviceConfig_Role)prefs.role;
                    nodeDB->installRoleDefaults(config.device.role);
                }
                return;
            }
        }
    }
    
    // Fallback valores por defecto
    prefs.magic = 0x52455349;
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
    prefs.sleepMsgs = 1;
    prefs.wasInSleep = 0;
    prefs.reserved = 0;
    prefs.version = 0x4E415653; // NAVARICO: F15 - marcador de formato
    navaAutoFavoriteEnabled = true;
    setBleForceDisabled(false);
    saveResiliencePrefs();
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
                if (fileSize != sizeof(tmp) || tmp.version != 0x4E415653) {
                    // NAVARICO: F15 - fichero de version previa, corrupto o sin
                    // marcador V2: defaults, igual que la migracion de loadResiliencePrefs
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
                    if (fileSize != sizeof(tmp) || tmp.version != 0x4E415653) {
                        // V2: migrar fichero previo o sin marcador (defaults de los campos nuevos)
                        tmp.autoFavCount = 0;
                        memset(tmp.autoFavIds, 0, sizeof(tmp.autoFavIds));
                        tmp.sleepMsgs = 1;
                        tmp.reserved = 0;
                        // NAVARICO: F15 - el byte 11 era padding en R2IG previo: rol sin fijar
                        tmp.role = 0xFF;
                        tmp.version = 0x4E415653;
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
        tmp.version = 0x4E415653; // NAVARICO: F15 - marcador de formato
    }
    tmp.wasInSleep = on ? 1 : 0;
    // NAVARICO: F15 - el FILE_O_WRITE de InternalFS NO trunca: recrear el fichero
    // para garantizar tamano exacto y evitar ficheros corruptos que crecen.
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

void NavaCLIModule::saveResiliencePrefs() {
    // NAVARICO: F15 - el FILE_O_WRITE de InternalFS NO trunca: recrear el fichero
    // para garantizar tamano exacto y evitar ficheros corruptos que crecen.
    FSCom.remove("/resilience.bin");
    File f = FSCom.open("/resilience.bin", FILE_O_WRITE);
    if (f) {
        f.write((uint8_t*)&prefs, sizeof(prefs));
        f.close();
    }
}

// --- V2: mensajes de sueño/vivo/listo (canal Navadmin) ---
// V2.4: nombre legible de la causa del ultimo reset (RESETREAS del nRF52) para [Boot]
static const char *navaricoResetReasonName(uint32_t reason)
{
    if (reason & (1UL << 1)) return "WDT";        // watchdog del firmware
    if (reason & (1UL << 3)) return "LOCKUP";     // CPU lockup
    if (reason & (1UL << 0)) return "RESETPIN";   // pin de reset externo (ATtiny/boton)
    if (reason & (1UL << 2)) return "SOFT";       // soft reset (reboot/storm/flash)
    if (reason & (1UL << 17)) return "LPCOMP";    // wake de System OFF por LPCOMP
    if (reason & (1UL << 16)) return "OFF";       // wake de System OFF (GPIO)
    if (reason & (1UL << 20)) return "VBUS";      // deteccion de USB
    return "?";
}

std::string NavaCLIModule::buildEnergyLine()
{
    char buf[96];
    int adcMv = powerStatus ? powerStatus->getBatteryVoltageMv() : 0;
    std::string line = "ADC " + std::to_string(adcMv) + " mV";
    // V2.6: solo ADC + temperatura del CHIP (sensor interno del nRF52): los sensores
    // I2C se inicializan mas tarde y no estan disponibles en estos momentos. Reintento
    // con breve espera: a los pocos segundos del boot el SoftDevice puede rechazar la
    // primera lectura (BUSY).
#ifdef NRF52840_XXAA
    {
        int32_t tempRaw = 0;
        uint32_t rc = sd_temp_get(&tempRaw);
        if (rc != NRF_SUCCESS) {
            delay(100);
            rc = sd_temp_get(&tempRaw);
        }
        if (rc == NRF_SUCCESS) {
            snprintf(buf, sizeof(buf), " | CPU %.1f C", tempRaw / 4.0f);
            line += buf;
        }
    }
#endif
    return line;
}

bool NavaCLIModule::handleLowBatteryEvent()
{
    if (!prefs.sleepMsgs || sleepPending) return false;
    // V2.6: sin atajos: el monitor ya confirmo las 5 lecturas bajas (~100s operando).
    // Se anuncia [Sueno] y se duerme TODO (doDeepSleep con la cadena completa, como Eclipse).
    prefs.wasInSleep = 1;
    saveResiliencePrefs();
    char buf[220];
    snprintf(buf, sizeof(buf), "[Sueno] %s id%08x | %s | sueno profundo, despertara >= %u mV",
             owner.long_name, (unsigned int)nodeDB->getNodeNum(), buildEnergyLine().c_str(),
             (unsigned int)navaGetLpcompWakeMv());
    enqueueResponse(NODENUM_BROADCAST, 1, buf, true, true);
    sleepPending = true;
    sleepTime = millis() + 5000; // estimacion; se recalcula tras el envio real
    return true;
}

// --- V2: listado persistente de auto-favoritos (distincion Auto/Manual real tras reinicio) ---
bool NavaCLIModule::isAutoFav(uint32_t nodeNum) const
{
    for (uint8_t i = 0; i < prefs.autoFavCount && i < sizeof(prefs.autoFavIds) / sizeof(prefs.autoFavIds[0]); i++) {
        if (prefs.autoFavIds[i] == nodeNum) return true;
    }
    return false;
}

bool NavaCLIModule::addAutoFav(uint32_t nodeNum)
{
    if (isAutoFav(nodeNum)) return false;
    if (prefs.autoFavCount >= sizeof(prefs.autoFavIds) / sizeof(prefs.autoFavIds[0])) return false;
    prefs.autoFavIds[prefs.autoFavCount++] = nodeNum;
    return true;
}

bool NavaCLIModule::removeAutoFav(uint32_t nodeNum)
{
    for (uint8_t i = 0; i < prefs.autoFavCount && i < sizeof(prefs.autoFavIds) / sizeof(prefs.autoFavIds[0]); i++) {
        if (prefs.autoFavIds[i] == nodeNum) {
            for (uint8_t j = i; j + 1 < prefs.autoFavCount; j++) {
                prefs.autoFavIds[j] = prefs.autoFavIds[j + 1];
            }
            prefs.autoFavCount--;
            return true;
        }
    }
    return false;
}

void NavaCLIModule::reconcileAutoFavs()
{
    if (!navaAutoFavoriteEnabled || !router) return;
    bool changed = false;
    for (NodeNum num : router->activeDirectRouters) {
        meshtastic_NodeInfoLite *n = nodeDB->getMeshNode(num);
        if (n && n->is_favorite && addAutoFav(num)) {
            changed = true;
        }
    }
    if (changed) saveResiliencePrefs();
}

bool NavaCLIModule::wantPacket(const meshtastic_MeshPacket *p)
{
    if (p != nullptr) {
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

    if (p != nullptr && p->decoded.portnum == ourPortNum && p->decoded.payload.size >= 5) {
        // Regla de aislamiento: solo aceptamos si es mensaje directo (DM)
        // o si viaja por el Canal 1 (Navadmin)
        bool isDM = !isBroadcast(p->to) && (p->to == nodeDB->getNodeNum());
        bool isChannel1 = (p->channel == 1);
        
        if (isDM || isChannel1) {
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

    std::string text((char *)mp.decoded.payload.bytes, mp.decoded.payload.size);
    std::string cmd = (text.length() > 6) ? text.substr(6) : "";

    // Determinar canal y destinatario de la respuesta
    uint8_t replyChannel = 0;
    NodeNum replyDest = mp.from;
    if (mp.channel == 1) {
        replyChannel = 1;
        replyDest = NODENUM_BROADCAST;
    }

    // --- AUTENTICACIÓN ---
    if (replyChannel == 0) {
        // Mensaje Directo: DEBE estar cifrado por PKI (evita suplantar la ID del admin).
        if (!mp.pki_encrypted) {
            LOG_WARN("Rechazado: Comando /nava por DM no cifrado PKI desde 0x%08x", mp.from);
            return ProcessMessage::STOP;
        }

        const meshtastic_NodeInfoLite *senderNode = nodeDB->getMeshNode(mp.from);
        if (!senderNode) {
            // Caso límite: DM PKI de un nodo sin entrada en NodeDB. Normalmente esto
            // no llega (PKI exige clave pública en la DB), pero respondemos una vez.
            if (unauthorizedReplied.insert(mp.from).second) {
                LOG_WARN("Rechazado: DM PKI de nodo sin registrar 0x%08x", mp.from);
                enqueueResponse(mp.from, 0, "NODO NO REGISTRADO EN NODEDB", true);
            }
            return ProcessMessage::STOP;
        }
        if (!nodeDB->isAdminNode(*senderNode)) {
            // Caso 1: nodo conocido pero no acreditado como admin. Respondemos una sola
            // vez por nodo para dar feedback sin abrir un vector de abuso por aire.
            if (unauthorizedReplied.insert(mp.from).second) {
                LOG_WARN("Rechazado: nodo 0x%08x no es admin verificado", mp.from);
                enqueueResponse(mp.from, 0, "NO AUTORIZADO COMO ADMINISTRADOR", true);
            }
            return ProcessMessage::STOP;
        }
    } else {
        // Canal Navadmin (1): solo responden los admins verificados, en silencio.
        const meshtastic_NodeInfoLite *senderNode = nodeDB->getMeshNode(mp.from);
        if (!senderNode || !nodeDB->isAdminNode(*senderNode)) {
            LOG_WARN("Rechazado: Comando /nava en canal sin firma PKI desde 0x%08x", mp.from);
            return ProcessMessage::STOP;
        }
        // Rate-limit generico del canal 1 (PSK publica, from falsificable): max 1 comando
        // cada 30s por nodo emisor para evitar agotamiento de bateria/airtime remoto.
        static std::map<NodeNum, uint32_t> lastChannel1Cmd;
        auto it = lastChannel1Cmd.find(mp.from);
        if (it != lastChannel1Cmd.end() && (int32_t)(millis() - it->second) < 30000) {
            return ProcessMessage::STOP; // Silencio: no revelar rate-limit
        }
        lastChannel1Cmd[mp.from] = millis();
    }

    executeCommand(mp.from, cmd, replyChannel, replyDest, mp.rx_snr);
    return ProcessMessage::STOP;
}

void NavaCLIModule::enqueueResponse(NodeNum toNode, uint8_t channel, const std::string &msg, bool isFirstFragment, bool quick)
{
    size_t pos = 0;
    while (pos < msg.length() && responseQueue.size() < 10) {
        NavaResponse resp;
        resp.dest = toNode;
        resp.channel = channel;
        size_t len = std::min<size_t>(190, msg.length() - pos);
        if (pos + len < msg.length()) {
            // Fix estetico V2: cortar primero en el ultimo salto de linea de la
            // ventana (lineas enteras en cada fragmento). Si la linea es mas larga
            // que la ventana, recaer en el ultimo espacio (no partir palabras).
            size_t cut = msg.find_last_of('\n', pos + len - 1);
            if (cut >= pos && cut <= pos + len - 1) {
                len = cut - pos + 1; // incluir el \n en el fragmento actual
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
            pos++; // no arrastrar el espacio de separacion al siguiente fragmento
        }
    }
    if (pos < msg.length()) {
        NavaResponse resp;
        resp.dest = toNode;
        resp.channel = channel;
        resp.text = "... [TRUNCADO POR LIMITES DE MTU]";
        responseQueue.push(resp);
    }

    if (isFirstFragment && channel == 1) {
        // Jitter dinámico anticolisión para el Canal 1. quick=true (mensajes de
        // sueno/vivo/listo): ventana corta 300-2300ms para que el re-sueno no
        // mate la transmision (la radio tarda ~1s en emitir SFNarrow).
        uint32_t jitter = quick ? 300 + (nodeDB->getNodeNum() % 8) * 250 + (rand() % 300)
                                : 500 + (nodeDB->getNodeNum() % 5) * 1200 + (rand() % 1000);
        setIntervalFromNow(jitter);
    } else {
        setIntervalFromNow(50);
    }
}

void NavaCLIModule::executeCommand(NodeNum fromNode, std::string cmd, uint8_t replyChannel, NodeNum replyDest, float rxSnr)
{
    // Limpieza de espacios y saltos de línea al final
    while (!cmd.empty() && (cmd.back() == ' ' || cmd.back() == '\r' || cmd.back() == '\n' || cmd.back() == '\t')) {
        cmd.pop_back();
    }

    // Normalizar comando a minúsculas para comparaciones (ANTES del filtro de canal,
    // para que REBOOT/SET_* en mayusculas no puedan saltarse la whitelist del canal 1)
    for (size_t i = 0; i < cmd.length() && i < 15; i++) {
        if (cmd[i] == '"') break;
        cmd[i] = tolower(cmd[i]);
    }
    
    // 1. Filtrado dinámico individual (!ID)
    if (cmd.rfind("!", 0) == 0) {
        size_t spacePos = cmd.find(" ");
        if (spacePos != std::string::npos) {
            std::string targetIdStr = cmd.substr(1, spacePos - 1);
            uint32_t targetId = strtoul(targetIdStr.c_str(), NULL, 16);
            if (targetId != nodeDB->getNodeNum()) {
                return; // Abortar en silencio, no es para nosotros
            }
            cmd = cmd.substr(spacePos + 1);
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
                return; // Abortar en silencio
            }
            cmd = cmd.substr(spacePos + 1);
        }
    }

    // 3. Filtro de canal híbrido seguro: WHITELIST (denegar por defecto)
    // Solo comandos de lectura/diagnóstico no destructivos. Todo lo demás solo DM cifrado PKI.
    if (replyChannel == 1) { // Viene por el canal Navadmin
        bool permitido = (cmd == "help" || cmd.rfind("help ", 0) == 0 ||
                          cmd == "ping" || cmd == "status" || cmd == "env" ||
                          cmd == "channel" || cmd == "peers" || cmd == "rxlog" || cmd == "afc" ||
                          cmd == "reset_reason" || cmd == "noise" || cmd == "bat" ||
                          cmd.rfind("route ", 0) == 0 || cmd.rfind("trace ", 0) == 0);
        if (!permitido) {
            enqueueResponse(replyDest, replyChannel, "ERR: SOLO DM SEGURO", true);
            return;
        }
    }

    // Interrogacion generica: "/nava <cmd> ?" o "/nava <cmd> help" -> ayuda/estado del comando (cualquier comando).
    // Excluido msg: "/nava msg help" sigue difundiendo el texto.
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
            enqueueResponse(replyDest, replyChannel, usageAndState(topic), true);
        } else {
            enqueueResponse(replyDest, replyChannel,
                "CMDS:\n[Q] ping / status / env / channel / peers\n[Q] rxlog / afc / reset_reason / route / noise / bat\n[E] set_chem [lipo/nimh/sodium/lifepo4]\n[E] set_vbat [mV] / set_vwake [1-5]\n[E] storm [h] / storm test1|test2 / txoff / txon / ble [on/off]\n[E] msg [T] / pos / nodeinfo / sendtel / bell\n[E] fav (add/rm/ls/auto) / ign / db_purge / db_clear\n[E] set_name / set_role / set_mqtt / set_tz / set_hops / set_txpower\n[E] sleepmsg [on|off] / reboot / factory_reset / admin_ls / power\n\nAYUDA: /nava help <comando>\nDIR: ![ID] / @[r/c/a] / @name:[pref]", true);
        }
    }
    else if (cmd == "ping") {
        // Rate-limit suave: max 1 ping respondido cada 10s por nodo emisor,
        // para evitar que un admin spammee el canal y gaste aire/bateria.
        auto it = lastPingTime.find(fromNode);
        if (it != lastPingTime.end() && (int32_t)(millis() - it->second) < 10000) {
            return;
        }
        lastPingTime[fromNode] = millis();

        char buf[160];
        uint32_t upSecs = millis() / 1000;
        uint32_t upD = upSecs / 86400;
        uint32_t upH = (upSecs % 86400) / 3600;
        // Piso de ruido si la radio esta disponible
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
        enqueueResponse(replyDest, replyChannel, buf, true);
    }
    else if (cmd == "factory_reset") {
        enqueueResponse(replyDest, replyChannel, "OK: RESET DE FABRICA PROGRAMADO", true);
        // Diferido: se ejecuta en runOnce() cuando la cola de respuestas esté vacía,
        // garantizando que el ACK llegue al aire antes de borrar la configuración.
        factoryResetPending = true;
        rebootScheduled = true;
        rebootTime = millis() + 3000;
    }
    else if (cmd.rfind("fav auto", 0) == 0) {
        std::string arg = (cmd.length() > 9) ? cmd.substr(9) : "";
        while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\r' || arg.back() == '\n')) arg.pop_back();
        if (arg == "on") {
            prefs.auto_fav = 1;
            navaAutoFavoriteEnabled = true;
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: AUTO-FAVORITEO ACTIVADO", true);
        } else if (arg == "off") {
            prefs.auto_fav = 0;
            navaAutoFavoriteEnabled = false;
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: AUTO-FAVORITEO DESACTIVADO", true);
        } else {
            char buf[96];
            uint32_t autoCount = router ? (uint32_t)router->activeDirectRouters.size() : 0;
            snprintf(buf, sizeof(buf), "AUTO-FAV: %s | auto-favs: %u. USO: fav auto [on|off]", navaAutoFavoriteEnabled ? "ON" : "OFF", autoCount);
            enqueueResponse(replyDest, replyChannel, buf, true);
        }
    }
    else if (cmd.rfind("sleepmsg", 0) == 0) {
        // F15 fix: "sleepmsg" son 8 chars + espacio separador -> substr(9)
        std::string arg = (cmd.length() > 9) ? cmd.substr(9) : "";
        while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\r' || arg.back() == '\n')) arg.pop_back();
        if (arg == "on") {
            prefs.sleepMsgs = 1;
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: MENSAJES SUENO/VIVO/LISTO ACTIVADOS", true);
        } else if (arg == "off") {
            prefs.sleepMsgs = 0;
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: MENSAJES SUENO/VIVO/LISTO DESACTIVADOS", true);
        } else {
            char buf[96];
            snprintf(buf, sizeof(buf), "SLEEPMSGS: %s. USO: sleepmsg [on|off]", prefs.sleepMsgs ? "ON" : "OFF");
            enqueueResponse(replyDest, replyChannel, buf, true);
        }
    }
    else if (cmd.rfind("fav add", 0) == 0) {
        if (cmd.length() > 8) {
            std::string idStr = cmd.substr(8);
            while (!idStr.empty() && (idStr.back() == ' ' || idStr.back() == '\r' || idStr.back() == '\n')) idStr.pop_back();
            
            uint32_t targetId = strtoul(idStr.c_str() + (idStr[0] == '!' ? 1 : 0), NULL, 16);
            meshtastic_NodeInfoLite *node = nodeDB->getOrCreateMeshNode(targetId);
            if (node != nullptr) {
                node->is_favorite = true;
                nodeDB->saveToDisk(SEGMENT_NODEDATABASE);
                enqueueResponse(replyDest, replyChannel, "OK: FAVORITO ANADIDO " + idStr, true);
            } else {
                enqueueResponse(replyDest, replyChannel, "ERR: BD LLENA", true);
            }
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("fav"), true);
        }
    }
    else if (cmd.rfind("fav rm", 0) == 0) {
        if (cmd.length() > 7) {
            std::string idStr = cmd.substr(7);
            while (!idStr.empty() && (idStr.back() == ' ' || idStr.back() == '\r' || idStr.back() == '\n')) idStr.pop_back();
            
            uint32_t targetId = strtoul(idStr.c_str() + (idStr[0] == '!' ? 1 : 0), NULL, 16);
            meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(targetId);
            if (node != nullptr) {
                node->is_favorite = false;
                nodeDB->saveToDisk(SEGMENT_NODEDATABASE);
                if (removeAutoFav(targetId)) {
                    saveResiliencePrefs();
                }
                enqueueResponse(replyDest, replyChannel, "OK: FAVORITO ELIMINADO " + idStr, true);
            } else {
                enqueueResponse(replyDest, replyChannel, "ERR: NO ENCONTRADO", true);
            }
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("fav"), true);
        }
    }
    else if (cmd == "fav ls") {
        std::string reply = "FAVORITOS:\n";
        bool empty = true;
        for (uint32_t i = 0; i < nodeDB->getNumMeshNodes(); i++) {
            auto n = nodeDB->getMeshNodeByIndex(i);
            if (n && n->is_favorite) {
                char buf[24];
                snprintf(buf, sizeof(buf), "%s !%08x\n", isAutoFav(n->num) ? "[AUTO]" : "[MAN] ", n->num);
                reply += buf;
                empty = false;
            }
        }
        if (empty) reply += "NINGUNO";
        enqueueResponse(replyDest, replyChannel, reply, true);
    }
    else if (cmd.rfind("ign add", 0) == 0) {
        if (cmd.length() > 8) {
            std::string idStr = cmd.substr(8);
            while (!idStr.empty() && (idStr.back() == ' ' || idStr.back() == '\r' || idStr.back() == '\n')) idStr.pop_back();
            
            uint32_t targetId = strtoul(idStr.c_str() + (idStr[0] == '!' ? 1 : 0), NULL, 16);
            
            if (targetId == fromNode) {
                enqueueResponse(replyDest, replyChannel, "ERR: NO PUEDES IGNORARTE A TI MISMO", true);
                return;
            }
            meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(targetId);
            if (node != nullptr) {
                bool isVerifiedAdmin = nodeDB->isAdminNode(*node);
                bool isHardcodedAdmin = false;
                if (node->user.public_key.size == 32) {
                    if ((config.security.admin_key[0].size == 32 && memcmp(node->user.public_key.bytes, config.security.admin_key[0].bytes, 32) == 0) ||
                        (config.security.admin_key[1].size == 32 && memcmp(node->user.public_key.bytes, config.security.admin_key[1].bytes, 32) == 0) ||
                        (config.security.admin_key[2].size == 32 && memcmp(node->user.public_key.bytes, config.security.admin_key[2].bytes, 32) == 0)) {
                        isHardcodedAdmin = true;
                    }
                }
                if (isVerifiedAdmin || isHardcodedAdmin) {
                    enqueueResponse(replyDest, replyChannel, "ERR: NO SE PUEDE IGNORAR A UN ADMIN", true);
                    return;
                }
            } else {
                // El nodo objetivo no está en caché: no podemos verificar que NO es admin.
                // Invertimos la carga de la prueba: rechazar en lugar de proceder a ciegas.
                enqueueResponse(replyDest, replyChannel, "ERR: NODO DESCONOCIDO, NO SE PUEDE VERIFICAR ADMIN", true);
                return;
            }

            node = nodeDB->getOrCreateMeshNode(targetId);
            if (node != nullptr) {
                node->is_ignored = true;
                node->has_device_metrics = false;
                node->has_position = false;
                node->user.public_key.size = 0;
                memset(node->user.public_key.bytes, 0, sizeof(node->user.public_key.bytes));
                nodeDB->saveToDisk(SEGMENT_NODEDATABASE);
                enqueueResponse(replyDest, replyChannel, "OK: NODO IGNORADO " + idStr, true);
            } else {
                enqueueResponse(replyDest, replyChannel, "ERR: BD LLENA", true);
            }
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("ign"), true);
        }
    }
    else if (cmd.rfind("ign rm", 0) == 0) {
        if (cmd.length() > 7) {
            std::string idStr = cmd.substr(7);
            while (!idStr.empty() && (idStr.back() == ' ' || idStr.back() == '\r' || idStr.back() == '\n')) idStr.pop_back();
            
            uint32_t targetId = strtoul(idStr.c_str() + (idStr[0] == '!' ? 1 : 0), NULL, 16);
            meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(targetId);
            if (node != nullptr) {
                node->is_ignored = false;
                nodeDB->saveToDisk(SEGMENT_NODEDATABASE);
                enqueueResponse(replyDest, replyChannel, "OK: NODO DESBLOQUEADO " + idStr, true);
            } else {
                enqueueResponse(replyDest, replyChannel, "ERR: NO ENCONTRADO", true);
            }
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("ign"), true);
        }
    }
    else if (cmd == "ign ls") {
        std::string reply = "IGNORADOS:\n";
        bool empty = true;
        for (uint32_t i = 0; i < nodeDB->getNumMeshNodes(); i++) {
            auto n = nodeDB->getMeshNodeByIndex(i);
            if (n && n->is_ignored) {
                char buf[16];
                snprintf(buf, sizeof(buf), "!%08x\n", n->num);
                reply += buf;
                empty = false;
            }
        }
        if (empty) reply += "NINGUNO";
        enqueueResponse(replyDest, replyChannel, reply, true);
    }
    else if (cmd == "peers") {
        std::string reply = "VECINOS (0 saltos):\n";
        bool empty = true;
        for (uint32_t i = 0; i < nodeDB->getNumMeshNodes(); i++) {
            auto n = nodeDB->getMeshNodeByIndex(i);
            if (n && n->has_hops_away && n->hops_away == 0) {
                uint32_t nowSecs = millis() / 1000;
                uint32_t agoSecs = (nowSecs > n->last_heard) ? (nowSecs - n->last_heard) : 0;
                char buf[80];
                snprintf(buf, sizeof(buf), "!%08x | R:%s | S:%.1f | Hace:%lus\n", 
                         n->num, getRoleName(n->user.role).c_str(), n->snr, (unsigned long)agoSecs);
                reply += buf;
                empty = false;
            }
        }
        if (empty) reply += "NINGUNO";
        enqueueResponse(replyDest, replyChannel, reply, true);
    }
    else if (cmd == "status") {
        uint32_t manualFavs = 0;
        uint32_t autoFavs = 0;
        for (uint32_t i = 0; i < nodeDB->getNumMeshNodes(); i++) {
            auto n = nodeDB->getMeshNodeByIndex(i);
            if (n && n->is_favorite) {
                if (isAutoFav(n->num)) {
                    autoFavs++;
                } else {
                    manualFavs++;
                }
            }
        }
        char buf[256];
        snprintf(buf, sizeof(buf), "Nodos RAM: %d/80\nFavs huerfanas: %d\nFavs (Manual): %d\nFavs (Auto): %d\nAuto-Fav: %s\nTiempo activo: %lu s\n%s", 
                 nodeDB->getNumMeshNodes(), nodeDB->countOrphanFavorites(), manualFavs, autoFavs, navaAutoFavoriteEnabled ? "ON" : "OFF", (unsigned long)(millis()/1000), buildEnergyLine().c_str());
        enqueueResponse(replyDest, replyChannel, buf, true);
    }
    else if (cmd == "env") {
        uint32_t freeHeap = memGet.getFreeHeap();
        float chipTemp = 0.0f;
        bool hasChipTemp = false;

        #ifdef NRF52840_XXAA
            int32_t temp_raw = 0;
            if (sd_temp_get(&temp_raw) == NRF_SUCCESS) {
                chipTemp = temp_raw / 4.0f;
                hasChipTemp = true;
            }
        #endif

        float latestTemp = 0.0f;
        float latestHum = 0.0f;
        bool hasExtSensor = false;

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR
        if (environmentTelemetryModule) {
            meshtastic_Telemetry extTelem = meshtastic_Telemetry_init_zero;
            if (environmentTelemetryModule->getEnvironmentTelemetry(&extTelem)) {
                if (extTelem.which_variant == meshtastic_Telemetry_environment_metrics_tag) {
                    latestTemp = extTelem.variant.environment_metrics.temperature;
                    latestHum = extTelem.variant.environment_metrics.relative_humidity;
                    hasExtSensor = true;
                }
            }
        }
#endif
        if (!hasExtSensor && hasTelemetryCache) {
            latestTemp = this->latestTemp;
            latestHum = this->latestHum;
            hasExtSensor = true;
        }

        char buf[200];
        char extBuf[64] = "Ext: ERROR/SIN I2C";
        if (hasExtSensor) {
            snprintf(extBuf, sizeof(extBuf), "Ext: %.1f C | %.1f%%", latestTemp, latestHum);
        }
        
        if (hasChipTemp) {
            snprintf(buf, sizeof(buf), "Bat: %d mV\nHeap: %lu B\nChip: %.1f C\n%s", 
                     powerStatus->getBatteryVoltageMv(), (unsigned long)freeHeap, chipTemp, extBuf);
        } else {
            snprintf(buf, sizeof(buf), "Bat: %d mV\nHeap: %lu B\nChip: NA\n%s", 
                     powerStatus->getBatteryVoltageMv(), (unsigned long)freeHeap, extBuf);
        }
        enqueueResponse(replyDest, replyChannel, buf, true);
    }
    else if (cmd == "channel") {
        float channelUtil = 0.0f;
        float txUtil = 0.0f;
        if (airTime) {
            channelUtil = airTime->channelUtilizationPercent();
            txUtil = airTime->utilizationTXPercent();
        }
        char buf[128];
        snprintf(buf, sizeof(buf), "Uso canal: %.1f%%\nUso TX: %.1f%%", channelUtil, txUtil);
        enqueueResponse(replyDest, replyChannel, buf, true);
    }
    else if (cmd.rfind("set_name", 0) == 0) {
        std::string args = (cmd.length() > 9) ? cmd.substr(9) : "";
        std::string longName = "";
        std::string shortName = "";
        
        if (!args.empty()) {
            size_t firstQuote = args.find('"');
            if (firstQuote != std::string::npos) {
                size_t secondQuote = args.find('"', firstQuote + 1);
                if (secondQuote != std::string::npos) {
                    longName = args.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                    size_t thirdQuote = args.find('"', secondQuote + 1);
                    if (thirdQuote != std::string::npos) {
                        size_t fourthQuote = args.find('"', thirdQuote + 1);
                        if (fourthQuote != std::string::npos) {
                            shortName = args.substr(thirdQuote + 1, fourthQuote - thirdQuote - 1);
                        }
                    }
                }
            }
            if (longName.empty() || shortName.empty()) {
                size_t space = args.find(' ');
                if (space != std::string::npos) {
                    longName = args.substr(0, space);
                    shortName = args.substr(space + 1);
                }
            }
        }
        
        if (!longName.empty() && !shortName.empty()) {
            strncpy(owner.long_name, longName.c_str(), sizeof(owner.long_name));
            owner.long_name[sizeof(owner.long_name) - 1] = '\0';
            strncpy(owner.short_name, shortName.c_str(), sizeof(owner.short_name));
            owner.short_name[sizeof(owner.short_name) - 1] = '\0';
            service->reloadOwner(true);
            nodeDB->saveToDisk(SEGMENT_DEVICESTATE | SEGMENT_NODEDATABASE);
            
            std::string reply = "OK: NOMBRE CAMBIADO A \"" + longName + "\" [" + shortName + "]";
            enqueueResponse(replyDest, replyChannel, reply, true);
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_name"), true);
        }
    }
    else if (cmd.rfind("set_role", 0) == 0) {
        std::string roleStr = (cmd.length() > 9) ? cmd.substr(9) : "";
        bool valid = true;
        if (roleStr == "client") {
            config.device.role = meshtastic_Config_DeviceConfig_Role_CLIENT;
        } else if (roleStr == "mute") {
            config.device.role = meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE;
        } else if (roleStr == "router") {
            config.device.role = meshtastic_Config_DeviceConfig_Role_ROUTER;
        } else {
            valid = false;
        }
        
        if (valid) {
            nodeDB->installRoleDefaults(config.device.role);
            owner.is_unmessagable = false;
            owner.has_is_unmessagable = true;
            nodeDB->saveToDisk(SEGMENT_CONFIG | SEGMENT_NODEDATABASE);
            // V2.1 Rama 1 y Rama 2: rol semi-permanente en /resilience.bin (sobrevive
            // a factory reset de la app y de /nava; solo se pierde con nrf erase o
            // corrupcion del fichero -> vuelve al rol del perfil del env)
            prefs.role = (uint8_t)config.device.role;
            saveResiliencePrefs();
            enqueueResponse(replyDest, replyChannel, "OK: ROL CAMBIADO", true);
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_role"), true);
        }
    }
    else if (cmd.rfind("set_mqtt", 0) == 0) {
        std::string stateStr = (cmd.length() > 9) ? cmd.substr(9) : "";
        if (stateStr == "on") {
            moduleConfig.mqtt.enabled = true;
            nodeDB->saveToDisk(SEGMENT_MODULECONFIG);
            enqueueResponse(replyDest, replyChannel, "OK: MQTT ACTIVADO", true);
        } else if (stateStr == "off") {
            moduleConfig.mqtt.enabled = false;
            nodeDB->saveToDisk(SEGMENT_MODULECONFIG);
            enqueueResponse(replyDest, replyChannel, "OK: MQTT DESACTIVADO", true);
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_mqtt"), true);
        }
    }
    else if (cmd.rfind("set_tz", 0) == 0) {
        if (cmd.length() > 7) {
            std::string tzStr = cmd.substr(7);
            strncpy(config.device.tzdef, tzStr.c_str(), sizeof(config.device.tzdef));
            config.device.tzdef[sizeof(config.device.tzdef) - 1] = '\0';
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            enqueueResponse(replyDest, replyChannel, "OK: ZONA HORARIA ESTABLECIDA", true);
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_tz"), true);
        }
    }
    else if (cmd.rfind("set_hops", 0) == 0) {
        if (cmd.length() > 9) {
            std::string hopsStr = cmd.substr(9);
            uint32_t hops = strtoul(hopsStr.c_str(), NULL, 10);
            if (hops >= 1 && hops <= 7) {
                config.lora.hop_limit = hops;
                nodeDB->saveToDisk(SEGMENT_CONFIG);
                char buf[32];
                snprintf(buf, sizeof(buf), "OK: SALTOS ESTABLECIDOS A %d", hops);
                enqueueResponse(replyDest, replyChannel, buf, true);
            } else {
                enqueueResponse(replyDest, replyChannel, "ERR: LOS SALTOS DEBEN SER 1-7", true);
            }
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_hops"), true);
        }
    }
    else if (cmd.rfind("set_txpower", 0) == 0) {
        if (cmd.length() > 12) {
            std::string pwrStr = cmd.substr(12);
            uint32_t pwr = strtoul(pwrStr.c_str(), NULL, 10);
            // NAVARICO: rango de potencia por radio (E22P 0-12 / SX1262 0-22) - lo decide el env
#ifdef NAVARICO_RADIO_E22P
            if (pwr >= 0 && pwr <= 12) {
#else
            if (pwr >= 0 && pwr <= 22) {
#endif
                config.lora.tx_power = pwr;
                nodeDB->saveToDisk(SEGMENT_CONFIG);
                char buf[32];
                snprintf(buf, sizeof(buf), "OK: POTENCIA TX ESTABLECIDA A %d", pwr);
                enqueueResponse(replyDest, replyChannel, buf, true);
            } else {
#ifdef NAVARICO_RADIO_E22P
                enqueueResponse(replyDest, replyChannel, "ERR: LA POTENCIA DEBE SER 0-12", true);
#else
                enqueueResponse(replyDest, replyChannel, "ERR: LA POTENCIA DEBE SER 0-22", true);
#endif
            }
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_txpower"), true);
        }
    }
    else if (cmd == "db_purge") {
        int writeIndex = 1; 
        for (uint32_t i = 1; i < nodeDB->getNumMeshNodes(); i++) {
            auto n = nodeDB->getMeshNodeByIndex(i);
            if (n && (n->is_favorite || nodeDB->isAdminNode(*n))) {
                *nodeDB->getMeshNodeByIndex(writeIndex++) = *n;
            }
        }
        nodeDB->numMeshNodes = writeIndex;
        char buf[64];
        snprintf(buf, sizeof(buf), "OK: RAM PURGADA. Nodos activos: %d", writeIndex);
        enqueueResponse(replyDest, replyChannel, buf, true);
    }
    else if (cmd == "db_clear") {
        nodeDB->numMeshNodes = 1;
        if (router) {
            router->activeDirectRouters.clear();
        }
        nodeDB->saveToDisk(SEGMENT_NODEDATABASE);
        enqueueResponse(replyDest, replyChannel, "OK: BD BORRADA. TODOS LOS NODOS ELIMINADOS.", true);
    }
    else if (cmd == "reboot") {
        enqueueResponse(replyDest, replyChannel, "OK: REINICIO PROGRAMADO", true);
        rebootScheduled = true;
        rebootTime = millis() + 3000;
    }
    // --- NUEVOS COMANDOS SECUENCIA REMOTA 2 ---
    else if (cmd.rfind("set_chem", 0) == 0) {
        std::string chem = (cmd.length() > 9) ? cmd.substr(9) : "";
        if (chem == "lipo") {
            prefs.chemistry = 0;
            prefs.vbat_cutoff = 3500;
            prefs.vwake_level = 3;
        } else if (chem == "nimh") {
            prefs.chemistry = 1;
            prefs.vbat_cutoff = 3400;
            prefs.vwake_level = 3;
        } else if (chem == "sodium") {
            prefs.chemistry = 2;
            prefs.vbat_cutoff = 2600;
            prefs.vwake_level = 3;
        } else if (chem == "lifepo4") {
#if defined(SEEED_SOLAR_NODE) || defined(SEEED_XIAO_NRF52840_KIT) || defined(HELTEC_T114)
            // LPCOMP fijo por hardware: umbral (3_8/2_8 ~3.67-4.04V) > Vmax LiFePO4 (~3.65V)
            enqueueResponse(replyDest, replyChannel, "ERR: LIFEPO4 NO COMPATIBLE, UMBRAL LPCOMP FIJO", true);
            return;
#endif
            prefs.chemistry = 3;
            prefs.vbat_cutoff = 2800;
            prefs.vwake_level = 5;
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_chem"), true);
            return;
        }
        power->setChemistryProfile(prefs.chemistry);
        power->updateOcvCurve(prefs.vbat_cutoff);
        currentWakeLevel = prefs.vwake_level;
        saveResiliencePrefs();
        enqueueResponse(replyDest, replyChannel, "OK: QUIMICA CAMBIADA A " + chem + ". AVISO: ROLLBACK SOLO: nrf erase", true);
    }
    else if (cmd.rfind("set_vbat", 0) == 0) {
        std::string arg = (cmd.length() > 9) ? cmd.substr(9) : "";
        while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\r' || arg.back() == '\n')) arg.pop_back();
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_vbat"), true);
        } else {
            uint32_t val = strtoul(arg.c_str(), NULL, 10);
            if (val >= 2400 && val <= 3600) {
                prefs.vbat_cutoff = val;
                power->updateOcvCurve(val);
                saveResiliencePrefs();
                enqueueResponse(replyDest, replyChannel, "OK: CORTE VBAT ESTABLECIDO. AVISO: ROLLBACK SOLO: nrf erase", true);
            } else {
                enqueueResponse(replyDest, replyChannel, "ERR: RANGO 2400-3600", true);
            }
        }
    }
    else if (cmd.rfind("set_vwake", 0) == 0) {
        std::string arg = (cmd.length() > 10) ? cmd.substr(10) : "";
        while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\r' || arg.back() == '\n')) arg.pop_back();
        if (arg.empty()) {
            enqueueResponse(replyDest, replyChannel, usageAndState("set_vwake"), true);
        } else {
            uint32_t val = strtoul(arg.c_str(), NULL, 10);
            if (val >= 1 && val <= 5) {
                prefs.vwake_level = val;
                currentWakeLevel = val;
                saveResiliencePrefs();
                enqueueResponse(replyDest, replyChannel, "OK: NIVEL VWAKE ESTABLECIDO. AVISO: ROLLBACK SOLO: nrf erase", true);
            } else {
                enqueueResponse(replyDest, replyChannel, "ERR: RANGO 1-5", true);
            }
        }
    }
    else if (cmd == "bat") {
        char buf[128];
        const char* chemNames[] = { "LIPO", "NIMH", "SODIO", "LIFEPO4" };
        snprintf(buf, sizeof(buf), "QUIMICA: %s\nBat: %d mV | OCV: %d%%\nTX: %s", 
                 chemNames[prefs.chemistry], powerStatus->getBatteryVoltageMv(), powerStatus->getBatteryChargePercent(),
                 config.lora.tx_enabled ? "ON" : "OFF");
        enqueueResponse(replyDest, replyChannel, buf, true);
    }
    else if (cmd.rfind("storm", 0) == 0) {
        // Modo test rápido: storm test1 = 60s, storm test2 = 120s
        std::string arg = (cmd.length() > 6) ? cmd.substr(6) : "";
        bool testMode = false;
        uint32_t seconds = 0;
        if (arg == "test1") {
            testMode = true;
            seconds = 60;
        } else if (arg == "test2") {
            testMode = true;
            seconds = 120;
        }
        uint32_t hours = (cmd.length() > 6) ? strtoul(cmd.substr(6).c_str(), NULL, 10) : 0;
        if (testMode) {
            char buf[96];
            snprintf(buf, sizeof(buf), "MODO TORMENTA ACTIVADO. Durmiendo durante %lu segundos.", (unsigned long)seconds);
            enqueueResponse(replyDest, replyChannel, buf, true);
            stormPending = true;
            stormSeconds = seconds;
            stormTime = millis() + 15000; // esperar 15s: da tiempo a transmitir el ACK
        } else if (hours >= 1 && hours <= 720) { // m�x 30 d�as
            char buf[96];
            snprintf(buf, sizeof(buf), "MODO TORMENTA ACTIVADO. Durmiendo durante %lu horas.", (unsigned long)hours);
            enqueueResponse(replyDest, replyChannel, buf, true);
            stormPending = true;
            stormSeconds = hours * 3600;
            stormTime = millis() + 15000; // esperar 15s: da tiempo a transmitir el ACK
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("storm"), true);
        }
    }
    else if (cmd == "txoff") {
        enqueueResponse(replyDest, replyChannel, "OK: TX DESACTIVADO. AVISO: ROLLBACK SOLO: nrf erase", true);
        txOffScheduled = true;
        txOffTime = millis() + 3000;
    }
    else if (cmd == "txon") {
        config.lora.tx_enabled = true;
        prefs.tx_disabled = 0;
        saveResiliencePrefs();
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        enqueueResponse(replyDest, replyChannel, "OK: TX ACTIVADO", true);
    }
    else if (cmd.rfind("ble", 0) == 0) {
        std::string mode = (cmd.length() > 4) ? cmd.substr(4) : "";
        if (mode == "on") {
            prefs.ble_disabled = 0;
            setBleForceDisabled(false);
            saveResiliencePrefs();
            rebootScheduled = true;
            rebootTime = millis() + 3000;
            enqueueResponse(replyDest, replyChannel, "OK: BLE ACTIVADO. REINICIO PROGRAMADO. AVISO: ROLLBACK SOLO: nrf erase", true);
        } else if (mode == "off") {
            prefs.ble_disabled = 1;
            setBleForceDisabled(true);
            saveResiliencePrefs();
            rebootScheduled = true;
            rebootTime = millis() + 3000;
            enqueueResponse(replyDest, replyChannel, "OK: BLE DESACTIVADO. REINICIO PROGRAMADO. AVISO: ROLLBACK SOLO: nrf erase", true);
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("ble"), true);
        }
    }
    else if (cmd == "rxlog") {
        std::string reply = "RXLOG (Ultimas 5):\n";
        for (int i = 0; i < rxLogCount; i++) {
            int idx = (rxLogIndex - 1 - i + 5) % 5;
            char buf[80];
            snprintf(buf, sizeof(buf), "!%08x | P:%d | S:%.1f | R:%d\n",
                     rxLog[idx].from, rxLog[idx].portnum, rxLog[idx].snr, rxLog[idx].rssi);
            reply += buf;
        }
        enqueueResponse(replyDest, replyChannel, reply, true);
    }
    else if (cmd == "afc") {
        char buf[64];
        snprintf(buf, sizeof(buf), "Deriva TCXO: %.2f Hz", lastRxFrequencyError);
        enqueueResponse(replyDest, replyChannel, buf, true);
    }
    else if (cmd == "reset_reason") {
        char buf[64];
        snprintf(buf, sizeof(buf), "Ultimo arranque: 0x%08X", rawResetReason);
        enqueueResponse(replyDest, replyChannel, buf, true);
    }
    else if (cmd.rfind("trace", 0) == 0) {
        if (cmd.length() > 6) {
            uint32_t targetId = strtoul(cmd.substr(6).c_str() + (cmd[6] == '!' ? 1 : 0), NULL, 16);
            traceRouteModule->startTraceRoute(targetId);
            enqueueResponse(replyDest, replyChannel, "OK: TRAZADO DE RUTA INICIADO", true);
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("trace"), true);
        }
    }
    else if (cmd.rfind("route", 0) == 0) {
        if (cmd.length() > 6) {
            uint32_t targetId = strtoul(cmd.substr(6).c_str() + (cmd[6] == '!' ? 1 : 0), NULL, 16);
            meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(targetId);
            if (node != nullptr) {
                char buf[128];
                uint32_t nowSecs = millis() / 1000;
                uint32_t agoSecs = (nowSecs > node->last_heard) ? (nowSecs - node->last_heard) : 0;
                snprintf(buf, sizeof(buf), "RUTA a !%08x:\nSaltos: %d\nSNR: %.1f dB\nOido hace: %lu s",
                         node->num, node->hops_away, node->snr, (unsigned long)agoSecs);
                enqueueResponse(replyDest, replyChannel, buf, true);
            } else {
                // El nodo no está en la NodeDB: no hay ruta conocida todavía.
                // Lanzamos un TraceRoute nativo (no requiere que el nodo esté en la BD)
                // para intentar descubrirlo en la malla.
                traceRouteModule->startTraceRoute(targetId);
                enqueueResponse(replyDest, replyChannel, "NODO NO OIDO AUN. TRAZADO DE RUTA INICIADO.", true);
            }
        } else {
            enqueueResponse(replyDest, replyChannel, usageAndState("route"), true);
        }
    }
    else if (cmd.rfind("msg", 0) == 0) {
        std::string text = (cmd.length() > 4) ? cmd.substr(4) : "";
        // Quitar espacios iniciales del texto
        while (!text.empty() && (text[0] == ' ' || text[0] == '\t')) text.erase(0, 1);
        if (text.empty()) {
            enqueueResponse(replyDest, replyChannel, "ERR: USE msg \"TEXTO\"", true);
        } else {
            meshtastic_MeshPacket *p = allocDataPacket();
            if (p) {
                p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
                p->decoded.payload.size = text.length();
                memcpy(p->decoded.payload.bytes, text.c_str(), text.length());
                p->to = NODENUM_BROADCAST;
                p->channel = 0;
                p->want_ack = false;
                service->sendToMesh(p);
                enqueueResponse(replyDest, replyChannel, "OK: MENSAJE DIFUNDIDO", true);
            } else {
                enqueueResponse(replyDest, replyChannel, "ERR: SIN MEMORIA PARA MENSAJE", true);
            }
        }
    }
    else if (cmd == "bell") {
        playStartMelody();
        enqueueResponse(replyDest, replyChannel, "OK: SONANDO", true);
    }
    else if (cmd == "pos") {
        positionModule->sendOurPosition();
        enqueueResponse(replyDest, replyChannel, "OK: POSICION ENVIADA", true);
    }
    else if (cmd == "nodeinfo") {
        nodeInfoModule->sendOurNodeInfo(NODENUM_BROADCAST, false);
        enqueueResponse(replyDest, replyChannel, "OK: NODEINFO ENVIADO", true);
    }
    else if (cmd == "sendtel") {
#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR
        if (environmentTelemetryModule) {
            environmentTelemetryModule->sendTelemetry(NODENUM_BROADCAST, false);
            enqueueResponse(replyDest, replyChannel, "OK: TELEMETRIA ENVIADA", true);
        } else {
            enqueueResponse(replyDest, replyChannel, "ERR: SIN SENSORES I2C", true);
        }
#else
        enqueueResponse(replyDest, replyChannel, "ERR: TELEMETRIA DESACTIVADA", true);
#endif
    }
    else if (cmd == "power") {
#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR
        if (environmentTelemetryModule) {
            meshtastic_Telemetry extTelem = meshtastic_Telemetry_init_zero;
            if (environmentTelemetryModule->getEnvironmentTelemetry(&extTelem)) {
                if (extTelem.which_variant == meshtastic_Telemetry_environment_metrics_tag) {
                    auto &m = extTelem.variant.environment_metrics;
                    if (m.voltage != 0.0f || m.current != 0.0f) {
                        char buf[160];
                        float pwr_mw = m.voltage * m.current;
                        snprintf(buf, sizeof(buf), "SENSOR DE POTENCIA:\nADC: %d mV\nINA: %.2f V | %+.0f mA %s\nPotencia: %.1f mW",
                                 powerStatus->getBatteryVoltageMv(), m.voltage, m.current,
                                 m.current >= 0.0f ? "CARGANDO" : "DESCARGANDO", pwr_mw);
                        enqueueResponse(replyDest, replyChannel, buf, true);
                        return;
                    }
                }
            }
        }
        enqueueResponse(replyDest, replyChannel, "ERR: SIN SENSOR DE POTENCIA", true);
#else
        enqueueResponse(replyDest, replyChannel, "ERR: TELEMETRIA DESACTIVADA", true);
#endif
    }
    else if (cmd == "noise") {
        if (router && router->getInterface()) {
            RadioLibInterface* rLib = static_cast<RadioLibInterface*>(router->getInterface());
            if (rLib) {
                char buf[64];
                snprintf(buf, sizeof(buf), "Piso de ruido: %d dBm", rLib->getNoiseFloor());
                enqueueResponse(replyDest, replyChannel, buf, true);
                return;
            }
        }
        enqueueResponse(replyDest, replyChannel, "ERR: RADIO APAGADA", true);
    }
    else if (cmd == "admin_ls") {
        std::string reply = "CLAVES ADMIN:\n";
        for (int i = 0; i < 3; i++) {
            if (config.security.admin_key[i].size == 32) {
                std::string b64 = base64Encode(config.security.admin_key[i].bytes, 32);
                reply += "Clave " + std::to_string(i) + ": " + b64 + "\n";
            } else {
                reply += "Clave " + std::to_string(i) + ": VACIA\n";
            }
        }
        enqueueResponse(replyDest, replyChannel, reply, true);
    }
    else {
        enqueueResponse(replyDest, replyChannel, "ERR: COMANDO DESCONOCIDO", true);
    }
}

int32_t NavaCLIModule::runOnce()
{
    // V2: reconciliar el listado persistente de auto-favoritos con los routers
    // directos actuales (persiste solo si cambia algo, max 1 escritura/60s).
    reconcileAutoFavs();

    // V2: primer tick tras el boot: mensajes [Vivo]/[Listo] segun como se desperto
    if (!firstRunDone) {
        firstRunDone = true;
        if (wokeFromSleep || vivoPending) {
            if (vivoPending && prefs.sleepMsgs) {
                // V2.6: despertado por reset externo con bateria en la banda [corte-100,
                // corte): anunciar [Vivo] y SEGUIR OPERANDO con normalidad. El monitor de
                // bateria de siempre (5 lecturas bajas, ~100s) decidira despues si dormir
                // ([Sueno]) o seguir: el ADC puede dar lecturas puntuales erroneas en campo
                // (RF, temperatura) y no se debe dormir sin confirmacion sostenida.
                prefs.wasInSleep = 1;
                saveResiliencePrefs();
                char buf[220];
                snprintf(buf, sizeof(buf), "[Vivo] %s id%08x | %s | sigo vivo, al limite de carga",
                         owner.long_name, (unsigned int)nodeDB->getNodeNum(), buildEnergyLine().c_str());
                enqueueResponse(NODENUM_BROADCAST, 1, buf, true, true);
            } else if (wokeFromSleep && prefs.sleepMsgs) {
                // V2: despertar real por LPCOMP (solar): bateria ya cargada
                prefs.wasInSleep = 0;
                saveResiliencePrefs();
                char buf[220];
                snprintf(buf, sizeof(buf), "[Listo] %s id%08x | %s | despierto, cargando, listo para trabajar",
                         owner.long_name, (unsigned int)nodeDB->getNodeNum(), buildEnergyLine().c_str());
                enqueueResponse(NODENUM_BROADCAST, 1, buf, true, true);
            } else {
                if (wokeFromSleep) {
                    prefs.wasInSleep = 0;
                    saveResiliencePrefs();
                }
            }
        }
    }

    // V2.4: aviso de arranque [Boot] DIFERIDO 2 minutos (idea del operador): sirve para
    // enterarse de reinicios externos/watchdog/brownouts, y el retraso es el anti-bucle:
    // un nodo en ciclo de resets nunca llega a los 2 min -> no inunda la malla. Solo
    // para arranques que NO vienen del ciclo de sueno (esos ya avisan con [Listo]/[Vivo]).
    {
        static bool bootNoticeSent = false;
        static uint32_t bootNoticeAt = 0;
        if (!bootNoticeSent && !wokeFromSleep && !vivoPending && prefs.sleepMsgs) {
            if (bootNoticeAt == 0) {
                bootNoticeAt = millis() + 120000;
            }
            if ((int32_t)(millis() - bootNoticeAt) >= 0) {
                bootNoticeSent = true;
                char buf[220];
                snprintf(buf, sizeof(buf), "[Boot] %s id%08x | %s | causa: 0x%08X (%s)",
                         owner.long_name, (unsigned int)nodeDB->getNodeNum(),
                         buildEnergyLine().c_str(), (unsigned int)rawResetReason,
                         navaricoResetReasonName(rawResetReason));
                enqueueResponse(NODENUM_BROADCAST, 1, buf, true, true);
            }
        }
    }

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
            service->sendToMesh(reply);
        }

        // V2.2: si hay re-sueno pendiente y la cola ya dreno, el sueno se programa
        // DESDE el envio (+3s de margen de airtime), no desde el encolado: sendToMesh
        // es asincrono y antes la radio podia apagarse con el paquete sin emitir.
        if (sleepPending && responseQueue.empty()) {
            sleepTime = millis() + 3000;
        }

        if (!responseQueue.empty()) {
            return 12000; // Retardo de 12 segundos para cumplir con el MTU de LoRa SFNarrow
        }
    }

    // Comprobar si hay un txoff diferido programado
    if (txOffScheduled) {
        if ((int32_t)(millis() - txOffTime) >= 0) {
            config.lora.tx_enabled = false;
            prefs.tx_disabled = 1;
            saveResiliencePrefs();
            nodeDB->saveToDisk(SEGMENT_CONFIG);
            txOffScheduled = false;
            LOG_INFO("LoRa TX disabled via NavaCLI");
        }
    }

    if (rebootScheduled && responseQueue.empty()) {
        if ((int32_t)(millis() - rebootTime) >= 0) {
            if (factoryResetPending) {
                LOG_INFO("Executing deferred factory reset...");
                factoryResetPending = false;
                nodeDB->factoryReset(true);
            }
            LOG_INFO("Executing deferred action (reboot/hibernate)...");
            rebootAtMsec = millis() + 25; 
        }
        return 1000;
    }

    // Ejecutar la hibernación de storm cuando la cola esté vacía y hayan pasado 15s
    if (stormPending && responseQueue.empty() && (int32_t)(millis() - stormTime) >= 0) {
        LOG_INFO("Entering storm mode: %lu seconds", (unsigned long)stormSeconds);
        stormPending = false;
        timedSystemSleepSeconds(stormSeconds);
    }

    // Si hay un storm pendiente, revisar cada segundo (no esperar los 60s del
    // intervalo normal) para que se duerma justo a los 15s de activarse.
    if (stormPending) {
        return 1000;
    }

    // V2.6: sueño por bateria diferido: esperar a que la cola de mensajes drene
    // (envia [Sueño]/[Vivo] al aire ANTES de apagar la radio). Se duerme por la
    // misma puerta que Eclipse (doDeepSleep): preflight + radio->sleep + GPS +
    // pantalla + System OFF -> apagado COMPLETO (~1 mA), para las 6 placas.
    if (sleepPending && responseQueue.empty() && (int32_t)(millis() - sleepTime) >= 0) {
        LOG_INFO("Entering battery sleep after status message");
        sleepPending = false;
        doDeepSleep(portMAX_DELAY, false, true);
        return 1000;
    }
    if (sleepPending) {
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
        return "trace: Lanza un trazado de ruta nativo hacia el nodo indicado. Uso: /nava trace !ID";
    else if (topic == "noise")
        return "noise: Piso de ruido instantaneo del chip de radio. Uso: /nava noise";
    else if (topic == "power")
        return "power: Metricas de energia: ADC interno + INA219/260 (V, +-mA, CARGANDO/DESCARGANDO, mW). SOLO DM SEGURO. Uso: /nava power";
    else if (topic == "bat")
        return "bat: Estado de bateria: quimica activa, voltaje, % OCV y estado TX. Uso: /nava bat";
    else if (topic == "fav")
        return "fav: Gestiona favoritos (nodos con bypass de saltos). Uso: /nava fav add !ID | fav rm !ID | fav ls | fav auto [on|off]";
    else if (topic == "ign")
        return "ign: Bloquea/desbloquea nodos (spam/sabotaje). Uso: /nava ign add !ID | ign rm !ID | ign ls";
    else if (topic == "set_chem")
        return "set_chem: Cambia la quimica y ajusta corte/OCV/LPCOMP. Uso: /nava set_chem [lipo|nimh|sodium|lifepo4]";
    else if (topic == "set_vbat")
        return "set_vbat: Corte de apagado por bateria baja. Uso: /nava set_vbat [2400-3600] mV";
    else if (topic == "set_vwake")
        return "set_vwake: Nivel LPCOMP de reencendido solar. 1=2.1V, 2=2.5V, 3=3.7V (LiPo/NiMH/Sodio), 4=4.5V, 5=3.3V (LiFePO4). Uso: /nava set_vwake [1-5]";
    else if (topic == "storm")
        return "storm: Hibernacion con radio apagada. Uso: /nava storm [1-720]h | storm test1 (60s) | storm test2 (120s)";
    else if (topic == "txoff")
        return "txoff: Apaga la transmision LoRa tras 3s (mantiene la escucha RX). Uso: /nava txoff";
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
        return "set_name: Cambia el nombre largo y corto del nodo. Uso: /nava set_name \"Nombre Largo\" \"Corto\"";
    else if (topic == "set_role")
        return "set_role: Cambia el rol del nodo. Uso: /nava set_role [client|mute|router]";
    else if (topic == "set_mqtt")
        return "set_mqtt: Activa/desactiva MQTT. Uso: /nava set_mqtt [on|off]";
    else if (topic == "set_tz")
        return "set_tz: Establece la zona horaria POSIX. Uso: /nava set_tz [tz_POSIX]";
    else if (topic == "set_hops")
        return "set_hops: Limite de saltos LoRa. Uso: /nava set_hops [1-7]";
    else if (topic == "set_txpower")
        // NAVARICO: ayuda de set_txpower por radio (E22P 0-12 / SX1262 0-22)
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
        return "reboot: Programa un reinicio limpio del nodo a los 3 segundos. Uso: /nava reboot";
    else if (topic == "factory_reset")
        return "factory_reset: Formateo remoto de emergencia; restaura valores de rescate. Uso: /nava factory_reset";
    else if (topic == "admin_ls")
        return "admin_ls: Muestra las 3 claves criptograficas de admin en base64. Uso: /nava admin_ls";
    else if (topic == "sleepmsg")
        return "sleepmsg: Activa/desactiva los avisos de sueno/vivo/listo al canal Navadmin. Uso: /nava sleepmsg [on|off]";
    else if (topic == "help")
        return "help: Muestra la lista de comandos o ayuda de uno concreto. Uso: /nava help [comando]";
}

std::string NavaCLIModule::usageAndState(const std::string &topic)
{
    char buf[220];
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
        // NAVARICO: consulta de set_txpower por radio (E22P 0-12 / SX1262 0-22)
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
        snprintf(buf, sizeof(buf), "NOMBRE: \"%s\" \"%s\". USO: set_name \"Largo\" \"Corto\"", owner.long_name, owner.short_name);
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
        return "USO: ign add !ID | ign rm !ID | ign ls";
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
