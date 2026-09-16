#include "SafeFile.h"

#include <cstring> // strcmp/strrchr: se usan para identificar el fichero temporal abierto

#ifdef FSCom

// Only way to work on both esp32 and nrf52
static File openFile(const char *filename, bool fullAtomic)
{
    concurrency::LockGuard g(spiLock);
    LOG_DEBUG("Opening %s, fullAtomic=%d", filename, fullAtomic);

    String filenameTmp = filename;
    filenameTmp += ".tmp";

    // NAVARICO (15/09/2026): en nRF52 esto hacia un borrado + escritura directa sobre el fichero
    // bueno, saltandose la verificacion y la atomicidad. Ahora TODAS las plataformas intentan el
    // camino con fichero temporal.
    if (!fullAtomic) {
        // Liberar hueco antes de crear el temporal, por si el sistema de ficheros es justo.
        // (Este borrado es del diseno original de upstream: el fichero bueno se sustituye al final.)
        FSCom.remove(filename);
    }

    // OJO nRF52 (fallo cazado en auditoria, 15/09/2026): FILE_O_WRITE NO trunca en nRF52
    // (Adafruit_LittleFS_File.cpp:56-57 usa LFS_O_RDWR|LFS_O_CREAT, sin LFS_O_TRUNC, y :77 hace
    // seek al FINAL). Si quedara un .tmp de un intento anterior, el contenido nuevo se AÑADIRIA al
    // viejo: testReadback() leeria los dos y el hash no cuadraria -> falso fallo -> FSCom.format().
    // Borrarlo antes es obligatorio (upstream lo tenia como FIXME sin hacer).
    FSCom.remove(filenameTmp.c_str());

    // clear any previous LFS errors
    File tmp = FSCom.open(filenameTmp.c_str(), FILE_O_WRITE);
    if (tmp) {
        return tmp;
    }

    // RED DE SEGURIDAD (nRF52): sin hueco para el temporal, se conserva el metodo tradicional en
    // vez de dejar el nodo sin poder guardar nada. En ESP32/RP2040 no se llega aqui en la practica.
#ifdef ARCH_NRF52
    // El remove previo es OBLIGATORIO en nRF52: alli FILE_O_WRITE NO trunca, asi que sin el se
    // escribiria al FINAL del fichero bueno -> contenido viejo + nuevo concatenados.
    // En el resto de plataformas NO se borra (FILE_O_WRITE es "w" y trunca al abrir), porque el
    // remove no aporta nada y destruiria la copia buena ANTES de saber si la apertura funciona.
    // Auditoria 15/09/2026 (F7): la guarda es NRF52, no "todo menos ESP32": en RP2040/STM32WL/
    // Portduino FILE_O_WRITE tambien trunca, asi que la condicion anterior abria ventana de mas.
    FSCom.remove(filename);
#endif
    LOG_WARN("SafeFile: no se pudo abrir '%s' para escritura verificable; escritura directa", filenameTmp.c_str());
    return FSCom.open(filename, FILE_O_WRITE);
}

SafeFile::SafeFile(const char *_filename, bool fullAtomic)
    : filename(_filename), f(openFile(_filename, fullAtomic)), fullAtomic(fullAtomic)
{
    // Camino verificable = lo que se abrio es REALMENTE el fichero temporal. Se pregunta al propio
    // fichero por su nombre en vez de suponerlo, porque openFile() puede haber caido al metodo
    // tradicional (escritura directa) y entonces el contenido no se puede comprobar.
    // OJO (fallo cazado en auditoria, 15/09/2026): File::name() devuelve SOLO EL NOMBRE BASE, no la
    // ruta. nRF52: Adafruit_LittleFS_File.cpp:149-150 hace strrchr('/') y guarda lo que viene
    // despues. ESP32: vfs_api.cpp usa pathToFileName(). Se compara NOMBRE BASE contra NOMBRE BASE
    // (mismo criterio que el framework) y no "ruta.endsWith(nombre)", que seria siempre falso
    // porque la cadena corta no puede terminar en la larga. Tampoco se usa "ruta.endsWith(otra
    // ruta)" porque daria falso positivo entre ficheros que compartan sufijo.
    String tmpSuffix = filename;
    tmpSuffix += ".tmp";
    const char *slash = strrchr(tmpSuffix.c_str(), '/');
    const char *baseTmp = slash ? (slash + 1) : tmpSuffix.c_str();
    rollback = f && (strcmp(f.name(), baseTmp) == 0);
    if (f && !rollback)
        LOG_WARN("SafeFile: escritura directa (no verificable) en %s", filename.c_str());
}

size_t SafeFile::write(uint8_t ch)
{
    if (!f) {
        ok = false;
        return 0;
    }

    hash ^= ch;
    size_t written = f.write(ch);
    if (written != 1)
        ok = false;
    return written;
}

size_t SafeFile::write(const uint8_t *buffer, size_t size)
{
    if (!f) {
        ok = false;
        return 0;
    }

    for (size_t i = 0; i < size; i++) {
        hash ^= buffer[i];
    }
    size_t written = f.write((uint8_t const *)buffer, size); // This nasty cast is _IMPORTANT_ otherwise the correct adafruit method does
                                                             // not get used (they made a mistake in their typing)
    if (written != size)
        ok = false;
    return written;
}

/**
 * Atomically close the file (overwriting any old version) and readback the contents to confirm the hash matches
 *
 * @return false for failure
 */
bool SafeFile::close()
{
    if (!f)
        return false;

    if (rollback) {
        // Camino verificable: cerrar el temporal, COMPROBAR que se escribio bien de verdad, y solo
        // entonces sustituir el original con un rename atomico.
        spiLock->lock();
        f.close();
        spiLock->unlock();

        if (!ok) {
            LOG_ERROR("SafeFile: escritura incompleta en %s; se descarta el temporal", filename.c_str());
            FSCom.remove((filename + ".tmp").c_str());
            return false;
        }

        if (!testReadback()) {
            LOG_ERROR("SafeFile: la comprobacion de %s fallo; se descarta el temporal", filename.c_str());
            FSCom.remove((filename + ".tmp").c_str());
            return false;
        }

        // Sustituir el original por el temporal con un rename ATOMICO. NO se borra el original
        // antes: lfs_rename SOBRESCRIBE el destino por si mismo (rama prevexists en lfs.c), asi que
        // el borrado previo no aporta nada y abre una ventana en la que el fichero no existe: si el
        // rename fallara justo ahi, el fichero bueno ya no estaria y NADA promueve un .tmp a
        // fichero bueno -> se perderia.
        // (Auditoria externa 15/09/2026, hallazgo 1: este remove sobrevivia de la version anterior,
        //  en la que renameFile() copiaba y hacia falta borrar antes. Al pasar a lfs_rename dejo de
        //  ser necesario y se convirtio en el mismo error que ya se habia corregido en
        //  saveResiliencePrefs()/navaSetWasInSleep(). Es el patron "blindaje a medias": aplicar el
        //  arreglo en unos caminos y no en el que usa casi todo el firmware.)
        String filenameTmp = filename;
        filenameTmp += ".tmp";
        if (!renameFile(filenameTmp.c_str(), filename.c_str())) {
            // El fichero bueno SIGUE EN SU SITIO (no se borro): aqui no se pierde nada. El temporal
            // se conserva sin promover, pero el original intacto es lo que importa. openFile() lo
            // borrara en el siguiente intento de guardar este mismo fichero.
            LOG_ERROR("SafeFile: no se pudo sustituir %s; el original queda intacto y el contenido "
                      "nuevo en %s", filename.c_str(), filenameTmp.c_str());
            return false;
        }

        return true;
    }

    // Camino tradicional (no verificable): la red de seguridad de openFile() cuando no hay hueco
    // para el temporal. Se conserva tal cual para no dejar el nodo sin poder guardar nada.
    spiLock->lock();
    f.close();
    spiLock->unlock();

    // Auditoria 15/09/2026 (F6): SI se puede verificar aqui. En este camino se escribio directo
    // sobre el fichero real, asi que se lee de vuelta ESE fichero con el mismo hash acumulado en
    // write(). Antes se devolvia `ok` a secas y, como File::flush()/close() devuelven void en todas
    // las plataformas, un fallo diferido de littlefs (el ultimo bloque se escribe al cerrar) pasaba
    // por exito: un "guardado" que no estaba guardado.
    return ok && readbackMatches(filename.c_str());
}
/// Read our (closed) tempfile back in and compare the hash
bool SafeFile::testReadback()
{
    String filenameTmp = filename;
    filenameTmp += ".tmp";
    return readbackMatches(filenameTmp.c_str());
}

/// Compare the hash of an arbitrary file with the hash accumulated in write()
bool SafeFile::readbackMatches(const char *path)
{
    concurrency::LockGuard g(spiLock);

    auto f2 = FSCom.open(path, FILE_O_READ);
    if (!f2) {
        LOG_ERROR("Readback: no se pudo abrir %s", path);
        return false;
    }

    // Bucle ACOTADO por el tamaño del fichero. La version anterior era `while ((c = f2.read()) >= 0)`
    // y dependia de que read() devolviera NEGATIVO al llegar al final: si en alguna plataforma
    // devolviera 0 en vez de negativo, el bucle NO TERMINARIA y el nodo se colgaria (y un falso
    // fallo de esta comprobacion dispara el formateo de emergencia de NodeDB). Con el numero de
    // bytes por delante, el limite es el propio fichero y el bucle no puede quedarse dando vueltas.
    // Auditoria externa 15/09/2026, hallazgo 5.
    uint8_t test_hash = 0;
    uint32_t remaining = f2.size();
    while (remaining > 0) {
        int c = f2.read();
        if (c < 0) {
            // Final inesperado: el fichero es mas corto de lo que dice su tamaño -> no cuadra.
            break;
        }
        test_hash ^= (uint8_t)c;
        remaining--;
    }
    f2.close();

    if (remaining != 0) {
        LOG_ERROR("Readback: %s quedo corto (%u bytes sin leer)", path, (unsigned int)remaining);
        return false;
    }

    if (test_hash != hash) {
        LOG_ERROR("Readback failed hash mismatch");
        return false;
    }

    return true;
}

#endif