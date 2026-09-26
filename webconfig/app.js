// Flasher web de NavaTastic.
//   ESP32  (Heltec V3 / V4): se graba por cable con esptool-js (WebSerial).
//   nRF52  (Promicro E22P, Faketec, Seed, T114, Xiao): el navegador NO puede escribir en la
//          unidad de memoria del bootloader, así que se manda el nodo a modo grabación y se
//          descarga el .uf2 para que el usuario lo copie a la unidad. Es el camino real.
// Sin backend: todo ocurre en el navegador. Textos en lenguaje llano.

// Versión que usa el flasher oficial de Meshtastic. Comprobado que se sirve con permiso de otro
// origen y que exporta ESPLoader y Transport, con los métodos setRTS/waitForUnlock/writeFlash.
const ESPTOOL = 'https://cdn.jsdelivr.net/npm/esptool-js@0.5.7/bundle.js';
const VID_ADAFRUIT = 0x239a; // familia Adafruit: incluye el cargador UF2 de las placas nRF52
const PIDS_CARGADOR = [0x0029, 0x002a, 0x4029]; // identificadores del cargador UF2

const $ = (id) => document.getElementById(id);

let indice = null;
const soportaSerial = 'serial' in navigator;

// ---------- arranque ----------
(async function init() {
  if (!soportaSerial) {
    $('avisoNavegador').hidden = false;
  }
  try {
    const r = await fetch('firmware.json', { cache: 'no-cache' });
    if (!r.ok) throw new Error('respuesta ' + r.status);
    indice = await r.json();
    $('versionFirmware').textContent = 'versión ' + indice.version;
  } catch (e) {
    mostrarError('No he podido leer la lista de ficheros (firmware.json). Recarga la página; si sigue igual, avísame.');
    return;
  }
  if (!indice.nrf52 && !indice.esp32) {
    mostrarError('La lista de ficheros está vacía. Avisa de esto: es un fallo de la web.');
    return;
  }
  rellenarPlacas();
  $('placa').addEventListener('change', actualizarDetalle);
  document.querySelectorAll('input[name=rama]').forEach((r) => r.addEventListener('change', actualizarDetalle));
  $('btnFlashear').addEventListener('click', flashear);
  $('btnDescargar').addEventListener('click', () => descargar());
  actualizarDetalle();
})();

// ---------- lista de placas ----------
function bonito(nombre) {
  return nombre.replace(/^HeltecV(\d)$/, 'Heltec V$1').replace(/\+/g, ' + ');
}

function rellenarPlacas() {
  const sel = $('placa');
  sel.innerHTML = '';
  const grupos = [
    ['Placas nRF52840 (fichero UF2)', indice.nrf52],
    ['Placas ESP32-S3 (grabado por cable)', indice.esp32],
  ];
  for (const [titulo, tabla] of grupos) {
    if (!tabla) continue;
    const g = document.createElement('optgroup');
    g.label = titulo;
    for (const p of Object.keys(tabla)) {
      const o = document.createElement('option');
      o.value = (indice.nrf52 && indice.nrf52[p] ? 'nrf52:' : 'esp32:') + p;
      o.textContent = bonito(p);
      g.appendChild(o);
    }
    sel.appendChild(g);
  }
}

// ---------- qué fichero toca ----------
function seleccion() {
  const [familia, placa] = $('placa').value.split(':');
  const rama = document.querySelector('input[name=rama]:checked').value;
  const tabla = (familia === 'nrf52' ? indice.nrf52 : indice.esp32)[placa];
  const principal = tabla[rama + (familia === 'nrf52' ? '_uf2' : '_FACTORY')];
  const extra = tabla[rama + (familia === 'nrf52' ? '_zip' : '_APP')];
  return { familia, placa, rama, principal, extra };
}

function actualizarDetalle() {
  $('error').hidden = true;
  $('progreso').hidden = true;
  $('resultado').textContent = 'Cuando termine te diré aquí qué hacer. Normalmente nada: el nodo arranca solo.';
  const s = seleccion();
  if (!s.principal) {
    $('detalleFichero').textContent = 'No hay fichero para esa combinación. Avisa de esto: es un fallo de la web.';
    $('btnFlashear').disabled = true;
    return;
  }
  const mb = (s.principal.bytes / 1048576).toFixed(1).replace('.', ',');
  $('detalleFichero').innerHTML =
    'Se instalará <b>' + s.principal.nombre + '</b> (' + mb + ' MB).';
  $('btnFlashear').disabled = !soportaSerial;
  $('btnFlashear').textContent = s.familia === 'esp32' ? 'Grabar por cable' : 'Poner en modo grabación';
  $('ayudaConexion').innerHTML =
    s.familia === 'esp32'
      ? 'Conecta la placa con un <b>cable de datos</b> y pulsa el botón. Si no aparece el puerto, mantén pulsado ' +
        '<b>BOOT</b> mientras conectas el cable. <b>Ojo:</b> este fichero <b>instala desde cero</b>: borra lo que ' +
        'tuviera el nodo (región, canales y ajustes) y después habrá que volver a configurarlo.'
      : 'Conecta la placa con un <b>cable de datos</b> y pulsa el botón: mando el nodo a modo grabación y te doy ' +
        'el fichero para que lo copies a la unidad que aparezca.';
}

// ---------- utilidades ----------
function mostrarError(texto) {
  const e = $('error');
  e.textContent = texto;
  e.hidden = false;
}

function progreso(porcentaje, texto) {
  $('progreso').hidden = false;
  $('barraRelleno').style.width = Math.max(0, Math.min(100, porcentaje)) + '%';
  $('progresoTexto').textContent = texto;
}

function dormir(ms) {
  return new Promise((r) => setTimeout(r, ms));
}

function traducirError(e) {
  const m = String((e && e.message) || e || '');
  if (/no he podido abrir la lista de puertos/i.test(m)) return 'El navegador ha bloqueado la elección del puerto. Vuelve a pulsar el botón y elige el puerto enseguida, sin esperar.';
  if (/No port selected|NotFoundError|cancel/i.test(m)) return 'No has elegido ningún puerto. Vuelve a intentarlo y elige el del nodo.';
  if (/Failed to open|NetworkError|InvalidState|busy|in use/i.test(m)) return 'El puerto está ocupado: solo un programa puede usarlo a la vez. Cierra la app de Meshtastic (o cualquier otro programa que hable con el nodo) y reinténtalo.';
  if (/Failed to fetch|NetworkError when|no se pudo descargar/i.test(m)) return 'No he podido descargar el fichero del firmware. Comprueba tu conexión y recarga la página.';
  if (/Failed to connect|sync|timeout|Wrong boot mode/i.test(m)) return 'No consigo hablar con la placa. Prueba otro cable, pulsa RESET (en ESP32, mantén BOOT al conectar) y reinténtalo.';
  return 'Algo ha fallado: ' + m;
}

// Descarga el fichero y lo devuelve como cadena binaria (1 carácter = 1 byte), que es
// exactamente lo que espera writeFlash de esptool-js: por dentro usa charCodeAt, NO base64.
async function descargarBinario(url, aviso, bytesEsperados) {
  const r = await fetch(url);
  if (!r.ok) throw new Error('no se pudo descargar el fichero (' + r.status + ')');
  // Ojo: si el servidor manda el fichero comprimido, content-length no es el tamaño real,
  // así que para el porcentaje se usa el tamaño que dice el índice.
  const total = bytesEsperados || Number(r.headers.get('content-length') || 0);
  const lector = r.body.getReader();
  const trozos = [];
  let leido = 0;
  for (;;) {
    const { done, value } = await lector.read();
    if (done) break;
    trozos.push(value);
    leido += value.length;
    if (total) progreso((leido / total) * 20, aviso + ' ' + Math.round((leido / total) * 100) + ' %');
  }
  const todo = new Uint8Array(leido);
  let pos = 0;
  for (const t of trozos) { todo.set(t, pos); pos += t.length; }
  // Red de seguridad: si el fichero no tiene el tamaño que dice el índice, no se graba nada.
  if (bytesEsperados && todo.length !== bytesEsperados) {
    throw new Error('el fichero descargado no cuadra: ' + todo.length + ' bytes en vez de ' + bytesEsperados);
  }
  let bin = '';
  for (let i = 0; i < todo.length; i += 8192) bin += String.fromCharCode.apply(null, todo.subarray(i, i + 8192));
  return bin;
}

async function puertoElegido() {
  try {
    return await navigator.serial.requestPort();
  } catch (e) {
    // NotFoundError = el usuario ha cancelado; cualquier otro = el navegador ha bloqueado el diálogo.
    if (e && e.name === 'NotFoundError') throw new Error('No port selected');
    throw new Error('no he podido abrir la lista de puertos (' + ((e && e.message) || e) + ')');
  }
}

// ---------- entrada ----------
async function flashear() {
  $('error').hidden = true;
  const s = seleccion();
  if (!s.principal) return;
  $('btnFlashear').disabled = true;
  try {
    if (s.familia === 'esp32') await flashearESP32(s);
    else await modoGrabacionNRF52(s);
  } catch (e) {
    mostrarError(traducirError(e));
    progreso(0, '');
  } finally {
    $('btnFlashear').disabled = !soportaSerial;
  }
}

// ---------- ESP32: grabado real por cable ----------
async function flashearESP32(s) {
  if (!soportaSerial) throw new Error('este navegador no puede grabar por cable');
  // El puerto se pide PRIMERO: Chrome solo deja elegirlo si el clic es reciente.
  progreso(2, 'Elige el puerto del nodo…');
  const puerto = await puertoElegido();
  progreso(22, 'Descargando el firmware…');
  const datos = await descargarBinario(s.principal.url, 'Descargando el firmware…', s.principal.bytes);
  const { ESPLoader, Transport } = await import(ESPTOOL);
  const transporte = new Transport(puerto, true);
  const cargador = new ESPLoader({
    transport: transporte,
    baudrate: 460800,
    terminal: { clean() {}, writeLine() {}, write() {} },
  });
  try {
    progreso(26, 'Conectando con la placa…');
    await cargador.main();
    progreso(30, 'Grabando el firmware. No desconectes el cable…');
    await cargador.writeFlash({
      fileArray: [{ data: datos, address: 0x0 }], // FACTORY = todo en uno, va en 0x0 (nunca 0x1000)
      flashMode: 'keep',
      flashFreq: 'keep',
      flashSize: 'keep',
      eraseAll: true,
      compress: true,
      reportProgress: (i, escrito, total) => {
        const pct = total ? escrito / total : 0;
        progreso(30 + pct * 65, 'Grabando… ' + Math.round(pct * 100) + ' %');
      },
    });
    progreso(97, 'Reiniciando el nodo…');
    // El reinicio fiable en estas placas es soltar RTS a mano. (hardReset no existe en 0.5.x.)
    try {
      await transporte.setRTS(true);
      await dormir(100);
      await transporte.setRTS(false);
    } catch (e) { /* si no hay setRTS, la placa se reinicia sola al cerrar el puerto */ }
    // Acotado con un tope de tiempo: waitForUnlock espera sin límite a que el puerto quede libre.
    if (typeof transporte.waitForUnlock === 'function') {
      try { await Promise.race([transporte.waitForUnlock(1500), dormir(1500)]); } catch (e) {}
    }
  } finally {
    try { await transporte.disconnect(); } catch (e) { /* ya estaba cerrado */ }
  }
  progreso(100, 'Terminado.');
  $('resultado').innerHTML =
    '<span class="ok">Listo.</span> El nodo ya tiene el firmware nuevo. Si en unos segundos no aparece en la app de ' +
    'Meshtastic, pulsa el botón <b>RESET</b> de la placa: es normal en la primera grabación.<br>' +
    '<span class="sub">Como este fichero <b>instala desde cero</b>, el nodo se ha quedado sin configurar: revisa la ' +
    'región y vuelve a poner tus canales.</span>';
}

// ---------- nRF52: modo grabación + copia manual del UF2 ----------
async function modoGrabacionNRF52(s) {
  if (!soportaSerial) throw new Error('este navegador no puede mandar el nodo a modo grabación');
  progreso(10, 'Elige el puerto del nodo…');
  const puerto = await puertoElegido();
  progreso(40, 'Mandando el nodo a modo grabación…');
  // El truco: abrir el puerto a 1200 bps y cerrarlo. El firmware lo interpreta como
  // "reinicia en modo grabación". Hay que darle tiempo a verlo antes de cerrar.
  try {
    await puerto.open({ baudRate: 1200 });
    await dormir(500);
  } finally {
    try { await puerto.close(); } catch (e) { /* si la placa ya se reinició, cerrar puede fallar */ }
  }
  const detectado = await esperarCargador();
  progreso(80, detectado ? 'Nodo en modo grabación.' : 'Descargando el fichero…');
  descargar();
  $('resultado').innerHTML = detectado
    ? '<span class="ok">Nodo en modo grabación.</span> En tu ordenador: 1) busca la unidad nueva que ha aparecido; ' +
      '2) copia dentro el fichero <b>' + s.principal.nombre + '</b> (está en Descargas); 3) espera unos segundos: ' +
      'el nodo se reinicia solo y la unidad desaparece. Si no desaparece, <b>expúlsala</b> desde el sistema.'
    : '<span class="ok">Fichero descargado.</span> Si no ha aparecido ninguna unidad nueva, haz <b>doble toque ' +
      'rápido al botón RESET</b> del nodo (o desconéctalo y vuelve a conectarlo) hasta que aparezca, y copia dentro ' +
      'el fichero <b>' + s.principal.nombre + '</b>. Después, si la unidad no desaparece sola, <b>expúlsala</b> ' +
      'desde el sistema.';
}

// Mira si el cargador UF2 ya está en el aire (la placa se ha reiniciado en modo grabación).
async function esperarCargador(intentos = 10, espera = 700) {
  if (!navigator.serial.getPorts) return false;
  for (let i = 0; i < intentos; i++) {
    await dormir(espera);
    try {
      const puertos = await navigator.serial.getPorts();
      const hay = puertos.some((p) => {
        const i = p.getInfo ? p.getInfo() : {};
        return i.usbVendorId === VID_ADAFRUIT && PIDS_CARGADOR.includes(i.usbProductId);
      });
      if (hay) return true;
    } catch (e) { /* seguimos intentando */ }
  }
  return false;
}

// ---------- descarga directa ----------
function descargar() {
  const s = seleccion();
  if (!s.principal) return;
  const a = document.createElement('a');
  a.href = s.principal.url;
  a.download = s.principal.nombre;
  a.rel = 'noopener';
  document.body.appendChild(a);
  a.click();
  a.remove();
  if (s.extra) {
    // Se reemplaza la nota anterior en vez de acumular una por cada clic.
    const base = $('resultado').innerHTML.split('<br><span class="sub">')[0];
    $('resultado').innerHTML = base + '<br><span class="sub">Para actualizar sin cable (OTA) se usa el fichero <b>' +
      s.extra.nombre + '</b>, pero eso se hace desde la app oficial de Meshtastic, no desde aquí.</span>';
  }
}
