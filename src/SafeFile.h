#pragma once

#include "FSCommon.h"
#include "SPILock.h"
#include "configuration.h"

#ifdef FSCom

/**
 * This class provides 'safe'/paranoid file writing.
 *
 * Some of our filesystems (in particular the nrf52) may have bugs beneath our layer.  Therefore we want to
 * be very careful about how we write files.  This class provides a restricted (Stream only) writing API for writing to files.
 *
 * Notably:
 * - we keep a simple xor hash of all characters that were written.
 * - We do not allow seeking (because we want to maintain our hash)
 * - we provide an close() method which is similar to close but returns false if we were unable to successfully write the
 * file.  Also this method
 * - atomically replaces any old version of the file on the disk with our new file (after first rereading the file from the disk
 * to confirm the hash matches)
 * - Some files are super huge so we can't do the full atomic rename/copy (because of filesystem size limits).  If !fullAtomic
 * then we still do the readback to verify file is valid so higher level code can handle failures.
 *
 * NAVARICO (15/09/2026) - REESCRITURA CONTROLADA DEL COMPORTAMIENTO EN nRF52:
 * Antes, en ARCH_NRF52 se saltaba TODA la verificacion y la atomicidad: se borraba el fichero
 * original y se escribia encima (openFile) y close() devolvia true sin comprobar nada. Eso dejaba
 * DOS agujeros en los 26 nodos de montana: (a) un corte a mitad de escritura dejaba el fichero a
 * medias SIN copia buena, y (b) un fallo de escritura se reportaba como exito, dejando muerto el
 * reintento de NodeDB::saveToDisk().
 * Ahora nRF52 usa el MISMO camino que el resto (fichero .tmp -> hash de comprobacion -> rename atomico),
 * PERO con una red de seguridad obligatoria: si no se puede abrir el fichero temporal (tipicamente
 * por falta de hueco en el sistema de ficheros), se cae al metodo tradicional de escritura directa
 * en vez de fallar. Es decir: se intenta lo bueno y se conserva lo que ya funcionaba.
 * OJO nRF52: FILE_O_WRITE NO trunca (abre sin LFS_O_TRUNC y hace seek al final), asi que el .tmp hay
 * que BORRARLO antes de abrirlo o el contenido nuevo se anadiria al viejo.
 * (Corregido el 15/09/2026 tras auditoria: antes esta nota decia que renameFile() copiaba y borraba.
 *  Ya no: renameFile() usa el rename REAL de littlefs, que sobrescribe y es atomico en todas las
 *  plataformas. La version de copiar+borrar era la que traia los fallos graves.)
 */
class SafeFile : public Print
{
  public:
    explicit SafeFile(char const *filepath, bool fullAtomic = false);

    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buffer, size_t size);

    /**
     * Atomically close the file (deleting any old versions) and readback the contents to confirm the hash matches
     *
     * @return false for failure
     */
    bool close();

  private:
    /// Read our (closed) tempfile back in and compare the hash
    bool testReadback();

    /// Compare the hash of an arbitrary file with the hash accumulated in write()
    bool readbackMatches(const char *path);

    String filename;
    File f;
    bool fullAtomic;
    uint8_t hash = 0;
    bool rollback = false; // NAVARICO: true si se esta escribiendo en el fichero temporal (verificable)
    bool ok = true;        // NAVARICO: false si alguna escritura fallo (para poder ser honestos en close())
};

#endif