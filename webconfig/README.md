# webconfig — Flasher web de NavaTastic

Página para **instalar o actualizar el firmware desde el navegador**, sin programas que instalar.
Es un sitio **estático** (no hay servidor ni backend): se puede publicar en GitHub Pages tal cual.

**Está publicado en <https://ea2oy.github.io/NavaTastic-Flasher/>** (repositorio
[NavaTastic-Flasher](https://github.com/EA2OY/NavaTastic-Flasher), rama `main`, carpeta raíz).

## Qué hace

- **Placas ESP32-S3 (Heltec V3 y V4)**: flashea por cable con `esptool-js` (WebSerial), escribiendo el
  fichero **FACTORY** del Release en la dirección `0x0` (instalación desde cero).
- **Placas nRF52840 (Promicro NRF52+E22P, Faketec, Seed Solar Node P1, Heltec T114, XiaoKitI2c y
  XiaoKitI2c+E22P)**: manda el nodo al **modo bootloader** abriendo el puerto serie a **1200 bps** y
  cerrándolo (el firmware lo detecta y reinicia en bootloader) y, después, el usuario **arrastra el
  fichero UF2** a la unidad que aparece en el ordenador.
- Los **UF2 y ZIP de nRF52** se descargan de los *assets* del Release público de
  [NavaTastic](https://github.com/EA2OY/NavaTastic/releases) mediante un enlace normal (eso no
  necesita CORS).
- Los **cuatro `.bin` FACTORY de ESP32** sí van copiados en la carpeta `firmware/` de este sitio,
  porque el navegador tiene que **leer su contenido** para escribirlo y GitHub **no permite** leer los
  assets del Release desde otra web (no manda la cabecera `Access-Control-Allow-Origin`; comprobado).
  Sirviéndolos desde el mismo sitio no hay problema de CORS.

## Ficheros

| Fichero | Para qué |
|---|---|
| `index.html` | La página: los cuatro pasos, los avisos y las ayudas. |
| `app.js` | La lógica: elegir placa y rama, descargar el fichero, flashear (ESP32) o mandar a bootloader (nRF52), progreso y errores en lenguaje llano. |
| `style.css` | Los estilos. |
| `firmware.json` | Qué fichero corresponde a cada placa y rama (con su URL o su ruta local). |
| `firmware/` | Los 4 `.bin` FACTORY de ESP32 (unos 2 MB cada uno). No van al repositorio del firmware: viven en el del flasher. |
| `.nojekyll` | Para que GitHub Pages sirva los ficheros tal cual (sin procesarlos con Jekyll). |

## Probarlo en local

WebSerial solo funciona en `localhost` o con HTTPS, así que **no vale abrir el fichero con doble clic**:

```bash
cd webconfig
python -m http.server 8000
```

Y abre <http://localhost:8000> con un navegador de ordenador: Chrome, Edge o un Firefox reciente
(WebSerial existe desde Firefox 151). La web avisa si el navegador no puede grabar y ofrece la
descarga manual.

## Nota para el mantenimiento

Esta carpeta es el **original**: los cambios se hacen aquí y se copian al repositorio del flasher,
incluido el fichero vacío `.nojekyll` (que es lo que hace que GitHub Pages sirva los ficheros tal cual).

## Publicarlo en GitHub Pages

El contenido de esta carpeta se copia a la raíz del repositorio público del flasher y se activa Pages
sobre la rama `main` (carpeta raíz). Queda servido en `https://<usuario>.github.io/<repositorio>/`.

## Actualizar a una versión nueva del firmware

Cuando se publique un Release nuevo hay que hacer dos cosas:

1. **Regenerar `firmware.json`** con la misma estructura (`version`, `release`, `fecha`, `nrf52` y
   `esp32`, con `nombre`, `url` y `bytes` por fichero) y cambiar el número de versión.
2. **Sustituir los 4 `.bin` de `firmware/`** por los del Release nuevo (mismos nombres salvo el número
   de versión) y actualizar sus rutas y tamaños en `firmware.json`.

El resto de ficheros no hay que tocarlos.

## Cómo se comprueba antes de publicar

```bash
node --check app.js          # sintaxis del JS
python -m http.server 8000   # y probar en el navegador (http://localhost:8000)
```
