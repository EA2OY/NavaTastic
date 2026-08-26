# Manual de Preguntas Frecuentes: Resiliencia, Canales y Despliegue en Produccion (NavaTastic V5)

> **Documento Tecnico de Referencia Operativa — NavaTastic V5 (2.7.26)**  
> Este documento recopila las casuisticas, comportamiento de memoria y reglas de decision que rigen el firmware NavaTastic ante instalaciones sobre nodos existentes, cambios de configuracion, gestion de claves y recuperacion ante desastres.

---

## 1. Despliegue sobre Nodos en Produccion

### 1.1. Que ocurre si instalo NavaTastic sobre un repetidor o nodo que ya esta en funcionamiento con Meshtastic oficial?
NavaTastic V5 incorpora el **Protocolo de Adopcion Pasiva No Destructiva (adoptExistingOperationalConfig)**:
- **Tus canales se respetan**: El Canal 0 y los canales del 2 al 7 se mantienen exactamente como los tenias.
- **Canal 1 (Navadmin)**: Si tenias un canal propio en el Slot 1, NavaTastic busca el primer slot libre (entre 2 y 7) y **lo traslada alli para que no lo pierdas**, aprovisionando Navadmin en el Slot 1.
- **Tu modulacion de radio se respeta**: Si operabas en MediumFast, LongFast o una modulacion personalizada, el nodo **continua transmitiendo y escuchando en tu modulacion activa**.
- **Tus claves y nombre se respetan**: El nombre del nodo, el rol y las claves de administracion se absorben y respaldan en /resilience.bin.
- **Cero necesidad de Factory Reset**: Ya no es necesario borrar la placa tras flashear para que Navadmin y las protecciones funcionen.

---

### 1.2. Que ocurre si mi nodo estaba operando en una malla en MediumFast o LongFast? Se cambiara solo a SFNarrow?
**No.** NavaTastic respeta la soberania de tu red local:
- Si tu nodo ya estaba operando en **MediumFast**, continua en **MediumFast**.
- Si tu nodo ya estaba operando en una malla **LongFast**, continua en **LongFast**.
- El Canal 0 y la capa fisica LoRa asociada se adoptan pasivamente en /resilience.bin.
- **Cuando se instala SFNarrow por defecto?**:
  Unicamente en **nodos virgenes recien salidos de la caja** (placas nuevas sin configurar o tras un nRF Erase completo), donde el firmware aplica la linea de base optimizada del proyecto.

---

### 1.3. Que ocurre si mi nodo estaba configurado como CLIENT o CLIENT_MUTE?
**Se respeta al 100%.**
- Los roles de Meshtastic estan ordenados internamente: 0 = CLIENT, 1 = CLIENT_MUTE, 2 = ROUTER.
- La rutina de respaldo pasivo comprueba el rol activo del nodo:
  - Si era CLIENT, se respalda y sigue operando como CLIENT.
  - Si era CLIENT_MUTE, se respalda y sigue operando en silencio como CLIENT_MUTE (sin retransmitir trafico ajeno).
  - Si era ROUTER, sigue como ROUTER.
- Un nodo CLIENT_MUTE jamas se transformara en un Router por error al actualizar a NavaTastic.

---

## 2. Gestion de Claves y Soberania del Dueno

### 2.1. Si el dueno ya tenia configurada su clave de administracion remota, que ocurre? Se inyecta la clave de MasterNode?
**No.** Se respeta de forma estricta la soberania y privacidad del propietario:
- Si en config.security.admin_key ya existe una clave publica legitima configurada por el dueno, **esa clave es la unica autoridad del nodo** y se respalda en resilience.bin.
- **La clave de MasterNode NO se inyecta en la lista activa del dueno**. El dueno sigue siendo el unico administrador de su dispositivo.

---

### 2.2. Cuando y solo cuando interviene la clave de MasterNode?
La clave de MasterNode actua **exclusivamente como salvavidas de emergencia en caso de catastrofe total**:
- Si el nodo sufre una corrupcion destructiva de memoria, un rayo o un formateo total de fabrica donde se pierde toda la memoria previa:
  - En Meshtastic oficial, un nodo reseteado se queda con **cero claves de administracion**, convirtiendose en un ladrillo sordo en la cima de la montana que exige subir fisicamente con un cable USB y un portatil.
  - En NavaTastic, el binario compilado inyecta de fabrica la clave de rescate de **MasterNode**. De este modo, el operador de la red puede conectarse por radio a distancia, auxiliar al repetidor y restaurarle su configuracion sin tener que escalar la montana.

---

## 3. Comportamiento ante Cambios de la App Oficial y Memoria Flash

### 3.1. Si el usuario borra o cambia un canal desde la App oficial de Meshtastic, lo resucitara resilience.bin al reiniciar?
**No.** NavaTastic V5 incorpora **Sincronizacion Bidireccional en Caliente**:
- Cada vez que el usuario modifica, renombra o desactiva (DISABLED) un canal desde la App movil oficial, Meshtastic notifica al hook de NavaTastic.
- La funcion syncCustomChannelFromConfig(slot) actualiza inmediatamente /resilience.bin. Si el canal fue borrado, se borra tambien del archivo de persistencia.
- Al reiniciar el nodo, **el borrado voluntario se respeta y el canal no resucita**.

---

### 3.2. Que escribe exactamente en /resilience.bin y cual es el riesgo de desgaste de la Flash?
El 100% de las escrituras en /resilience.bin son **estrictamente puntuales y guiadas por eventos raros**:
1. **Comandos manuales del operador**: Solo cuando una persona ejecuta un comando /nava set_* (muy puntual, una vez cada meses).
2. **Cambios desde la App movil**: Solo cuando el usuario guarda un ajuste en la pantalla del movil.
3. **Auto-descubrimiento inicial de Routers**: Ocurre **unicamente 2 o 3 veces en los primeros 10 minutos** tras instalar el nodo, cuando descubre a sus repetidores vecinos directos. Una vez en la lista de 32 slots, **no vuelve a escribir jamas**.
4. **Eventos de bateria critica**: Si no hay sol en invierno y la bateria baja del umbral de seguridad, escribe para entrar en sueno profundo y al despertar con el sol (maximo 1 o 2 veces al dia en dias de temporal).
5. **Boton del panico**: Solo en simulacros o incidencias reales de interferencia.

**Lo que NUNCA escribe**:
- Cero escrituras por trafico de radio o retransmision de mensajes.
- Cero escrituras por paquetes de telemetria o sensores.
- Cero escrituras en los reinicios diarios programados si nada ha cambiado.
- Cero escrituras en bucle en el loop principal.

**Riesgo de desgaste o corrupcion**:
- Escribir 760 bytes tarda **~1,5 milisegundos**.
- En 1 ano de operacion, el nodo realiza menos de 80 escrituras en total (apenas 0,12 segundos al ano con la Flash activa).
- La probabilidad de coincidir un corte de energia durante una escritura es inferior al **0,00000038%**.
- Si ocurriera esa coincidencia astronomica, el sistema **Clean Slate** detecta que el archivo no mide 760 bytes exactos, lo purga de raiz y arranca en la Linea de Base de Supervivencia sin colgarse jamas.

---

## 4. Gestion de Nodos Favoritos

### 4.1. Por que no se migran los favoritos antiguos de la memoria Flash al instalar NavaTastic?
- En Meshtastic oficial, la tabla de nodos (/prefs/db.proto) almacena todos los dispositivos escuchados historicamente (muchos de ellos moviles de excursionistas que pasaron hace meses y nunca volveran).
- Migrar esa tabla antigua llenaria la memoria con **nodos fantasma inactivos**.
- NavaTastic utiliza **Descubrimiento Organico en Vivo**:
  - En los primeros minutos de escucha, identifica que repetidores estan emitiendo en enlace directo (rx_hops == 0).
  - Los marca como favoritos automaticamente (hasta 32 routers vecinos) activando la **Preservacion del Hop Limit** (los paquetes entre repetidores favoritos no pierden saltos en la cordillera).
  - Marca automaticamente como favoritos a los administradores verificados por clave publica (PKC Admin).
  - Guarda estos repetidores reales y activos en /resilience.bin para todos los reinicios futuros.

---

## 5. Matriz Resumen de Casuisticas y Decisiones

| Escenario | Estado Inicial en Flash | Diagnostico NavaTastic | Accion Ejecutada |
| :--- | :--- | :--- | :--- |
| **Repetidor en produccion (MediumFast / LongFast)** | Archivos .proto con configuracion previa. resilience.bin no existe. | Nodo operativo de otra malla. | **Adopta pasivamente** su preset LoRa, Canal 0, canales 2..7, rol, nombre y clave del dueno en resilience.bin. Reubica Slot 1 para meter Navadmin. MasterNode **NO** se inyecta. |
| **Nodo virgen nuevo o nRF Erase previo** | Flash completamente vacia. | Instalacion limpia desde cero. | Instala la **Linea de Base NavaTastic** (SFNarrow en Canal 0, Navadmin en Canal 1, clave MasterNode de rescate). |
| **Reset accidental con resilience.bin sano (NAV6)** | Archivos .proto borrados. resilience.bin intacto (760 bytes). | Amnesia involuntaria de Meshtastic. | **Auto-reparacion total**: Restaura 100% de canales del dueno, preset LoRa, nombre, rol y claves del dueno. |
| **Catastrofe total (Flash corrupta / resilience.bin danado)** | resilience.bin con tamano erroneo o bytes corruptos. | Fallo catastrofico de hardware. | **Clean Slate**: Purga el archivo danado, ejecuta reset de fabrica y arranca en la Linea de Base de Supervivencia (SFNarrow + Navadmin + MasterNode) para auxilio por radio. |
| **Cambio voluntario de canal o clave en App movil** | El usuario desactiva un canal o edita un ajuste. | Accion deliberada del operador. | Sincronizacion en caliente: actualiza resilience.bin en el acto. Al reiniciar, el cambio se respeta. |
