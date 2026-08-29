<img src="branding/cartel_navatastic_github.jpg" alt="Cartel NavaTastic Eclipse V5" width="640"/>

# NavaTastic Eclipse V5

[![Licencia GPL v3](https://img.shields.io/badge/Licencia-GPLv3-blue)](LICENSE)
[![Estado: ALPHA](https://img.shields.io/badge/Estado-ALPHA-orange)](https://github.com/EA2OY/NavaTastic/releases)

> **AVISO IMPORTANTE â€” V5 ESTÃ EN FASE ALPHA.**
> La versiÃ³n V5 estÃ¡ siendo probada en campo por el autor. Incluye funciones potentes y nuevas
> (el BotÃ³n del PÃ¡nico consolidable, la instalaciÃ³n automÃ¡tica, el reenvÃ­o configurable...), pero
> todavÃ­a no ha pasado una validaciÃ³n final: **Ãºsala con criterio y no la consideres "estable"**
> hasta que el autor confirme su comportamiento tras las pruebas reales.

NavaTastic es un **fork de Meshtastic** (firmware oficial 2.7.26) pensado para **repetidores solares
de infraestructura** en malla LoRa de banda estrecha (SFNarrow), con gestiÃ³n remota completa por
mensajes de texto cifrados. Un solo cÃ³digo genera compilaciones listas para usar en **8 placas**.

---

## Lo nuevo en V5 (para entenderlo sin tecnicismos)

- **El BotÃ³n del PÃ¡nico funciona de verdad**: si una evacuaciÃ³n de frecuencia va bien, el
  administrador puede confirmarla con un mensaje y los repetidores **se quedan en la nueva
  frecuencia** (antes el confirmar no siempre llegaba). Si un repetidor se reinicia durante el
  aviso, **vuelve a unirse a la evacuaciÃ³n** en vez de quedarse fuera.
- **Se acabÃ³ el "factory reset obligatorio" al instalar**: al flashear NavaTastic, el propio
  firmware **se configura solo** (frecuencia, canal de administraciÃ³n, buenas prÃ¡cticas) y
  **respeta las claves del dueÃ±o** si el nodo ya tenÃ­a alguna.
- **MigraciÃ³n de frecuencia completa**: al cambiar de preset (a mano o con el pÃ¡nico), el nodo
  escribe **todos** los ajustes de radio de una vez â€” ya no quedan parÃ¡metros "a medias" que
  rompÃ­an el enlace.
- **Corregido el salto a LONG_FAST**: antes aplicaba ajustes vacÃ­os; ahora salta correctamente.
- **Modo de retransmisiÃ³n configurable** (`/nava set_rebroadcast`) y que **vuelve al valor del
  perfil** despuÃ©s de una vuelta atrÃ¡s automÃ¡tica.
- **Nombres de canal con mayÃºsculas respetadas** (el nombre del canal forma parte de su identidad).
- **Compatible con Heltec V3 y Heltec V4** (ESP32-S3), ademÃ¡s de las 6 placas nRF52840.

---

## InstalaciÃ³n en 3 pasos

1. **Descarga** la compilaciÃ³n de tu placa desde los **Assets del Release** (archivos con `V5` en
   el nombre): `.uf2` / `.zip` OTA para las placas nRF52840, `.bin` para Heltec.
2. **Flashea** con el mÃ©todo habitual de tu placa (cable USB, web flasher, OTA...).
3. **Espera un minuto**: el firmware se despliega solo (frecuencia SFNarrow, canal Navadmin,
   buenas prÃ¡cticas). **No hace falta ningÃºn reset de fÃ¡brica** â€” y si el nodo ya tenÃ­a claves
   del dueÃ±o, se respetan.

---

## Placas soportadas (8)

| Placa | Radio | Compilaciones |
|---|---|---|
| Pro Micro + E22P (nRF52840) | SX1268/E22P | Router / Cliente |
| Faketec HT-RA62 (nRF52840) | SX1262 | Router / Cliente |
| Seeed Solar Node P1 (nRF52840) | SX1262 | Router / Cliente |
| Heltec T114 (nRF52840) | SX1262 | Router / Cliente |
| Seeed Xiao Kit i2c (nRF52840) | SX1262 | Router / Cliente |
| Seeed Xiao Kit + E22P (nRF52840) | SX1268/E22P | Router / Cliente |
| **Heltec V3 (ESP32-S3)** | SX1262 | Router / Cliente |
| **Heltec V4 (ESP32-S3)** | SX1262 | Router / Cliente |

> Nota de energÃ­a: la gestiÃ³n de baterÃ­a profunda (LPCOMP, quÃ­mica, hibernaciÃ³n por tormenta) es
> especÃ­fica de nRF52840. En las placas Heltec se usa la gestiÃ³n de energÃ­a estÃ¡ndar de Meshtastic
> y los comandos de baterÃ­a/hibernaciÃ³n no aplicables quedan desactivados.

---

## GestiÃ³n remota por mensajes (`/nava`)

Todo se controla por mensajes de texto (cifrados por DM o por el canal privado de flota):

| Comando | QuÃ© hace |
|---|---|
| `ping`, `status`, `bat`, `power`, `env`, `channel`, `noise` | Estado y salud en una lÃ­nea |
| `set_preset`, `set_lora`, `set_freq` | Cambiar la radio de la flota |
| `panic`, `panic_ok` | EvacuaciÃ³n de emergencia coordinada y su confirmaciÃ³n |
| `set_rebroadcast` | Modo de retransmisiÃ³n (a peticiÃ³n de cada red) |
| `set_pos_tx`, `set_nodeinfo_tx`, `set_telem_tx` | Control de lo que se difunde y cada cuÃ¡nto |
| `fav`, `ign` | Favoritos y lista negra persistente |
| `ch_set`, `ch_del`, `ch_url`, `ch_mqtt`, `ch_reset` | GestiÃ³n de canales |
| `set_role`, `set_name`, `set_hops`, `set_txpower`, `set_pin`, `set_tz` | ConfiguraciÃ³n por nodo |
| `reboot`, `factory_reset`, `full_reset`, `wipe` | Operaciones de mantenimiento (con confirmaciÃ³n) |

Lista completa y manuales en los PDFs de la documentaciÃ³n.

---

## DocumentaciÃ³n (PDFs)

- [Manual de comandos NavaTastic (PDF)](docs/pdf/Manual_NavaTastic.pdf)
- [Manual de uso e instalaciÃ³n (PDF)](docs/pdf/Manual_uso_NavaTastic.pdf)

> **Compilaciones listas para usar**: estÃ¡n en los **Assets de los Releases** de este repositorio
> (panel derecho / pestaÃ±a Releases). Los nombres de archivo incluyen la versiÃ³n (`V5`) y la placa.

---

## Seguridad

- **Canal Navadmin** (canal 1): solo lectura y solo para administradores verificados; los
  comandos de configuraciÃ³n, el pÃ¡nico y los destructivos estÃ¡n bloqueados ahÃ­ (DM cifrado o
  canal privado de flota).
- **AcreditaciÃ³n por clave**: el estado de administrador se concede Ãºnicamente tras descifrar el
  primer mensaje privado cifrado (PKI) de un mando con la clave correcta.
- **Clave de rescate integrada**: tras un restablecimiento duro, el nodo vuelve con la clave del
  proyecto para poder reentrar y restaurarlo.
- Cambiar la clave preinstalada: edita `profiles/<Rama>_<Placa>.jsonc` â†’ `USERPREFS_USE_ADMIN_KEY_0`
  con tu clave pÃºblica (base64 â†’ hex) y recompila.

---

## Compilar

GuÃ­a completa para compilar desde el cÃ³digo: [Compilar_NavaTastic.md](Compilar_NavaTastic.md)

---

## Fork auditable

La rama `main` de este repositorio estÃ¡ basada directamente en el commit oficial de Meshtastic
2.7.26 (`54e0d8d`) con un Ãºnico commit de cambios encima, de modo que puedes **comparar el fork
con el firmware original en un clic** (pestaÃ±a *Files changed* / comparaciÃ³n de forks).

---

## Agradecimientos

- **JBAU92** y su firmware_solar_fix (origen del proyecto)
- Amig@s y conocid@s de la malla de Navarra
- Grupo Meshtastic EspaÃ±a (Telegram)

El proyecto es y serÃ¡ **100% libre, gratuito y totalmente altruista**. Si te resulta Ãºtil y
quieres apoyar el cafÃ© del autor: [ko-fi.com/ea2oy](https://ko-fi.com/ea2oy) (voluntario).

---

## Descargo

Las instalaciones deben cumplir la normativa aplicable (nacional, autonÃ³mica, local y europea:
emplazamiento, permisos, seguridad, medio ambiente). DÃ³nde y cÃ³mo se monta es responsabilidad
exclusiva del instalador. El proyecto queda desvinculado de montajes o usos de terceros que no se
ajusten a la legislaciÃ³n.

---

# NavaTastic Eclipse V5 (English)

> **IMPORTANT NOTICE â€” V5 IS IN ALPHA PHASE.** The V5 version is being field-tested by the author.
> It includes powerful new features, but it has not passed final validation yet: **use it with
> judgment and do not consider it "stable"** until the author confirms its behavior after real
> world testing.

NavaTastic is a **fork of Meshtastic** (official firmware 2.7.26) designed for **solar-powered
infrastructure repeaters** on narrow-band LoRa mesh (SFNarrow), with full remote management via
encrypted text messages. A single codebase produces ready-to-flash builds for **8 boards**.

## What's new in V5 (plain language)

- **The Panic Button now really works**: after a successful frequency evacuation, the
  administrator can confirm it with a message and the repeaters **stay on the new frequency**.
  If a repeater reboots during the warning, it **rejoins the evacuation** instead of being left
  behind.
- **No more mandatory "factory reset" on install**: after flashing, the firmware **configures
  itself** (frequency, admin channel, best practices) and **respects the owner's keys** if the
  node already had any.
- **Complete frequency migration**: when switching presets (manually or via panic), the node
  writes **all** radio settings at once â€” no more half-applied parameters that broke the link.
- **Fixed LONG_FAST jump**: it used to apply empty settings; now it jumps correctly.
- **Configurable rebroadcast mode** (`/nava set_rebroadcast`) that **returns to the profile
  value** after an automatic rollback.
- **Channel names keep their capitalization** (the channel name is part of its identity).
- **Heltec V3 and Heltec V4 support** (ESP32-S3), in addition to the 6 nRF52840 boards.

## Install in 3 steps

1. **Download** the build for your board from the **Release Assets** (files with `V5` in the
   name): `.uf2` / OTA `.zip` for nRF52840 boards, `.bin` for Heltec.
2. **Flash** with the usual method for your board (USB cable, web flasher, OTA...).
3. **Wait a minute**: the firmware deploys itself (SFNarrow frequency, Navadmin channel, best
   practices). **No factory reset needed** â€” and if the node already had owner keys, they are
   respected.

## Supported boards (8)

Pro Micro + E22P, Faketec HT-RA62, Seeed Solar Node P1, Heltec T114, Seeed Xiao Kit i2c, Seeed
Xiao Kit + E22P (all nRF52840), plus **Heltec V3** and **Heltec V4** (ESP32-S3). Router and
Client builds for each.

> Energy note: deep battery management (LPCOMP, chemistry, storm hibernation) is nRF52840
> specific. Heltec boards use Meshtastic's standard energy management and the non-applicable
> battery/hibernation commands are disabled.

## Remote management (`/nava`)

Everything is controlled by text messages (encrypted DM or private fleet channel): status and
health queries, preset/radio changes, the Panic Button (`panic` / `panic_ok`), rebroadcast mode,
telemetry cadence, favorites and ignore list, channel management, per-node configuration and
maintenance operations with confirmation.

Full command list and manuals in the documentation PDFs:
[Command manual (PDF)](docs/pdf/Manual_NavaTastic.pdf) Â·
[User & install manual (PDF)](docs/pdf/Manual_uso_NavaTastic.pdf)

> **Ready-to-flash builds**: they live in the **Release Assets** of this repository (right panel /
> Releases tab). File names include the version (`V5`) and the board.

## Auditable fork

The `main` branch of this repository is based directly on the official Meshtastic 2.7.26 commit
(`54e0d8d`) with a single commit of changes on top, so you can **compare this fork with the
original firmware in one click**.

## Acknowledgements

JBAU92 and his firmware_solar_fix (origin of the project); friends of the Navarra mesh;
Meshtastic EspaÃ±a group (Telegram). The project is and will remain **100% free, open and
altruistic**. Optional support: [ko-fi.com/ea2oy](https://ko-fi.com/ea2oy).

## Disclaimer

Installations must comply with applicable regulations (national, regional, local and European:
siting, permits, safety, environment). Where and how it is installed is the sole responsibility
of the installer. This project is not affiliated with any third-party installation or use that
does not comply with the law.

