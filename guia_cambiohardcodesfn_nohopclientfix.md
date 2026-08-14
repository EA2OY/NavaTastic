# Guía de Cambios: Hardcodeo SFN y Estado del Hop-Limit en Clientes Favoritos (E22P)

Esta guía documenta los cambios técnicos realizados en el espacio de trabajo de la radio **E22P-868M30S** para implementar la red monocanal SFNarrow, la persistencia de la clave de administración y el comportamiento final del límite de saltos (hop-limit).

---

## 1. Mantenimiento de Potencia E22P (Límite 12 dBm)
A diferencia de la versión HT-RA62, la radio E22P-868M30S requiere límites de potencia de transmisión estrictos:
* **Potencia de fábrica tras reset (Default)**: **8 dBm**
* **Potencia máxima por hardware (Max)**: **12 dBm**
* *Nota*: En `variant.h` se conservaron sin cambios las macros nativas de límite:
  ```cpp
  #define SX126X_MAX_POWER 12
  #define HARDWARE_TX_POWER_LIMIT 12
  ```

---

## 2. Configuración SFNarrow e Inyección de Clave de Admin

### A. Canal 0 y Perfil LoRa Manual
* **Archivo**: [Channels.cpp](file:///C:/Users/Jesus/Desktop/Promicro%20fix%202.7.26%20Sleep%20+%20FactoryReset%20+%20SFN%20Hardcode%20ADC%202.0%20-%20copia/src/mesh/Channels.cpp)
* **Cambio**:
  1. En `initDefaultLoraConfig()`, al final de la función y bajo la macro `FIX_NATIVE_CORE_RESET`, se fuerza el perfil manual y la potencia a **8 dBm**:
     ```cpp
     #ifdef FIX_NATIVE_CORE_RESET
         loraConfig.region = meshtastic_Config_LoRaConfig_RegionCode_EU_868;
         loraConfig.use_preset = false;
         loraConfig.bandwidth = 62;
         loraConfig.spread_factor = 7;
         loraConfig.coding_rate = 5;
         loraConfig.channel_num = 4;
         loraConfig.override_frequency = 869.618f;
         loraConfig.tx_power = 8; // Default safety limit for E22P / DIY Pro Micro TCXO
     #endif
     ```
  2. En `initDefaultChannel()`, bajo el `case 0` se fuerza el nombre `"SFNarrow"` y el PSK `0x01` (Base64 `AQ==`):
     ```cpp
         switch (chIndex) {
         case 0:
     #ifdef FIX_NATIVE_CORE_RESET
             strcpy(channelSettings.name, "SFNarrow");
             channelSettings.psk.bytes[0] = 0x01;
             channelSettings.psk.size = 1;
     #else
         ...
     ```

### B. Clave de Admin Persistente
* **Archivo**: [NodeDB.cpp](file:///C:/Users/Jesus/Desktop/Promicro%20fix%202.7.26%20Sleep%20+%20FactoryReset%20+%20SFN%20Hardcode%20ADC%202.0%20-%20copia/src/mesh/NodeDB.cpp)
* **Cambio**:
  1. En `installDefaultConfig()`, se inyecta la clave de administrador PKI baseline:
     ```cpp
     #ifdef FIX_NATIVE_CORE_RESET
         static const uint8_t default_admin_key_0[] = { 0xc7, 0xdc, ... 0x55 };
         memcpy(config.security.admin_key[0].bytes, default_admin_key_0, 32);
         config.security.admin_key[0].size = 32;
         if (config.security.admin_key_count == 0) {
             config.security.admin_key_count = 1;
         }
     #endif
     ```
  2. En `loadFromDisk()`, se implementó la carga de seguridad por fallback de la clave si los datos leídos de la partición de flash son nulos.

---

## 3. Estado del Hop-Limit en el Rol CLIENT (Revertido a Original)
* **Archivo**: [Router.cpp](file:///C:/Users/Jesus/Desktop/Promicro%20fix%202.7.26%20Sleep%20+%20FactoryReset%20+%20SFN%20Hardcode%20ADC%202.0%20-%20copia/src/mesh/Router.cpp)
* **Estado**: El cambio experimental para omitir el decremento de saltos en repetidores con el rol `CLIENT` **ha sido completamente revertido**. Se ha restaurado la lógica nativa del ruteador de Meshtastic, donde solo los nodos con rol de infraestructura (`ROUTER`, `ROUTER_LATE` o `CLIENT_BASE`) aplican las exenciones de saltos para favoritos:
  ```cpp
  // Lógica nativa restaurada
  bool localIsRouter =
      IS_ONE_OF(config.device.role, meshtastic_Config_DeviceConfig_Role_ROUTER, meshtastic_Config_DeviceConfig_Role_ROUTER_LATE,
                meshtastic_Config_DeviceConfig_Role_CLIENT_BASE);
  
  if (!localIsRouter) {
      return true; // Siempre decrementa para cualquier rol CLIENT ordinario
  }
  ```

---

## 4. Archivos Configurados y Modificados

| Componente | Ruta del Archivo | Descripción |
| --- | --- | --- |
| Configuración de Radio | [Channels.cpp](file:///C:/Users/Jesus/Desktop/Promicro%20fix%202.7.26%20Sleep%20+%20FactoryReset%20+%20SFN%20Hardcode%20ADC%202.0%20-%20copia/src/mesh/Channels.cpp) | SFNarrow y potencia de fábrica inicial fijada en 8 dBm. |
| Clave de Admin | [NodeDB.cpp](file:///C:/Users/Jesus/Desktop/Promicro%20fix%202.7.26%20Sleep%20+%20FactoryReset%20+%20SFN%20Hardcode%20ADC%202.0%20-%20copia/src/mesh/NodeDB.cpp) | Instalación e inyección estática/fallback de la Admin Key. |
| Algoritmo Inundación | [Router.cpp](file:///C:/Users/Jesus/Desktop/Promicro%20fix%202.7.26%20Sleep%20+%20FactoryReset%20+%20SFN%20Hardcode%20ADC%202.0%20-%20copia/src/mesh/Router.cpp) | Revertido al estado original (sólo ROUTERs y CLIENT_BASE aplican preservación). |
| Preferencias compilación | [userPrefs.jsonc](file:///C:/Users/Jesus/Desktop/Promicro%20fix%202.7.26%20Sleep%20+%20FactoryReset%20+%20SFN%20Hardcode%20ADC%202.0%20-%20copia/userPrefs.jsonc) | Vinculación del canal SFNarrow, potencia de 8 dBm y clave de admin. |
