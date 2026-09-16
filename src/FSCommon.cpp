/**
 * @file FSCommon.cpp
 * @brief This file contains functions for common filesystem operations such as copying, renaming, listing and deleting files and
 * directories.
 *
 * The functions in this file are used to perform common filesystem operations such as copying, renaming, listing and deleting
 * files and directories. These functions are used in the Meshtastic-device project to manage files and directories on the
 * device's filesystem.
 *
 */
#include "FSCommon.h"
#include "SPILock.h"
#include "configuration.h"

// Software SPI is used by MUI so disable SD card here until it's also implemented
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
#include <SD.h>
#include <SPI.h>

#ifdef SDCARD_USE_SPI1
SPIClass SPI_HSPI(HSPI);
#define SDHandler SPI_HSPI
#else
#define SDHandler SPI
#endif

#ifndef SD_SPI_FREQUENCY
#define SD_SPI_FREQUENCY 4000000U
#endif

#endif // HAS_SDCARD

/**
 * @brief Copies a file from one location to another.
 *
 * @param from The path of the source file.
 * @param to The path of the destination file.
 * @return true if the file was successfully copied, false otherwise.
 *
 * NAVARICO (15/09/2026, tras auditoria): renameFile() YA NO usa esta funcion (ahora usa el rename
 * real de littlefs, que es atomico). Se conserva porque es API publica declarada en FSCommon.h.
 * OJO: NO puede detectar un fallo diferido del volcado final, porque File::flush() y File::close()
 * devuelven void en TODAS las plataformas (littlefs escribe el ultimo bloque al cerrar): por eso no
 * sirve como base de una sustitucion atomica. La comprobacion de write() de abajo si es correcta.
 */
bool copyFile(const char *from, const char *to)
{
#ifdef FSCom
    // take SPI Lock
    concurrency::LockGuard g(spiLock);
    unsigned char cbuffer[16];

    File f1 = FSCom.open(from, FILE_O_READ);
    if (!f1) {
        LOG_ERROR("Failed to open source file %s", from);
        return false;
    }

    File f2 = FSCom.open(to, FILE_O_WRITE);
    if (!f2) {
        LOG_ERROR("Failed to open destination file %s", to);
        return false;
    }

    // Bucle ACOTADO por el tamaño del origen (no por available()): si read() no devolviera un valor
    // negativo al llegar al final, `while (f1.available() > 0)` podria no terminar nunca. Con el
    // numero de bytes por delante, el limite es el propio fichero y no puede quedarse dando vueltas.
    // Auditoria externa 15/09/2026, hallazgo 5 (aplicado aqui por el mismo motivo que en SafeFile).
    uint32_t remaining = f1.size();
    while (remaining > 0) {
        byte i = f1.read(cbuffer, 16);
        if (i == 0) {
            // Lectura corta inesperada: no se puede copiar el resto, se aborta sin borrar el origen.
            LOG_ERROR("copyFile: lectura corta en %s", from);
            f2.close();
            f1.close();
            return false;
        }
        if (i > remaining) {
            i = (byte)remaining;
        }
        // NAVARICO (15/09/2026): comprobar la escritura. Antes se ignoraba, asi que una copia
        // corta (destino sin hueco) terminaba el bucle, f2.flush()/close() tampoco se comprobaban,
        // y copyFile() devolvia true -> quien la llamara borraba el ORIGEN y el llamante creia haber
        // guardado. El resultado era un fichero TRUNCADO presentado como exito.
        // OJO: copyFile() YA NO la usa nadie (renameFile usa el rename real de littlefs). Sigue
        // siendo API publica, pero NO es base valida para una sustitucion atomica, porque el volcado
        // final (flush/close, que devuelven void) no se puede comprobar desde aqui.
        if (f2.write(cbuffer, i) != i) {
            LOG_ERROR("copyFile: escritura incompleta en %s", to);
            f2.close();
            f1.close();
            return false;
        }
        remaining -= i;
    }

    f2.flush();
    f2.close();
    f1.close();
    return true;
#endif
}

/**
 * Renames a file from pathFrom to pathTo.
 *
 * @param pathFrom The original path of the file.
 * @param pathTo The new path of the file.
 *
 * @return True if the file was successfully renamed, false otherwise.
 */
bool renameFile(const char *pathFrom, const char *pathTo)
{
#ifdef FSCom
    // NAVARICO (15/09/2026, auditoria): se usa el rename REAL de littlefs en TODAS las plataformas.
    // Antes, fuera de ESP32 se hacia `copyFile() && FSCom.remove(origen)`, y eso traia tres fallos:
    //   - Si la copia iba bien pero el remove del origen fallaba (lfs_remove puede dar NOSPC justo
    //     despues de que la copia consuma el ultimo hueco), devolvia FALSE: un guardado CORRECTO se
    //     reportaba como fallo y el llamante (NodeDB::saveToDisk) reaccionaba con FSCom.format(),
    //     borrando /prefs, /resilience.bin y las claves admin.
    //   - copyFile no puede comprobar el volcado final: File::flush()/close() devuelven void, asi
    //     que un fallo diferido de littlefs (el ultimo bloque se escribe al cerrar) pasaba como exito.
    //   - La copia necesita 2N de espacio (origen + destino) en una particion de ~28 KB, y borra el
    //     original ANTES de copiar: un corte ahi deja el fichero perdido.
    // lfs_rename SOBRESCRIBE el destino y es atomico (rama prevexists en lfs.c), asi que no hace
    // falta ni copiar ni borrar. Es ademas lo que hace la otra rama del proyecto.
    spiLock->lock();
    bool result = FSCom.rename(pathFrom, pathTo);
    spiLock->unlock();
    return result;
#endif
}

#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

#ifdef FSCom
namespace
{
bool pathEndsWithDot(const char *path)
{
    if (!path)
        return false;

    size_t length = strlen(path);
    return length > 0 && path[length - 1] == '.';
}

bool copyFilePath(char *dest, size_t destSize, const char *path, bool *wasLimited)
{
    if (!path || destSize == 0) {
        if (wasLimited)
            *wasLimited = true;
        return false;
    }

    if (strlcpy(dest, path, destSize) >= destSize) {
        if (wasLimited)
            *wasLimited = true;
        return false;
    }

    return true;
}

void collectFiles(const char *dirname, uint8_t levels, size_t maxCount, std::vector<meshtastic_FileInfo> &filenames,
                  bool *wasLimited)
{
    if (!dirname)
        return;

    File root = FSCom.open(dirname, FILE_O_READ);
    if (!root)
        return;
    if (!root.isDirectory()) {
        root.close();
        return;
    }

    File file = root.openNextFile();
    // file.name()[0] check is a workaround for a bug in the Adafruit LittleFS nrf52 glue (see issue 4395)
    while (file && file.name()[0]) {
        if (filenames.size() >= maxCount) {
            if (wasLimited)
                *wasLimited = true;
            file.close();
            break;
        }
        const char *fileName = file.name();
        if (file.isDirectory() && !pathEndsWithDot(fileName)) {
            char pathBuffer[sizeof(((meshtastic_FileInfo *)nullptr)->file_name)] = {};
#ifdef ARCH_ESP32
            const char *subDirPath = file.path();
#else
            const char *subDirPath = fileName;
#endif
            bool hasSubDirPath = copyFilePath(pathBuffer, sizeof(pathBuffer), subDirPath, wasLimited);
            file.close();

            if (levels && hasSubDirPath) {
                collectFiles(pathBuffer, levels - 1, maxCount, filenames, wasLimited);
            } else if (wasLimited) {
                *wasLimited = true;
            }
        } else {
            meshtastic_FileInfo fileInfo = {"", static_cast<uint32_t>(file.size())};
#ifdef ARCH_ESP32
            bool hasFilePath = copyFilePath(fileInfo.file_name, sizeof(fileInfo.file_name), file.path(), wasLimited);
#else
            bool hasFilePath = copyFilePath(fileInfo.file_name, sizeof(fileInfo.file_name), file.name(), wasLimited);
#endif
            if (hasFilePath && !pathEndsWithDot(fileInfo.file_name)) {
                filenames.push_back(fileInfo);
            }
            file.close();
        }
        file = root.openNextFile();
    }
    root.close();
}
} // namespace
#endif

// Callers must hold the SPI lock; recursion prevents taking it here.
std::vector<meshtastic_FileInfo> getFiles(const char *dirname, uint8_t levels, size_t maxCount, bool *wasLimited)
{
    std::vector<meshtastic_FileInfo> filenames = {};
    if (wasLimited)
        *wasLimited = false;
#ifdef FSCom
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    size_t reservedCount = maxCount;
    while (reservedCount > 0) {
        try {
            filenames.reserve(reservedCount);
            break;
        } catch (const std::bad_alloc &) {
            reservedCount /= 2;
        } catch (const std::length_error &) {
            reservedCount /= 2;
        }
    }
    if (reservedCount == 0) {
        if (wasLimited)
            *wasLimited = true;
        return filenames;
    }
    if (reservedCount < maxCount) {
        if (wasLimited)
            *wasLimited = true;
        maxCount = reservedCount;
    }
#endif
    collectFiles(dirname, levels, maxCount, filenames, wasLimited);
#endif
    return filenames;
}

/**
 * Lists the contents of a directory.
 * We can't use SPILOCK here because of recursion. Callers of this function should use SPILOCK.
 *
 * @param dirname The name of the directory to list.
 * @param levels The number of levels of subdirectories to list.
 * @param del Whether or not to delete the contents of the directory after listing.
 */
void listDir(const char *dirname, uint8_t levels, bool del)
{
#ifdef FSCom
#if (defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO))
    char buffer[255];
#endif
    File root = FSCom.open(dirname, FILE_O_READ);
    if (!root) {
        return;
    }
    if (!root.isDirectory()) {
        return;
    }

    File file = root.openNextFile();
    while (
        file &&
        file.name()[0]) { // This file.name() check is a workaround for a bug in the Adafruit LittleFS nrf52 glue (see issue 4395)
        if (file.isDirectory() && !String(file.name()).endsWith(".")) {
            if (levels) {
#ifdef ARCH_ESP32
                listDir(file.path(), levels - 1, del);
                if (del) {
                    LOG_DEBUG("Remove %s", file.path());
                    strncpy(buffer, file.path(), sizeof(buffer));
                    file.close();
                    FSCom.rmdir(buffer);
                } else {
                    file.close();
                }
#elif (defined(ARCH_RP2040) || defined(ARCH_PORTDUINO))
                listDir(file.name(), levels - 1, del);
                if (del) {
                    LOG_DEBUG("Remove %s", file.name());
                    strncpy(buffer, file.name(), sizeof(buffer));
                    file.close();
                    FSCom.rmdir(buffer);
                } else {
                    file.close();
                }
#else
                LOG_DEBUG(" %s (directory)", file.name());
                listDir(file.name(), levels - 1, del);
                file.close();
#endif
            }
        } else {
#ifdef ARCH_ESP32
            if (del) {
                LOG_DEBUG("Delete %s", file.path());
                strncpy(buffer, file.path(), sizeof(buffer));
                file.close();
                FSCom.remove(buffer);
            } else {
                LOG_DEBUG(" %s (%i Bytes)", file.path(), file.size());
                file.close();
            }
#elif (defined(ARCH_RP2040) || defined(ARCH_PORTDUINO))
            if (del) {
                LOG_DEBUG("Delete %s", file.name());
                strncpy(buffer, file.name(), sizeof(buffer));
                file.close();
                FSCom.remove(buffer);
            } else {
                LOG_DEBUG(" %s (%i Bytes)", file.name(), file.size());
                file.close();
            }
#else
            LOG_DEBUG("   %s (%i Bytes)", file.name(), file.size());
            file.close();
#endif
        }
        file = root.openNextFile();
    }
#ifdef ARCH_ESP32
    if (del) {
        LOG_DEBUG("Remove %s", root.path());
        strncpy(buffer, root.path(), sizeof(buffer));
        root.close();
        FSCom.rmdir(buffer);
    } else {
        root.close();
    }
#elif (defined(ARCH_RP2040) || defined(ARCH_PORTDUINO))
    if (del) {
        LOG_DEBUG("Remove %s", root.name());
        strncpy(buffer, root.name(), sizeof(buffer));
        root.close();
        FSCom.rmdir(buffer);
    } else {
        root.close();
    }
#else
    root.close();
#endif
#endif
}

/**
 * @brief Removes a directory and all its contents.
 *
 * This function recursively removes a directory and all its contents, including subdirectories and files.
 *
 * @param dirname The name of the directory to remove.
 */
void rmDir(const char *dirname)
{
#ifdef FSCom

#if (defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO))
    listDir(dirname, 10, true);
#elif defined(ARCH_NRF52)
    // nRF52 implementation of LittleFS has a recursive delete function
    FSCom.rmdir_r(dirname);
#endif

#endif
}

/**
 * Some platforms (nrf52) might need to do an extra step before FSBegin().
 */
__attribute__((weak, noinline)) void preFSBegin() {}

void fsInit()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    preFSBegin();
    if (!FSBegin()) {
        LOG_ERROR("Filesystem mount failed");
        // assert(0); This auto-formats the partition, so no need to fail here.
    }
#if defined(ARCH_ESP32)
    LOG_DEBUG("Filesystem files (%d/%d Bytes):", FSCom.usedBytes(), FSCom.totalBytes());
#else
    LOG_DEBUG("Filesystem files:");
#endif
    listDir("/", 10);
#endif
}

/**
 * Initializes the SD card and mounts the file system.
 */
void setupSDCard()
{
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    concurrency::LockGuard g(spiLock);
    SDHandler.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    if (!SD.begin(SDCARD_CS, SDHandler, SD_SPI_FREQUENCY)) {
        LOG_DEBUG("No SD_MMC card detected");
        return;
    }
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        LOG_DEBUG("No SD_MMC card attached");
        return;
    }
    LOG_DEBUG("SD_MMC Card Type: ");
    if (cardType == CARD_MMC) {
        LOG_DEBUG("MMC");
    } else if (cardType == CARD_SD) {
        LOG_DEBUG("SDSC");
    } else if (cardType == CARD_SDHC) {
        LOG_DEBUG("SDHC");
    } else {
        LOG_DEBUG("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    LOG_DEBUG("SD Card Size: %lu MB", (uint32_t)cardSize);
    LOG_DEBUG("Total space: %lu MB", (uint32_t)(SD.totalBytes() / (1024 * 1024)));
    LOG_DEBUG("Used space: %lu MB", (uint32_t)(SD.usedBytes() / (1024 * 1024)));
#endif
}
