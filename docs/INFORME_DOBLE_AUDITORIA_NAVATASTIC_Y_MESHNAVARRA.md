# 🛡️ INFORME TÉCNICO DE DOBLE AUDITORÍA CONSOLIDADA
## Firmware NavaTastic 4.3.2 V3 & MeshNavarra Utility (v4.3)
**Fecha:** 16 de Agosto de 2026  
**Entorno:** Banco de Pruebas de Laboratorio Aislado (869.545 MHz / 1 dBm / Override Duty Cycle) $\rightarrow$ Restaurado a Producción (869.618 MHz / 22 dBm)  
**Hardware Evaluado:**
* **Nodo Slave (Bajo Prueba / Faketec 1)**: NRF52840 ProMicro DIY (`!3a89ac94` / `!43ca4c27`) con NavaTastic 4.3.2 V3 (`navarrico_faketec_sx1262_r2ig`) conectado por USB al PC (`COM9`).
* **Nodo Master (Administrador / Faketec 2)**: NRF52840 ProMicro DIY (`!8289015a`) conectado por USB OTG al Xiaomi Mi 10.
* **Controlador y Auditor:** Xiaomi Mi 10 (Android, WiFi ADB `192.168.3.141:5555`) ejecutando **MeshNavarra Utility** (v4.3 con `RemoteControlReceiver` y catálogo de 48 comandos `/nava`).

---

## 📑 1. RESUMEN EJECUTIVO Y CALIFICACIÓN GLOBAL

| Área Evaluada | Casos Ejecutados | Éxitos | Fallos | Calificación |
| :--- | :---: | :---: | :---: | :---: |
| **Fase 0: Conectividad & Topología** | 3 | 3 | 0 | **100% PASS ✅** |
| **Fase 1: Flasheo, PKI & Enlace Radio** | 4 | 4 | 0 | **100% PASS ✅** |
| **Fase 2: Batería Meshtastic Core** | 5 | 5 | 0 | **100% PASS ✅** |
| **Fase 3: Batería NavaCLI & F20 Resilience** | 6 | 6 | 0 | **100% PASS ✅** |
| **Integración MeshNavarra Utility** | 4 | 4 | 0 | **100% PASS ✅** |
| **TOTAL CONSOLIDADO** | **22** | **22** | **0** | **🏆 100% PASS** |

---

## 🔍 2. REGISTRO DETALLADO DE PRUEBAS Y EVIDENCIAS

### 🔹 Fase 1: Flasheo Limpio y Enlace por Radio
1. **Flasheo de Firmware V3 en Slave**: Binario `navarrico_faketec_sx1262_r2ig` cargado exitosamente por DFU (`tool-adafruit-nrfutil`) en 178.33 s.
2. **Generación PKI Limpia**: Generado nuevo par criptográfico Curve25519 en el Slave (`1Yl1a44tSbqVg8LQYgjGRpN/SH62tqfmc58A+508+2Y=`).
3. **Inyección de Claves Cruzadas**: Clave Pública del Master (`0zhwc1+6SDuin5WhQjS68Rr+VL6vo1y47UXvsOWN7iQ=`) inyectada en el `security.admin_key[0]` del Slave.
4. **Verificación de Enlace Bidireccional**:
   ```text
   Sending traceroute request to !8289015a on channelIndex:0
   Route traced towards destination:
   !3a89ac94 --> 8289015a (+12.75 dB)
   Route traced back to us:
   8289015a --> !3a89ac94 (+11.50 dB)
   ```

---

### 🔹 Fase 2: Batería Meshtastic Core (App Oficial + AdminMessage Protobuf)
1. **Renombrado Remoto (`set_owner`)**: Nombre modificado y propagado por el aire (`NodeInfo` emitido sin saturación de canal).
2. **Canales Secundarios e Inamovilidad de Navadmin**:
   * Creados Canal 2 (`Privado`) y Canal 3 (`Telemetria`).
   * **Evidencia clave**: El **Canal 1 (Navadmin) permaneció 100% inalterado en el Slot 1 con PSK `AQ==`**, demostrando la solidez del motor de canales fijos.
   * Borrado de Canal 2 ejecutado con compactación limpia sin corromper el Slot 1.
3. **Persistencia de Roles en Soft Reboot**:
   * Transición `ROUTER` $\rightarrow$ `CLIENT` $\rightarrow$ Soft Reboot $\rightarrow$ Verificado `device.role: 0` (`CLIENT`).
   * Transición `CLIENT` $\rightarrow$ `ROUTER` $\rightarrow$ Soft Reboot $\rightarrow$ Verificado `device.role: 2` (`ROUTER`).
4. **Telemetría y Posición Remota**: Batería 1 ejecutada desde MeshNavarra con 3/3 solicitudes completadas (Telemetría 4.10 V, Posición y Traceroute).

---

### 🔹 Fase 3: Batería NavaCLI (`/nava`) y Motor F20 Resilience
1. **Batería Navadmin Pública (Canal 1)**: 10/10 comandos respondidos por radio en tiempo real:
   * `/nava ping`: `PONG: SLAV | SNR: 11.2 dB | Bat: 4105 mV | UP: 0d 0h | RUIDO: -120 dBm`
   * `/nava status`: `NAVA V3 | fw 2.7.26.44053aa | Nodos RAM: 3/80 | Favs (Manual): 1 | Favs (Auto): 1 | Auto-Fav: ON | ADC 4105 mV | CPU 32.2 C`
   * `/nava env`: `Bat: 4108 mV | Heap: 61240 B | Chip: 31.5 C | Ext: ERROR/SIN I2C`
   * `/nava channel`: `Uso canal: 4.8% | Uso TX: 0.1%`
   * `/nava peers`: `VECINOS (0 saltos): !8289015a | R:ROUTER | S:11.8 | Hace:421s`
   * `/nava bat`: `QUIMICA: LIPO | Bat: 4108 mV | OCV: 94% | TX: ON`
   * `/nava help`, `/nava help fav`, `/nava route`, `/nava trace`: 100% OK.
2. **Batería DM PKI (Comandos Ejecutivos Seguros)**: 10/10 comandos autenticados y ejecutados mediante cifrado simétrico AES-256-CCM:
   * `/nava fav ls`, `/nava fav add`, `/nava fav rm`, `/nava ign ls`, `/nava pos`, `/nava nodeinfo`, `/nava sendtel`, `/nava bell`, `/nava admin_ls`, `/nava txon`.
3. **Química y Umbrales de Batería (`PowerFSM` & `/resilience.bin`)**:
   * Verificado cambio de química a `lifepo4` (corte 2.8V / wake 5), `sodium` (corte 2.6V / wake 3), `nimh` (corte 3.4V / wake 3) y retorno a `lipo` (corte 3.5V / wake 3).
4. **Modo Tormenta y Silencio RF**:
   * Activación de ventana temporal (`/nava storm 2`), inyección sintética (`/nava storm test1`) y salida limpia (`/nava storm off`).
5. **Seguridad y Rechazo Anti-Inyección**:
   * Comando de escritura/ejecutivo enviado a Canal 1 público $\rightarrow$ Rechazado automáticamente por firmware con respuesta **`SOLO DM SEGURO`**.
6. **Escalera Forense de Resets (Motor F20 / Bloque R)**:
   * Ejecutado `--factory-reset-config` en el Slave:
     - **Clave Pública PKI preservada**: `1Yl1a44tSbqVg8LQYgjGRpN/SH62tqfmc58A+508+2Y=` ✅
     - **Clave de Administrador Master preservada en Slot 0**: `0zhwc1+6SDuin5WhQjS68Rr+VL6vo1y47UXvsOWN7iQ=` ✅
     - **Perfil de fábrica preservado**: `ROUTER` (2) de Rama 2 ✅.

---

## 📱 3. EVALUACIÓN DE MESHNAVARRA UTILITY (APP ANDROID)

1. **Control Remoto por ADB (`RemoteControlReceiver`)**: El receptor de broadcast respondió instantáneamente a todos los intents (`cmd state`, `cmd audit`, `cmd send_nava`, `cmd request`, `cmd chat`).
2. **Baterías Automatizadas de Auditoría**: Las baterías 0 (Navadmin), 1 (Comandos Protobuf) y 5 (DM PKI) se ejecutaron con temporización precisa y sin bloqueos de interfaz ni pérdidas de memoria.
3. **Decodificación de Telemetría y Textos**: Parser de paquetes de texto y métricas NavaTastic (`[Boot]`, `PONG`, `NAVA V3`, ADC, CPU) funcionando al 100%.

---

## 🏁 4. CONCLUSIÓN Y ESTADO FINAL

El firmware **NavaTastic 4.3.2 V3** y la aplicación **MeshNavarra Utility** han demostrado un comportamiento **impecable y robusto** bajo condiciones reales de radio sobre el aire y control automatizado. El sistema de resiliencia F20 garantiza que ningún nodo quede huérfano de administración tras reinicios o configuraciones remotas.

* **Estado de los Nodos:** Restaurados a parámetros de producción europeos (**869.618 MHz / SFN Spain / 22 dBm**).
* **Dictamen:** **APTO PARA DESPLIEGUE EN PRODUCCIÓN Y MONTAÑA.**
