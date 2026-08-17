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
#define NAVATASTIC_BUILD "V4"
#endif

// NAVARICO F21/F22: marcador de formato del struct ResiliencePrefs. Bump V5 (0x4E415635,
// "NAV5"): se añaden control de difusión periódica (pos_tx, nodeinfo_tx, telem_tx)
// y lista negra global persistente (ignoredNodes). Ficheros previos ("NAV4" / "NAV3" / "NAVS") migran con defaults.
#define NAVS_RESILIENCE_VERSION 0x4E415635

struct NavaResponse {
    NodeNum dest;
    uint8_t channel;
    std::string text;
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

// Estructura binaria persistente a reset de fábrica
struct ResiliencePrefs {
    uint32_t magic;         // 0x52455349
    uint16_t vbat_cutoff;   // 2400-3600
    uint8_t vwake_level;    // 1-5
    uint8_t chemistry;      // 0=LIPO, 1=NIMH, 2=SODIUM
    uint8_t tx_disabled;    // 0=ON, 1=OFF
    uint8_t ble_disabled;   // 0=ON, 1=OFF
    uint8_t auto_fav;       // 1=auto-favoriteo ON (default), 0=OFF (/nava fav auto)
    uint8_t role;           // V2.1 Rama 1 y Rama 2: rol semi-permanente. 0xFF=sin fijar; 0=CLIENT, 1=CLIENT_MUTE, 2=ROUTER. Sobrevive a factory reset
    uint32_t autoFavIds[16]; // V2: ids (NodeNum) de nodos favoritados por auto-fav. Persistente para que /nava status los distinga de manual tras reinicio
    uint8_t autoFavCount;    // V2: numero de ids validos en autoFavIds (0-16)
    uint8_t sleepMsgs;       // V2: 1=mensajes sueño/vivo/listo ON (default), 0=OFF (/nava sleepmsg)
    uint8_t wasInSleep;      // V2: 1=dormido por bateria (se setea antes de cpuDeepSleep, se lee al boot para Listo/Vivo)
    uint8_t reserved;        // V2: reservado
    uint32_t version;        // Marcador de formato NAVS_RESILIENCE_VERSION ("NAV5"); ficheros previos migran con defaults
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

    // NAVARICO F21/F22: chequeos estáticos para enrutamiento y diagnóstico en RAM
    static bool navaIsMuteActive();
    static void recordRoutedPacket();
    static void logRamEvent(const char *msg);
    static bool isNodeIgnored(NodeNum node);

    // V2: el monitor de bateria (Power.cpp) delega el sueño aqui para mandar el
    // mensaje [Sueño] antes de dormir. Devuelve true si tomo el control.
    bool handleLowBatteryEvent();

    // NAVARICO F20: sincronizar las claves admin del usuario (slots 0-2 de la config)
    // hacia /resilience.bin. Lo llama AdminModule tras cada set_config de seguridad.
    // Merge: un slot entrante no vacio se persiste (slot 0 = proyecto -> limpia
    // keySlot0Own); un slot vacio NUNCA borra lo persistido (purgar = keys_clear/wipe).
    void syncAdminKeysFromConfig();

  protected:
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual int32_t runOnce() override;

  private:
    // La cola almacena respuestas de tipo NavaResponse
    std::queue<NavaResponse> responseQueue;
    
    bool rebootScheduled = false;
    uint32_t rebootTime = 0;

    // V2: sueño diferido tras enviar [Sueño]/[Vivo]/[Reserva] (mismo patron que storm/reboot)
    bool sleepPending = false;
    uint32_t sleepTime = 0;
    bool firstRunDone = false;
    bool vivoPending = false;
    bool reservaPending = false;
    bool wokeFromSleep = false;

    // Reset de fábrica diferido: se ejecuta en runOnce() cuando la cola de respuestas esté vacía
    bool factoryResetPending = false;

    // V3 (FASE R1): reset completo (conserva claves PKI y bonds BLE) y wipe
    // (purga total: par PKI nuevo + bonds). Diferidos con el mismo patron que factory_reset.
    bool fullResetPending = false;
    bool wipePending = false;

    // Storm diferido: temporizador de hibernación ejecutado en runOnce() tras vaciar la cola
    bool stormPending = false;
    uint32_t stormSeconds = 0;
    uint32_t stormTime = 0; // milis cuando se activo; se esperan 15s antes de dormir
    
    // Temporizador diferido para txoff
    bool txOffScheduled = false;
    uint32_t txOffTime = 0;
    
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

    // NAVARICO F20: restauracion al primer tick del boot (DESPUES de NodeDB::init):
    // keySlot0Own -> slot 0 (desplaza la del proyecto); keySlot1/2 -> slots vacios.
    // Guarda el config SOLO si algo cambio. Adopcion = copiar claves de usuario del
    // config hacia la persistencia cuando esta vacia (migracion legacy / fichero perdido).
    void applyPersistedAdminKeys();
    void adoptPersistedAdminKeys();
    static bool navaKeyIsEmpty(const uint8_t *key);
    static bool navaKeyIsProjectKey(const uint8_t *key);

    // NAVARICO F21: Restauración y respaldo de canales secundarios
    void applyPersistedChannels();
    void adoptPersistedChannels();

    // NAVARICO F20 (fix banco 2a): full_reset debe resetear los semi-persistentes a
    // defaults de perfil CONSERVANDO las 3 claves admin persistidas (no borrar el fichero).
    void navaFullResetKeepKeys();

    // NAVARICO F20: borrado diferido de las claves persistidas (/nava keys_clear):
    // ACK encolado -> ejecucion en runOnce con la cola vacia tras ~3s (patron ANEXO).
    bool keysClearPending = false;
    uint32_t keysClearTime = 0;

    // V2: helpers del listado persistente de auto-favoritos (status real tras reinicio)
    bool isAutoFav(uint32_t nodeNum) const;
    bool addAutoFav(uint32_t nodeNum);    // true si cambio
    bool removeAutoFav(uint32_t nodeNum); // true si cambio
    void reconcileAutoFavs();             // sincroniza con activeDirectRouters (runOnce)

    // NAVARICO F22: helpers de la lista negra global persistente (ign)
    bool addIgnoredNode(uint32_t nodeNum);
    bool removeIgnoredNode(uint32_t nodeNum);
    void clearIgnoredNodes();

    // Rate-limit de respuesta a no-admins: solo se responde una vez por nodo
    // para evitar que un atacante haga transmitir al repetidor en bucle.
    std::set<NodeNum> unauthorizedReplied;

    // Rate-limit suave del ping: ultimo ping respondido por nodo emisor (ms)
    std::map<NodeNum, uint32_t> lastPingTime;
    
    void enqueueResponse(NodeNum toNode, uint8_t channel, const std::string &msg, bool isFirstFragment = false, bool quick = false);
    void executeCommand(NodeNum fromNode, std::string cmd, uint8_t replyChannel, NodeNum replyDest, float rxSnr);
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
