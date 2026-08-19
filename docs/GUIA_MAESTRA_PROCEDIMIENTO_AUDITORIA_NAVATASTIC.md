# 🛠️ GUÍA MAESTRA Y PROTOCOLO DE AUDITORÍA DE FIRMWARE
## Procedimiento de Validación en Banco Físico y Certificación de Resiliencia
**Versión de Referencia:** NavaTastic 4.3.2 V3 / MeshNavarra Utility v4.3  
**Propósito:** Guía técnica estandarizada para replicar, auditar o ampliar la batería de pruebas de laboratorio en futuras versiones del firmware.

---

## 🎯 1. Filosofía de la Auditoría en Banco Real

Las pruebas unitarias en emuladores o simuladores de software no reproducen el comportamiento crítico de un repetidor LoRa en la montaña (caídas de tensión transitorias, picos de consumo de radio a +22 dBm, reconexión de pilas USB, comportamiento del comparador analógico LPCOMP en System OFF y degradación de memoria flash).

Por ello, la auditoría de NavaTastic se realiza **sobre hardware físico real, comunicando por radiofrecuencia en el aire y alimentado por fuente regulable de laboratorio**.

---

## 🏗️ 2. Topología del Banco de Laboratorio

```
  ┌────────────────────────────────────────────────────────┐
  │                 ORDENADOR DE CONTROL                   │
  │  - Compilación PlatformIO                              │
  │  - Flasheo DFU por USB (COM9)                          │
  │  - Puente ADB inalámbrico (WiFi 192.168.3.141:5555)   │
  └──────────────────────────┬─────────────────────────────┘
                             │
            ┌────────────────┴────────────────┐
            ▼                                 ▼
┌───────────────────────┐         ┌───────────────────────────────┐
│     NODO MASTER       │         │          NODO SLAVE           │
│  - Xiaomi Mi 10       │  LoRa   │  - Faketec / ProMicro         │
│  - MeshNavarra App    │ ◄─────► │  - NavaTastic 4.3.2 V3        │
│  - Nodo OTG (!8289015a)│ 869.618 │  - Alimentación por Fuente    │
│  - RemoteControlRecv  │  (SFN)  │    Regulable de Laboratorio   │
└───────────────────────┘         └───────────────────────────────┘
```

### Componentes y Roles:
1. **Nodo Master (Administrador)**: Teléfono Android (Xiaomi Mi 10) conectado por USB OTG a un nodo LoRa (`!8289015a`), ejecutando **MeshNavarra Utility** con el receptor automatizado `RemoteControlReceiver` activo.
2. **Nodo Slave (Bajo Prueba)**: Placa nRF52840 (Faketec HT-RA62 o ProMicro DIY `!43ca4c27`) alimentada exclusivamente por la fuente de alimentación regulable de laboratorio.
3. **PC de Control y Diagnóstico**: Conectado por ADB vía WiFi al teléfono para enviar comandos, capturar paquetes recibidos en tiempo real (`app_log.txt`) y monitorizar las trazas.

---

## 📋 3. Protocolo de Ejecución de las 5 Fases

### 🔹 Fase 0: Inicialización y Topología
* **Objetivo**: Asegurar comunicación limpia y aislamiento de canales.
* **Pasos**:
  1. Conectar Master a la app y verificar enlace ADB (`adb shell "tail -n 10 app_log.txt"`).
  2. Ajustar frecuencia de trabajo y potencia si se desea aislamiento de prueba (ej. 869.545 MHz / 1 dBm) o usar frecuencia oficial (869.618 MHz / 22 dBm).
  3. Comprobar que no hay USB conectado al Slave durante las pruebas de batería (el USB desactiva la detección de batería baja).

### 🔹 Fase 1: Flasheo Limpio y Enlace Criptográfico PKI
* **Objetivo**: Verificar que el binario compila, se programa por DFU y establece enlace cifrado.
* **Pasos**:
  1. Flashear la variante correspondiente (`pio run -e <env> -t upload`).
  2. Generar el par de claves Curve25519 e inyectar la clave pública del Master en `security.admin_key[0]`.
  3. Ejecutar un Traceroute bidireccional para validar SNR y línea de vista.

### 🔹 Fase 2: Batería Core de Meshtastic
* **Objetivo**: Probar que las funciones estándar de Meshtastic respetan los blindajes de NavaTastic.
* **Pasos**:
  1. Renombrar el nodo por la app oficial (`set_owner`).
  2. Crear y borrar canales secundarios $\rightarrow$ **Verificar que el Canal 1 (Navadmin) permanece inamovible en el Slot 1 con PSK `AQ==`**.
  3. Cambiar rol `ROUTER` $\leftrightarrow$ `CLIENT` y verificar persistencia tras reinicio.

### 🔹 Fase 3: Batería NavaCLI y Persistencia F20
* **Objetivo**: Validar el catálogo de comandos `/nava` y la resistencia a resets.
* **Pasos**:
  1. **Batería Abierta (Canal 1 Navadmin)**: Ejecutar `/nava ping`, `/nava status`, `/nava env`, `/nava peers`, `/nava channel`, `/nava bat` $\rightarrow$ Verificar respuesta en broadcast.
  2. **Batería Ejecutiva (DM PKI)**: Probar comandos críticos (`/nava fav`, `/nava ign`, `/nava set_chem`, `/nava storm`) $\rightarrow$ Verificar cifrado AES-256-CCM.
  3. **Seguridad**: Enviar un comando de escritura por el canal público $\rightarrow$ Verificar que el nodo lo rechaza con `SOLO DM SEGURO`.
  4. **Prueba Nuclear F20**: Ejecutar `--factory-reset-config` $\rightarrow$ Comprobar que la clave Master, el par PKI y el rol de fábrica sobreviven al 100%.

### 🔹 Fase 4: Batería de Resiliencia Solar y Fuente Regulable
* **Objetivo**: Simular el ciclo día/noche y la recarga solar paso a paso.
* **Pasos**:
  1. **Plena Carga (4.10 V)**: Verificar lectura ADC y porcentaje OCV (~93%).
  2. **Descenso a 3.35 V**: Observar el suavizado del filtro IIR en `Power.cpp` $\rightarrow$ Tras 8 lecturas (~160s), capturar `[Sueño]` y comprobar que el consumo cae a **0.4 mA** (radio SX1262 apagada por SPI).
  3. **Reset en Nivel 1 ($3.30\text{V} - 3.40\text{V}$)**: Pulsar reset a 3.34 V $\rightarrow$ Capturar `[Vivo]`, verificar operación durante 160s y segundo aviso `[Sueño]`.
  4. **Reset en Nivel 2 ($< 3.30\text{V}$)**: Ajustar fuente a 3.24 V y pulsar reset $\rightarrow$ Capturar `[Critico]`, verificar operación durante 160s y apagado limpio a 0.4 mA.
  5. **Rampa Solar Ascendente**: Subir gradualmente el voltaje hacia 3.75 V – 3.80 V $\rightarrow$ Registrar el instante exacto en que el comparador LPCOMP despierta la CPU, capturar `[Listo]` y verificar que el ADC reporta el voltaje exacto de la fuente ($\pm 1\text{ mV}$).
  6. **Anti-Bucle [Boot]**: Tras un arranque en frío, esperar exactamente **2 minutos** de uptime para verificar la emisión del aviso de diagnóstico `[Boot]` con la causa de reinicio (`RESETREAS`).

---

## 🔍 4. Diagnósticos Clave y Lecciones de Laboratorio

1. **El Filtro IIR Virtual en `Power.cpp:370`**:
   * *Comportamiento*: Al variar el voltaje de la fuente, el firmware no cambia la lectura de golpe, sino que desciende gradualmente ($\alpha = 0.5$).
   * *Razón*: El filtro protege al repetidor frente a caídas momentáneas de tensión provocadas por ráfagas de transmisión a +22 dBm o frío intenso. Es una función deseada de estabilidad.
2. **Apagado Canónico de la Radio por SPI (`doDeepSleep`)**:
   * *Regla de Oro*: Toda transición a System OFF por batería baja **debe realizarse a través del pipeline `doDeepSleep()` de Meshtastic**.
   * *Motivo*: Si se llama a `cpuDeepSleep()` directo antes de inicializar la radio, el chip SX1262 no recibe la orden SPI de apagado y se queda en escucha consumiendo 5–10 mA. Con `doDeepSleep()`, el consumo en reposo es de **0.4 mA** estrictos.
3. **Gestión de Permisos USB en Android por ADB**:
   * Si la app pierde foco tras reconexiones USB, capturar la pantalla con `uiautomator dump /sdcard/window_dump.xml` y enviar un tap sobre el botón `ACEPTAR` (`input tap X Y`).
4. **Comportamiento Hardware Faketec HT-RA62 en USB sin Batería**:
   * *Diagnóstico*: Si la placa Faketec/Promicro se alimenta únicamente por el conector USB de 5V sin celda física LiPo conectada a sus bornes de batería, el divisor analógico de VBAT mide ~0 V. Como la placa no dispone de pin de sensado hardware VBUS, `getHasUSB()` devuelve `false` y el firmware interpreta que hay una batería agotada, iniciando el ciclo de reposo tras 8 lecturas consecutivas (~160s).
   * *Solución en banco*: Conectar una celda LiPo a los bornes o desactivar temporalmente `USERPREFS_LOW_BATTERY_LOWPOWER_ENABLED` durante pruebas continuas de laboratorio.
5. **Entrecomillado de Intents ADB y Temporización RF en Automatización F21**:
   * *Formato de Intent*: En PowerShell, para evitar que ADB divida los argumentos de comandos con espacios (ej. `ch_set 2 Privada AQ==` o `set_pin 123456`), pasar el argumento con entrecomillado simple anidado:
     `adb shell am broadcast -a com.meshkachoutility.REMOTE -n com.meshkachoutility/.RemoteControlReceiver --es cmd send_nava --es arg "'ch_set 2 Privada AQ=='" --es arg2 "!43ca4c27"`
   * *Ventana de Guarda RF*: En módems de bajo ancho de banda (SFNarrow / 62.5 kHz), cada paquete de texto o respuesta fragmentada requiere de 1.5 a 3.5 segundos de tiempo de aire. Los scripts automatizados deben conceder un retardo de al menos 4 segundos entre comandos consecutivos para no saturar la cola de transmisión.

---

## 🚀 5. Protocolo para Extender la Suite ante Nuevas Funciones

Cuando se añadan nuevos comandos `/nava` o drivers de sensores:
1. Añadir el comando al catálogo en `src/modules/NavaCLIModule.cpp` y su ayuda en `helpForCommand()`.
2. Registrar el caso de prueba en la tabla de [docs/cerebro/12_auditoria_navatastic.md](file:///c:/NavaTastic%20Codigo%20completo/docs/cerebro/12_auditoria_navatastic.md).
3. Validar la respuesta tanto por Canal 1 (si es lectura) como por DM PKI (si es ejecutivo).
4. Recompilar los manuales y PDFs con `powershell -File herramientas\generar_pdf.ps1`.
