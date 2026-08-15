#pragma once
#include "concurrency/OSThread.h"
#include "SinglePortModule.h"
#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>

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
    uint32_t version;        // V2.3: marcador de formato 0x4E415653 ("NAVS"); ficheros <=80B (Eclipse/V2.0) migran con defaults
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

    // V2: el monitor de bateria (Power.cpp) delega el sueño aqui para mandar el
    // mensaje [Sueño] antes de dormir. Devuelve true si tomo el control.
    bool handleLowBatteryEvent();

  protected:
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual int32_t runOnce() override;

  private:
    // La cola almacena respuestas de tipo NavaResponse
    std::queue<NavaResponse> responseQueue;
    
    bool rebootScheduled = false;
    uint32_t rebootTime = 0;

    // V2: sueño diferido tras enviar [Sueño]/[Vivo] (mismo patron que storm/reboot)
    bool sleepPending = false;
    uint32_t sleepTime = 0;
    bool firstRunDone = false;
    bool vivoPending = false;
    bool wokeFromSleep = false;

    // Reset de fábrica diferido: se ejecuta en runOnce() cuando la cola de respuestas esté vacía
    bool factoryResetPending = false;

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
    
    // Carga/Guardado de parámetros
    ResiliencePrefs prefs;
    void loadResiliencePrefs();
    void saveResiliencePrefs();

    // V2: helpers del listado persistente de auto-favoritos (status real tras reinicio)
    bool isAutoFav(uint32_t nodeNum) const;
    bool addAutoFav(uint32_t nodeNum);    // true si cambio
    bool removeAutoFav(uint32_t nodeNum); // true si cambio
    void reconcileAutoFavs();             // sincroniza con activeDirectRouters (runOnce)

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
    std::string buildEnergyLine(); // V2: ADC mV + INA (V, ±mA, cargando/descargando) si disponible
};

extern NavaCLIModule *navaCLIModule;
extern bool navaAutoFavoriteEnabled;
