#pragma once
#include "concurrency/OSThread.h"
#include "SinglePortModule.h"
#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>

// NAVARICO F22: etiqueta de la generacion de firmware, compilada en el binario.
// Bump manual en CADA release (visible en el commit). Se muestra en /nava status
// y en el aviso [Boot]; permite saber por radio que version lleva un nodo.
#ifndef NAVATASTIC_BUILD
#define NAVATASTIC_BUILD "V5"
#endif

// NAVARICO F21/F22/V5: marcador de formato del struct ResiliencePrefs. Bump V5 (0x4E415636,
// "NAV6"): soporte para persistencia blindada de Capa Física LoRa (Preset/Custom),
// Canal 0 Primario, Protocolo "Botón del Pánico" y ampliación a 32 auto-favoritos.
// Ficheros previos ("NAV5" / "NAV4" / "NAV3" / "NAVS") migran con defaults seguros.
#define NAVS_RESILIENCE_VERSION 0x4E415636

struct NavaResponse {
    NodeNum dest;
    uint8_t channel;
    std::string text;
    uint8_t hops; // Hop-Aware Timing
};

// Estructura de logs de paquetes recibidos
struct RxLogEntry {
    uint32_t from;
    uint8_t portnum;
    int8_t snr;
    int16_t rssi;
    uint32_t timestamp;
};

// NAVARICO F21: Estructura para respaldar canales secundarios (slots 2..7) en /resilience.bin
struct ResilientChannel {
    char name[12];
    uint8_t psk[32];
    uint8_t psk_len;
    uint8_t uplink_enabled;
    uint8_t downlink_enabled;
    uint8_t is_active;
};

// NAVARICO F21: Estructura circular en RAM para log forense de eventos (100% RAM-Only)
struct NavaLogEntry {
    uint32_t uptime;
    char msg[48];
};

// NAVARICO V5: Pulso binario ultracorto para Protocolo "Botón del Pánico" (exactamente 24 Bytes)
struct __attribute__((packed)) NavaPanicPulse {
    uint32_t magic;             // 0x50414E43 ("PANC")
    uint8_t use_preset;         // 1=Preset estándar, 0=Custom
    uint8_t modem_preset;       // 0..13 enum meshtastic_Config_LoRaConfig_ModemPreset
    uint8_t sf;                 // Spreading Factor (5..12)
    uint8_t cr;                 // Coding Rate (4..8)
    uint16_t bw_code;           // Bandwidth Code (31, 62, 125, 200, 250, 500)
    uint16_t channel_slot;      // Slot de canal (1..N)
    float freq_mhz;             // Frecuencia exacta en MHz (ej. 869.618f)
    uint16_t remaining_seconds; // Segundos restantes de cuenta atrás monotónica
    uint16_t rollback_minutes;  // Minutos de espera en destino antes de rollback (0=permanente)
    uint32_t sender_nodenum;    // Nodo emisor / retransmisor
};

// Estructura binaria persistente a reset de fábrica
struct ResiliencePrefs {
    uint32_t magic;         // 0x52455349
    uint16_t vbat_cutoff;   // 2400-3600
    uint8_t vwake_level;    // 1-5
    uint8_t chemistry;      // 0=LIPO, 1=NIMH, 2=SODIUM, 3=LIFEPO4
    uint8_t tx_disabled;    // 0=ON, 1=OFF
    uint8_t ble_disabled;   // 0=ON, 1=OFF
    uint8_t auto_fav;       // 1=auto-favoriteo ON (default), 0=OFF (/nava fav auto)
    uint8_t role;           // V2.1 Rama 1 y Rama 2: rol semi-permanente. 0xFF=sin fijar; 0=CLIENT, 1=CLIENT_MUTE, 2=ROUTER. Sobrevive a factory reset
    uint32_t autoFavIds[32]; // V5: ampliado de 16 a 32 ids de nodos favoritados por auto-fav
    uint8_t autoFavCount;    // V5: número de ids válidos en autoFavIds (0-32)
    uint8_t sleepMsgs;       // V2: 1=mensajes sueño/vivo/listo ON (default), 0=OFF (/nava sleepmsg)
    uint8_t wasInSleep;      // V2: 1=dormido por bateria (se setea antes de cpuDeepSleep, se lee al boot para Listo/Vivo)
    uint8_t reserved;        // V2: reservado
    uint32_t version;        // Marcador de formato NAVS_RESILIENCE_VERSION ("NAV6"); ficheros previos migran con defaults
    // NAVARICO F20 (V3): claves admin PUBLICAS del usuario persistidas para que sobrevivan
    // a los resets de fabrica. keySlot0Own = clave propia que el usuario puso en slot 0
    // (desautorizando la de fabrica): se restaura EN el slot 0 (regla "slot 0 = estado
    // previo del usuario"); keySlot1/keySlot2 se restauran en sus slots si estan vacios.
    uint8_t keySlot1[32];
    uint8_t keySlot2[32];
    uint8_t keySlot0Own[32];
    // NAVARICO F21: parámetros semi-permanentes de canales y gestión de infraestructura
    uint8_t cliChannelSlot;              // Slot asignado a NavaCLI (1-7, default: 1)
    uint8_t navadminMuted;               // 0=Navadmin (Canal 1) activo, 1=silenciado
    ResilientChannel customChannels[6];  // Slots 2..7 respaldados
    uint8_t ok_to_mqtt;                  // 0=default, 1=ON, 2=OFF
    uint32_t fixed_pin;                  // PIN BT fijo (>0 si personalizado)
    int32_t fixed_pos_lat;               // Latitud * 1e7
    int32_t fixed_pos_lon;               // Longitud * 1e7
    int32_t fixed_pos_alt;               // Altitud (metros)
    uint8_t fixed_pos_enabled;           // 1=Posición fija activa
    uint32_t beacon_interval_secs;       // Intervalo de baliza en segundos
    // NAVARICO F22: Control de difusión periódica de flota y lista negra persistente
    uint32_t pos_tx_secs;                // Difusión de posición (0=OFF, >0 segundos, default: 259200 = 72h)
    uint32_t nodeinfo_tx_secs;           // Difusión de NodeInfo (0=OFF, >0 segundos, default: 259200 = 72h)
    uint32_t telem_tx_secs;              // Difusión de Telemetría (0=OFF, >0 segundos, default: 900s)
    uint32_t ignoredNodes[8];            // Lista negra global de nodos ignorados (NodeNum)
    uint8_t ignoredCount;                // Número de nodos ignorados (0-8)
    // NAVARICO V5: Bloque A - Capa Física LoRa Persistente
    uint8_t lora_use_preset;             // 0=Custom, 1=Preset estándar
    uint8_t lora_modem_preset;           // 0..13 meshtastic_Config_LoRaConfig_ModemPreset
    uint32_t lora_bandwidth;             // 31, 62, 125, 200, 250, 500 kHz
    uint8_t lora_spread_factor;          // SF 5..12
    uint8_t lora_coding_rate;            // CR 4..8
    uint32_t lora_channel_num;           // Slot de frecuencia (1..N)
    float lora_override_frequency;       // Frecuencia explícita (863.0000f - 873.3000f MHz)
    uint8_t lora_tx_power;               // Potencia de transmisión (1..22 dBm)
    uint8_t lora_configured;             // 1=parámetros LoRa configurados/activos
    // NAVARICO V5: Bloque B - Capa Lógica Canal 0 Primario Persistente
    char ch0_name[12];                   // Nombre del Canal 0
    uint8_t ch0_psk[32];                 // Clave PSK (1B 0x01 o 16B/32B AES)
    uint8_t ch0_psk_len;                 // Longitud de PSK (1, 16 o 32)
    uint8_t ch0_configured;              // 1=Canal 0 configurado/activo
    // NAVARICO V5: Bloque C - Protocolo "Botón del Pánico" (Evacuación de Emergencia)
    uint8_t panic_active;                // 1=modo pánico / evacuación en curso
    uint8_t panic_target_preset;         // Preset destino
    uint8_t panic_target_sf;             // SF destino
    uint8_t panic_target_cr;             // CR destino
    uint32_t panic_target_bw;            // BW destino
    uint32_t panic_target_slot;          // Slot destino
    float panic_target_freq;             // Frecuencia destino
    uint32_t panic_rollback_mins;        // Minutos de espera en destino para rollback (0=permanente)
    uint32_t panic_target_time_ms;       // Instante millis() absoluto del salto
    uint32_t panic_last_pulse_ms;        // Último pulso de pánico emitido
    uint8_t panic_trial_active;          // 1=nodo operando en modo prueba post-salto esperando panic_ok
    uint32_t panic_trial_deadline_ms;    // Límite millis() para recibir panic_ok antes de revertir
    // NAVARICO V5: Bloque D - Nombre Persistente de Nodo (Hardcodeo / Modo Natural)
    char custom_long_name[40];           // Nombre largo persistente (/nava set_name)
    char custom_short_name[5];           // Nombre corto persistente (4 caracteres + '\0')
};

enum NavaDeferredAction {
    NAVA_DEFERRED_NONE = 0,
    NAVA_DEFERRED_REBOOT,
    NAVA_DEFERRED_FACTORY_RESET,
    NAVA_DEFERRED_FULL_RESET,
    NAVA_DEFERRED_WIPE,
    NAVA_DEFERRED_STORM,
    NAVA_DEFERRED_TXOFF,
    NAVA_DEFERRED_KEYS_CLEAR,
    NAVA_DEFERRED_LORA_CHANGE,
    NAVA_DEFERRED_PANIC_JUMP
};

class NavaCLIModule : public SinglePortModule, public concurrency::OSThread
{
  public:
    NavaCLIModule();

    // V2: acceso estatico desde main.cpp (pre-check de arranque, antes de construir el modulo)
    static bool peekSleepMsgsEnabled();
    static bool peekWasInSleep();
    static void navaSetWasInSleep(bool on);
    static void navaSetVivoPending();
    static bool navaGetVivoPending();
    static void navaSetReservaPending();
    static bool navaGetReservaPending();

    // NAVARICO F21/F22/V5: chequeos estáticos para enrutamiento y diagnóstico en RAM
    static bool navaIsMuteActive();
    static void recordRoutedPacket();
    static void logRamEvent(const char *msg);
    static bool isNodeIgnored(NodeNum node);
    static bool navaIsPanicActive();
    static bool navaIsPanicTunnelMode();

    // V2: el monitor de bateria (Power.cpp) delega el sueño aqui para mandar el
    // mensaje [Sueño] antes de dormir. Devuelve true si tomo el control.
    bool handleLowBatteryEvent();

    // NAVARICO F20: sincronizar las claves admin del usuario (slots 0-2 de la config)
    // hacia /resilience.bin. Lo llama AdminModule tras cada set_config de seguridad.
    void syncAdminKeysFromConfig();

    // NAVARICO V5: Sincronización bidireccional transparente desde App Oficial (AdminModule.cpp)
    void syncDeviceRoleFromConfig();
    void syncOkToMqttFromConfig();
    void syncTelemetryIntervalFromConfig();
    void syncNodeInfoIntervalFromConfig();
    void syncPositionIntervalFromConfig();
    void syncFixedPositionFromConfig();
    void syncBluetoothPinFromConfig();
    void syncLoraConfigFromConfig();
    void syncChannel0FromConfig();
    void syncCustomChannelFromConfig(uint8_t slot);

  protected:
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual int32_t runOnce() override;

  private:
    // La cola almacena respuestas de tipo NavaResponse
    std::queue<NavaResponse> responseQueue;
    
    // NAVARICO V5: Gestión unificada de ventana de gracia pre-reboot y acciones diferidas
    NavaDeferredAction deferredAction = NAVA_DEFERRED_NONE;
    bool preRebootArmed = false;
    uint32_t deferredExecutionTime = 0;

    // V2: sueño diferido tras enviar [Sueño]/[Vivo]/[Reserva] (mismo patron que storm/reboot)
    bool sleepPending = false;
    uint32_t sleepTime = 0;
    bool firstRunDone = false;
    bool vivoPending = false;
    bool reservaPending = false;
    bool wokeFromSleep = false;

    // Storm diferido
    uint32_t stormSeconds = 0;

    // NAVARICO V5: Desacople asíncrono secuencial de Traceroute
    bool tracePending = false;
    NodeNum traceTarget = 0;
    uint32_t traceExecutionTime = 0;
    
    // Variables de caché en RAM para lectura inmediata de sensores I2C
    float latestTemp = 0.0f;
    float latestHum = 0.0f;
    bool hasTelemetryCache = false;
    
    // Ring buffer circular de 5 paquetes recibidos
    RxLogEntry rxLog[5];
    uint8_t rxLogIndex = 0;
    uint8_t rxLogCount = 0;
    
    // NAVARICO F21: Métricas de rendimiento 100% en RAM (CERO desgaste de Flash)
    float statsMinTemp = 999.0f;
    float statsMaxTemp = -999.0f;
    uint16_t statsMinBattMv = 65535;
    uint32_t statsRxPackets = 0;
    uint32_t statsTxPackets = 0;
    uint32_t statsRoutedPackets = 0;

    // NAVARICO F21: Buffer forense circular de eventos 100% en RAM (16 entradas)
    NavaLogEntry ramLogs[16];
    uint8_t ramLogHead = 0;
    uint8_t ramLogCount = 0;

    // NAVARICO F21: Modo Silencioso temporal en RAM
    uint32_t muteUntilMs = 0;

    // NAVARICO F21: Ráfaga de prueba RF periódica (test_tx)
    uint8_t testTxCountRemaining = 0;
    uint32_t testTxNextMs = 0;

    // Carga/Guardado de parámetros
    ResiliencePrefs prefs;
    void loadResiliencePrefs();
    void saveResiliencePrefs();

    // NAVARICO F20: restauración y adopción de claves admin
    void applyPersistedAdminKeys();
    void adoptPersistedAdminKeys();
    static bool navaKeyIsEmpty(const uint8_t *key);
    static bool navaKeyIsProjectKey(const uint8_t *key);

    // NAVARICO F21: Restauración y respaldo de canales secundarios
    void applyPersistedChannels();
    void adoptPersistedChannels();

    // NAVARICO V5: Restauración de Capa Física LoRa y Canal 0 Primario
    void applyPersistedLoraConfig();
    void adoptPersistedLoraConfig();
    void applyPersistedChannel0();
    void adoptPersistedChannel0();

    // NAVARICO V5: Protocolo "Botón del Pánico"
    void startPanic(const NavaPanicPulse &pulse);
    void emitPanicPulse();
    void cancelPanicRollback();
    void handlePanicPulse(const meshtastic_MeshPacket &mp);

    // NAVARICO F20 (fix banco 2a): full_reset debe resetear los semi-persistentes a
    // defaults de perfil CONSERVANDO las 3 claves admin persistidas (no borrar el fichero).
    void navaFullResetKeepKeys();

    // V2/V5: helpers del listado persistente de auto-favoritos (status real tras reinicio)
    bool isAutoFav(uint32_t nodeNum) const;
    bool addAutoFav(uint32_t nodeNum);    // true si cambio
    bool removeAutoFav(uint32_t nodeNum); // true si cambio
    void reconcileAutoFavs();             // sincroniza con activeDirectRouters (runOnce)

    // NAVARICO F22: helpers de la lista negra global persistente (ign)
    bool addIgnoredNode(uint32_t nodeNum);
    bool removeIgnoredNode(uint32_t nodeNum);
    void clearIgnoredNodes();

    // Rate-limit de respuesta a no-admins
    std::set<NodeNum> unauthorizedReplied;

    // Rate-limit suave del ping
    std::map<NodeNum, uint32_t> lastPingTime;
    
    void enqueueResponse(NodeNum toNode, uint8_t channel, const std::string &msg, bool isFirstFragment = false, bool quick = false, uint8_t hops = 0);
    void executeCommand(NodeNum fromNode, std::string cmd, uint8_t replyChannel, NodeNum replyDest, float rxSnr, uint8_t hops);
    std::string getRoleName(meshtastic_Config_DeviceConfig_Role role);
    std::string helpForCommand(const std::string &topic);
    std::string usageAndState(const std::string &topic);
    std::string base64Encode(const uint8_t *data, size_t len);
    static bool base64Decode(const std::string &in, uint8_t *out, size_t &outLen, size_t maxLen);
    std::string generateChannelUrl(uint8_t channelIndex);
    std::string buildEnergyLine(); // V2: ADC mV + INA (V, ±mA, cargando/descargando) si disponible
    void logEvent(const char *fmt, ...);
};

extern NavaCLIModule *navaCLIModule;
extern bool navaAutoFavoriteEnabled;
