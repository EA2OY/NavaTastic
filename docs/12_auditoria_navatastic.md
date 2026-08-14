# 12 — AUDITORÍA NAVATASTIC (operativa + estado, sesión 14/08/2026)

> Neurona nueva creada en sesión de auditoría (14/08). Fuente de verdad de detalle: `Rama 2 Infraestructura\Infraestructura Propia\Faketec Auditoria\auditoria\` (FIX_LOG.md, OPERATIVA.md, evidencias). La operación se llama **Auditoría NavaTastic** (independiente del hardware de banco: hoy dos Faketec).

## Topología de banco (auditoría NavaTastic)
| Rol | Hardware | Conexión | Firmware |
|---|---|---|---|
| Nodo de montaña (prueba) | Faketec 1 | USB → PC (COM9) | NavaTastic 4.3 Rama 2 Propia = **Eclipse Edition** (12/08 17:09-17:15; MD5 sanity: NodeDB `5234C510`, NavaCLIModule.h `B3565F59`, NavaCLIModule.cpp `6C2EE6AB` 22dBm, jsonc `30B258CD`) |
| Nodo admin (mando) | Faketec 2 "Timonel" | OTG → Mi 10 | **2.7.7.5ae4ff9 (NO es stock 2.7.26 ni Navarrico)**; acreditado como admin en la malla (su clave pública `PzgE...` = K1 del `admin_key[]` de fábrica del de montaña). Regla operador: **solo se le toca la frecuencia** |
| Control del admin | Mi 10 (app oficial `com.geeksville.mesh`) | adb WiFi `192.168.3.141:5555` | activity: `com.geeksville.mesh/org.meshtastic.app.MainActivity` |

- Banda de pruebas (aislamiento): **869.545 MHz / hop 1 / duty ON / SFN PSK `AQ==`** en ambos nodos. Restaurar `override_duty_cycle false` al cierre.
- Limitación banco: ambos por USB → no probar deep sleep/storm largo/anti-brownout (storm solo test1/test2). Anotar "no probado por banco".
- Mi 10: pantalla suspende a ~10 min; el agente NO desbloquea → pedir al operador. Nada se borra en el Mi 10 (datos del operador).

## Estado al inicio de sesión
- Montaña: factory reset hecho (claves nuevas, DB 1 nodo), re-aislada 869.545/hop1/duty ON, canal 0 SFNarrow + canal 1 Navadmin `AQ==` en slot 1 (el fork lo crea al factory reset ✓).
- Rollback local: `auditoria\rollback\UF2_PREVIO_Eclipse_20260814.uf2` (MD5 `D39940D341E78D09EA6DE29F44C5BD5A`).
- Timonel: se le conectó por USB vía app oficial del Mi 10 (permiso USB aceptado); pendiente pasar su frecuencia a 869.545.

## Re-key / re-acreditación (matriz, plan Desktop\Auto_debug_NavaTastic.md §5)
- Caso A (factory reset montaña): `--factory-reset-device` → clave nueva; el admin ya es válido si su pubkey está en `admin_key[]` del de montaña (K1 `PzgE...` de fábrica ✓). Forzar NodeInfo del admin → H3 (a2) acredita (primera clave = admin_key → bitfield directo). Fallback pre-H3: un AdminMessage PKI (set-favorite) desde el admin.
- Caso B/C (db_clear / ign rm): forzar NodeInfo broadcast del admin (reboot/set-owner); NO basta un DM.
- Caso D (mismatch): NodeInfo con clave == admin_key → acepta + estrella + bitfield (H3).
- Caso E (primera toma): DB limpia + clave admin en admin_key[] → solo NodeInfo del mando acredita canal+DM sin DM previo.
- ⚠️ Si la clave del mando NO está en admin_key[] → DM descifra pero `NO AUTORIZADO` (1 vez) y bitfield jamás se otorga.

## FIX LOG acumulado (detalle en carpeta auditoria)
- **F-00** (Bajo, herramienta CLI): `--set` encadenados en ráfaga → 3º falla PermissionError 13 (puerto ocupado). Solución: pausa ≥2 s o repetir. No es firmware. Verificado: duty reintentado OK.
- **F-01** (Medio, app oficial Mi 10): campo "Sobreescribir frecuencia" (LoRa→Avanzado) NO persiste el valor en el nodo al confirmar (ENTER): queda en la UI local pero el nodo sigue en la frecuencia anterior. Además `input text "."` se convierte en "0"; **`input keyevent 56` (KEYCODE_PERIOD) SÍ inserta el punto**. Método que funciona para ver el valor en el campo: borrar del final (tap en borde derecho del campo + DEL×N) hasta dejar vacío real `[]`, escribir dígitos + keyevent 56 + dígitos, verificar con dump, ENTER. **Pero el guardado no vale → vía fiable: CLI del PC** (F-02).
- **F-02** (Alto, operativa — admin remota por radio): `--set X Y --dest !ID` desde el de montaña a Timonel (2.7.7.5ae4ff9): 1ª vez ACK de entrega SIN aplicar el cambio; 2ª vez NAK `NO_CHANNEL`. Conclusión: **el ACK de entrega no prueba el procesamiento** (lección del plan) y el admin remoto de config a ese firmware no es fiable → **mover el nodo al PC y usar CLI local** (funcionó a la 1ª).

## Flujo de preparación de banco (documentado 14/08, para auditorías futuras)
1. Montaña (PC): factory reset + aislamiento (869.545/hop1/duty ON) — CLI local.
2. Timonel (PC): CLI local `--set lora.override_frequency 869.545` + `--reboot` (la app oficial NO persiste ese campo — F-01; el admin remoto por radio no aplica — F-02).
3. Cruz de claves: clave pública del de montaña en `admin_key[]` de Timonel (app Mi 10: Ajustes → Seguridad → Claves administración → Añadir → pegar base64). Timonel en la DB del de montaña: URL de contacto de Timonel desde la app (Detalles → Compartir contacto → URL `https://meshtastic.org/v/#...`) + `--add-contact "<URL>"` en el de montaña.
4. Verificación de enlace: `--trace !4e311ab3` (debe devolver ruta ida/vuelta).
5. Si se tocan claves (factory reset del de montaña): re-añadir su clave nueva en Timonel (o restaurar la privada antigua del de montaña para regenerar la pública anterior).

## Reglas de la sesión
- Solo escritura en `Faketec Auditoria\auditoria\`; fallos grandes → documentar + **proponer y auditar antes de aplicar**; rollback UF2+MD5+backup antes de cada cambio; nunca factory_reset al admin.

## Operativa detallada (tiempos, flujos adb/app/CLI)
→ Ver `OPERATIVA.md` en la carpeta auditoria (copy-paste en sesiones nuevas).

## Estado (14/08, actualización parcial)
- Montaña (COM9): factory reset + 869.545/hop1/duty ON; quedó a 869.618 (override 0) al inicio del discriminador F-03 (volver a 545 al reanudar).
- Timonel: 869.545 + canal Navadmin slot 1 añadido + clave de la montaña en admin_key[2]; DESCONECTADO del PC (COM11 caído) — pendiente reconectar (VM en despliegue).
- **F-03 abierto**: DM Timonel->montana = NAK NO_CHANNEL (asimetria vs traceroute OK). Discriminador pendiente: ambos a 869.618 sin override -> DM; si persiste: borrar tarjeta de la montana en Timonel (--remove-node !3a89ac94); plan B: 2.7.26 oficial a Timonel (oferta del operador).
